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
    F_REGX, F_REGY, F_REGZ, F_REGT,
    F_AND, F_XOR, F_OR, F_NOT, F_NAND, F_NOR, F_PCT,
    F_CUBE, F_CBRT
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
    /* the logic functions in the function form of p.11-4, and % as %(y,x) (ours: p.G-2 marks %
       usable in equations and gives no form) (unit 015) */
    {"AND", K2, F_AND}, {"XOR", K2, F_XOR}, {"OR", K2, F_OR}, {"NOT", K1, F_NOT},
    {"NAND", K2, F_NAND}, {"NOR", K2, F_NOR}, {"%", K2, F_PCT},
    /* unit 041 D: the 33s's spellings, in 33s-stamped equations only (33s p.6-15, 6-16; name33) */
    {"CB", K1, F_CUBE}, {"CBRT", K1, F_CBRT}, {"\xe2\x86\x92HR", K1, F_HMSINV}, {"Cn,r", K2, F_NCR},
    {"Pn,r", K2, F_NPR}, {"RANDOM", K0, F_RAND},
};

/* Unit 041 D (abacus #4880, strict both ways): a name only the 33s's equations take (33), only the 35s's and STU's (35),
   or both (0). The 33s spells nCr, nPr, HMS→ and RAND as Cn,r, Pn,r, →HR and RANDOM (p.6-15) and has CB and CBRT. */
static int name_model(const char *name)
{
    static const char *const ONLY33[] = {"CB", "CBRT", "\xe2\x86\x92HR", "Cn,r", "Pn,r", "RANDOM"};
    static const char *const ONLY35[] = {"nCr", "nPr", "HMS\xe2\x86\x92", "RAND"};
    for (size_t k = 0; k < sizeof ONLY33 / sizeof ONLY33[0]; k++)
        if (!strcmp(name, ONLY33[k])) return 33;
    for (size_t k = 0; k < sizeof ONLY35 / sizeof ONLY35[0]; k++)
        if (!strcmp(name, ONLY35[k])) return 35;
    return 0;
}

/* Whether name is a name in an equation of model m: the other model's spellings aren't (041 D; fresh eyes: in STU,
   CB(2) is C×B×(2), RANDOM is RAND×O×M, as before D; in a 35s equation C B is refused as adjacent letters). */
static bool name_in(const char *name, int m)
{
    int nm = name_model(name);
    return !(nm == 33 && m != 33) && !(nm == 35 && m == 33);
}

/* The keyboard operation of each name (unit 015: an equation's operations are the keys'). */
static const ab_op OPS[] = {
    [F_CUBE] = AB_CUBE, [F_CBRT] = AB_CBRT,         /* unit 041 D */
    [F_LN] = AB_LN, [F_LOG] = AB_LOG, [F_EXP] = AB_EXP, [F_ALOG] = AB_TENX, [F_SQ] = AB_SQ,
    [F_SQRT] = AB_SQRT, [F_INV] = AB_INV, [F_IP] = AB_IP, [F_FP] = AB_FP, [F_RND] = AB_RND,
    [F_ABS] = AB_ABS, [F_SGN] = AB_SGN, [F_INTG] = AB_INTG, [F_IDIV] = AB_IDIV, [F_RMDR] = AB_RMDR,
    [F_SIN] = AB_SIN, [F_COS] = AB_COS, [F_TAN] = AB_TAN, [F_ASIN] = AB_ASIN, [F_ACOS] = AB_ACOS,
    [F_ATAN] = AB_ATAN, [F_SINH] = AB_SINH, [F_COSH] = AB_COSH, [F_TANH] = AB_TANH,
    [F_ASINH] = AB_ASINH, [F_ACOSH] = AB_ACOSH, [F_ATANH] = AB_ATANH, [F_TODEG] = AB_TODEG,
    [F_TORAD] = AB_TORAD, [F_HMSINV] = AB_HMSINV, [F_HMS] = AB_HMS, [F_PCTCHG] = AB_PCTCHG,
    [F_XROOT] = AB_XROOT, [F_TOL] = AB_TOL, [F_TOGAL] = AB_TOGAL, [F_TOMILE] = AB_TOMILE,
    [F_TOKM] = AB_TOKM, [F_NCR] = AB_NCR, [F_NPR] = AB_NPR, [F_TOKG] = AB_TOKG, [F_TOLB] = AB_TOLB,
    [F_TOC] = AB_TOC, [F_TOF] = AB_TOF, [F_TOCM] = AB_TOCM, [F_TOIN] = AB_TOIN, [F_SEED] = AB_SEED,
    [F_ARG] = AB_ARG, [F_RAND] = AB_RAND, [F_PI] = AB_PI, [F_SX] = AB_SX, [F_SY] = AB_SY,
    [F_PSX] = AB_PSX, [F_PSY] = AB_PSY, [F_XBAR] = AB_XBAR, [F_YBAR] = AB_YBAR,
    [F_XWBAR] = AB_XWBAR, [F_XHAT] = AB_XHAT, [F_YHAT] = AB_YHAT, [F_CORR] = AB_CORR,
    [F_SLOPE] = AB_SLOPE, [F_YINT] = AB_YINT, [F_N] = AB_NSUM, [F_SUMX] = AB_SUMX,
    [F_SUMY] = AB_SUMY, [F_SUMX2] = AB_SUMX2, [F_SUMY2] = AB_SUMY2, [F_SUMXY] = AB_SUMXY,
    [F_AND] = AB_AND, [F_XOR] = AB_XOR, [F_OR] = AB_OR, [F_NOT] = AB_NOT, [F_NAND] = AB_NAND,
    [F_NOR] = AB_NOR, [F_PCT] = AB_PCT,
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
    bool rtl;               /* unit 031: ^ groups right to left (an STU equation) */
    bool implied;           /* unit 031: two operands side by side multiply (an STU equation) */
    bool m33;               /* unit 041 Part A: typed in 33s mode, the 35s's features refused (SYNTAX) */
    bool chain;             /* a^b^c at one level was read (the mark, rule 3) */
    int nimp;               /* the implied products: the byte offset of each right operand */
    uint8_t imp[AB_EQN_LEN];    /* (offsets below AB_EQN_LEN, 192: a byte each, for the stack) */
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

static ab_val real_val(abn v)
{
    ab_val r;
    memset(&r, 0, sizeof r);
    r.kind = AB_REAL;
    r.re = v;
    return r;
}

/* An operation, done by the keyboard itself (ab_eqn_op): y (NULL for one number) and x in Y and X,
 * the key's function run; its X replaces x. So a complex or vector value, a base mode, INVALID
 * DATA and every rounding are exactly the keys' (unit 015). */
static void kop(parser *p, ab_op op, const ab_val *y, ab_val *x)
{
    ab_val r;
    abn_status st;
    if (p->check || p->st != ABN_OK) return;
    if ((st = ab_eqn_op(p->c, op, y, x, &r)) != ABN_OK) fail(p, st);
    else *x = r;
}

/* Two arguments, as typed (a, b): RPN's Y, X order kept, XROOT's reversed (p.6-16). */
static void apply2(parser *p, int id, ab_val *a, const ab_val *b)
{
    if (id == F_XROOT) {
        ab_val i = *a;
        *a = *b;
        kop(p, AB_XROOT, a, &i);
        if (p->st == ABN_OK && !p->check) *a = i;
        return;
    }
    {
        ab_val x = *b;
        kop(p, OPS[id], a, &x);
        if (p->st == ABN_OK && !p->check) *a = x;
    }
}

/* Unit 041 Part A: the names an equation typed in 33s mode refuses: ARG (A2), to km and to mile (A7), REGX..REGT
   (A11), the logic functions (A5); not on the 33s's p.6-15 list. */
static bool m33_hidden(int id)
{
    return id == F_ARG || id == F_TOKM || id == F_TOMILE || (id >= F_REGX && id <= F_REGT) || id == F_AND ||
           id == F_XOR || id == F_OR || id == F_NOT || id == F_NAND || id == F_NOR;
}

static void apply0(parser *p, int id, ab_val *v)
{
    if (id >= F_REGX && id <= F_REGT) {
        /* the stack register as evaluation found it: kop leaves the stack as it was (UG p.B-7) */
        if (!p->check) *v = id == F_REGX ? p->c->x : id == F_REGY ? p->c->y : id == F_REGZ ? p->c->z : p->c->t;
        return;
    }
    *v = real_val(abn_zero());
    kop(p, OPS[id], NULL, v);
}

static void binop(parser *p, const char *op, ab_val *a, const ab_val *b)
{
    ab_val x = *b;
    ab_op o = *op == '+' ? AB_ADD : *op == '-' ? AB_SUB : *op == '*' ? AB_MUL : *op == '/' ? AB_DIV : AB_POW;
    kop(p, o, a, &x);
    if (p->st == ABN_OK && !p->check) *a = x;
}

static void parse_add(parser *p, ab_val *v);

/* A number marked with a base (UG p.11-3): digits of that base, then b, o, d or h; its length in
   bytes, 0 if s does not start with one. */
static int based_len(const char *s, int *base)
{
    int n = 0, b;
    while ((s[n] >= '0' && s[n] <= '9') || (s[n] >= 'A' && s[n] <= 'F')) n++;
    if (n == 0) return 0;
    b = s[n] == 'b' ? 2 : s[n] == 'o' ? 8 : s[n] == 'd' ? 10 : s[n] == 'h' ? 16 : 0;
    if (!b) return 0;
    for (int i = 0; i < n; i++) {
        int d = s[i] <= '9' ? s[i] - '0' : s[i] - 'A' + 10;
        if (d >= b) return 0;
    }
    *base = b;
    return n + 1;
}

/* The digits, point and exponent of a decimal number from s: its length, and the counts. */
static int decimal_len(const char *s, int *points, int *digits)
{
    int n = 0;
    *points = *digits = 0;
    while (s[n] == '.' || (s[n] >= '0' && s[n] <= '9')) {
        if (s[n] == '.') (*points)++;
        else (*digits)++;
        n++;
    }
    if ((*digits || *points) && s[n] == 'E' &&
        (s[n + 1] == '-' ? (s[n + 2] >= '0' && s[n + 2] <= '9') : (s[n + 1] >= '0' && s[n + 1] <= '9'))) {
        n++;
        if (s[n] == '-') n++;
        while (s[n] >= '0' && s[n] <= '9') n++;
    }
    return n;
}

/* One decimal part (n bytes at s): a number, or with two points a fraction a.b.c (unit 007). */
static abn_status decimal_value(const char *s, int n, int points, abn *v)
{
    if (points == 2) {
        char ip[AB_EQN_LEN], nu[AB_EQN_LEN], de[AB_EQN_LEN];
        int a = 0, b = 0, d = 0, part = 0;
        for (int i = 0; i < n; i++) {
            if (s[i] == 'E') return ABN_SYNTAX;     /* a fraction has no exponent */
            if (s[i] == '.') part++;
            else if (part == 0) ip[a++] = s[i];
            else if (part == 1) nu[b++] = s[i];
            else de[d++] = s[i];
        }
        ip[a] = nu[b] = de[d] = '\0';
        return d ? fr_from_parts(v, false, a ? ip : "0", b ? nu : "0", de) : ABN_SYNTAX;
    }
    {
        char buf[AB_EQN_LEN + 4];
        int k = 0;
        abn_status st;
        if (s[0] == '.') buf[k++] = '0';
        memcpy(buf + k, s, (size_t)n);
        buf[k + n] = '\0';
        st = abn_from_text(v, buf);
        /* a typed number beyond the range keeps the range error it had (unit 026 rule 7) */
        return st == ABN_BAD_TEXT ? ABN_SYNTAX : st == ABN_OVERFLOW || st == ABN_UNDERFLOW ? ABN_RANGE : st;
    }
}

/* A number as typed (UG p.6-5): decimal (digits, a point, E and an exponent; two points a
 * fraction), marked with a base (1Fh, 1100b: read as the keyboard reads typed digits), or complex,
 * x i y or r θ a (a in the angle mode, converted as polar entry is, p.9-2), the second part
 * possibly negative. */
static bool parse_number(parser *p, ab_val *v)
{
    const char *s = p->s + p->i;
    int n, points, digits, base;
    abn a, b;
    abn_status st = ABN_OK;
    if ((n = based_len(s, &base)) > 0) {
        if (p->m33) {                           /* the 33s has no suffixes (041 A5, 33s p.10-1) */
            p->i += n;
            fail(p, ABN_SYNTAX);
            return true;
        }
        if (!p->check && p->st == ABN_OK && (st = ab_eqn_based(s, n - 1, base, &a)) == ABN_OK) *v = real_val(a);
        if (st != ABN_OK) fail(p, st);
        p->i += n;
        return true;
    }
    n = decimal_len(s, &points, &digits);
    if (n == 0) return false;
    if (!digits || points > 2) {
        p->i += n;
        fail(p, ABN_SYNTAX);
        return true;
    }
    if (!p->check && p->st == ABN_OK) st = decimal_value(s, n, points, &a);
    if (st != ABN_OK) {
        fail(p, st);
        p->i += n;
        return true;
    }
    p->i += n;
    if (!p->m33 && (at(p, "i") || at(p, "\xce\xb8"))) {     /* the 33s: no one-value complex numbers (041 A1) */
        bool polar = s[n] != 'i', neg = at(p, "-");
        const char *s2 = p->s + p->i;
        int n2 = decimal_len(s2, &points, &digits);
        if (n2 == 0 || !digits || points > 1) {
            fail(p, ABN_SYNTAX);
            return true;
        }
        if (!p->check && p->st == ABN_OK && (st = decimal_value(s2, n2, points, &b)) == ABN_OK) {
            ab_val z;
            memset(&z, 0, sizeof z);
            z.kind = AB_COMPLEX;
            if (neg) b = abn_neg(&b);
            if (polar) st = fn_torec(&z.re, &z.im, &a, &b, angle(p->c));
            else z.re = a, z.im = b;
            if (st == ABN_OK) *v = z;
        }
        p->i += n2;
        if (st != ABN_OK) fail(p, st);
        return true;
    }
    if (!p->check && p->st == ABN_OK) *v = real_val(a);
    return true;
}

static void parse_prim(parser *p, ab_val *v)
{
    const char *s = p->s + p->i;
    *v = real_val(abn_zero());
    if (p->st != ABN_OK) return;
    if (parse_number(p, v)) return;
    if (at(p, "(")) {
        parse_add(p, v);
        if (!at(p, ")")) fail(p, ABN_SYNTAX);
        return;
    }
    if (at(p, "[")) {
        if (p->m33) {                           /* the 33s has no vectors (041 A3) */
            fail(p, ABN_SYNTAX);
            return;
        }
        /* a vector of one to three elements, each an expression with a real value (UG p.10-6,
           10-8) */
        ab_val e[3];
        int k = 0;
        do {
            if (k == 3) {
                fail(p, ABN_SYNTAX);
                return;
            }
            parse_add(p, &e[k++]);
        } while (p->st == ABN_OK && at(p, ","));
        if (p->st == ABN_OK && !at(p, "]")) fail(p, ABN_SYNTAX);
        if (p->check || p->st != ABN_OK) return;
        memset(v, 0, sizeof *v);
        v->kind = AB_VECTOR;
        v->len = k;
        for (int i = 0; i < k; i++) {
            if (e[i].kind != AB_REAL) {
                fail(p, ABN_DOMAIN);        /* a vector holds real numbers (unit 010) */
                return;
            }
        }
        v->re = e[0].re;
        if (k > 1) v->im = e[1].re;
        if (k > 2) v->p3 = e[2].re;
        return;
    }
    /* a name: the longest that fits; a function name needs its "(" */
    {
        int best = -1;
        size_t bl = 0;
        for (size_t k = 0; k < sizeof NAMES / sizeof NAMES[0]; k++) {
            size_t n = strlen(NAMES[k].name);
            if (n > bl && strncmp(s, NAMES[k].name, n) == 0 && (NAMES[k].kind == K0 || s[n] == '(') &&
                name_in(NAMES[k].name, p->m33 ? 33 : 35)) {
                best = (int)k;
                bl = n;
            }
        }
        if (best >= 0) {
            const fname *f = &NAMES[best];
            p->i += (int)bl;
            if (p->m33 && m33_hidden(f->id)) {  /* a 35s feature, in an equation typed in 33s mode (041 A) */
                fail(p, ABN_SYNTAX);
                return;
            }
            if (f->kind == K0) {
                apply0(p, f->id, v);
                return;
            }
            p->i++;                     /* the "(" */
            parse_add(p, v);
            if (f->id == F_RND && p->m33 && p->st == ABN_OK && at(p, ":")) {
                /* PROVISIONAL (abacus #4880; Michael's field check on his 33s): RND(x:n), p.6-15's two arguments,
                   read as x rounded as the display mode would with digit setting n */
                ab_val nv;
                parse_add(p, &nv);
                if (!at(p, ")")) {
                    fail(p, ABN_SYNTAX);
                    return;
                }
                if (p->check || p->st != ABN_OK) return;
                int n = -1;
                for (int k = 0; k <= AB_FMT_N_MAX && n < 0 && nv.kind == AB_REAL; k++) {
                    abn kk = abn_from_int(k);
                    if (!abn_cmp(&nv.re, &kk)) n = k;   /* a digit setting, 0..33 */
                }
                if (n >= 0) {
                    int was = p->c->disp_n;
                    p->c->disp_n = n;
                    kop(p, AB_RND, NULL, v);
                    p->c->disp_n = was;
                } else {
                    fail(p, ABN_DOMAIN);
                }
                return;
            }
            if (f->kind == K2) {
                ab_val b;
                if (!at(p, p->m33 ? ":" : ",")) {     /* the 33s's colon, the 35s's comma (041 D; 33s p.6-5) */
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
            kop(p, OPS[f->id], NULL, v);
            return;
        }
    }
    if (p->c->alg_eval && at(p, "ANS")) {
        /* the expression line's last result (unit 029); a name on that line only */
        if (!p->check && p->st == ABN_OK) *v = p->c->ans;
        return;
    }
    if (at(p, IND_I) || at(p, IND_J)) {
        /* the variable or register the number in I (J) names (UG p.14-23) */
        ab_val x;
        abn_status st;
        if (p->check || p->st != ABN_OK) return;
        st = ab_indirect(p->c, strncmp(s, IND_I, 3) == 0 ? AB_VAR_IND_I : AB_VAR_IND_J, &x);
        if (st != ABN_OK) fail(p, st);
        else *v = x;
        return;
    }
    if (*s >= 'A' && *s <= 'Z') {
        p->i++;
        if (!p->check && p->st == ABN_OK) *v = p->c->var[*s - 'A'];
        return;
    }
    if (*s == 'i' && p->m33) {
        /* typed in 33s mode: a lone i is the variable i; (i) is the indirect glyph above (33s p.13-23); by the
           equation's stamp, as the rest of its syntax (unit 041 A, fresh eyes; it was the mode in force) */
        p->i++;
        if (!p->check && p->st == ABN_OK) *v = p->c->var[26];
        return;
    }
    fail(p, ABN_SYNTAX);
}

static void parse_post(parser *p, ab_val *v)
{
    parse_prim(p, v);
    while (p->st == ABN_OK && at(p, "!")) kop(p, AB_FACT, NULL, v);
}

/* Unary minus: the keyboard's CHS of a finished number (UG p.1-15, 9-3; INVALID DATA for a vector,
   unit 010). */
static void negate(parser *p, ab_val *v)
{
    kop(p, AB_KCHS, NULL, v);
}

static void parse_parg(parser *p, ab_val *v)
{
    if (at(p, "-")) {
        parse_parg(p, v);
        negate(p, v);
        return;
    }
    parse_post(p, v);
}

static void parse_pow_r(parser *p, ab_val *v, bool exponent);

/* An exponent read right to left (unit 031 rule 1): unary minus, then a power, its own chain
   included (2^-3^2 is 2^(-(3^2))). */
static void parse_parg_r(parser *p, ab_val *v)
{
    if (at(p, "-")) {
        parse_parg_r(p, v);
        negate(p, v);
        return;
    }
    parse_pow_r(p, v, true);
}

static void parse_pow_r(parser *p, ab_val *v, bool exponent)
{
    parse_post(p, v);
    if (p->st == ABN_OK && at(p, "^")) {
        ab_val e;
        if (exponent) p->chain = true;
        parse_parg_r(p, &e);
        binop(p, "^", v, &e);
    }
}

static void parse_pow(parser *p, ab_val *v)
{
    if (p->rtl) {                               /* STU: a^b^c is a^(b^c) (unit 031 rule 1) */
        parse_pow_r(p, v, false);
        return;
    }
    parse_post(p, v);
    for (int n = 0; p->st == ABN_OK && at(p, "^"); n++) {   /* 33s, 35s: left to right (014 O03) */
        ab_val e;
        if (n) p->chain = true;
        parse_parg(p, &e);
        binop(p, "^", v, &e);
    }
}

static void parse_neg(parser *p, ab_val *v)
{
    if (at(p, "-")) {
        parse_neg(p, v);
        negate(p, v);
        return;
    }
    parse_pow(p, v);
}

/* Whether an implied product starts here (unit 031 rule 5, answers D-H): before "(" or a letter-start
   operand (a variable, (I), a known name with its "(", a constant name), never before a number, a
   vector, a space or the base-suffix letters b, o, d, h; an E right after a number's digits is that
   number's exponent, unfinished (rule 8: 2E, 2EXP(1) SYNTAX). */
static bool implied_here(const parser *p)
{
    const char *s = p->s + p->i;
    if (*s == '(') return true;
    if (*s == 'E' && p->i > 0 && ((s[-1] >= '0' && s[-1] <= '9') || s[-1] == '.')) return false;
    if (*s >= 'A' && *s <= 'Z') return true;
    if (!strncmp(s, IND_I, 3) || !strncmp(s, IND_J, 3)) return true;
    if (*s == 'b' || *s == 'o' || *s == 'd' || *s == 'h') return false;
    for (size_t k = 0; k < sizeof NAMES / sizeof NAMES[0]; k++) {
        size_t n = strlen(NAMES[k].name);
        if (strncmp(s, NAMES[k].name, n) == 0 && (NAMES[k].kind == K0 || s[n] == '(') &&
            name_in(NAMES[k].name, p->m33 ? 33 : 35))
            return true;
    }
    return false;
}

static void parse_mul(parser *p, ab_val *v)
{
    parse_neg(p, v);
    while (p->st == ABN_OK) {
        const char *op;
        ab_val r;
        if (at(p, "\xc3\x97")) op = "*";
        else if (at(p, "\xc3\xb7")) op = "/";
        else if (p->implied && implied_here(p)) {   /* ranks exactly as a typed x (rule 6) */
            op = "*";
            p->imp[p->nimp++] = (uint8_t)p->i;
        } else break;
        parse_neg(p, &r);
        binop(p, op, v, &r);
    }
}

static void parse_add(parser *p, ab_val *v)
{
    parse_mul(p, v);
    while (p->st == ABN_OK) {
        const char *op;
        ab_val r;
        if (at(p, "+")) op = "+";
        else if (at(p, "-")) op = "-";
        else break;
        parse_mul(p, &r);
        binop(p, op, v, &r);
    }
}

_Static_assert(AB_EQN_LEN <= 256, "implied-product offsets fit a byte");

static void parse_all(parser *p, bool rhs_only, ab_val *out)
{
    ab_val l, r;
    parse_add(p, &l);
    if (p->st == ABN_OK && at(p, "=")) {
        parse_add(p, &r);
        if (p->st == ABN_OK && !rhs_only) binop(p, "-", &l, &r);
        else l = r;
    }
    if (p->st == ABN_OK && p->s[p->i] != '\0') fail(p, ABN_SYNTAX);
    *out = l;
}

abn_status eqn_eval(ab_calc *c, const char *text, int model, bool rhs_only, bool check_only, ab_val *out, int *errpos)
{
    parser p;
    ab_val l;
    memset(&p, 0, sizeof p);
    p.c = c;
    p.s = text;
    p.check = check_only;
    p.st = ABN_OK;
    p.rtl = p.implied = model == AB_MODEL_STU;  /* the model it was typed in (unit 031 rule 3) */
    p.m33 = model == 33;
    parse_all(&p, rhs_only, &l);
    *errpos = p.errpos;
    if (p.st != ABN_OK) return p.st;
    *out = l;
    return ABN_OK;
}

int eqn_write_implied(ab_calc *c, const char *text, char *out, int size)
{
    parser p;
    ab_val l;
    memset(&p, 0, sizeof p);
    p.c = c;
    p.s = text;
    p.check = true;
    p.st = ABN_OK;
    p.rtl = p.implied = true;
    parse_all(&p, false, &l);
    if (p.st != ABN_OK) return 0;               /* not an expression: kept as typed (answer J) */
    int len = (int)strlen(text);
    if (len + 2 * p.nimp >= size || len + 2 * p.nimp >= AB_EQN_LEN) return -1;   /* answer I */
    int o = 0, from = 0;
    for (int k = 0; k < p.nimp; k++) {
        memcpy(out + o, text + from, (size_t)(p.imp[k] - from));
        o += p.imp[k] - from;
        memcpy(out + o, "\xc3\x97", 2);
        o += 2;
        from = p.imp[k];
    }
    memcpy(out + o, text + from, (size_t)(len - from) + 1);
    return 1;
}

bool eqn_has_chain(ab_calc *c, const char *text, int model)
{
    parser p;
    ab_val l;
    memset(&p, 0, sizeof p);
    p.c = c;
    p.s = text;
    p.m33 = model == 33;                        /* its names and separator (041 D; fresh eyes) */
    p.check = true;
    p.st = ABN_OK;
    parse_all(&p, false, &l);
    return p.chain && p.st == ABN_OK;   /* read left to right: a chain is a chain either way; a message
                                           (it does not parse) has none */
}

int eqn_assign_var(const char *text, bool with_i)
{
    if (with_i && text[0] == 'i' && text[1] == '=') return 26;   /* var[26] (unit 025) */
    return text[0] >= 'A' && text[0] <= 'Z' && text[1] == '=' ? text[0] - 'A' : -1;
}

int eqn_vars(const char *text, bool rhs_only, bool with_i, int model, int out[27])
{
    bool seen[27] = {false};
    int n = 0;
    const char *s = text;
    if (rhs_only) {
        const char *eq = strchr(text, '=');
        if (eq) s = eq + 1;
    }
    for (; *s; s++) {
        /* names are skipped as the parser reads them: a function name with its "(", a name that
           takes none (RAND, REGX) anywhere (unit 015: RAND was read as R, A, N, D) */
        size_t best = 0;                        /* the longest, as the parser (RANDOM, not RAND then O M: 041 D) */
        for (size_t k = 0; k < sizeof NAMES / sizeof NAMES[0]; k++) {
            size_t len = strlen(NAMES[k].name);
            if (len > best && strncmp(s, NAMES[k].name, len) == 0 && (NAMES[k].kind == K0 || s[len] == '(') &&
                name_in(NAMES[k].name, model))
                best = len;
        }
        if (best) {
            s += best - 1;
            continue;
        }
        if (*s == 'E' && s > text && ((s[-1] >= '0' && s[-1] <= '9') || s[-1] == '.')) continue;  /* exponent */
        {
            int base, n = (s == text || !((s[-1] >= '0' && s[-1] <= '9') || (s[-1] >= 'A' && s[-1] <= 'F')))
                              ? based_len(s, &base) : 0;
            if (n > 0) {                                /* a based number: 1Fh's F is a digit */
                s += n - 1;
                continue;
            }
        }
        if (*s >= 'A' && *s <= 'Z' && !seen[*s - 'A']) {
            seen[*s - 'A'] = true;
            out[n++] = *s - 'A';
        }
        /* i, not a complex number's (2i3: after a digit or a point) */
        if (with_i && *s == 'i' && !(s > text && ((s[-1] >= '0' && s[-1] <= '9') || s[-1] == '.')) && !seen[26]) {
            seen[26] = true;
            out[n++] = 26;
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
