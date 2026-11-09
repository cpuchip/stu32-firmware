/* eqn: equations parsed and evaluated (see eqn.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eqn.h"
#include "rows.h"
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

/* Unit 058: whether NAMES[k] can start at s, by its first byte alone, tested before its strlen and strncmp. A name is
   never empty, so this only skips what strncmp would refuse: every scan matches what it matched before (the S3 spent
   about 31 µs a variable and 34 µs an STU operand calling them for all 84 names). */
static inline bool name_could(size_t k, const char *s)
{
    return (unsigned char)NAMES[k].name[0] == (unsigned char)*s;
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
    int depth, peak;        /* unit 047: the recursion's depth in units (descend), and the most it reached */
    int limit;              /* past it ABN_TOO_DEEP; 0 is EQN_DEPTH_MAX */
    struct say_rec *rec;    /* unit 086: the parse's operations recorded in postfix (ab_eqn_words), NULL none */
} parser;

/* Unit 086: one operation of the parse, as ab_eqn_words rebuilds the tree from them (a number's text by its offset and
   length, a variable's or an indirect's letter, a name's id, a binary operator's character) */
enum { SN_NUM, SN_VAR, SN_NAME0, SN_ANS, SN_IND, SN_NEG, SN_FACT, SN_FN1, SN_FN2, SN_BIN, SN_VEC, SN_EQ };
#define SAY_NODES (2 * AB_EQN_LEN)  /* a node a byte at most, and an implied product one more between two */
typedef struct {
    uint8_t kind, op, n;            /* n: its operands, the last n nodes before it */
    int16_t id, at, len;
    int16_t kid[3];
} say_node;
struct say_rec {
    say_node nd[SAY_NODES];
    int n;
    bool over;
};

#ifdef ESP_PLATFORM
/* (abacus #13571: the devices have no screen reader, so their parse records nothing and has no words) */
#define rec(p, kind, id, op, at, len, n) ((void)0)
#else
static void rec(parser *p, int kind, int id, int op, int at, int len, int n)
{
    if (!p->rec || p->st != ABN_OK) return;
    if (p->rec->n >= SAY_NODES) {
        p->rec->over = true;
        return;
    }
    say_node *d = &p->rec->nd[p->rec->n++];
    memset(d, 0, sizeof *d);
    d->kind = (uint8_t)kind, d->op = (uint8_t)op, d->n = (uint8_t)n;
    d->id = (int16_t)id, d->at = (int16_t)at, d->len = (int16_t)len;
}
#endif

/* Unit 047 (abacus #5153): the parse's recursion, bounded. A group "(", a vector "[" or a function's argument list
   costs 8 units (about 1,010 bytes of stack on the S3: parse_add down to parse_prim again), a step of STU's power
   chain a^b^c 1 (about 130), a unary minus group before an operand 2 while the operand is read (its loop's frame,
   about 160 on a host); the minus itself is a loop. 153 units leave 4 KB of the calc task's 32 KB at
   the deepest parse measured, an equation line run as SOLVE's function 19 XEQ levels deep (work/047): 19 groups, 17
   of X^(. Past the limit the parse stops with ABN_TOO_DEEP (NESTED TOO DEEP), so nothing recurses unbounded, an
   equation from an older image included. */
#define EQN_DEPTH_MAX 153

static void fail(parser *p, abn_status st);

static bool descend(parser *p, int w)
{
    if (p->st != ABN_OK) return false;          /* a failed parse goes no deeper (its first error stands) */
    p->depth += w;
    if (p->depth > p->peak) p->peak = p->depth;
    if (p->depth > (p->limit ? p->limit : EQN_DEPTH_MAX)) {
        p->depth -= w;
        fail(p, ABN_TOO_DEEP);
        return false;
    }
    return true;
}

static void ascend(parser *p, int w)
{
    p->depth -= w;
}

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
    if (!ab_mark_tracked(op, 0)) abn_round_note();     /* (096: an op the mark doesn't track: pi, the statistics, RAND) */
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
   (A11), the logic functions (A5); not on the 33s's p.6-15 list. And % (041 D, abacus #4883: p.6-14 has %CHG, no %). */
static bool m33_hidden(int id)
{
    return id == F_ARG || id == F_TOKM || id == F_TOMILE || (id >= F_REGX && id <= F_REGT) || id == F_AND ||
           id == F_XOR || id == F_OR || id == F_NOT || id == F_NAND || id == F_NOR || id == F_PCT;
}

static void apply0(parser *p, int id, ab_val *v)
{
    if (id >= F_REGX && id <= F_REGT) {
        /* the stack register as evaluation found it: kop leaves the stack as it was (UG p.B-7) */
        if (!p->check) *v = id == F_REGX ? p->c->x : id == F_REGY ? p->c->y : id == F_REGZ ? p->c->z : p->c->t;
        if (!p->check && v->rounded) abn_round_note();      /* (096) */
        return;
    }
    *v = real_val(abn_zero());
    kop(p, OPS[id], NULL, v);
}

static void binop(parser *p, const char *op, ab_val *a, const ab_val *b)
{
    rec(p, SN_BIN, 0, *op, 0, 0, 2);
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
static __attribute__((noinline)) bool parse_number(parser *p, ab_val *v)
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
        rec(p, SN_NUM, 0, 0, p->i, n, 0);
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
        rec(p, SN_NUM, 0, 0, (int)(s - p->s), p->i - (int)(s - p->s), 0);
        return true;
    }
    if (!p->check && p->st == ABN_OK) *v = real_val(a);
    rec(p, SN_NUM, 0, 0, (int)(s - p->s), n, 0);
    return true;
}

/* Unit 049: parse_prim's rarer branches, each in a function of its own (never inlined), so that their values (a
   vector's three, a second argument, an indirect read: 64 bytes each) take stack only while that branch runs, not on
   every level of a nest of groups; number reading too (parse_number). On the S3 a "(" level's frames went from 880
   bytes to 688 (work/049). An operation's application (kop, apply0, apply2) stays inline: out of line it saved 112
   bytes more a level but made an integral 2% slower, past the unit's bar. */

/* a vector of one to three elements, each an expression with a real value (UG p.10-6, 10-8) */
static __attribute__((noinline)) void parse_vector(parser *p, ab_val *v)
{
    ab_val e[3];
    int k = 0;
    if (!descend(p, 8)) return;
    do {
        if (k == 3) {
            fail(p, ABN_SYNTAX);
            break;
        }
        parse_add(p, &e[k++]);
    } while (p->st == ABN_OK && at(p, ","));
    ascend(p, 8);
    if (k == 3 && p->st != ABN_OK) return;
    if (p->st == ABN_OK && !at(p, "]")) fail(p, ABN_SYNTAX);
    rec(p, SN_VEC, 0, 0, 0, 0, k);
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
}

/* RND(x:n)'s n, after RND's x is in v (PROVISIONAL: abacus #4880; Michael's field check on his 33s): x rounded as the
   display mode would with digit setting n (33s p.6-15) */
static __attribute__((noinline)) void parse_rnd2(parser *p, ab_val *v)
{
    ab_val nv;
    if (descend(p, 8)) {
        parse_add(p, &nv);
        ascend(p, 8);
    }
    if (!at(p, ")")) {
        fail(p, ABN_SYNTAX);
        return;
    }
    rec(p, SN_FN2, F_RND, 0, 0, 0, 2);
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
}

/* a two-argument function's second argument, after its first is in v; then the function applied */
static __attribute__((noinline)) void parse_k2(parser *p, int id, ab_val *v)
{
    ab_val b;
    if (!at(p, p->m33 ? ":" : ",")) {     /* the 33s's colon, the 35s's comma (041 D; 33s p.6-5) */
        fail(p, ABN_SYNTAX);
        return;
    }
    if (descend(p, 8)) {
        parse_add(p, &b);
        ascend(p, 8);
    }
    if (!at(p, ")")) {
        fail(p, ABN_SYNTAX);
        return;
    }
    rec(p, SN_FN2, id, 0, 0, 0, 2);
    apply2(p, id, v, &b);
}

/* (I) or (J): the variable or register the number in I (J) names (UG p.14-23) */
/* Unit 058: the longest name at s that model m reads (a function name needs its "("): its index, or -1, and its length.
   Out of line, so its locals stay out of parse_post's frame, which every level of a nested equation holds (inlined, the
   first-byte test grew that frame from 240 to 256 B: 288 B less free at 047's deepest site; 049's lesson). */
static __attribute__((noinline)) int name_at(const char *s, int m, size_t *len)
{
    int best = -1;
    size_t bl = 0;
    for (size_t k = 0; k < sizeof NAMES / sizeof NAMES[0]; k++) {
        if (!name_could(k, s)) continue;
        size_t n = strlen(NAMES[k].name);
        if (n > bl && strncmp(s, NAMES[k].name, n) == 0 && (NAMES[k].kind == K0 || s[n] == '(') && name_in(NAMES[k].name, m)) {
            best = (int)k;
            bl = n;
        }
    }
    *len = bl;
    return best;
}

/* A value an equation reads (a letter, ANS, an indirect register): an expression or a name in it is NOT A NUMBER (unit
   082, before 084's arithmetic), so no number is computed from one. */
static void take_val(parser *p, ab_val *v, const ab_val *from)
{
    if (ab_is_sym(from)) fail(p, ABN_NOT_NUMBER);
    else *v = *from;
    if (from->rounded) abn_round_note();        /* (096: a rounded letter, ANS or indirect read makes the line rounded) */
}

static __attribute__((noinline)) void parse_ind(parser *p, const char *s, ab_val *v)
{
    ab_val x;
    abn_status st;
    if (p->check || p->st != ABN_OK) return;
    st = ab_indirect(p->c, strncmp(s, IND_I, 3) == 0 ? AB_VAR_IND_I : AB_VAR_IND_J, &x);
    if (st != ABN_OK) fail(p, st);
    else take_val(p, v, &x);
}

static void parse_prim(parser *p, ab_val *v)
{
    const char *s = p->s + p->i;
    *v = real_val(abn_zero());
    if (p->st != ABN_OK) return;
    if (parse_number(p, v)) return;
    if (at(p, "(")) {
        if (descend(p, 8)) {
            parse_add(p, v);
            ascend(p, 8);
        }
        if (!at(p, ")")) fail(p, ABN_SYNTAX);
        return;
    }
    if (at(p, "[")) {
        if (p->m33) {                           /* the 33s has no vectors (041 A3) */
            fail(p, ABN_SYNTAX);
            return;
        }
        parse_vector(p, v);
        return;
    }
    /* a name: the longest that fits; a function name needs its "(" */
    {
        size_t bl;
        int best = name_at(s, p->m33 ? 33 : 35, &bl);
        if (best >= 0) {
            const fname *f = &NAMES[best];
            p->i += (int)bl;
            if (p->m33 && m33_hidden(f->id)) {  /* a 35s feature, in an equation typed in 33s mode (041 A) */
                fail(p, ABN_SYNTAX);
                return;
            }
            if (f->kind == K0) {
                rec(p, SN_NAME0, f->id, 0, 0, 0, 0);
                apply0(p, f->id, v);
                return;
            }
            p->i++;                     /* the "(" */
            if (descend(p, 8)) {
                parse_add(p, v);
                ascend(p, 8);
            }
            if (f->id == F_RND && p->m33 && p->st == ABN_OK && at(p, ":")) {
                parse_rnd2(p, v);               /* RND(x:n), the 33s's two arguments */
                return;
            }
            if (f->kind == K2) {
                parse_k2(p, f->id, v);
                return;
            }
            if (!at(p, ")")) {
                fail(p, ABN_SYNTAX);
                return;
            }
            rec(p, SN_FN1, f->id, 0, 0, 0, 1);
            kop(p, OPS[f->id], NULL, v);
            return;
        }
    }
    if (p->c->alg_eval && at(p, "ANS")) {
        /* the expression line's last result (unit 029); a name on that line only */
        rec(p, SN_ANS, 0, 0, 0, 0, 0);
        if (!p->check && p->st == ABN_OK) ab_rows_read(p->c, &p->c->ans), take_val(p, v, &p->c->ans);
        return;
    }
    if (at(p, IND_I) || at(p, IND_J)) {
        rec(p, SN_IND, strncmp(s, IND_I, 3) == 0 ? 'I' : 'J', 0, 0, 0, 0);
        parse_ind(p, s, v);
        return;
    }
    if (*s >= 'A' && *s <= 'Z') {
        rec(p, SN_VAR, *s, 0, 0, 0, 0);
        p->i++;
        if (!p->check && p->st == ABN_OK) ab_rows_read(p->c, &p->c->var[*s - 'A']), take_val(p, v, &p->c->var[*s - 'A']);
        return;
    }
    if (*s == 'i' && p->m33) {
        /* typed in 33s mode: a lone i is the variable i; (i) is the indirect glyph above (33s p.13-23); by the
           equation's stamp, as the rest of its syntax (unit 041 A, fresh eyes; it was the mode in force) */
        rec(p, SN_VAR, 'i', 0, 0, 0, 0);
        p->i++;
        if (!p->check && p->st == ABN_OK) ab_rows_read(p->c, &p->c->var[26]), take_val(p, v, &p->c->var[26]);
        return;
    }
    fail(p, ABN_SYNTAX);
}

static void parse_post(parser *p, ab_val *v)
{
    parse_prim(p, v);
    while (p->st == ABN_OK && at(p, "!")) {
        rec(p, SN_FACT, 0, 0, 0, 0, 1);
        kop(p, AB_FACT, NULL, v);
    }
}

/* Unary minus: the keyboard's CHS of a finished number (UG p.1-15, 9-3; INVALID DATA for a vector,
   unit 010). */
static void negate(parser *p, ab_val *v)
{
    rec(p, SN_NEG, 0, 0, 0, 0, 1);
    kop(p, AB_KCHS, NULL, v);
}

static void parse_parg(parser *p, ab_val *v)
{
    int n = 0;
    while (at(p, "-")) n++;                     /* a loop, not a recursion (unit 047): negated once a minus, as before */
    parse_post(p, v);
    while (n-- > 0) negate(p, v);
}

static void parse_pow_r(parser *p, ab_val *v, bool exponent);

/* An exponent read right to left (unit 031 rule 1): unary minus, then a power, its own chain
   included (2^-3^2 is 2^(-(3^2))). */
static void parse_parg_r(parser *p, ab_val *v)
{
    int n = 0;
    while (at(p, "-")) n++;                     /* a loop, not a recursion (unit 047) */
    parse_pow_r(p, v, true);
    while (n-- > 0) negate(p, v);
}

static void parse_pow_r(parser *p, ab_val *v, bool exponent)
{
    parse_post(p, v);
    if (p->st == ABN_OK && at(p, "^")) {
        ab_val e;
        if (exponent) p->chain = true;
        if (descend(p, 1)) {                    /* a step of the chain (unit 047) */
            parse_parg_r(p, &e);
            ascend(p, 1);
            binop(p, "^", v, &e);
        }
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
    int n = 0;
    while (at(p, "-")) n++;                     /* a loop, not a recursion (unit 047): -(-x) negated twice, as before */
    if (n && !descend(p, 2)) return;            /* its frame stays while the operand is read (with no minus a compiler
                                                   may make the call a jump): 2 units (047's stack fuzz) */
    parse_pow(p, v);
    if (n) ascend(p, 2);
    while (n-- > 0) negate(p, v);
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
        if (!name_could(k, s)) continue;        /* (after an operand: mostly + - ^ ), no name's first byte) */
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
        rec(p, SN_EQ, 0, 0, 0, 0, 2);
        if (p->st == ABN_OK && !rhs_only && !p->rec) binop(p, "-", &l, &r);   /* (recorded: the = itself) */
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

static int depth_parse(ab_calc *c, const char *text, int model, int limit, abn_status *st)
{
    parser p;
    ab_val l;
    memset(&p, 0, sizeof p);
    p.c = c;
    p.s = text;
    p.check = true;
    p.st = ABN_OK;
    p.rtl = p.implied = model == AB_MODEL_STU;
    p.m33 = model == 33;
    p.limit = limit;
    parse_all(&p, false, &l);
    *st = p.st;
    return p.peak;
}

bool eqn_too_deep(ab_calc *c, const char *text, int model)
{
    abn_status st;
    depth_parse(c, text, model, 0, &st);
    return st == ABN_TOO_DEEP;
}

#ifndef ESP_PLATFORM
int eqn_depth_peak(ab_calc *c, const char *text, int model)
{
    abn_status st;
    return depth_parse(c, text, model, 1 << 30, &st);
}
#endif

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
            if (!name_could(k, s)) continue;
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

#ifdef ESP_PLATFORM
/* abacus #13571: on the devices, no words, so none of their strings in flash; ab_graph_words falls back to the text */
int ab_eqn_words(ab_calc *c, const char *text, int model, char *out, int size)
{
    (void)c, (void)text, (void)model;
    if (size > 0) out[0] = '\0';
    return -1;
}
#else
/* ---- unit 086: the spoken equation (work/086-spoken-equation.md, revision 3; abacus #13546, #13551, #13555,
   #13557). The parse records its own operations (rec), the record is rebuilt as a tree, and the tree is said in
   order: groups ("the quantity ... end quantity") only where the grammar or a listener needs them (rules 2 and 3),
   exponents that end themselves (rule 4), unary minus as "negative" ---- */
typedef struct { int id; const char *pre, *mid, *post; } say_name;
static const say_name SAY_NAMES[] = {
    {F_LN, "the natural log of", 0, 0}, {F_LOG, "the log of", 0, 0}, {F_EXP, "the exponential of", 0, 0},
    {F_ALOG, "the antilog of", 0, 0}, {F_SQ, "the square of", 0, 0}, {F_SQRT, "the square root of", 0, 0},
    {F_INV, "the reciprocal of", 0, 0}, {F_IP, "the integer part of", 0, 0}, {F_FP, "the fractional part of", 0, 0},
    {F_RND, "the rounding of", 0, 0}, {F_ABS, "the absolute value of", 0, 0}, {F_SGN, "the sign of", 0, 0},
    {F_INTG, "the floor of", 0, 0}, {F_SIN, "the sine of", 0, 0}, {F_COS, "the cosine of", 0, 0},
    {F_TAN, "the tangent of", 0, 0}, {F_ASIN, "the arcsine of", 0, 0}, {F_ACOS, "the arccosine of", 0, 0},
    {F_ATAN, "the arctangent of", 0, 0}, {F_SINH, "the hyperbolic sine of", 0, 0},
    {F_COSH, "the hyperbolic cosine of", 0, 0}, {F_TANH, "the hyperbolic tangent of", 0, 0},
    {F_ASINH, "the inverse hyperbolic sine of", 0, 0}, {F_ACOSH, "the inverse hyperbolic cosine of", 0, 0},
    {F_ATANH, "the inverse hyperbolic tangent of", 0, 0}, {F_TODEG, "the conversion to degrees of", 0, 0},
    {F_TORAD, "the conversion to radians of", 0, 0}, {F_HMSINV, "the conversion from hours minutes seconds of", 0, 0},
    {F_HMS, "the conversion to hours minutes seconds of", 0, 0}, {F_TOL, "the conversion to liters of", 0, 0},
    {F_TOGAL, "the conversion to gallons of", 0, 0}, {F_TOMILE, "the conversion to miles of", 0, 0},
    {F_TOKM, "the conversion to kilometers of", 0, 0}, {F_TOKG, "the conversion to kilograms of", 0, 0},
    {F_TOLB, "the conversion to pounds of", 0, 0}, {F_TOC, "the conversion to Celsius of", 0, 0},
    {F_TOF, "the conversion to Fahrenheit of", 0, 0}, {F_TOCM, "the conversion to centimeters of", 0, 0},
    {F_TOIN, "the conversion to inches of", 0, 0}, {F_SEED, "the seeding with", 0, 0}, {F_ARG, "the argument of", 0, 0},
    {F_XHAT, "x hat of", 0, 0}, {F_YHAT, "y hat of", 0, 0}, {F_NOT, "the logical not of", 0, 0},
    {F_CUBE, "the cube of", 0, 0}, {F_CBRT, "the cube root of", 0, 0},
    /* two arguments */
    {F_IDIV, "the integer quotient of", "and", 0}, {F_RMDR, "the remainder of", "divided by", 0},
    {F_PCTCHG, "the percent change from", "to", 0}, {F_XROOT, "the root of index", "of", 0},
    {F_NCR, "the combinations of", "choose", 0}, {F_NPR, "the permutations of", "taken", "at a time"},
    {F_AND, "the logical and of", "with", 0}, {F_XOR, "the logical exclusive or of", "with", 0},
    {F_OR, "the logical or of", "with", 0}, {F_NAND, "the logical nand of", "with", 0},
    {F_NOR, "the logical nor of", "with", 0}, {F_PCT, "the percentage of", "at", "percent"},
    /* names without arguments */
    {F_RAND, "a random number", 0, 0}, {F_PI, "pi", 0, 0}, {F_SX, "s x", 0, 0}, {F_SY, "s y", 0, 0},
    {F_PSX, "sigma x", 0, 0}, {F_PSY, "sigma y", 0, 0}, {F_XBAR, "x bar", 0, 0}, {F_YBAR, "y bar", 0, 0},
    {F_XWBAR, "x bar w", 0, 0}, {F_CORR, "the correlation r", 0, 0}, {F_SLOPE, "the slope m", 0, 0},
    {F_YINT, "the intercept b", 0, 0}, {F_N, "the count n", 0, 0}, {F_SUMX, "the sum of x", 0, 0},
    {F_SUMY, "the sum of y", 0, 0}, {F_SUMX2, "the sum of x squares", 0, 0}, {F_SUMY2, "the sum of y squares", 0, 0},
    {F_SUMXY, "the sum of x y products", 0, 0}, {F_REGX, "register X", 0, 0}, {F_REGY, "register Y", 0, 0},
    {F_REGZ, "register Z", 0, 0}, {F_REGT, "register T", 0, 0},
};
static const say_name *say_find(int id)
{
    for (size_t k = 0; k < sizeof SAY_NAMES / sizeof SAY_NAMES[0]; k++)
        if (SAY_NAMES[k].id == id) return &SAY_NAMES[k];
    return NULL;
}
typedef struct { char *o; int size, n; bool bad; const char *src; const say_node *nd; } say_out;
static void say_w(say_out *w, const char *t)
{
    if (w->n && w->n < w->size - 1) w->o[w->n++] = ' ';
    for (; *t; t++)
        if (w->n < w->size - 1) w->o[w->n++] = *t;
        else w->bad = true;
    w->o[w->n] = '\0';
}
static void say_wn(say_out *w, const char *t, int len)
{
    char b[200];
    snprintf(b, sizeof b, "%.*s", len, t);
    say_w(w, b);
}
enum { PR_EQ, PR_ADD, PR_MUL, PR_NEG, PR_POW, PR_POST, PR_ATOM };
static bool say_frac(const say_out *w, const say_node *d)
{
    int pts = 0;
    for (int i = 0; i < d->len; i++) pts += w->src[d->at + i] == '.';
    return d->kind == SN_NUM && pts == 2;
}
static int say_pr(const say_out *w, const say_node *d)
{
    (void)w;
    switch (d->kind) {
    case SN_EQ: return PR_EQ;
    case SN_BIN: return d->op == '+' || d->op == '-' ? PR_ADD : d->op == '^' ? PR_POW : PR_MUL;
    case SN_NEG: return PR_NEG;
    case SN_FACT: return PR_POST;
    case SN_NUM: return PR_ATOM;
    default: return PR_ATOM;
    }
}
/* what follows a node at its own level: nothing (a closer or the end), more words, or a postfix (squared, a power, !) */
enum { CX_LAST, CX_MORE, CX_POST };
static void say_say(say_out *w, int k, int cx);
/* a group: "the quantity ... end quantity", its inside closed by its end */
static void say_kid(say_out *w, int k, bool group, int cx)
{
    if (group) say_w(w, "the quantity");
    say_say(w, k, group ? CX_LAST : cx);
    if (group) say_w(w, "end quantity");
}
/* #13551 ask 3: an integer as an ordinal in words (to nine digits) */
static void say_card(char *o, int size, long n);
static const char *const ONES[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
    "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
static const char *const TENS[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
static void say_card(char *o, int size, long n)
{
    char a[160] = "", b[160] = "";
    if (n >= 1000000) {
        say_card(a, sizeof a, n / 1000000);
        if (n % 1000000) say_card(b, sizeof b, n % 1000000);
        snprintf(o, (size_t)size, "%s million%s%s", a, n % 1000000 ? " " : "", b);
    } else if (n >= 1000) {
        say_card(a, sizeof a, n / 1000);
        if (n % 1000) say_card(b, sizeof b, n % 1000);
        snprintf(o, (size_t)size, "%s thousand%s%s", a, n % 1000 ? " " : "", b);
    } else if (n >= 100) {
        if (n % 100) say_card(b, sizeof b, n % 100);
        snprintf(o, (size_t)size, "%s hundred%s%s", ONES[n / 100], n % 100 ? " " : "", b);
    } else if (n >= 20) {
        snprintf(o, (size_t)size, "%s%s%s", TENS[n / 10], n % 10 ? "-" : "", n % 10 ? ONES[n % 10] : "");
    } else {
        snprintf(o, (size_t)size, "%s", ONES[n]);
    }
}
static void say_ordinal(char *o, int size, long n)
{
    static const struct { const char *c, *o; } IRR[] = {{"one", "first"}, {"two", "second"}, {"three", "third"},
        {"five", "fifth"}, {"eight", "eighth"}, {"nine", "ninth"}, {"twelve", "twelfth"}};
    char c[200];
    say_card(c, sizeof c, n);
    size_t L = strlen(c), keep = L;
    const char *end = "th";
    for (size_t k = 0; k < sizeof IRR / sizeof IRR[0]; k++) {
        size_t m = strlen(IRR[k].c);
        if (L >= m && !strcmp(c + L - m, IRR[k].c) && (L == m || c[L - m - 1] == ' ' || c[L - m - 1] == '-')) {
            keep = L - m, end = IRR[k].o;
            break;
        }
    }
    if (keep == L && L && c[L - 1] == 'y') keep = L - 1, end = "ieth";
    size_t e = strlen(end);
    if (keep + e + 1 > (size_t)size) {         /* (nine digits' words fit any caller's 200) */
        o[0] = '\0';
        return;
    }
    memcpy(o, c, keep);
    memcpy(o + keep, end, e + 1);
}
static void say_num(say_out *w, const say_node *d)
{
    const char *t = w->src + d->at;
    int n = d->len, b = t[n - 1];
    if (n > 1 && (b == 'h' || b == 'b' || b == 'o' || b == 'd') && !memchr(t, '.', (size_t)n) && !memchr(t, 'i', (size_t)n)) {
        say_w(w, b == 'h' ? "hex" : b == 'b' ? "binary" : b == 'o' ? "octal" : "decimal");
        say_wn(w, t, n - 1);
        return;
    }
    const char *ci = NULL;
    for (int i = 0; i < n; i++)
        if (t[i] == 'i' || (unsigned char)t[i] == 0xce) { ci = t + i; break; }
    if (ci) {
        say_w(w, "the complex");
        say_wn(w, t, (int)(ci - t));
        bool polar = *ci != 'i';
        say_w(w, polar ? "at angle" : "i");
        const char *r = ci + (polar ? 2 : 1);
        if (*r == '-') say_w(w, "negative"), r++;
        say_wn(w, r, (int)(t + n - r));
        return;
    }
    if (say_frac(w, d)) {
        const char *p1 = memchr(t, '.', (size_t)n), *p2 = memchr(p1 + 1, '.', (size_t)(t + n - p1 - 1));
        say_w(w, "the fraction");
        if (p1 > t) say_wn(w, t, (int)(p1 - t)), say_w(w, "and");
        say_wn(w, p1 + 1, (int)(p2 - p1 - 1));
        say_w(w, "over");
        say_wn(w, p2 + 1, (int)(t + n - p2 - 1));
        return;
    }
    const char *e = memchr(t, 'E', (size_t)n);
    if (e) {
        say_wn(w, t, (int)(e - t));
        say_w(w, "E");
        const char *r = e + 1;
        if (*r == '-') say_w(w, "negative"), r++;
        say_wn(w, r, (int)(t + n - r));
        return;
    }
    say_wn(w, t, n);
}
/* #13551 ask 1: a lone letter or number (a variable, a name without arguments, a number that isn't a fraction or a
   complex) */
static bool say_lone(const say_out *w, const say_node *d)
{
    if (d->kind == SN_VAR || d->kind == SN_NAME0 || d->kind == SN_IND || d->kind == SN_ANS) return true;
    if (d->kind != SN_NUM || say_frac(w, d)) return false;
    for (int i = 0; i < d->len; i++)
        if (w->src[d->at + i] == 'i' || (unsigned char)w->src[d->at + i] == 0xce) return false;
    return true;
}
static bool say_neglone(const say_out *w, const say_node *d)
{
    return say_lone(w, d) || (d->kind == SN_NEG && say_lone(w, &w->nd[d->kid[0]]));
}
static void say_say(say_out *w, int k, int cx)
{
    const say_node *d = &w->nd[k];
    const say_node *a = d->n > 0 ? &w->nd[d->kid[0]] : NULL, *b = d->n > 1 ? &w->nd[d->kid[1]] : NULL;
    switch (d->kind) {
    case SN_NUM: say_num(w, d); return;
    case SN_VAR: { char v[2] = {(char)d->id, 0}; say_w(w, v); return; }
    case SN_ANS: say_w(w, "the last answer"); return;
    case SN_IND: say_w(w, d->id == 'I' ? "indirect I" : "indirect J"); return;
    case SN_NAME0: { const say_name *f = say_find(d->id); if (f) say_w(w, f->pre); else w->bad = true; return; }
    case SN_EQ: say_say(w, d->kid[0], CX_LAST); say_w(w, "equals"); say_say(w, d->kid[1], cx); return;
    case SN_NEG: { bool g = say_pr(w, a) < PR_NEG; say_w(w, "negative"); say_kid(w, d->kid[0], g, cx); return; }
    case SN_FACT: { bool g = say_pr(w, a) < PR_POST; say_kid(w, d->kid[0], g, CX_POST); say_w(w, "factorial"); return; }
    case SN_FN1: case SN_FN2: {
        bool rnd2 = d->kind == SN_FN2 && d->id == F_RND;
        const say_name *f = rnd2 ? NULL : say_find(d->id);
        if (!rnd2 && !f) { w->bad = true; return; }
        /* revision 3 (#13555): an argument that isn't lone is said grouped, and its end quantity ends the call; a
           call whose last word is a bare argument is closed when more follows it; a call ending in a group is closed
           only before a postfix (squared, a power, !), which could otherwise land inside */
        bool g1 = d->kind == SN_FN1 ? !say_lone(w, a) : !say_neglone(w, a);
        bool g2 = d->kind == SN_FN2 && !say_neglone(w, b);
        bool phrase = rnd2 || (f && f->post);           /* the call ends in its own words */
        bool group_end = d->kind == SN_FN1 ? g1 : g2;
        bool wrap = phrase ? false : group_end ? cx == CX_POST : cx != CX_LAST;
        if (wrap) say_w(w, "the quantity");
        say_w(w, rnd2 ? "the rounding of" : f->pre);
        say_kid(w, d->kid[0], g1, CX_MORE);
        if (d->kind == SN_FN2) {
            say_w(w, rnd2 ? "to" : f->mid);
            say_kid(w, d->kid[1], g2, CX_MORE);
            if (rnd2) say_w(w, "digits");
            else if (f->post) say_w(w, f->post);
        }
        if (wrap) say_w(w, "end quantity");
        return;
    }
    case SN_VEC:
        say_w(w, "the vector");
        for (int i = 0; i < d->n; i++) { if (i) say_w(w, "and"); say_say(w, d->kid[i], i == d->n - 1 ? CX_LAST : CX_MORE); }
        say_w(w, "end vector");
        return;
    case SN_BIN:
        if (d->op == '+' || d->op == '-') {
            say_say(w, d->kid[0], CX_MORE);
            say_w(w, d->op == '+' ? "plus" : "minus");
            say_kid(w, d->kid[1], say_pr(w, b) <= PR_ADD, cx);
        } else if (d->op == '*' || d->op == '/') {
            say_kid(w, d->kid[0], say_pr(w, a) < PR_MUL, CX_MORE);
            say_w(w, d->op == '*' ? "times" : "divided by");
            say_kid(w, d->kid[1], say_pr(w, b) < PR_NEG, cx);
        } else {    /* ^: the base is an atom or a factorial, followed by its power's words */
            say_kid(w, d->kid[0], say_pr(w, a) < PR_POST, CX_POST);
            const say_node *e = b;
            bool neg = e->kind == SN_NEG;
            const say_node *ee = neg ? &w->nd[e->kid[0]] : e;
            bool simple = say_pr(w, ee) == PR_ATOM && ee->kind != SN_FN1 && ee->kind != SN_FN2;
            if (!neg && simple && ee->kind == SN_NUM && ee->len == 1 && w->src[ee->at] == '2') { say_w(w, "squared"); return; }
            if (!neg && simple && ee->kind == SN_NUM && ee->len == 1 && w->src[ee->at] == '3') { say_w(w, "cubed"); return; }
            if (!simple) {
                say_w(w, "to the power");
                say_kid(w, d->kid[1], true, CX_LAST);
                return;
            }
            say_w(w, "to the");
            bool digits = !neg && ee->kind == SN_NUM && ee->len <= 9;
            for (int i = 0; digits && i < ee->len; i++) digits = w->src[ee->at + i] >= '0' && w->src[ee->at + i] <= '9';
            if (digits) {
                char o[200];
                say_ordinal(o, sizeof o, atol(w->src + ee->at));
                say_w(w, o);
            } else {
                say_say(w, d->kid[1], CX_MORE);
            }
            say_w(w, "power");
        }
        return;
    }
}

int ab_eqn_words(ab_calc *c, const char *text, int model, char *out, int size)
{
    struct say_rec r;
    parser p;
    ab_val l;
    if (size <= 0) return -1;
    out[0] = '\0';
    memset(&p, 0, sizeof p);
    r.n = 0;
    r.over = false;
    p.c = c;
    p.s = text;
    p.check = true;
    p.st = ABN_OK;
    p.rtl = p.implied = model == AB_MODEL_STU;  /* read as the model reads it (unit 031 rule 3) */
    p.m33 = model == 33;
    p.rec = &r;
    parse_all(&p, false, &l);
    if (p.st != ABN_OK || r.over || r.n == 0) return -1;
    int stk[SAY_NODES], sn = 0;                 /* the postfix record rebuilt as a tree */
    for (int i = 0; i < r.n; i++) {
        say_node *d = &r.nd[i];
        if (d->n > sn || d->n > 3) return -1;
        for (int j = d->n - 1; j >= 0; j--) d->kid[j] = (int16_t)stk[--sn];
        stk[sn++] = i;
    }
    if (sn != 1) return -1;
    say_out w = {out, size, 0, false, text, r.nd};
    say_say(&w, stk[0], CX_LAST);
    if (w.bad) {
        out[0] = '\0';
        return -1;
    }
    return w.n;
}
#endif
