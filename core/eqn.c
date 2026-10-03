/* eqn: equations parsed and evaluated (see eqn.h). */
#include <string.h>

#include "eqn.h"
#include "fn.h"
#include "frac.h"
#include "stat.h"

typedef enum { K1, K2, K0 } fkind;

typedef struct {
    const char *name;       /* as shown, UTF-8 */
    fkind kind;
    int id;
} fname;

/* p.6-16's table (the operators apart), with the ids below. */
enum {
    F_LN, F_LOG, F_EXP, F_ALOG, F_SQ, F_SQRT, F_INV, F_IP, F_FP, F_RND, F_ABS, F_SGN, F_INTG,
    F_IDIV, F_RMDR, F_SIN, F_COS, F_TAN, F_ASIN, F_ACOS, F_ATAN, F_SINH, F_COSH, F_TANH, F_ASINH,
    F_ACOSH, F_ATANH, F_TODEG, F_TORAD, F_HMSINV, F_HMS, F_PCTCHG, F_XROOT, F_TOL, F_TOGAL,
    F_TOMILE, F_TOKM, F_NCR, F_NPR, F_TOKG, F_TOLB, F_TOC, F_TOF, F_TOCM, F_TOIN, F_SEED, F_ARG,
    F_RAND, F_PI, F_SX, F_SY, F_PSX, F_PSY, F_XBAR, F_YBAR, F_XWBAR, F_XHAT, F_YHAT, F_CORR,
    F_SLOPE, F_YINT, F_N, F_SUMX, F_SUMY, F_SUMX2, F_SUMY2, F_SUMXY,
    F_REGX, F_REGY, F_REGZ, F_REGT
};

static const fname NAMES[] = {
    {"LN", K1, F_LN}, {"LOG", K1, F_LOG}, {"EXP", K1, F_EXP}, {"ALOG", K1, F_ALOG}, {"SQ", K1, F_SQ},
    {"SQRT", K1, F_SQRT}, {"INV", K1, F_INV}, {"IP", K1, F_IP}, {"FP", K1, F_FP}, {"RND", K1, F_RND},
    {"ABS", K1, F_ABS}, {"SGN", K1, F_SGN}, {"INTG", K1, F_INTG}, {"IDIV", K2, F_IDIV},
    {"RMDR", K2, F_RMDR}, {"SIN", K1, F_SIN}, {"COS", K1, F_COS}, {"TAN", K1, F_TAN},
    {"ASIN", K1, F_ASIN}, {"ACOS", K1, F_ACOS}, {"ATAN", K1, F_ATAN}, {"SINH", K1, F_SINH},
    {"COSH", K1, F_COSH}, {"TANH", K1, F_TANH}, {"ASINH", K1, F_ASINH}, {"ACOSH", K1, F_ACOSH},
    {"ATANH", K1, F_ATANH}, {"\xe2\x86\x92" "DEG", K1, F_TODEG}, {"\xe2\x86\x92" "RAD", K1, F_TORAD},
    {"HMS\xe2\x86\x92", K1, F_HMSINV}, {"\xe2\x86\x92" "HMS", K1, F_HMS}, {"%CHG", K2, F_PCTCHG},
    {"XROOT", K2, F_XROOT}, {"\xe2\x86\x92L", K1, F_TOL}, {"\xe2\x86\x92GAL", K1, F_TOGAL},
    {"\xe2\x86\x92MILE", K1, F_TOMILE}, {"\xe2\x86\x92KM", K1, F_TOKM}, {"nCr", K2, F_NCR},
    {"nPr", K2, F_NPR}, {"\xe2\x86\x92KG", K1, F_TOKG}, {"\xe2\x86\x92LB", K1, F_TOLB},
    {"\xe2\x86\x92\xc2\xb0" "C", K1, F_TOC}, {"\xe2\x86\x92\xc2\xb0" "F", K1, F_TOF},
    {"\xe2\x86\x92" "CM", K1, F_TOCM}, {"\xe2\x86\x92IN", K1, F_TOIN}, {"SEED", K1, F_SEED},
    {"ARG", K1, F_ARG}, {"RAND", K0, F_RAND}, {"\xcf\x80", K0, F_PI},
    {"sx", K0, F_SX}, {"sy", K0, F_SY}, {"\xcf\x83x", K0, F_PSX}, {"\xcf\x83y", K0, F_PSY},
    {"x\xcc\x84w", K0, F_XWBAR}, {"x\xcc\x84", K0, F_XBAR}, {"\xc8\xb3", K0, F_YBAR},
    {"x\xcc\x82", K1, F_XHAT}, {"\xc5\xb7", K1, F_YHAT}, {"r", K0, F_CORR}, {"m", K0, F_SLOPE},
    {"b", K0, F_YINT}, {"n", K0, F_N}, {"\xce\xa3x\xc2\xb2", K0, F_SUMX2}, {"\xce\xa3y\xc2\xb2", K0, F_SUMY2},
    {"\xce\xa3xy", K0, F_SUMXY}, {"\xce\xa3x", K0, F_SUMX}, {"\xce\xa3y", K0, F_SUMY},
    /* the stack registers, read when the equation is evaluated (UG p.B-7; unit 015) */
    {"REGX", K0, F_REGX}, {"REGY", K0, F_REGY}, {"REGZ", K0, F_REGZ}, {"REGT", K0, F_REGT},
};

/* (I) and (J) as one token each, apart from a plain I in parentheses (UG p.14-23): shown as the
 * guide's single glyph, kept here as U+24A4 and U+24A5 (parenthesized i and j). */
#define IND_I "\xe2\x92\xa4"
#define IND_J "\xe2\x92\xa5"

typedef struct {
    ab_calc *c;
    const char *s;
    int i;                  /* byte offset */
    bool check;             /* syntax only */
    abn_status st;          /* the first error */
    int errpos;             /* its character offset */
} parser;

static int char_at(const char *s, int byte)
{
    int n = 0;
    for (int i = 0; i < byte && s[i]; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) n++;
    return n;
}

static void fail(parser *p, abn_status st)
{
    if (p->st == ABN_OK) {
        p->st = st;
        p->errpos = char_at(p->s, p->i);
    }
}

static bool at(parser *p, const char *tok)
{
    size_t n = strlen(tok);
    if (strncmp(p->s + p->i, tok, n) == 0) {
        p->i += (int)n;
        return true;
    }
    return false;
}

static fn_angle angle(const ab_calc *c)
{
    return c->angle == AB_ANGLE_RAD ? FN_RAD : c->angle == AB_ANGLE_GRAD ? FN_GRAD : FN_DEG;
}

/* One operation on values, with the keyboard's functions. */
static void apply1(parser *p, int id, abn *v)
{
    abn r = *v;
    abn_status st = ABN_OK;
    fn_angle m = angle(p->c);
    if (p->check || p->st != ABN_OK) return;
    switch (id) {
    case F_LN: st = fn_ln(&r, v); break;
    case F_LOG: st = fn_log(&r, v); break;
    case F_EXP: st = fn_exp(&r, v); break;
    case F_ALOG: st = fn_tenx(&r, v); break;
    case F_SQ: st = fn_sq(&r, v); break;
    case F_SQRT: st = fn_sqrt(&r, v); break;
    case F_INV: st = fn_inv(&r, v); break;
    case F_IP: st = fn_ip(&r, v); break;
    case F_FP: st = fn_fp(&r, v); break;
    case F_ABS: st = fn_abs(&r, v); break;
    case F_SGN: st = fn_sgn(&r, v); break;
    case F_INTG: st = fn_intg(&r, v); break;
    case F_SIN: st = fn_sin(&r, v, m); break;
    case F_COS: st = fn_cos(&r, v, m); break;
    case F_TAN: st = fn_tan(&r, v, m); break;
    case F_ASIN: st = fn_asin(&r, v, m); break;
    case F_ACOS: st = fn_acos(&r, v, m); break;
    case F_ATAN: st = fn_atan(&r, v, m); break;
    case F_SINH: st = fn_sinh(&r, v); break;
    case F_COSH: st = fn_cosh(&r, v); break;
    case F_TANH: st = fn_tanh(&r, v); break;
    case F_ASINH: st = fn_asinh(&r, v); break;
    case F_ACOSH: st = fn_acosh(&r, v); break;
    case F_ATANH: st = fn_atanh(&r, v); break;
    case F_TODEG: st = fn_todeg(&r, v); break;
    case F_TORAD: st = fn_torad(&r, v); break;
    case F_HMSINV: st = fn_hmsinv(&r, v); break;
    case F_HMS: st = fn_hms(&r, v); break;
    case F_TOL: st = fn_convert(&r, v, FN_TOL); break;
    case F_TOGAL: st = fn_convert(&r, v, FN_TOGAL); break;
    case F_TOMILE: st = fn_convert(&r, v, FN_TOMILE); break;
    case F_TOKM: st = fn_convert(&r, v, FN_TOKM); break;
    case F_TOKG: st = fn_convert(&r, v, FN_TOKG); break;
    case F_TOLB: st = fn_convert(&r, v, FN_TOLB); break;
    case F_TOC: st = fn_convert(&r, v, FN_TOC); break;
    case F_TOF: st = fn_convert(&r, v, FN_TOF); break;
    case F_TOCM: st = fn_convert(&r, v, FN_TOCM); break;
    case F_TOIN: st = fn_convert(&r, v, FN_TOIN); break;
    case F_SEED: fn_seed(&p->c->rng, v); break;            /* reseeds; its value is x (ours) */
    case F_ARG: {
        abn rr, zero = abn_zero();
        st = fn_topol(&rr, &r, v, &zero, m);
        break;
    }
    case F_XHAT: st = st_xhat(&r, p->c->stat, v); break;
    case F_YHAT: st = st_yhat(&r, p->c->stat, v); break;
    case F_RND:
        /* as the keyboard's RND: in Fraction display the fraction shown (p.G-12, 5-8) */
        if (p->c->fdisp) {
            fr_result fr;
            fr_format f = !p->c->flag8 ? FR_PRECISE : (p->c->flag9 ? FR_FIXED : FR_FACTORS);
            if (fr_approx(&fr, v, p->c->frac_c, f) == ABN_OK) {
                st = fr_value(&r, &fr);
                break;
            }
        }
        st = fn_rnd(&r, v, p->c->disp_mode, p->c->disp_n);
        break;
    default: break;
    }
    if (st != ABN_OK) fail(p, st);
    else *v = r;
}

/* Two arguments, as typed (a, b): RPN's Y, X order kept, XROOT's reversed (p.6-16). */
static void apply2(parser *p, int id, abn *a, const abn *b)
{
    abn r = *a;
    abn_status st = ABN_OK;
    if (p->check || p->st != ABN_OK) return;
    switch (id) {
    case F_IDIV: st = fn_idiv(&r, a, b); break;
    case F_RMDR: st = fn_rmdr(&r, a, b); break;
    case F_PCTCHG: st = fn_pctchg(&r, a, b); break;
    case F_XROOT: st = fn_xroot(&r, b, a); break;
    case F_NCR: st = fn_ncr(&r, a, b); break;
    default: st = fn_npr(&r, a, b); break;
    }
    if (st != ABN_OK) fail(p, st);
    else *a = r;
}

static void apply0(parser *p, int id, abn *v)
{
    abn_status st = ABN_OK;
    const abn *R = p->c->stat;
    if (p->check || p->st != ABN_OK) return;
    switch (id) {
    case F_PI: *v = fn_pi(); break;
    case F_RAND: *v = fn_rand(&p->c->rng); break;
    case F_SX: st = st_sdev(v, R, false, false); break;
    case F_SY: st = st_sdev(v, R, true, false); break;
    case F_PSX: st = st_sdev(v, R, false, true); break;
    case F_PSY: st = st_sdev(v, R, true, true); break;
    case F_XBAR: st = st_mean(v, R, false); break;
    case F_YBAR: st = st_mean(v, R, true); break;
    case F_XWBAR: st = st_wmean(v, R); break;
    case F_CORR: st = st_corr(v, R); break;
    case F_SLOPE: st = st_slope(v, R); break;
    case F_YINT: st = st_yint(v, R); break;
    case F_N: *v = R[ST_N]; break;
    case F_SUMX: *v = R[ST_SX]; break;
    case F_SUMY: *v = R[ST_SY]; break;
    case F_SUMX2: *v = R[ST_SX2]; break;
    case F_SUMY2: *v = R[ST_SY2]; break;
    case F_REGX: case F_REGY: case F_REGZ: case F_REGT: {
        const ab_val *r = id == F_REGX ? &p->c->x : id == F_REGY ? &p->c->y : id == F_REGZ ? &p->c->z : &p->c->t;
        if (r->kind != AB_REAL) st = ABN_DOMAIN;            /* complex in equations: part 2 */
        else *v = r->re;
        break;
    }
    default: *v = R[ST_SXY]; break;
    }
    if (st != ABN_OK) fail(p, st);
}

static void binop(parser *p, const char *op, abn *a, const abn *b)
{
    abn r;
    abn_status st;
    if (p->check || p->st != ABN_OK) return;
    if (*op == '+') st = abn_add(&r, a, b);
    else if (*op == '-') st = abn_sub(&r, a, b);
    else if (*op == '*') st = abn_mul(&r, a, b);
    else if (*op == '/') st = abn_div(&r, a, b);
    else st = fn_pow(&r, a, b);
    if (st != ABN_OK) fail(p, st);
    else *a = r;
}

static void parse_add(parser *p, abn *v);

/* A number as typed: digits, a point, E and an exponent (with its minus); two points a fraction. */
static bool parse_number(parser *p, abn *v)
{
    const char *s = p->s + p->i;
    int n = 0, points = 0, digits = 0;
    char buf[AB_EQN_LEN + 4];
    while (s[n] >= '0' && s[n] <= '9') n++, digits++;
    while (s[n] == '.' || (s[n] >= '0' && s[n] <= '9')) {
        if (s[n] == '.') points++;
        else digits++;
        n++;
    }
    if (!digits && !points) return false;
    if (s[n] == 'E' && (s[n + 1] == '-' ? (s[n + 2] >= '0' && s[n + 2] <= '9') : (s[n + 1] >= '0' && s[n + 1] <= '9'))) {
        if (points == 2) {
            p->i += n;
            fail(p, ABN_SYNTAX);        /* a fraction has no exponent (unit 007) */
            return true;
        }
        n++;
        if (s[n] == '-') n++;
        while (s[n] >= '0' && s[n] <= '9') n++;
    }
    if (!digits || points > 2 || n >= (int)sizeof buf - 2) {
        fail(p, ABN_SYNTAX);
        p->i += n;
        return true;
    }
    if (!p->check && p->st == ABN_OK) {
        abn_status st;
        if (points == 2) {
            char ip[AB_EQN_LEN], nu[AB_EQN_LEN], de[AB_EQN_LEN];
            int a = 0, b = 0, d = 0, part = 0;
            for (int i = 0; i < n; i++) {
                if (s[i] == '.') part++;
                else if (part == 0) ip[a++] = s[i];
                else if (part == 1) nu[b++] = s[i];
                else de[d++] = s[i];
            }
            ip[a] = nu[b] = de[d] = '\0';
            st = d ? fr_from_parts(v, false, a ? ip : "0", b ? nu : "0", de) : ABN_SYNTAX;
        } else {
            int k = 0;
            if (s[0] == '.') buf[k++] = '0';
            memcpy(buf + k, s, (size_t)n);
            buf[k + n] = '\0';
            st = abn_from_text(v, buf);
        }
        if (st != ABN_OK) fail(p, st == ABN_BAD_TEXT ? ABN_SYNTAX : st);
    }
    p->i += n;
    return true;
}

static void parse_prim(parser *p, abn *v)
{
    const char *s = p->s + p->i;
    *v = abn_zero();
    if (p->st != ABN_OK) return;
    if (parse_number(p, v)) return;
    if (at(p, "(")) {
        parse_add(p, v);
        if (!at(p, ")")) fail(p, ABN_SYNTAX);
        return;
    }
    /* a name: the longest that fits; a function name needs its "(" */
    {
        int best = -1;
        size_t bl = 0;
        for (size_t k = 0; k < sizeof NAMES / sizeof NAMES[0]; k++) {
            size_t n = strlen(NAMES[k].name);
            if (n > bl && strncmp(s, NAMES[k].name, n) == 0 && (NAMES[k].kind == K0 || s[n] == '(')) {
                best = (int)k;
                bl = n;
            }
        }
        if (best >= 0) {
            const fname *f = &NAMES[best];
            p->i += (int)bl;
            if (f->kind == K0) {
                apply0(p, f->id, v);
                return;
            }
            p->i++;                     /* the "(" */
            parse_add(p, v);
            if (f->kind == K2) {
                abn b;
                if (!at(p, ",")) {
                    fail(p, ABN_SYNTAX);
                    return;
                }
                parse_add(p, &b);
                if (!at(p, ")")) {
                    fail(p, ABN_SYNTAX);
                    return;
                }
                apply2(p, f->id, v, &b);
                return;
            }
            if (!at(p, ")")) {
                fail(p, ABN_SYNTAX);
                return;
            }
            apply1(p, f->id, v);
            return;
        }
    }
    if (at(p, IND_I) || at(p, IND_J)) {
        /* the variable or register the number in I (J) names (UG p.14-23) */
        ab_val x;
        abn_status st;
        if (p->check || p->st != ABN_OK) return;
        st = ab_indirect(p->c, strncmp(s, IND_I, 3) == 0 ? AB_VAR_IND_I : AB_VAR_IND_J, &x);
        if (st == ABN_OK && x.kind != AB_REAL) st = ABN_DOMAIN;
        if (st != ABN_OK) fail(p, st);
        else *v = x.re;
        return;
    }
    if (*s >= 'A' && *s <= 'Z') {
        const ab_val *x = &p->c->var[*s - 'A'];
        p->i++;
        if (p->check || p->st != ABN_OK) return;
        if (x->kind != AB_REAL) fail(p, ABN_DOMAIN);    /* complex equations: a later unit (ours) */
        else *v = x->re;
        return;
    }
    fail(p, ABN_SYNTAX);
}

static void parse_post(parser *p, abn *v)
{
    parse_prim(p, v);
    while (p->st == ABN_OK && at(p, "!")) {
        abn r;
        abn_status st;
        if (p->check) continue;
        if ((st = fn_fact(&r, v)) != ABN_OK) fail(p, st);
        else *v = r;
    }
}

static void parse_parg(parser *p, abn *v)
{
    if (at(p, "-")) {
        parse_parg(p, v);
        *v = abn_neg(v);
        return;
    }
    parse_post(p, v);
}

static void parse_pow(parser *p, abn *v)
{
    parse_post(p, v);
    while (p->st == ABN_OK && at(p, "^")) {
        abn e;
        parse_parg(p, &e);
        binop(p, "^", v, &e);
    }
}

static void parse_neg(parser *p, abn *v)
{
    if (at(p, "-")) {
        parse_neg(p, v);
        *v = abn_neg(v);
        return;
    }
    parse_pow(p, v);
}

static void parse_mul(parser *p, abn *v)
{
    parse_neg(p, v);
    while (p->st == ABN_OK) {
        const char *op;
        abn r;
        if (at(p, "\xc3\x97")) op = "*";
        else if (at(p, "\xc3\xb7")) op = "/";
        else break;
        parse_neg(p, &r);
        binop(p, op, v, &r);
    }
}

static void parse_add(parser *p, abn *v)
{
    parse_mul(p, v);
    while (p->st == ABN_OK) {
        const char *op;
        abn r;
        if (at(p, "+")) op = "+";
        else if (at(p, "-")) op = "-";
        else break;
        parse_mul(p, &r);
        binop(p, op, v, &r);
    }
}

abn_status eqn_eval(ab_calc *c, const char *text, bool rhs_only, bool check_only, abn *out, int *errpos)
{
    parser p = {c, text, 0, check_only, ABN_OK, 0};
    abn l, r;
    parse_add(&p, &l);
    if (p.st == ABN_OK && at(&p, "=")) {
        parse_add(&p, &r);
        if (p.st == ABN_OK && !rhs_only) binop(&p, "-", &l, &r);
        else l = r;
    }
    if (p.st == ABN_OK && text[p.i] != '\0') fail(&p, ABN_SYNTAX);
    *errpos = p.errpos;
    if (p.st != ABN_OK) return p.st;
    *out = l;
    return ABN_OK;
}

int eqn_assign_var(const char *text)
{
    return text[0] >= 'A' && text[0] <= 'Z' && text[1] == '=' ? text[0] - 'A' : -1;
}

int eqn_vars(const char *text, bool rhs_only, int out[26])
{
    bool seen[26] = {false};
    int n = 0;
    const char *s = text;
    if (rhs_only) {
        const char *eq = strchr(text, '=');
        if (eq) s = eq + 1;
    }
    for (; *s; s++) {
        /* names are skipped as the parser reads them: a function name with its "(", a name that
           takes none (RAND, REGX) anywhere (unit 015: RAND was read as R, A, N, D) */
        bool name = false;
        for (size_t k = 0; k < sizeof NAMES / sizeof NAMES[0] && !name; k++) {
            size_t len = strlen(NAMES[k].name);
            if (strncmp(s, NAMES[k].name, len) == 0 && (NAMES[k].kind == K0 || s[len] == '(')) {
                s += len - 1;
                name = true;
            }
        }
        if (name) continue;
        if (*s == 'E' && s > text && ((s[-1] >= '0' && s[-1] <= '9') || s[-1] == '.')) continue;  /* exponent */
        if (*s >= 'A' && *s <= 'Z' && !seen[*s - 'A']) {
            seen[*s - 'A'] = true;
            out[n++] = *s - 'A';
        }
    }
    return n;
}

int eqn_name_before(const char *s, int pos)
{
    int best = 0;
    if (pos < 1 || s[pos - 1] != '(') return 0;
    for (size_t k = 0; k < sizeof NAMES / sizeof NAMES[0]; k++) {
        int n = (int)strlen(NAMES[k].name);
        if (NAMES[k].kind != K0 && n + 1 <= pos && n + 1 > best && strncmp(s + pos - 1 - n, NAMES[k].name, (size_t)n) == 0)
            best = n + 1;
    }
    return best;
}
