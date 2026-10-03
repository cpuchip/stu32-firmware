/* vectors: runs a work unit's acceptance vectors against the core.
 *
 *   build/vectors work/001-vectors.txt
 *
 * Line format: ID | source | keys | expectations. Every vector starts from ab_init().
 * Numbers in expectations are compared by this file's own normalizer (sign, significant
 * digits, exponent of the leading digit), not by the engine's parser, so a parser bug
 * cannot hide itself. "=" is exact equality; "~" compares after rounding both sides to
 * 12 significant digits (half away from zero, on the digit strings); "^" means within 8
 * units in the 34th significant digit of the value (unit 003 amendment 1). The expectation
 * ERR means at least one operation in the vector must return an error; without it, any
 * error fails the vector.
 *
 * Exit status: 0 when every expectation passes, 1 when any fails, 2 on a malformed file.
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"

#include "calc.h"
#include "norm.h"

#define LINE_MAX_ 16384
static const struct { const char *name; ab_op op; } NAMED[] = {
    {"E", AB_KE}, {"CHS", AB_KCHS}, {"BKSP", AB_KBKSP}, {"ENTER", AB_ENTER},
    {"RDN", AB_RDN}, {"RUP", AB_RUP}, {"XY", AB_XY}, {"LASTX", AB_LASTX},
    {"CLX", AB_CLX}, {"CLSTK", AB_CLSTK},
    {"+", AB_ADD}, {"-", AB_SUB}, {"*", AB_MUL}, {"/", AB_DIV},
    {"EXP", AB_EXP}, {"LN", AB_LN}, {"TENX", AB_TENX}, {"LOG", AB_LOG},
    {"IDIV", AB_IDIV}, {"RMDR", AB_RMDR},
    {"SQ", AB_SQ}, {"SQRT", AB_SQRT}, {"INV", AB_INV}, {"POW", AB_POW}, {"XROOT", AB_XROOT},
    {"PI", AB_PI}, {"DEG", AB_DEG}, {"RAD", AB_RAD}, {"GRAD", AB_GRAD},
    {"SIN", AB_SIN}, {"COS", AB_COS}, {"TAN", AB_TAN},
    {"ASIN", AB_ASIN}, {"ACOS", AB_ACOS}, {"ATAN", AB_ATAN},
    {"SINH", AB_SINH}, {"COSH", AB_COSH}, {"TANH", AB_TANH},
    {"ASINH", AB_ASINH}, {"ACOSH", AB_ACOSH}, {"ATANH", AB_ATANH},
    {"PCT", AB_PCT}, {"PCTCHG", AB_PCTCHG},
    {"HMS", AB_HMS}, {"HMSINV", AB_HMSINV}, {"TODEG", AB_TODEG}, {"TORAD", AB_TORAD},
    {"TOKG", AB_TOKG}, {"TOLB", AB_TOLB}, {"TOC", AB_TOC}, {"TOF", AB_TOF},
    {"TOCM", AB_TOCM}, {"TOIN", AB_TOIN}, {"TOL", AB_TOL}, {"TOGAL", AB_TOGAL},
    {"TOKM", AB_TOKM}, {"TOMILE", AB_TOMILE}, {"FACT", AB_FACT}, {"NCR", AB_NCR}, {"NPR", AB_NPR},
    {"RAND", AB_RAND}, {"SEED", AB_SEED}, {"IP", AB_IP}, {"FP", AB_FP}, {"INTG", AB_INTG},
    {"RND", AB_RND}, {"ABS", AB_ABS}, {"SGN", AB_SGN}, {"TOPOL", AB_TOPOL}, {"TOREC", AB_TOREC},
    {"ALL", AB_SETALL},
    {"DEC", AB_DEC}, {"HEX", AB_HEX}, {"OCT", AB_OCT}, {"BIN", AB_BIN},
    {"HA", AB_KA}, {"HB", AB_KB}, {"HC", AB_KC}, {"HD", AB_KD}, {"HE", AB_KE_HEX}, {"HF", AB_KF},
    {"CI", AB_KI}, {"CA", AB_KTHETA}, {"XIY", AB_XIY}, {"RTA", AB_RTA}, {"ARG", AB_ARG},
    {"[", AB_KVEC}, {",", AB_KCOMMA},
    {"PRGM", AB_PRGM}, {"RTN", AB_RTN}, {"RS", AB_RS}, {"PSE", AB_PSE}, {"SST", AB_SST},
    {"BST", AB_BST}, {"CLPGM", AB_CLPGM}, {"C", AB_CANCEL},
    {"EQN", AB_EQN}, {"SPACE", AB_KSPACE}, {"=", AB_KEQ}, {"()", AB_KPAREN}, {"<", AB_KLEFT}, {">", AB_KRIGHT},
    {"AND", AB_AND}, {"XOR", AB_XOR}, {"OR", AB_OR}, {"NOT", AB_NOT}, {"NAND", AB_NAND}, {"NOR", AB_NOR},
    {"SIGMA+", AB_SIGMAPLUS}, {"SIGMA-", AB_SIGMAMINUS}, {"CLSIGMA", AB_CLSIGMA},
    {"XBAR", AB_XBAR}, {"YBAR", AB_YBAR}, {"XWBAR", AB_XWBAR}, {"SX", AB_SX}, {"SY", AB_SY},
    {"PSX", AB_PSX}, {"PSY", AB_PSY}, {"XHAT", AB_XHAT}, {"YHAT", AB_YHAT}, {"CORR", AB_CORR},
    {"SLOPE", AB_SLOPE}, {"YINT", AB_YINT}, {"NSUM", AB_NSUM}, {"SUMX", AB_SUMX},
    {"SUMY", AB_SUMY}, {"SUMX2", AB_SUMX2}, {"SUMY2", AB_SUMY2}, {"SUMXY", AB_SUMXY},
};

/* The last error any key returned in the current vector, for %ERR (unit 012). */
static abn_status last_err;
/* X as SAVEX found it, for %SAMEX (unit 014: an equation against the keyboard path). */
static ab_val saved_x;

static abn_status do1(ab_calc *c, ab_op op, int arg)
{
    abn_status st = ab_do_arg(c, op, arg);
    if (st != ABN_OK) last_err = st;
    return st;
}

static const char *const STATUS_NAMES[] = {"OK", "DIV_ZERO", "RANGE", "BAD_TEXT", "DOMAIN", "INEXACT",
                                           "CLAMPED", "NONEXISTENT", "XEQ_OVERFLOW", "DUP_LABEL",
                                           "MEMORY_FULL", "NONPROGRAMMABLE", "BUDGET", "NO_ROOT",
                                           "SELECT_FN", "NESTED", "SYNTAX", "INVALID_VAR",
                                           "NO_SOLUTION", "MULT_SOLUTION"};

/* Applies one key token. Returns 0, or -1 for an unknown token. */
static int apply_token(ab_calc *c, const char *tok, int *errors)
{
    for (size_t i = 0; i < sizeof NAMED / sizeof NAMED[0]; i++) {
        if (strcmp(tok, NAMED[i].name) == 0) {
            if (do1(c, NAMED[i].op, 0) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    static const struct { const char *prefix; ab_op op; } STORE[] = {
        {"STO:", AB_STO}, {"RCL:", AB_RCL}, {"STO+:", AB_STOADD}, {"STO-:", AB_STOSUB},
        {"STO*:", AB_STOMUL}, {"STO/:", AB_STODIV}, {"RCL+:", AB_RCLADD}, {"RCL-:", AB_RCLSUB},
        {"RCL*:", AB_RCLMUL}, {"RCL/:", AB_RCLDIV}, {"XVAR:", AB_XVAR}, {"VIEW:", AB_VIEW},
        {"INPUT:", AB_INPUT}, {"SOLVE:", AB_SOLVE}, {"INTEG:", AB_INTEG}};
    for (size_t i = 0; i < sizeof STORE / sizeof STORE[0]; i++) {
        size_t n = strlen(STORE[i].prefix);
        if (strncmp(tok, STORE[i].prefix, n) == 0) {
            const char *v = tok + n;
            int code;
            if (v[0] >= 'A' && v[0] <= 'Z' && v[1] == '\0') code = AB_VAR_A + (v[0] - 'A');
            else if (strcmp(v, "(I)") == 0) code = AB_VAR_IND_I;
            else if (strcmp(v, "(J)") == 0) code = AB_VAR_IND_J;
            else return -1;
            if (do1(c, STORE[i].op, code) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    static const struct { const char *name; ab_op op; int arg; } FRACT[] = {
        {"FDISP", AB_FDISP, 0}, {"/C", AB_SETC, 0}, {"RCL/C", AB_RCLC, 0}};
    for (size_t i = 0; i < sizeof FRACT / sizeof FRACT[0]; i++) {
        if (strcmp(tok, FRACT[i].name) == 0) {
            if (do1(c, FRACT[i].op, FRACT[i].arg) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    /* unit 011: program keys. LBL:A; XEQ:A or XEQ:A005; GTO:A or GTO:A005; GTO.:A005 (the
       pointer-only form); GTO.. (PRGM TOP); SF:n CF:n FS?:n; ISG:A DSE:(I); X<Y? X=0? etc. */
    {
        static const struct { const char *prefix; ab_op op; } TGT[] = {
            {"XEQ:", AB_XEQ}, {"GTO.:", AB_GTOPOS}, {"GTO:", AB_GTO}};
        for (size_t i = 0; i < sizeof TGT / sizeof TGT[0]; i++) {
            size_t n = strlen(TGT[i].prefix);
            if (strncmp(tok, TGT[i].prefix, n) == 0) {
                const char *v = tok + n;
                if (strcmp(v, "(I)") == 0 || strcmp(v, "(J)") == 0) {
                    /* XEQ (I), GTO (I): the label the index names (unit 012) */
                    if (do1(c, TGT[i].op, v[1] == 'I' ? AB_TGT_IND_I : AB_TGT_IND_J) != ABN_OK) (*errors)++;
                    return 0;
                }
                if (v[0] < 'A' || v[0] > 'Z') return -1;
                int line = v[1] ? atoi(v + 1) : 0;
                if (v[1] && (strlen(v + 1) != 3 || line < 1)) return -1;
                if (do1(c, TGT[i].op, (v[0] - 'A') * 1000 + line) != ABN_OK) (*errors)++;
                return 0;
            }
        }
        if (strncmp(tok, "REG:", 4) == 0 && tok[4] && strchr("XYZT", tok[4]) && !tok[5]) {
            /* in an equation: R-down's menu, the register chosen (unit 015) */
            if (do1(c, AB_KREG, (int)(strchr("XYZT", tok[4]) - "XYZT")) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "SOLVE") == 0) {      /* SOLVE with no letter: a built-in solver shown */
            if (do1(c, AB_SOLVE, -1) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "RSALL") == 0) {       /* answer every equation prompt with R/S (runner) */
            while (c->eqn_pending && c->stop == AB_STOP_INPUT)
                if (do1(c, AB_RS, 0) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "SAVEX") == 0) {       /* remember X (a runner token, not a key) */
            saved_x = c->x;
            return 0;
        }
        if (strcmp(tok, "XEQ") == 0) {         /* in Equation mode: evaluate the equation shown */
            if (do1(c, AB_XEQ, AB_TGT_EQN) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strncmp(tok, "EQ:", 3) == 0) {     /* an equation's text, added to the list (unit 014) */
            abn_status st = ab_eqn_add(c, tok + 3);
            if (st != ABN_OK) {
                (*errors)++;
                last_err = st;
            }
            return 0;
        }
        if (strncmp(tok, "FN=:", 4) == 0) {
            const char *v = tok + 4;
            int a;
            if (strcmp(v, "(I)") == 0) a = AB_TGT_IND_I;
            else if (strcmp(v, "(J)") == 0) a = AB_TGT_IND_J;
            else if (v[0] >= 'A' && v[0] <= 'Z' && !v[1]) a = v[0] - 'A';
            else return -1;
            if (do1(c, AB_FN, a) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strcmp(tok, "GTO..") == 0) {
            if (do1(c, AB_GTOPOS, AB_PC_TOP) != ABN_OK) (*errors)++;
            return 0;
        }
        if (strncmp(tok, "LBL:", 4) == 0) {
            if (tok[4] < 'A' || tok[4] > 'Z' || tok[5]) return -1;
            if (do1(c, AB_LBL, tok[4] - 'A') != ABN_OK) (*errors)++;
            return 0;
        }
        static const struct { const char *prefix; ab_op op; } FLG[] = {
            {"SF:", AB_SF}, {"CF:", AB_CF}, {"FS?:", AB_FS}};
        for (size_t i = 0; i < sizeof FLG / sizeof FLG[0]; i++) {
            size_t n = strlen(FLG[i].prefix);
            if (strncmp(tok, FLG[i].prefix, n) == 0 && isdigit((unsigned char)tok[n])) {
                if (do1(c, FLG[i].op, atoi(tok + n)) != ABN_OK) (*errors)++;
                return 0;
            }
        }
        static const struct { const char *prefix; ab_op op; } LOOP[] = {{"ISG:", AB_ISG}, {"DSE:", AB_DSE}};
        for (size_t i = 0; i < 2; i++) {
            if (strncmp(tok, LOOP[i].prefix, 4) == 0) {
                const char *v = tok + 4;
                int code;
                if (v[0] >= 'A' && v[0] <= 'Z' && v[1] == '\0') code = AB_VAR_A + (v[0] - 'A');
                else if (strcmp(v, "(I)") == 0) code = AB_VAR_IND_I;
                else if (strcmp(v, "(J)") == 0) code = AB_VAR_IND_J;
                else return -1;
                if (do1(c, LOOP[i].op, code) != ABN_OK) (*errors)++;
                return 0;
            }
        }
        static const char *const REL[] = {"!=", "<=", "<", ">", ">=", "="};
        if (tok[0] == 'X' && strlen(tok) >= 4 && tok[strlen(tok) - 1] == '?') {
            char rel[4] = "";
            char other = tok[strlen(tok) - 2];
            size_t rl = strlen(tok) - 3;
            if (rl < 1 || rl > 2 || (other != 'Y' && other != '0')) return -1;
            memcpy(rel, tok + 1, rl);
            rel[rl] = '\0';
            for (int r = 0; r < 6; r++) {
                if (strcmp(rel, REL[r]) == 0) {
                    if (do1(c, AB_TEST, r + (other == '0' ? 6 : 0)) != ABN_OK) (*errors)++;
                    return 0;
                }
            }
            return -1;
        }
    }
    if (strncmp(tok, "SUF:", 4) == 0) {
        int b = tok[4] == 'b' ? 2 : tok[4] == 'o' ? 8 : tok[4] == 'd' ? 10 : tok[4] == 'h' ? 16 : 0;
        if (!b || tok[5]) return -1;
        if (do1(c, AB_SUF, b) != ABN_OK) (*errors)++;
        return 0;
    }
    if (strcmp(tok, "CLVARS") == 0) {
        if (do1(c, AB_CLVARS, 0) != ABN_OK) (*errors)++;
        return 0;
    }
    if (strncmp(tok, "CONST:", 6) == 0) {
        int k = ab_const_index(tok + 6);
        if (k < 0) return -1;
        if (do1(c, AB_CONST, k) != ABN_OK) (*errors)++;
        return 0;
    }
    static const struct { const char *prefix; ab_op op; } SETTING[] = {
        {"FIX", AB_SETFIX}, {"SCI", AB_SETSCI}, {"ENG", AB_SETENG}};
    for (size_t i = 0; i < sizeof SETTING / sizeof SETTING[0]; i++) {
        size_t n = strlen(SETTING[i].prefix);
        if (strncmp(tok, SETTING[i].prefix, n) == 0 && isdigit((unsigned char)tok[n])) {
            int digits = atoi(tok + n);
            if (digits > AB_FMT_N_MAX) return -1;
            if (do1(c, SETTING[i].op, digits) != ABN_OK) (*errors)++;
            return 0;
        }
    }
    for (const char *p = tok; *p; p++)
        if (!isdigit((unsigned char)*p) && *p != '.') return -1;
    for (const char *p = tok; *p; p++) {
        ab_op op = (*p == '.') ? AB_KPOINT : (ab_op)(AB_K0 + (*p - '0'));
        if (do1(c, op, 0) != ABN_OK) (*errors)++;
    }
    return 0;
}

static const ab_val *reg(const ab_calc *c, char r)
{
    switch (r) {
    case 'X': return &c->x;
    case 'Y': return &c->y;
    case 'Z': return &c->z;
    case 'T': return &c->t;
    case 'L': return &c->lastx;
    default: return NULL;
    }
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

/* Runs one vectors file from f (opened by the caller, who closes it); name labels the report.
 * Returns 0 when every vector passes, 1 on a failure, 2 on a malformed file. */
int vectors_run(const char *name, FILE *f)
{
    char line[LINE_MAX_];
    int lineno = 0, vectors = 0, vectors_ok = 0, checks = 0, checks_ok = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        char *s = trim(line);
        if (!*s || *s == '#') continue;
        char *field[4];
        int nf = 0;
        for (char *p = s; nf < 4; nf++) {
            field[nf] = p;
            char *bar = strchr(p, '|');
            if (!bar) { nf++; break; }
            *bar = '\0';
            p = bar + 1;
        }
        if (nf != 4) {
            fprintf(stderr, "%s:%d: expected 4 fields\n", name, lineno);
            return 2;
        }
        char *id = trim(field[0]), *keys = trim(field[2]), *expect = trim(field[3]);

        static ab_calc c;                   /* 92 KB: off the stack, for a device task */
        ab_init(&c);
        int op_errors = 0;
        bool holdpse = false;
        last_err = ABN_OK;
        for (char *tok = strtok(keys, " \t"); tok; tok = strtok(NULL, " \t")) {
            if (strcmp(tok, "HOLDPSE") == 0) {
                holdpse = true;         /* see the PSE stops instead of resuming them */
                continue;
            }
            if (apply_token(&c, tok, &op_errors) != 0) {
                fprintf(stderr, "%s:%d: %s: unknown key token '%s'\n", name, lineno, id, tok);
                return 2;
            }
            /* a PSE stop resumes by itself after the device's pause (unit 012 rule 1): the runner
               plays the device */
            while (!holdpse && c.stop == AB_STOP_PSE)
                if (do1(&c, AB_RS, 0) != ABN_OK) op_errors++;
        }

        int want_error = 0;
        char *exps[64];
        int nexp = 0;
        for (char *ex = strtok(expect, " \t"); ex; ex = strtok(NULL, " \t")) {
            if (strcmp(ex, "ERR") == 0) want_error = 1;
            else if (nexp < 64) exps[nexp++] = ex;
        }
        int vec_ok = want_error ? (op_errors > 0) : (op_errors == 0);
        if (!vec_ok) {
            if (want_error) printf("FAIL %s: expected an error, none returned\n", id);
            else printf("FAIL %s: %d operation(s) returned an error\n", id, op_errors);
        }
        for (int k = 0; k < nexp; k++) {
            char *ex = exps[k];
            if (ex[0] == '%') {
                /* program state (unit 011): %PC=A005 or TOP; %GO:A004=A003 (that line's target);
                   %LINES=n; %YES=0|1 (the last test); %Fn=0|1 (flag n) */
                char *eq = strchr(ex, '=');
                char got[AB_EQN_LEN + 8] = "?";   /* the longest text compared: an equation */
                if (!eq) {
                    fprintf(stderr, "%s:%d: %s: bad expectation '%s'\n", name, lineno, id, ex);
                    return 2;
                }
                *eq = '\0';
                if (strcmp(ex, "%PC") == 0) {
                    ab_prog_addr(&c, c.pc, got);
                } else if (strncmp(ex, "%GO:", 4) == 0) {
                    int i = ab_prog_find(&c, ex + 4);
                    if (i >= 0 && (c.prog[i].op == AB_XEQ || c.prog[i].op == AB_GTO)) {
                        int a = c.prog[i].arg;
                        if (a / 1000 >= 26) snprintf(got, sizeof got, "NONE");
                        else snprintf(got, sizeof got, "%c%03d", 'A' + a / 1000, a % 1000);
                    }
                } else if (strcmp(ex, "%ERR") == 0) {
                    snprintf(got, sizeof got, "%s", (unsigned)last_err < sizeof STATUS_NAMES / sizeof STATUS_NAMES[0]
                                                         ? STATUS_NAMES[last_err] : "?");
                } else if (strcmp(ex, "%COVER") == 0) {
                    /* the uncertainty in Y covers the true error of X: |x - v| <= y (unit 013) */
                    abn v, d;
                    bool okc = abn_from_text(&v, eq + 1) == ABN_OK && c.x.kind == AB_REAL &&
                               abn_sub(&d, &c.x.re, &v) == ABN_OK;
                    d = abn_abs(&d);
                    snprintf(got, sizeof got, "%s", okc && abn_cmp(&d, &c.y.re) <= 0 ? eq + 1 : "not covered");
                } else if (strncmp(ex, "%EVALS<", 7) == 0) {
                    /* "%EVALS<=n" is written %EVALS<=n: the evaluations used, at most n */
                    snprintf(got, sizeof got, "%s", c.evals <= atol(eq + 1) ? eq + 1 : "more");
                } else if (strcmp(ex, "%XVAR") == 0) {
                    /* "%XVAR=A": variable A holds exactly what X holds (unit 013: SOLVE's root) */
                    int vi = eq[1] >= 'A' && eq[1] <= 'Z' ? eq[1] - 'A' : -1;
                    bool same = vi >= 0 && c.var[vi].kind == c.x.kind && abn_cmp(&c.var[vi].re, &c.x.re) == 0 &&
                                abn_cmp(&c.var[vi].im, &c.x.im) == 0;
                    snprintf(got, sizeof got, "%s", same ? eq + 1 : "different");
                } else if (strcmp(ex, "%STOP") == 0) {
                    ab_stop_text(&c, got);
                } else if (strcmp(ex, "%SAMEX") == 0) {
                    /* X is exactly what SAVEX remembered */
                    bool same = c.x.kind == saved_x.kind && abn_cmp(&c.x.re, &saved_x.re) == 0 &&
                                abn_cmp(&c.x.im, &saved_x.im) == 0;
                    snprintf(got, sizeof got, "%s", same ? eq + 1 : "different");
                } else if (strcmp(ex, "%MSG") == 0) {
                    /* the equation line shown as a message (flag 10, unit 015); blanks written _ */
                    bool msg = c.stop == AB_STOP_MSG || (c.stop == AB_STOP_PSE && c.stop_var == -2);
                    snprintf(got, sizeof got, "%s", msg ? c.peqn[c.prog[c.stop_line].arg] : "none");
                    for (char *q = got; *q; q++)
                        if (*q == ' ') *q = '_';
                } else if (strcmp(ex, "%EQ") == 0) {
                    /* the equation shown (unit 014) */
                    /* a built-in linear solver as its LCD name, blanks written _ (unit 015) */
                    const char *e = c.eqn_cur >= 0 ? c.eqn[c.eqn_cur] : "TOP";
                    snprintf(got, sizeof got, "%s", e[0] == '\x01' ? e + 1 : e);
                    for (char *q = got; *q; q++)
                        if (*q == ' ') *q = '_';
                } else if (strcmp(ex, "%EQN") == 0) {
                    int n = 0;                          /* the built-in solvers apart (unit 015) */
                    for (int q = 0; q < c.eqn_count; q++) n += c.eqn[q][0] != '\x01';
                    snprintf(got, sizeof got, "%d", n);
                } else if (strcmp(ex, "%ERRPOS") == 0) {
                    snprintf(got, sizeof got, "%d", c.eqn_errpos);
                } else if (strcmp(ex, "%LINES") == 0) {
                    snprintf(got, sizeof got, "%d", c.prog_len);
                } else if (strcmp(ex, "%YES") == 0) {
                    snprintf(got, sizeof got, "%d", c.yes ? 1 : 0);
                } else if (ex[1] == 'F' && isdigit((unsigned char)ex[2])) {
                    snprintf(got, sizeof got, "%d", ab_flag(&c, atoi(ex + 2)) ? 1 : 0);
                } else {
                    fprintf(stderr, "%s:%d: %s: bad expectation '%s'\n", name, lineno, id, ex);
                    return 2;
                }
                checks++;
                if (strcmp(got, eq + 1) == 0) {
                    checks_ok++;
                } else {
                    vec_ok = 0;
                    printf("FAIL %s: expected %s=%s, got %s\n", id, ex, eq + 1, got);
                }
                continue;
            }
            const ab_val *rv = reg(&c, ex[0]);
            const abn *r = rv ? &rv->re : NULL;
            ab_val var_val;
            char orig[96];
            snprintf(orig, sizeof orig, "%.*s", (int)sizeof orig - 1, ex);
            if (ex[0] == '@') {
                /* @A=v: variable A; @#n=v: unnamed indirect variable n (unit 005); any of the
                   comparisons = ~ ^ # after the name (unit 013) */
                char *eq = ex[1] == '#' ? strchr(ex, '=') : strpbrk(ex + 1, "=~^#");
                if (eq && ex[1] >= 'A' && ex[1] <= 'Z' && eq == ex + 2) {
                    var_val = c.var[ex[1] - 'A'];
                    rv = &var_val;
                    r = &var_val.re;
                } else if (eq && ex[1] == '#') {
                    int a = atoi(ex + 2);
                    r = (a >= 0 && a <= AB_IND_MAX) ? &c.ind[a] : NULL;
                    rv = NULL;
                }
                if (r) {
                    memmove(ex + 1, eq, strlen(eq) + 1);    /* now "@=v" */
                }
            }
            if (r && ex[1] == '#') {
                /* "X#v,t": |x - v| <= t (unit 013: results judged against a tolerance) */
                char vs[128], *comma = strchr(ex + 2, ',');
                char gt[ABN_TEXT_MAX];
                abn want, tol, d, got;
                if (!comma || (size_t)(comma - ex - 2) >= sizeof vs) {
                    fprintf(stderr, "%s:%d: %s: bad expectation '%s'\n", name, lineno, id, ex);
                    return 2;
                }
                memcpy(vs, ex + 2, (size_t)(comma - ex - 2));
                vs[comma - ex - 2] = '\0';
                bool okt = rv && rv->kind == AB_REAL && abn_from_text(&want, vs) == ABN_OK &&
                           abn_from_text(&tol, comma + 1) == ABN_OK;
                if (okt) {
                    got = *r;
                    okt = abn_sub(&d, &got, &want) == ABN_OK;
                    d = abn_abs(&d);
                    okt = okt && abn_cmp(&d, &tol) <= 0;
                }
                checks++;
                if (okt) {
                    checks_ok++;
                } else {
                    vec_ok = 0;
                    abn_to_text(r, gt, sizeof gt);
                    printf("FAIL %s: expected %s, got %s\n", id, orig, gt);
                }
                continue;
            }
            if (!r || (ex[1] != '=' && ex[1] != '~' && ex[1] != '^')) {
                fprintf(stderr, "%s:%d: %s: bad expectation '%s'\n", name, lineno, id, ex);
                return 2;
            }
            /* "[a,b,c]" is a vector expectation (unit 010): the register must hold a vector of that
               length; "=" compares each element exactly, "^" each within 8 units of its own 34th
               digit. */
            if (ex[2] == '[') {
                char buf[256], *el[4];
                int n = 0, okv = 1;
                size_t bl = strlen(ex + 3);
                snprintf(buf, sizeof buf, "%.*s", (int)sizeof buf - 1, ex + 3);
                if (bl == 0 || buf[bl - 1] != ']') {
                    fprintf(stderr, "%s:%d: %s: bad vector '%s'\n", name, lineno, id, ex + 2);
                    return 2;
                }
                buf[bl - 1] = '\0';
                for (char *q = strtok(buf, ","); q && n < 4; q = strtok(NULL, ",")) el[n++] = q;
                if (n < 1 || n > 3) {
                    fprintf(stderr, "%s:%d: %s: bad vector '%s'\n", name, lineno, id, ex + 2);
                    return 2;
                }
                okv = rv && rv->kind == AB_VECTOR && rv->len == n;
                for (int j = 0; okv && j < n; j++) {
                    const abn *g = j == 0 ? &rv->re : j == 1 ? &rv->im : &rv->p3;
                    char gt[ABN_TEXT_MAX];
                    norm w, gv;
                    if (normalize(el[j], &w) != 0) {
                        fprintf(stderr, "%s:%d: %s: bad element '%s'\n", name, lineno, id, el[j]);
                        return 2;
                    }
                    abn_to_text(g, gt, sizeof gt);
                    okv = normalize(gt, &gv) == 0 &&
                          (ex[1] == '^' ? norm_within_units(&w, &gv, 8) : norm_equal(&w, &gv));
                }
                checks++;
                if (okv) {
                    checks_ok++;
                } else {
                    char g[3][ABN_TEXT_MAX] = {"?", "?", "?"};
                    if (rv) {
                        abn_to_text(&rv->re, g[0], sizeof g[0]);
                        abn_to_text(&rv->im, g[1], sizeof g[1]);
                        abn_to_text(&rv->p3, g[2], sizeof g[2]);
                    }
                    vec_ok = 0;
                    printf("FAIL %s: expected %s, got %s (len %d) %s %s %s\n", id, orig,
                           !rv ? "?" : rv->kind == AB_VECTOR ? "vector" : rv->kind == AB_COMPLEX ? "complex" : "real",
                           rv ? rv->len : 0, g[0], g[1], g[2]);
                }
                continue;
            }
            /* "a i b" is a complex expectation (unit 009): the register must hold a complex number;
               "=" compares each part exactly, "^" each part within 8 units of the expected
               value's leading digit (normwise). A real expectation needs a real register. */
            char *ipos = strchr(ex + 2, 'i');
            int ok;
            if (ipos) {
                char re_s[128], im_s[128];
                size_t lr = (size_t)(ipos - (ex + 2));
                if (lr >= sizeof re_s) lr = sizeof re_s - 1;
                memcpy(re_s, ex + 2, lr);
                re_s[lr] = '\0';
                snprintf(im_s, sizeof im_s, "%.*s", (int)sizeof im_s - 1, ipos + 1);
                norm wr, wi, gr, gi;
                char t_re[ABN_TEXT_MAX], t_im[ABN_TEXT_MAX];
                if (normalize(re_s, &wr) != 0 || normalize(im_s, &wi) != 0) {
                    fprintf(stderr, "%s:%d: %s: bad complex '%s'\n", name, lineno, id, ex + 2);
                    return 2;
                }
                ok = rv && rv->kind == AB_COMPLEX;
                if (ok) {
                    abn_to_text(&rv->re, t_re, sizeof t_re);
                    abn_to_text(&rv->im, t_im, sizeof t_im);
                    ok = normalize(t_re, &gr) == 0 && normalize(t_im, &gi) == 0;
                }
                if (ok && ex[1] == '^') {
                    long lead = wr.zero ? (wi.zero ? 0 : wi.exp) : (wi.zero ? wr.exp : (wr.exp > wi.exp ? wr.exp : wi.exp));
                    ok = norm_within_unit_exp(&wr, &gr, 8, lead - 33) && norm_within_unit_exp(&wi, &gi, 8, lead - 33);
                } else if (ok) {
                    ok = norm_equal(&wr, &gr) && norm_equal(&wi, &gi);
                }
                checks++;
                if (ok) {
                    checks_ok++;
                } else {
                    vec_ok = 0;
                    char g1[ABN_TEXT_MAX] = "?", g2[ABN_TEXT_MAX] = "?";
                    if (rv) {
                        abn_to_text(&rv->re, g1, sizeof g1);
                        abn_to_text(&rv->im, g2, sizeof g2);
                    }
                    printf("FAIL %s: expected %s, got %s %si%s\n", id, orig,
                           rv && rv->kind == AB_COMPLEX ? "complex" : "real", g1, g2);
                }
                continue;
            }
            if (rv && rv->kind != AB_REAL) {
                checks++;
                vec_ok = 0;
                printf("FAIL %s: expected real %s, got a %s\n", id, orig,
                       rv->kind == AB_VECTOR ? "vector" : "complex number");
                continue;
            }
            char got_text[ABN_TEXT_MAX];
            abn_to_text(r, got_text, sizeof got_text);
            norm want, got;
            if (normalize(ex + 2, &want) != 0) {
                fprintf(stderr, "%s:%d: %s: bad number '%s'\n", name, lineno, id, ex + 2);
                return 2;
            }
            ok = normalize(got_text, &got) == 0;
            if (ok && ex[1] == '~') {
                round_norm(&want, 12);
                round_norm(&got, 12);
            }
            if (ex[1] == '^') ok = ok && norm_within_units(&want, &got, 8);
            else ok = ok && norm_equal(&want, &got);
            checks++;
            if (ok) {
                checks_ok++;
            } else {
                vec_ok = 0;
                printf("FAIL %s: expected %s, got %s\n", id, orig, got_text);
            }
        }
        vectors++;
        if (vec_ok) vectors_ok++;
        if (getenv("AB_EVALS")) fprintf(stderr, "%s evals %ld\n", id, c.evals);   /* unit 013 report */
    }
    printf("%s: %d/%d vectors, %d/%d expectations\n", name, vectors_ok, vectors,
           checks_ok, checks);
    if (getenv("AB_ARENA_PEAK")) fprintf(stderr, "arena peak %d\n", ar_peak());   /* unit 010b measurement */
    return (vectors_ok == vectors && checks_ok == checks) ? 0 : 1;
}

#ifndef AB_DEVICE
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s VECTORS_FILE\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    int r = vectors_run(argv[1], f);
    fclose(f);
    return r;
}
#endif
