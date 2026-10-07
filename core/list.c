/* list: a program line's text, as program entry shows it (unit 018; UG Appendix G, "This name is
 * the one used in program lines", G-1, with the listings of ch.13-14 where the two differ in
 * spacing: STO+ X (14-8, line T010), RCL- B (14-17), FS? 0 (14-13)). Where a function has a keyboard name and an
 * equation name (e^x and EXP, x^2 and SQ), a program line is the keyboard's (G lists both; the
 * equation names are 6-16's). Ours where the guide has no name: ->POL and ->REC (unit 004's
 * rectangular/polar pair), RCL /c (unit 007's token for 1 /c) and a target whose line was deleted
 * (----). Typed numbers are shown in
 * the display setting given (ALL in the current base when none): the guide's line A010 reads 10
 * with DEC set and 10b with BIN set (13-25). */
#include <stdio.h>
#include <string.h>

#include "calc.h"
#include "const.h"

/* the overline and the hat of x-bar and x-hat are combining marks after their letter */
#define BAR "\xcc\x84"
#define HAT "\xcc\x82"

static const char *const NAMES[AB_OP_LAST + 1] = {
    [AB_KCHS] = "+/-", [AB_ENTER] = "ENTER", [AB_RDN] = "R\xe2\x86\x93", [AB_RUP] = "R\xe2\x86\x91",
    [AB_XY] = "x<>y", [AB_LASTX] = "LASTx", [AB_CLX] = "CLx", [AB_CLSTK] = "CLSTK",
    [AB_ADD] = "+", [AB_SUB] = "-", [AB_MUL] = "\xc3\x97", [AB_DIV] = "\xc3\xb7",
    [AB_EXP] = "e\xcb\xa3", [AB_LN] = "LN", [AB_TENX] = "10\xcb\xa3", [AB_LOG] = "LOG",
    [AB_IDIV] = "INT\xc3\xb7", [AB_RMDR] = "RMDR",
    [AB_SQ] = "x\xc2\xb2", [AB_SQRT] = "\xe2\x88\x9ax", [AB_INV] = "1/x", [AB_POW] = "y\xcb\xa3",
    [AB_XROOT] = "\xcb\xa3\xe2\x88\x9ay",
    [AB_PI] = "\xcf\x80", [AB_DEG] = "DEG", [AB_RAD] = "RAD", [AB_GRAD] = "GRAD",
    [AB_SIN] = "SIN", [AB_COS] = "COS", [AB_TAN] = "TAN", [AB_ASIN] = "ASIN", [AB_ACOS] = "ACOS",
    [AB_ATAN] = "ATAN", [AB_SINH] = "SINH", [AB_COSH] = "COSH", [AB_TANH] = "TANH",
    [AB_ASINH] = "ASINH", [AB_ACOSH] = "ACOSH", [AB_ATANH] = "ATANH",
    [AB_PCT] = "%", [AB_PCTCHG] = "%CHG",
    [AB_HMS] = "\xe2\x86\x92HMS", [AB_HMSINV] = "HMS\xe2\x86\x92", [AB_TODEG] = "\xe2\x86\x92" "DEG",
    [AB_TORAD] = "\xe2\x86\x92RAD", [AB_TOKG] = "\xe2\x86\x92KG", [AB_TOLB] = "\xe2\x86\x92LB",
    [AB_TOC] = "\xe2\x86\x92\xc2\xb0" "C", [AB_TOF] = "\xe2\x86\x92\xc2\xb0" "F", [AB_TOCM] = "\xe2\x86\x92" "CM",
    [AB_TOIN] = "\xe2\x86\x92IN", [AB_TOL] = "\xe2\x86\x92L", [AB_TOGAL] = "\xe2\x86\x92GAL",
    [AB_TOKM] = "\xe2\x86\x92KM", [AB_TOMILE] = "\xe2\x86\x92MILE",
    [AB_FACT] = "!", [AB_NCR] = "nCr", [AB_NPR] = "nPr", [AB_RAND] = "RANDOM", [AB_SEED] = "SEED",
    [AB_IP] = "IP", [AB_FP] = "FP", [AB_INTG] = "INTG", [AB_RND] = "RND", [AB_ABS] = "ABS", [AB_SGN] = "SGN",
    [AB_TOPOL] = "\xe2\x86\x92POL", [AB_TOREC] = "\xe2\x86\x92REC",
    [AB_SETFIX] = "FIX", [AB_SETSCI] = "SCI", [AB_SETENG] = "ENG", [AB_SETALL] = "ALL",
    [AB_STO] = "STO", [AB_RCL] = "RCL", [AB_STOADD] = "STO+", [AB_STOSUB] = "STO-",
    [AB_STOMUL] = "STO\xc3\x97", [AB_STODIV] = "STO\xc3\xb7", [AB_RCLADD] = "RCL+", [AB_RCLSUB] = "RCL-",
    [AB_RCLMUL] = "RCL\xc3\x97", [AB_RCLDIV] = "RCL\xc3\xb7", [AB_XVAR] = "x<>", [AB_VIEW] = "VIEW",
    [AB_CLVARS] = "CLVARS",
    [AB_SIGMAPLUS] = "\xce\xa3+", [AB_SIGMAMINUS] = "\xce\xa3-", [AB_CLSIGMA] = "CL\xce\xa3",
    [AB_XBAR] = "x" BAR, [AB_YBAR] = "y" BAR, [AB_XWBAR] = "x" BAR "w", [AB_SX] = "sx", [AB_SY] = "sy",
    [AB_PSX] = "\xcf\x83x", [AB_PSY] = "\xcf\x83y", [AB_XHAT] = "x" HAT, [AB_YHAT] = "y" HAT,
    [AB_CORR] = "r", [AB_SLOPE] = "m", [AB_YINT] = "b",
    [AB_NSUM] = "n", [AB_SUMX] = "\xce\xa3x", [AB_SUMY] = "\xce\xa3y", [AB_SUMX2] = "\xce\xa3x\xc2\xb2",
    [AB_SUMY2] = "\xce\xa3y\xc2\xb2", [AB_SUMXY] = "\xce\xa3xy",
    [AB_SETC] = "/c", [AB_RCLC] = "RCL /c", [AB_SF] = "SF", [AB_CF] = "CF",
    [AB_DEC] = "DEC", [AB_HEX] = "HEX", [AB_OCT] = "OCT", [AB_BIN] = "BIN",
    [AB_AND] = "AND", [AB_XOR] = "XOR", [AB_OR] = "OR", [AB_NOT] = "NOT", [AB_NAND] = "NAND", [AB_NOR] = "NOR",
    [AB_XIY] = "xiy", [AB_RTA] = "r\xce\xb8" "a", [AB_ARG] = "ARG",
    [AB_LBL] = "LBL", [AB_RTN] = "RTN", [AB_XEQ] = "XEQ", [AB_GTO] = "GTO", [AB_RS] = "STOP", [AB_PSE] = "PSE", [AB_BEEP] = "BEEP",
    [AB_FS] = "FS?", [AB_ISG] = "ISG", [AB_DSE] = "DSE", [AB_INPUT] = "INPUT",
    [AB_FN] = "FN=", [AB_SOLVE] = "SOLVE", [AB_INTEG] = "\xe2\x88\xab" "FN d",
    [AB_EXPM1] = "e\xcb\xa3-1", [AB_LN1P] = "LN(1+x)", [AB_TWOX] = "2\xcb\xa3", [AB_LOG2] = "LOG2",
    [AB_CLVARX] = "CLVAR",
};

/* x?y and x?0, in the order of the menu (UG p.14-7) */
static const char *const TESTS[12] = {
    "x\xe2\x89\xa0y?", "x\xe2\x89\xa4y?", "x<y?", "x>y?", "x\xe2\x89\xa5y?", "x=y?",
    "x\xe2\x89\xa0" "0?", "x\xe2\x89\xa4" "0?", "x<0?", "x>0?", "x\xe2\x89\xa5" "0?", "x=0?",
};

/* (I) is the index of the line's mode: (i) under a 33s label (unit 025) */
static bool line_m35;

static void var_text(int code, char *out, size_t size)
{
    if (code >= AB_VAR_A && code < AB_VAR_A + 26) snprintf(out, size, "%c", 'A' + code - AB_VAR_A);
    else if (code == AB_VAR_IND_I) snprintf(out, size, line_m35 ? "(I)" : "(i)");
    else if (code == AB_VAR_IND_J) snprintf(out, size, "(J)");
    else if (code == AB_VAR_SI) snprintf(out, size, "i");
    else snprintf(out, size, "?");
}

static void target_text(int arg, char *out, size_t size)
{
    if (arg == AB_TGT_IND_I) snprintf(out, size, line_m35 ? "(I)" : "(i)");
    else if (arg == AB_TGT_IND_J) snprintf(out, size, "(J)");
    else if (arg >= 0 && arg / 1000 < 26) snprintf(out, size, "%c%03d", 'A' + arg / 1000, arg % 1000);
    else snprintf(out, size, "----");           /* its line was deleted (ours) */
}

int ab_prog_line(const ab_calc *c, int i, const ab_display *d, char *out, int size)
{
    char a[AB_EQN_LEN];
    if (size <= 0) return -1;
    out[0] = '\0';
    if (i < 0 || i >= c->prog_len) return -1;
    int op = c->prog[i].op, arg = c->prog[i].arg;
    {   /* the line's mode: its label's stamp, else the calculator's (unit 025) */
        int best = -1, pos = -1;
        for (int l = 0; l < 26; l++)
            if (c->labelpos[l] >= 0 && c->labelpos[l] <= i && c->labelpos[l] > pos) {
                pos = c->labelpos[l];
                best = l;
            }
        line_m35 = best >= 0 && c->lmode[best] ? c->lmode[best] != 33 : c->m35;   /* STU's as 35s */
    }
    if ((op == AB_LIT && (arg < 0 || arg >= AB_PROG_LITS)) || (op == AB_EQLINE && (arg < 0 || arg >= AB_PEQN_MAX))) {
        snprintf(out, (size_t)size, "?");       /* a pool index out of range: not a line the core makes */
        return -1;
    }
    if (op == AB_LIT) {                         /* a typed number, in the display given */
        ab_display all = {{AB_ALL, 0, 40, '.', false}, false, 4095, 0, c->base, c->cpolar, (int)c->angle};
        const ab_display *u = d ? d : &all;
        const ab_val *v = &c->lit[arg];
        int ind, n;
        if (v->kind == AB_COMPLEX) {
            n = ab_format_complex(&v->re, &v->im, u, a, sizeof a);
        } else if (v->kind == AB_VECTOR) {
            abn el[3] = {v->re, v->im, v->p3};
            n = ab_format_vector(el, v->len, u, a, sizeof a);
        } else {
            n = ab_format_display(&v->re, u, a, sizeof a, &ind);
        }
        if (n < 0) snprintf(a, sizeof a, "?");
        snprintf(out, (size_t)size, "%s", a);
        return (int)strlen(out);
    }
    if (op == AB_EQLINE) {                      /* an equation line: its text (UG p.13-7) */
        snprintf(out, (size_t)size, "%s%s", c->peqn[arg], ab_eqn_mark(c, c->peqn[arg], c->peqn_model[arg]));
        return (int)strlen(out);                /* (the mark: unit 031 rule 3) */
    }
    if (op == AB_TEST) {
        snprintf(out, (size_t)size, "%s", arg >= 0 && arg < 12 ? TESTS[arg] : "?");
        return (int)strlen(out);
    }
    if (op == AB_CONST) {
        const char *n = ab_const_name(arg);
        snprintf(out, (size_t)size, "%s", n ? n : "?");
        return (int)strlen(out);
    }
    if (op < 0 || op > AB_OP_LAST || !NAMES[op]) {
        snprintf(out, (size_t)size, "?");
        return -1;
    }
    switch (op) {
    case AB_STO: case AB_RCL: case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:
    case AB_RCLADD: case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV: case AB_XVAR: case AB_VIEW:
    case AB_ISG: case AB_DSE: case AB_INPUT: case AB_SOLVE: case AB_INTEG:
        var_text(arg, a, sizeof a);
        break;
    case AB_XEQ: case AB_GTO:
        target_text(arg, a, sizeof a);
        break;
    case AB_LBL:                                /* with its mode: LBL A ·33 (unit 025 rule 3) */
        if (arg >= 0 && arg < 26 && c->lmode[arg] == AB_MODEL_STU) snprintf(a, sizeof a, "%c \xc2\xb7STU", 'A' + arg);
        else if (arg >= 0 && arg < 26 && c->lmode[arg]) snprintf(a, sizeof a, "%c \xc2\xb7%d", 'A' + arg, c->lmode[arg]);
        else snprintf(a, sizeof a, "%c", arg >= 0 && arg < 26 ? 'A' + arg : '?');
        break;
    case AB_FN:                                 /* a label, or (I) / (J) */
        if (arg >= 0 && arg < 26) snprintf(a, sizeof a, "%c", 'A' + arg);
        else target_text(arg, a, sizeof a);
        break;
    case AB_SETFIX: case AB_SETSCI: case AB_SETENG: case AB_SF: case AB_CF: case AB_FS:
        snprintf(a, sizeof a, "%d", arg);
        break;
    case AB_CLVARX:                             /* CLVAR056, no space (UG p.1-5) */
        snprintf(out, (size_t)size, "%s%03d", NAMES[op], arg);
        return (int)strlen(out);
    default:
        a[0] = '\0';
    }
    snprintf(out, (size_t)size, a[0] ? "%s %s" : "%s", NAMES[op], a);
    return (int)strlen(out);
}
