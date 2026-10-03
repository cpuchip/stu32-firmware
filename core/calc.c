/* calc: the four-level RPN stack, number entry and the operations around them.
 *
 * Behaviour is written from the user's guide's description of the stack, cited by page
 * (UG p.N), in our own words.
 */
#include <string.h>

#include "calc.h"
#include "fn.h"
#include "const.h"
#include "stat.h"
#include "frac.h"
#include "base.h"
#include "cplx.h"

/* A real number as a stack value. */
static ab_val ab_real(abn v)
{
    ab_val r;
    r.kind = AB_REAL;
    r.re = v;
    r.im = abn_zero();
    return r;
}

void ab_init(ab_calc *c)
{
    memset(c, 0, sizeof *c);
    c->x = c->y = c->z = c->t = c->lastx = ab_real(abn_zero());
    c->lift = true;             /* a cleared calculator has stack lift enabled (UG p.B-4) */
    c->angle = AB_ANGLE_DEG;    /* and is in degrees (UG p.B-4) */
    c->disp_mode = AB_ALL;      /* ALL is the default display (ours, unit 002) */
    c->disp_n = 0;
    c->ind_top = -1;
    c->frac_c = 4095;          /* the default maximum denominator (UG p.5-5) */
    c->base = 10;
    {
        abn zero = abn_zero();
        fn_seed(&c->rng, &zero);    /* a fixed start: the core has no clock */
    }
}

/* The stack lifts: T is lost, Z to T, Y to Z, X to Y; X keeps its value (UG p.2-5). */
static void stack_lift(ab_calc *c)
{
    c->t = c->z;
    c->z = c->y;
    c->y = c->x;
}

/* The stack drops after a two-number operation: Z to Y, T to Z, and T keeps its value
 * (UG p.2-5, p.2-6). */
static void stack_drop(ab_calc *c)
{
    c->y = c->z;
    c->z = c->t;
}

/* A number arrives in X: if stack lift is enabled the stack lifts first; otherwise the
 * number writes over X (UG p.B-4, p.B-5). */
static void place_val(ab_calc *c, ab_val v)
{
    if (c->lift) stack_lift(c);
    c->x = v;
}

static void place_in_x(ab_calc *c, abn v)
{
    place_val(c, ab_real(v));
}

/* ---- complex values (unit 009) ---- */

static bool is_cx(const ab_val *v)
{
    return v->kind == AB_COMPLEX;
}

/* A stack value as a complex number: a real one has imaginary part 0 (unit 009 rule 3). */
static cplx cx_of(const ab_val *v)
{
    cplx z;
    z.re = v->re;
    z.im = v->im;
    return z;
}

static ab_val ab_cx(cplx z)
{
    ab_val r;
    r.kind = AB_COMPLEX;
    r.re = z.re;
    r.im = z.im;
    return r;
}

static fn_angle angle_of(const ab_calc *c);
static abn_status entry_value(const ab_entry *e, abn *out, int base);

/* The complex number being typed, from its first part and the value v of its second: x i y, or
 * r theta a with a in the angle mode (UG p.9-2, p.4-11; polar entry exact as unit 004's
 * conversion is, so 1 theta 90 in degrees is 0 i 1). */
static abn_status complex_entry_value(const ab_calc *c, const abn *v, ab_val *out)
{
    abn v1;
    cplx z;
    abn_status st;
    if ((st = entry_value(&c->entry1, &v1, 10)) != ABN_OK) return st;
    if (c->cpart == 1) {
        z.re = v1;
        z.im = *v;
    } else if ((st = fn_torec(&z.re, &z.im, &v1, v, angle_of(c))) != ABN_OK) {
        return st;
    }
    *out = ab_cx(z);
    return ABN_OK;
}

/* ---- number entry (UG p.1-15 to p.1-17) ---- */

static bool has_char(const ab_entry *e, char ch)
{
    return memchr(e->text, ch, (size_t)e->len) != NULL;
}

static int count_digits(const ab_entry *e, bool exponent)
{
    int n = 0;
    bool in_exp = false;
    for (int i = 0; i < e->len; i++) {
        if (e->text[i] == 'E') in_exp = true;
        else if (e->text[i] != '.' && in_exp == exponent) n++;
    }
    return n;
}

/* The value the keys typed so far stand for. A mantissa with no digit yet reads as 0, and
 * an exponent with no digit yet as E0. */
static int count_char(const ab_entry *e, char ch)
{
    int n = 0;
    for (int i = 0; i < e->len; i++) n += e->text[i] == ch;
    return n;
}

static abn_status entry_value(const ab_entry *e, abn *out, int base)
{
    /* In HEX, OCT or BIN the keys are an integer's digits in that base (UG p.11-3); a value past
     * the largest word cannot be typed (UG p.11-7): the key is refused by the caller. */
    if (base != 10) {
        int64_t v;
        char d[AB_ENTRY_TEXT];
        memcpy(d, e->text, (size_t)e->len);
        d[e->len] = '\0';
        if (e->len == 0) {
            *out = abn_zero();
            return ABN_OK;
        }
        if (!base_parse(d, base, &v)) return ABN_RANGE;
        *out = base_from_word(e->mant_neg ? -v : v);
        return ABN_OK;
    }
    /* A second point makes the entry a fraction: integer . numerator . denominator (UG p.5-1),
     * the exact rational rounded once (unit 007 rule 2). With no denominator digit yet, the
     * number is the decimal typed so far (ours, unit 007). */
    if (count_char(e, '.') == 2) {
        char ip[AB_ENTRY_TEXT], nu[AB_ENTRY_TEXT], de[AB_ENTRY_TEXT];
        int part = 0, a = 0, b = 0, c2 = 0;
        for (int i = 0; i < e->len; i++) {
            char ch = e->text[i];
            if (ch == '.') part++;
            else if (part == 0) ip[a++] = ch;
            else if (part == 1) nu[b++] = ch;
            else de[c2++] = ch;
        }
        ip[a] = nu[b] = de[c2] = '\0';
        if (c2 > 0) return fr_from_parts(out, e->mant_neg, ip, nu, de);
        char s[AB_ENTRY_TEXT + 4];
        int n = 0;
        if (e->mant_neg) s[n++] = '-';
        s[n++] = '0';
        for (int i = 0; i < a; i++) s[n++] = ip[i];
        s[n++] = '.';
        for (int i = 0; i < b; i++) s[n++] = nu[i];
        s[n++] = '0';
        s[n] = '\0';
        return abn_from_text(out, s);
    }
    char s[sizeof e->text + 8];
    int n = 0;
    if (e->mant_neg) s[n++] = '-';
    if (count_digits(e, false) == 0) s[n++] = '0';
    for (int i = 0; i < e->len; i++) {
        if (e->text[i] == 'E') {
            s[n++] = 'E';
            if (e->exp_neg) s[n++] = '-';
            if (count_digits(e, true) == 0) s[n++] = '0';
            continue;
        }
        if (count_digits(e, false) == 0 && e->text[i] == '.') continue;
        s[n++] = e->text[i];
    }
    s[n] = '\0';
    return abn_from_text(out, s);
}

/* A key that builds the number being entered. Keys the entry cannot take (a second
 * point, a digit past the limit, E before any mantissa digit) are ignored. */
static abn_status entry_key(ab_calc *c, ab_op op)
{
    ab_entry e = c->entry;
    if (!e.active) {
        memset(&e, 0, sizeof e);
    }
    bool in_exp = has_char(&e, 'E');
    int points = count_char(&e, '.');
    if (c->base != 10 && (op == AB_KPOINT || op == AB_KE)) return ABN_OK;  /* integers only (ours) */
    if (op <= AB_K9 || (op >= AB_KA && op <= AB_KF)) {
        /* a digit outside the base is ignored, and A-F are digits only in HEX (ours, unit 008) */
        int dv = op <= AB_K9 ? (int)(op - AB_K0) : 10 + (int)(op - AB_KA);
        if (dv >= c->base) return ABN_OK;
        if (dv >= 10) {
            /* hex digits are kept in lower case so that 'E' always means the exponent */
            e.text[e.len++] = (char)('a' + dv - 10);
            goto value;
        }
    }
    if (op <= AB_K9) {
        /* in a base mode the range, not a digit count, limits entry: up to a word's binary
           digits (a 35-digit binary number is the largest positive word, UG p.11-7) */
        int limit = c->base != 10 ? AB_WORD_BITS : in_exp ? AB_ENTRY_EXP : AB_ENTRY_MANT;
        if (count_digits(&e, in_exp) >= limit) return ABN_OK;
        /* a denominator never starts with 0 (ours, unit 007): the key is ignored */
        if (op == AB_K0 && points == 2 && e.text[e.len - 1] == '.') return ABN_OK;
        e.text[e.len++] = (char)('0' + (op - AB_K0));
    } else if (op == AB_KPOINT) {
        /* the second point starts a fraction's denominator (UG p.5-1); a third is ignored, as is
           a point in the exponent (ours, unit 007) */
        if (in_exp || points >= 2) return ABN_OK;
        e.text[e.len++] = '.';
    } else if (op == AB_KE) {
        /* E starts the exponent of the mantissa typed so far (UG p.1-16). With no
         * mantissa the guide gives no meaning (it keys 1 E 6 for a million): ignored. A
         * fraction has no exponent (ours, unit 007): ignored too. */
        if (in_exp || points == 2 || count_digits(&e, false) == 0) return ABN_OK;
        e.text[e.len++] = 'E';
    } else if (op == AB_KCHS) {
        /* During entry, change sign applies to the part being typed: the exponent once E
         * has been pressed, the mantissa before (UG p.1-15, p.1-16). */
        if (in_exp) e.exp_neg = !e.exp_neg;
        else e.mant_neg = !e.mant_neg;
    } else if (op == AB_KBKSP) {
        /* Backspace erases the last key typed (UG p.1-17). Erasing the E also drops the
         * exponent's sign. Erasing everything leaves 0 with lift disabled, as clear X
         * does (our choice; the guide does not say). */
        if (c->cpart && e.len == 0) {
            /* with the second part empty, backspace erases the i (or theta) and the first part is
               being typed again (ours; the guide does not say) */
            c->cpart = 0;
            e = c->entry1;
            if (e.len == 0) {
                c->entry.active = false;
                c->x = ab_real(abn_zero());
                c->lift = false;
                return ABN_OK;
            }
            e.active = true;
            c->entry = e;
            goto value;
        }
        if (e.len > 0 && e.text[--e.len] == 'E') e.exp_neg = false;
        if (e.len == 0 && !c->cpart) {
            c->entry.active = false;
            c->x = ab_real(abn_zero());
            c->lift = false;
            return ABN_OK;
        }
    }
    abn v;
    ab_val nv;
value:
    if (entry_value(&e, &v, c->base) != ABN_OK) return ABN_OK;  /* out of range: key ignored */
    nv = ab_real(v);
    if (c->cpart && complex_entry_value(c, &v, &nv) != ABN_OK) return ABN_OK;
    if (!c->entry.active) {
        /* The first key of a number places it in X, lifting if lift is enabled. Digit
         * entry itself leaves the lift flag alone (UG p.B-5). */
        place_val(c, nv);
    } else {
        c->x = nv;
    }
    e.active = true;
    c->entry = e;
    return ABN_OK;
}

/* The i of x i y or the theta of r theta a (UG p.9-2, p.4-11): what was typed becomes the first
 * part and the second part starts empty (0). With nothing typed the first part is 0 and the
 * number arrives in X as any new number does (ours). A second i or theta is ignored (ours). In
 * HEX, OCT and BIN a complex number cannot be typed (ours): an error, nothing changed. */
static abn_status complex_key(ab_calc *c, ab_op op)
{
    bool was_active = c->entry.active;
    abn zero = abn_zero();
    ab_val nv;
    ab_entry saved = c->entry, saved1 = c->entry1;
    int saved_part = c->cpart;
    if (c->base != 10) return ABN_DOMAIN;
    if (c->cpart) return ABN_OK;
    if (was_active) c->entry1 = c->entry;
    else memset(&c->entry1, 0, sizeof c->entry1);
    c->cpart = op == AB_KI ? 1 : 2;
    if (complex_entry_value(c, &zero, &nv) != ABN_OK) {
        c->entry = saved;
        c->entry1 = saved1;
        c->cpart = saved_part;
        return ABN_OK;
    }
    memset(&c->entry, 0, sizeof c->entry);
    c->entry.active = true;
    if (!was_active) place_val(c, nv);
    else c->x = nv;
    return ABN_OK;
}

/* ---- operations ---- */

/* Flags 8 and 9 choose the fraction format (UG p.5-6, p.14-10): 8 clear, most precise; 8 set,
 * factors of /c; 8 and 9 set, fixed denominator /c. */
static fr_format frac_format_of(const ab_calc *c)
{
    return !c->flag8 ? FR_PRECISE : (c->flag9 ? FR_FIXED : FR_FACTORS);
}

static fn_angle angle_of(const ab_calc *c)
{
    return c->angle == AB_ANGLE_RAD ? FN_RAD : (c->angle == AB_ANGLE_GRAD ? FN_GRAD : FN_DEG);
}

/* A function of X alone replaces X and saves the old X in LAST X (UG p.B-6); an error leaves
 * the stack and LAST X as they were (ours, unit 003). */
/* The functions of a complex x (UG p.9-3): 1/z, ln z, e^z, sin z, cos z, tan z (radians whatever
 * the angle mode: the guide's sin(2 i 3)) and ABS, which is real. LAST X as for real x (UG p.B-6). */
static abn_status one_complex(ab_calc *c, ab_op op)
{
    cplx z = cx_of(&c->x), r;
    ab_val nv;
    abn_status st;
    switch (op) {
    case AB_INV: st = cx_inv(&r, &z); break;
    case AB_LN: st = cx_ln(&r, &z); break;
    case AB_EXP: st = cx_exp(&r, &z); break;
    case AB_SIN: st = cx_sin(&r, &z); break;
    case AB_COS: st = cx_cos(&r, &z); break;
    case AB_TAN: st = cx_tan(&r, &z); break;
    default: st = cx_abs(&r.re, &z); break;    /* AB_ABS */
    }
    if (st != ABN_OK) return st;
    nv = op == AB_ABS ? ab_real(r.re) : ab_cx(r);
    c->lastx = c->x;
    c->x = nv;
    c->lift = true;
    return ABN_OK;
}

static abn_status one_number_fn(ab_calc *c, ab_op op)
{
    abn r;
    abn_status st;
    fn_angle m = angle_of(c);
    if (is_cx(&c->x)) return one_complex(c, op);
    switch (op) {
    case AB_EXP: st = fn_exp(&r, &c->x.re); break;
    case AB_LN: st = fn_ln(&r, &c->x.re); break;
    case AB_TENX: st = fn_tenx(&r, &c->x.re); break;
    case AB_LOG: st = fn_log(&r, &c->x.re); break;
    case AB_SQ: st = fn_sq(&r, &c->x.re); break;
    case AB_SQRT: st = fn_sqrt(&r, &c->x.re); break;
    case AB_INV: st = fn_inv(&r, &c->x.re); break;
    case AB_SIN: st = fn_sin(&r, &c->x.re, m); break;
    case AB_COS: st = fn_cos(&r, &c->x.re, m); break;
    case AB_TAN: st = fn_tan(&r, &c->x.re, m); break;
    case AB_ASIN: st = fn_asin(&r, &c->x.re, m); break;
    case AB_ACOS: st = fn_acos(&r, &c->x.re, m); break;
    case AB_ATAN: st = fn_atan(&r, &c->x.re, m); break;
    case AB_SINH: st = fn_sinh(&r, &c->x.re); break;
    case AB_COSH: st = fn_cosh(&r, &c->x.re); break;
    case AB_TANH: st = fn_tanh(&r, &c->x.re); break;
    case AB_ASINH: st = fn_asinh(&r, &c->x.re); break;
    case AB_ACOSH: st = fn_acosh(&r, &c->x.re); break;
    case AB_ATANH: st = fn_atanh(&r, &c->x.re); break;
    case AB_HMS: st = fn_hms(&r, &c->x.re); break;           /* UG p.4-13 */
    case AB_HMSINV: st = fn_hmsinv(&r, &c->x.re); break;
    case AB_TODEG: st = fn_todeg(&r, &c->x.re); break;       /* UG p.4-13, p.4-14 */
    case AB_TORAD: st = fn_torad(&r, &c->x.re); break;
    case AB_TOKG: st = fn_convert(&r, &c->x.re, FN_TOKG); break;     /* UG p.4-14 */
    case AB_TOLB: st = fn_convert(&r, &c->x.re, FN_TOLB); break;
    case AB_TOC: st = fn_convert(&r, &c->x.re, FN_TOC); break;
    case AB_TOF: st = fn_convert(&r, &c->x.re, FN_TOF); break;
    case AB_TOCM: st = fn_convert(&r, &c->x.re, FN_TOCM); break;
    case AB_TOIN: st = fn_convert(&r, &c->x.re, FN_TOIN); break;
    case AB_TOL: st = fn_convert(&r, &c->x.re, FN_TOL); break;
    case AB_TOGAL: st = fn_convert(&r, &c->x.re, FN_TOGAL); break;
    case AB_TOKM: st = fn_convert(&r, &c->x.re, FN_TOKM); break;
    case AB_TOMILE: st = fn_convert(&r, &c->x.re, FN_TOMILE); break;
    case AB_FACT: st = fn_fact(&r, &c->x.re); break;         /* UG p.4-15 */
    case AB_IP: st = fn_ip(&r, &c->x.re); break;             /* UG p.4-17, p.4-18 */
    case AB_FP: st = fn_fp(&r, &c->x.re); break;
    case AB_INTG: st = fn_intg(&r, &c->x.re); break;
    case AB_ABS: st = fn_abs(&r, &c->x.re); break;
    case AB_SGN: st = fn_sgn(&r, &c->x.re); break;
    default:    /* AB_RND */
        if (c->fdisp) {
            /* RND in Fraction-display mode: x becomes the shown fraction, rounded once (UG p.5-8,
               unit 007 rule 6); beyond fractions, as the decimal format */
            fr_result fr;
            if (fr_approx(&fr, &c->x.re, c->frac_c, frac_format_of(c)) == ABN_OK) {
                st = fr_value(&r, &fr);
                break;
            }
        }
        st = fn_rnd(&r, &c->x.re, c->disp_mode, c->disp_n);
        break;
    }
    if (st != ABN_OK) return st;
    c->lastx = c->x;
    c->x = ab_real(r);
    c->lift = true;
    return ABN_OK;
}

/* Percent and percent change give their result in X and keep the base number in Y: the stack
 * does not drop (UG p.4-6). Both save x in LAST X (UG p.B-6). */
static abn_status percent_fn(ab_calc *c, ab_op op)
{
    abn r;
    abn_status st = op == AB_PCT ? fn_pct(&r, &c->y.re, &c->x.re) : fn_pctchg(&r, &c->y.re, &c->x.re);
    if (st != ABN_OK) return st;
    c->lastx = c->x;
    c->x = ab_real(r);
    c->lift = true;
    return ABN_OK;
}

/* z1 op z2 with z1 in Y and z2 in X, either complex, a real one with imaginary part 0; the result
 * is complex even when its imaginary part is 0 (unit 009 rule 3). */
static abn_status two_complex(ab_calc *c, ab_op op)
{
    cplx a = cx_of(&c->y), b = cx_of(&c->x), r;
    abn_status st;
    switch (op) {
    case AB_ADD: st = cx_add(&r, &a, &b); break;
    case AB_SUB: st = cx_sub(&r, &a, &b); break;
    case AB_MUL: st = cx_mul(&r, &a, &b); break;
    case AB_DIV: st = cx_div(&r, &a, &b); break;
    default: st = cx_pow(&r, &a, &b); break;   /* AB_POW: y^x */
    }
    if (st != ABN_OK) return st;
    c->lastx = c->x;
    c->x = ab_cx(r);
    stack_drop(c);
    c->lift = true;
    return ABN_OK;
}

static abn_status two_number_op(ab_calc *c, ab_op op)
{
    abn r;
    abn_status st;
    if (is_cx(&c->x) || is_cx(&c->y)) return two_complex(c, op);
    /* Y op X: the earlier number is on the left (UG p.2-5). */
    switch (op) {
    case AB_ADD: st = abn_add(&r, &c->y.re, &c->x.re); break;
    case AB_SUB: st = abn_sub(&r, &c->y.re, &c->x.re); break;
    case AB_MUL: st = abn_mul(&r, &c->y.re, &c->x.re); break;
    case AB_DIV: st = abn_div(&r, &c->y.re, &c->x.re); break;
    case AB_POW: st = fn_pow(&r, &c->y.re, &c->x.re); break;        /* y^x (UG p.4-2) */
    case AB_XROOT: st = fn_xroot(&r, &c->y.re, &c->x.re); break;    /* x-th root of y (UG p.4-3) */
    case AB_IDIV: st = fn_idiv(&r, &c->y.re, &c->x.re); break;      /* INT÷ (UG p.4-2) */
    case AB_NCR: st = fn_ncr(&r, &c->y.re, &c->x.re); break;        /* n in Y, r in X (UG p.4-16) */
    case AB_NPR: st = fn_npr(&r, &c->y.re, &c->x.re); break;
    default: st = fn_rmdr(&r, &c->y.re, &c->x.re); break;           /* Rmdr (UG p.4-2) */
    }
    if (st != ABN_OK) return st;    /* the stack stays as it was */
    c->lastx = c->x;                /* these operations save x in LAST X (UG p.B-6) */
    c->x = ab_real(r);
    stack_drop(c);
    c->lift = true;
    return ABN_OK;
}

/* ---- storage (unit 005; UG p.3-1 to 3-9, p.14-20 to 14-24) ---- */

/* A storage location: a lettered variable (which may hold a complex number), or a register that
 * holds a real (a statistics register or an unnamed indirect variable, with its address). */
typedef struct {
    ab_val *val;            /* A..Z, or NULL */
    abn *reg;               /* otherwise */
    int ind;                /* 0..800 for an unnamed indirect variable, otherwise -1 */
} slot;

/* The integer part of x as an int when it lies in [lo, hi]. */
static bool int_in(const abn *x, int lo, int hi, int *v)
{
    abn t;
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead, n, r = 0;
    if (abn_trunc(&t, x) != ABN_OK) return false;
    abn_digits(&t, &neg, d, &lead);
    n = (int)strlen(d);
    if (n == 0) {
        *v = 0;
        return lo <= 0 && 0 <= hi;
    }
    if (lead > 4) return false;
    for (int i = 0; i <= lead; i++) r = r * 10 + (i < n ? d[i] - '0' : 0);
    r = neg ? -r : r;
    if (r < lo || r > hi) return false;
    *v = r;
    return true;
}

/* A variable code names A..Z directly, or (I)/(J): the number in I or J chooses (UG p.14-22):
 * -1..-26 are A..Z, -27..-32 the statistics registers (n, sum x, sum y, sum x^2, sum y^2,
 * sum xy), 0..800 the unnamed indirect variables. A non-integer index is truncated (ours, unit
 * 005; the guide does not say). An index outside -32..800 is INVALID (I) (UG p.14-22); so is
 * reading an unnamed variable above the defined range (UG p.14-24). */
static abn_status resolve(ab_calc *c, int code, bool for_read, slot *out)
{
    int k;
    out->ind = -1;
    out->val = NULL;
    out->reg = NULL;
    if (code >= AB_VAR_A && code < AB_VAR_A + 26) {
        out->val = &c->var[code - AB_VAR_A];
        return ABN_OK;
    }
    if (code != AB_VAR_IND_I && code != AB_VAR_IND_J) return ABN_DOMAIN;
    const ab_val *idx = &c->var[(code == AB_VAR_IND_I ? 'I' : 'J') - 'A'];
    if (is_cx(idx) || !int_in(&idx->re, -32, AB_IND_MAX, &k)) return ABN_DOMAIN;  /* INVALID (I) */
    if (k <= -27) {
        out->reg = &c->stat[-k - 27];
    } else if (k < 0) {
        out->val = &c->var[-k - 1];
    } else {
        if (for_read && k > c->ind_top) return ABN_DOMAIN;          /* INVALID (I): undefined */
        out->reg = &c->ind[k];
        out->ind = k;
    }
    return ABN_OK;
}

/* Writes v to a location. The unnamed indirect variables are defined from address 0 up to the
 * last nonzero one: storing a nonzero value above extends the range, and storing 0 in the last
 * one releases it down to the next nonzero (UG p.14-24). */
static void write_slot(ab_calc *c, const slot *sl, abn v)
{
    *sl->reg = v;
    if (sl->ind < 0) return;
    if (!abn_is_zero(&v)) {
        if (sl->ind > c->ind_top) c->ind_top = sl->ind;
    } else if (sl->ind == c->ind_top) {
        while (c->ind_top >= 0 && abn_is_zero(&c->ind[c->ind_top])) c->ind_top--;
    }
}

/* The value at a location. */
static ab_val load_val(const slot *sl)
{
    return sl->val ? *sl->val : ab_real(*sl->reg);
}

/* Stores v. The registers that hold reals refuse a complex number (ours, unit 009: the guide
 * shows complex numbers in the lettered variables, UG p.9-2): INVALID DATA, nothing changed. */
static abn_status store_val(ab_calc *c, const slot *sl, ab_val v)
{
    if (sl->val) {
        *sl->val = v;
        return ABN_OK;
    }
    if (is_cx(&v)) return ABN_DOMAIN;
    write_slot(c, sl, v.re);
    return ABN_OK;
}

/* a op b for storage and recall arithmetic; complex when either is (unit 009 rule 3). */
static abn_status arith(ab_val *r, ab_op op, const ab_val *a, const ab_val *b)
{
    abn_status st;
    if (is_cx(a) || is_cx(b)) {
        cplx x = cx_of(a), y = cx_of(b), z;
        switch (op) {
        case AB_STOADD: case AB_RCLADD: st = cx_add(&z, &x, &y); break;
        case AB_STOSUB: case AB_RCLSUB: st = cx_sub(&z, &x, &y); break;
        case AB_STOMUL: case AB_RCLMUL: st = cx_mul(&z, &x, &y); break;
        default: st = cx_div(&z, &x, &y); break;
        }
        if (st == ABN_OK) *r = ab_cx(z);
        return st;
    }
    abn v;
    switch (op) {
    case AB_STOADD: case AB_RCLADD: st = abn_add(&v, &a->re, &b->re); break;
    case AB_STOSUB: case AB_RCLSUB: st = abn_sub(&v, &a->re, &b->re); break;
    case AB_STOMUL: case AB_RCLMUL: st = abn_mul(&v, &a->re, &b->re); break;
    default: st = abn_div(&v, &a->re, &b->re); break;
    }
    if (st == ABN_OK) *r = ab_real(v);
    return st;
}

static abn_status storage_op(ab_calc *c, ab_op op, int code)
{
    slot sl;
    ab_val r, v;
    abn_status st;
    bool reads = op != AB_STO;
    if ((st = resolve(c, code, reads, &sl)) != ABN_OK) return st;
    switch (op) {
    case AB_STO:
        /* STO copies x into the variable; X keeps its value (UG p.3-2). */
        if ((st = store_val(c, &sl, c->x)) != ABN_OK) return st;
        break;
    case AB_RCL:
        /* RCL brings a copy into X like any recalled number (UG p.3-2, p.3-4; unit 005 rule 2). */
        place_val(c, load_val(&sl));
        break;
    case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:
        /* Storage arithmetic: variable = variable op x; the stack is untouched (UG p.3-6) and so
           is LAST X (not in UG p.B-6's list). An error changes nothing (unit 005 rule 3). */
        v = load_val(&sl);
        if ((st = arith(&r, op, &v, &c->x)) != ABN_OK) return st;
        if ((st = store_val(c, &sl, r)) != ABN_OK) return st;
        break;
    case AB_RCLADD: case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV:
        /* Recall arithmetic: x = x op variable; only X changes (UG p.3-7), and x is saved in
           LAST X (UG p.B-6). */
        v = load_val(&sl);
        if ((st = arith(&r, op, &c->x, &v)) != ABN_OK) return st;
        c->lastx = c->x;
        c->x = r;
        break;
    case AB_XVAR:
        /* x<>variable swaps them; Y, Z, T are untouched (UG p.3-8). */
        v = load_val(&sl);
        if ((st = store_val(c, &sl, c->x)) != ABN_OK) return st;
        c->x = v;
        break;
    default:
        /* VIEW shows the variable and changes nothing; it is not among the neutral operations
           of UG p.B-5, so like any other it enables lift (ours, unit 005). */
        break;
    }
    c->lift = true;
    return ABN_OK;
}

/* ---- bases and logic (unit 008; UG ch.11) ---- */

/* A suffix rereads the digits being typed in its base and ends digit entry without ENTER
 * (UG p.11-3). A digit too big for that base is INVALID DATA (UG p.F) and the entry stays as it
 * was. With no entry in progress it does nothing (ours). */
static abn_status suffix(ab_calc *c, int base)
{
    char d[AB_ENTRY_TEXT];
    int64_t v;
    abn val;
    if (!c->entry.active) return ABN_OK;
    if (c->cpart) return ABN_DOMAIN;    /* a suffix reads a real entry (ours, unit 009) */
    if (base != 16 && base != 8 && base != 2 && base != 10) return ABN_DOMAIN;
    memcpy(d, c->entry.text, (size_t)c->entry.len);
    d[c->entry.len] = '\0';
    if (base == 10) {
        for (int i = 0; i < c->entry.len; i++)
            if (d[i] >= 'a' && d[i] <= 'f') return ABN_DOMAIN;
        if (entry_value(&c->entry, &val, 10) != ABN_OK) return ABN_DOMAIN;
    } else {
        if (strchr(d, '.') || strchr(d, 'E') || !base_parse(d, base, &v)) return ABN_DOMAIN;
        val = base_from_word(c->entry.mant_neg ? -v : v);
    }
    c->x = ab_real(val);
    c->entry.active = false;
    c->lift = true;
    return ABN_OK;
}

/* a * b clamped to the word's range, without overflowing the 64-bit product. */
static int64_t mul_clamp(int64_t a, int64_t b, bool *clamped)
{
    if (a == 0 || b == 0) return 0;
    int64_t aa = a < 0 ? -a : a, bb = b < 0 ? -b : b;
    if (aa > (int64_t)1 << 37 || bb > ((int64_t)1 << 62) / aa) {
        *clamped = true;
        return (a < 0) != (b < 0) ? base_min() : base_max();
    }
    return base_clamp(a * b, clamped);
}

/* + - * / in HEX, OCT, BIN: on the integer parts, the result truncated toward zero (UG p.11-4,
 * 11-5; toward zero is ours). An operand or result beyond the range is replaced by the nearest
 * end and the operation completes, reported as ABN_CLAMPED (UG p.11-7; ours, unit 008 rule 3). */
static abn_status base_arith(ab_calc *c, ab_op op)
{
    int64_t a, b, r;
    bool clamped = false;
    abn_status st;
    if ((st = base_to_word(&c->y.re, &a, &clamped)) != ABN_OK) return st;
    if ((st = base_to_word(&c->x.re, &b, &clamped)) != ABN_OK) return st;
    switch (op) {
    case AB_ADD: r = base_clamp(a + b, &clamped); break;
    case AB_SUB: r = base_clamp(a - b, &clamped); break;
    case AB_MUL: r = mul_clamp(a, b, &clamped); break;
    default:
        if (b == 0) return ABN_DIV_ZERO;
        r = base_clamp(a / b, &clamped);
        break;
    }
    c->lastx = c->x;
    c->x = ab_real(base_from_word(r));
    stack_drop(c);
    c->lift = true;
    return clamped ? ABN_CLAMPED : ABN_OK;
}

/* AND XOR OR NAND NOR on Y and X, NOT on X: the integer patterns of the word, the signed result
 * (UG p.11-4). A non-integer argument is INVALID DATA (UG p.F-2); one beyond the range is first
 * replaced as for arithmetic, and reported. LAST X is untouched (not in UG p.B-6). */
static abn_status logic(ab_calc *c, ab_op op)
{
    int64_t a = 0, b, r;
    bool clamped = false;
    abn_status st;
    if (!abn_is_integer(&c->x.re) || (op != AB_NOT && !abn_is_integer(&c->y.re))) return ABN_DOMAIN;
    if ((st = base_to_word(&c->x.re, &b, &clamped)) != ABN_OK) return st;
    if (op == AB_NOT) {
        r = base_not(b);
        c->x = ab_real(base_from_word(r));
    } else {
        if ((st = base_to_word(&c->y.re, &a, &clamped)) != ABN_OK) return st;
        base_logic bl = op == AB_AND ? BL_AND : op == AB_OR ? BL_OR : op == AB_XOR ? BL_XOR
                      : op == AB_NAND ? BL_NAND : BL_NOR;
        r = base_logic2(bl, a, b);
        c->x = ab_real(base_from_word(r));
        stack_drop(c);
    }
    c->lift = true;
    return clamped ? ABN_CLAMPED : ABN_OK;
}

abn_status ab_do(ab_calc *c, ab_op op)
{
    return ab_do_arg(c, op, 0);
}

/* Operations that take x (and y) as numbers. */
static bool reads_x(ab_op op)
{
    return (op >= AB_ADD && op <= AB_PCTCHG && op != AB_PI && op != AB_DEG && op != AB_RAD &&
            op != AB_GRAD) ||
           (op >= AB_HMS && op <= AB_TOREC && op != AB_RAND) || op == AB_SIGMAPLUS ||
           op == AB_SIGMAMINUS || op == AB_XHAT || op == AB_YHAT || op == AB_SETC ||
           (op >= AB_AND && op <= AB_NOR) || op == AB_ARG;
}

static bool reads_y(ab_op op)
{
    switch (op) {
    case AB_ADD: case AB_SUB: case AB_MUL: case AB_DIV: case AB_IDIV: case AB_RMDR: case AB_POW:
    case AB_XROOT: case AB_PCT: case AB_PCTCHG: case AB_NCR: case AB_NPR: case AB_TOPOL:
    case AB_TOREC: case AB_SIGMAPLUS: case AB_SIGMAMINUS: case AB_AND: case AB_XOR: case AB_OR:
    case AB_NAND: case AB_NOR:
        return true;
    default:
        return false;
    }
}

/* The guide's list of what takes complex numbers (UG p.9-3), in decimal; ABS and ARG give reals. */
static bool takes_complex(ab_op op)
{
    switch (op) {
    case AB_ADD: case AB_SUB: case AB_MUL: case AB_DIV: case AB_POW: case AB_INV: case AB_LN:
    case AB_EXP: case AB_SIN: case AB_COS: case AB_TAN: case AB_ABS: case AB_ARG:
        return true;
    default:
        return false;
    }
}

abn_status ab_do_arg(ab_calc *c, ab_op op, int arg)
{
    if (op == AB_KI || op == AB_KTHETA) return complex_key(c, op);
    if (op <= AB_KE || (op >= AB_KA && op <= AB_KF)) return entry_key(c, op);
    if (op == AB_SUF) return suffix(c, arg);
    if ((op == AB_KCHS || op == AB_KBKSP) && c->entry.active) return entry_key(c, op);

    /* Every other key completes the number being entered (UG p.1-17). A completed typed
     * number enables lift before the operation acts, so a recall after `1 ENTER 2` keeps
     * the 2 (work unit 001, amendment 2, ruling 1; the guide does not show this case). */
    if (c->entry.active) {
        c->entry.active = false;
        c->lift = true;
    }
    c->cpart = 0;

    /* Any other function given a complex number is INVALID DATA (UG p.9-3, p.F), nothing changed. */
    if (((reads_x(op) && is_cx(&c->x)) || (reads_y(op) && is_cx(&c->y))) &&
        !(takes_complex(op) && c->base == 10))
        return ABN_DOMAIN;

    /* In HEX the keys of these functions type digits, and the guide lists them as unavailable
     * (UG p.11-4): an error here, changing nothing. */
    if (c->base == 16 && (op == AB_SQRT || op == AB_EXP || op == AB_LN || op == AB_POW ||
                          op == AB_INV || op == AB_SIGMAPLUS))
        return ABN_DOMAIN;

    switch (op) {
    case AB_KBKSP:  /* with no entry in progress backspace acts as clear X (UG p.1-17) */
    case AB_CLX:
        /* Clear X puts 0 in X and disables lift, so the next number replaces it
         * (UG p.2-7, p.B-5). */
        c->x = ab_real(abn_zero());
        c->lift = false;
        return ABN_OK;
    case AB_CLSTK:
        /* Clear the stack: all four registers to 0, lift disabled (UG p.2-7, p.B-5). */
        c->x = c->y = c->z = c->t = ab_real(abn_zero());
        c->lift = false;
        return ABN_OK;
    case AB_ENTER:
        /* ENTER lifts the stack, copying X into Y, and disables lift so the next number
         * writes over the copy left in X (UG p.2-6, p.B-5). */
        stack_lift(c);
        c->lift = false;
        return ABN_OK;
    case AB_RDN: {
        /* Roll down: Y to X, Z to Y, T to Z, X to T (UG p.2-3). */
        ab_val x = c->x;
        c->x = c->y;
        c->y = c->z;
        c->z = c->t;
        c->t = x;
        break;
    }
    case AB_RUP: {
        /* Roll up: X to Y, Y to Z, Z to T, T to X (UG p.2-4). */
        ab_val t = c->t;
        c->t = c->z;
        c->z = c->y;
        c->y = c->x;
        c->x = t;
        break;
    }
    case AB_XY: {
        /* Exchange X and Y; Z and T are untouched (UG p.2-4). */
        ab_val x = c->x;
        c->x = c->y;
        c->y = x;
        break;
    }
    case AB_LASTX:
        /* LAST X brings back the number X held before the last numeric operation; it
         * arrives like any new number, and the LAST X register keeps it (UG p.2-8, p.2-11). */
        place_val(c, c->lastx);
        break;
    case AB_KCHS:
        /* Change sign of a completed X (UG p.1-15); of both parts of a complex one (UG p.9-3). */
        c->x.re = abn_neg(&c->x.re);
        c->x.im = abn_neg(&c->x.im);
        break;
    case AB_ARG: {
        /* The angle of x i y in the angle mode (UG p.9-3); a real x is x i 0. Saves x in LAST X as
           ABS does (UG p.B-6). */
        abn r, th;
        abn_status st = fn_topol(&r, &th, &c->x.re, &c->x.im, angle_of(c));
        if (st != ABN_OK) return st;
        c->lastx = c->x;
        c->x = ab_real(th);
        break;
    }
    case AB_XIY:
    case AB_RTA:
        /* the complex display form; neutral for lift (UG p.9-1, p.B-5) */
        c->cpolar = op == AB_RTA;
        return ABN_OK;
    case AB_ADD:
    case AB_SUB:
    case AB_MUL:
    case AB_DIV:
        if (c->base != 10) return base_arith(c, op);
        return two_number_op(c, op);
    case AB_POW:
    case AB_XROOT:
    case AB_IDIV:
    case AB_RMDR:
    case AB_NCR:
    case AB_NPR:
        return two_number_op(c, op);
    case AB_HMS: case AB_HMSINV: case AB_TODEG: case AB_TORAD:
    case AB_TOKG: case AB_TOLB: case AB_TOC: case AB_TOF: case AB_TOCM: case AB_TOIN:
    case AB_TOL: case AB_TOGAL: case AB_TOKM: case AB_TOMILE:
    case AB_FACT: case AB_IP: case AB_FP: case AB_INTG: case AB_ABS: case AB_SGN: case AB_RND:
        return one_number_fn(c, op);
    case AB_TOPOL:
    case AB_TOREC: {
        /* Two numbers in, two out: x and y in X and Y become r and theta, or back (ours, unit
           004). LAST X keeps the old x, as for the other conversions (UG p.B-6). */
        abn a, b;
        abn_status st = op == AB_TOPOL ? fn_topol(&a, &b, &c->x.re, &c->y.re, angle_of(c))
                                       : fn_torec(&a, &b, &c->x.re, &c->y.re, angle_of(c));
        if (st != ABN_OK) return st;
        c->lastx = c->x;
        c->x = ab_real(a);
        c->y = ab_real(b);
        break;
    }
    case AB_RAND:
        /* RAND arrives like a recalled number (UG p.4-15). */
        place_in_x(c, fn_rand(&c->rng));
        break;
    case AB_SEED:
        /* SEED takes x as the seed and leaves the stack as it is (UG p.4-15; ours: X stays). */
        fn_seed(&c->rng, &c->x.re);
        break;
    case AB_CONST: {
        abn v;
        abn_status st = ab_const_value(&v, arg);
        if (st != ABN_OK) return st;
        place_in_x(c, v);   /* a constant arrives like a recalled number (UG p.4-8) */
        break;
    }
    case AB_STO: case AB_RCL:
    case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:
    case AB_RCLADD: case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV:
    case AB_XVAR: case AB_VIEW:
        return storage_op(c, op, arg);
    case AB_SIGMAPLUS:
    case AB_SIGMAMINUS: {
        /* Sigma+ / Sigma- add or remove the pair (x in X, y in Y; UG p.12-2): X becomes n, Y keeps
           y, x goes to LAST X (UG p.12-3, p.B-6) and lift is disabled (UG p.B-5). All six
           registers change or none does (ours, unit 006 rule 2). */
        abn_status st = st_accumulate(c->stat, &c->x.re, &c->y.re, op == AB_SIGMAMINUS);
        if (st != ABN_OK) return st;
        c->lastx = c->x;
        c->x = ab_real(c->stat[ST_N]);
        c->lift = false;
        return ABN_OK;
    }
    case AB_CLSIGMA:
        /* Clear the six statistics registers; neutral for lift (UG p.12-2, p.B-5). */
        for (int i = 0; i < AB_STAT_REGS; i++) c->stat[i] = abn_zero();
        return ABN_OK;
    case AB_XHAT:
    case AB_YHAT: {
        /* x-hat of the y in X, y-hat of the x in X (UG p.12-8); the input goes to LAST X
           (UG p.B-6). */
        abn r;
        abn_status st = op == AB_XHAT ? st_xhat(&r, c->stat, &c->x.re) : st_yhat(&r, c->stat, &c->x.re);
        if (st != ABN_OK) return st;
        c->lastx = c->x;
        c->x = ab_real(r);
        break;
    }
    case AB_XBAR: case AB_YBAR: case AB_XWBAR: case AB_SX: case AB_SY: case AB_PSX: case AB_PSY:
    case AB_CORR: case AB_SLOPE: case AB_YINT: {
        /* A statistic arrives like a recalled number, one value in X (unit 006 rule 1: the guide
           is silent on lift); LAST X is untouched (not in UG p.B-6). STAT ERROR cases change
           nothing. */
        abn r;
        abn_status st;
        switch (op) {
        case AB_XBAR: st = st_mean(&r, c->stat, false); break;
        case AB_YBAR: st = st_mean(&r, c->stat, true); break;
        case AB_XWBAR: st = st_wmean(&r, c->stat); break;
        case AB_SX: st = st_sdev(&r, c->stat, false, false); break;
        case AB_SY: st = st_sdev(&r, c->stat, true, false); break;
        case AB_PSX: st = st_sdev(&r, c->stat, false, true); break;
        case AB_PSY: st = st_sdev(&r, c->stat, true, true); break;
        case AB_CORR: st = st_corr(&r, c->stat); break;
        case AB_SLOPE: st = st_slope(&r, c->stat); break;
        default: st = st_yint(&r, c->stat); break;
        }
        if (st != ABN_OK) return st;
        place_in_x(c, r);
        break;
    }
    case AB_NSUM: case AB_SUMX: case AB_SUMY: case AB_SUMX2: case AB_SUMY2: case AB_SUMXY:
        /* The summation registers, recalled (UG p.12-11). */
        place_in_x(c, c->stat[op - AB_NSUM]);
        break;
    case AB_DEC:
    case AB_HEX:
    case AB_OCT:
    case AB_BIN:
        /* the base mode changes only the display; neutral for lift (UG p.11-2, p.B-5) */
        c->base = op == AB_DEC ? 10 : op == AB_HEX ? 16 : op == AB_OCT ? 8 : 2;
        return ABN_OK;
    case AB_AND: case AB_XOR: case AB_OR: case AB_NOT: case AB_NAND: case AB_NOR:
        return logic(c, op);
    case AB_FDISP:
        /* Fraction-display mode on or off; neutral for lift (UG p.5-1, p.B-5) */
        c->fdisp = !c->fdisp;
        return ABN_OK;
    case AB_SF:
    case AB_CF:
        /* Flags 7 (Fraction display), 8 and 9 (its format) (UG p.14-10); others come with
           programming. Not in UG p.B-5's neutral list, so like any other they enable lift. */
        if (arg == 7) c->fdisp = op == AB_SF;
        else if (arg == 8) c->flag8 = op == AB_SF;
        else if (arg == 9) c->flag9 = op == AB_SF;
        else return ABN_DOMAIN;
        break;
    case AB_SETC: {
        /* /c from |IP(x)| (UG p.5-5): 1 recalls the maximum denominator into X; 0 or anything
           above 4095 resets it to 4095; otherwise it is set. Setting turns Fraction display on.
           LAST X is untouched. After setting, X keeps the number and lift is disabled, so the
           next number replaces it (ours, unit 007: the guide's examples show Y = 0 there). */
        abn t, big = abn_from_int(4095), one = abn_from_int(1);
        if (abn_trunc(&t, &c->x.re) != ABN_OK) return ABN_DOMAIN;
        t = abn_abs(&t);
        if (abn_cmp(&t, &one) == 0) {
            c->x = ab_real(abn_from_int((int32_t)c->frac_c));
            break;
        }
        if (abn_is_zero(&t) || abn_cmp(&t, &big) > 0) {
            c->frac_c = 4095;
        } else {
            int v;
            bool ng;
            char dg[ABN_DIGITS + 1];
            int ld;
            abn_digits(&t, &ng, dg, &ld);
            v = 0;
            for (int i = 0; i <= ld; i++) v = v * 10 + (i < (int)strlen(dg) ? dg[i] - '0' : 0);
            c->frac_c = (uint32_t)v;
        }
        c->fdisp = true;
        c->lift = false;
        return ABN_OK;
    }
    case AB_RCLC:
        /* /c recalled like any number (ours, unit 007: the guide recalls with 1 /c) */
        place_in_x(c, abn_from_int((int32_t)c->frac_c));
        break;
    case AB_CLVARS:
        /* Clear the direct variables A..Z (UG p.3-5); neutral for stack lift (UG p.B-5). The
           unnamed indirect variables have their own clearing (UG p.1-5, a later unit). */
        for (int i = 0; i < 26; i++) c->var[i] = ab_real(abn_zero());
        return ABN_OK;
    case AB_SETFIX:
    case AB_SETSCI:
    case AB_SETENG:
    case AB_SETALL:
        /* Display settings change no number and leave stack lift alone (UG p.1-21, p.B-5). */
        if (arg < 0 || arg > AB_FMT_N_MAX) return ABN_DOMAIN;
        c->disp_mode = op == AB_SETFIX ? AB_FIX : op == AB_SETSCI ? AB_SCI : op == AB_SETENG ? AB_ENG : AB_ALL;
        c->fdisp = false;   /* choosing a decimal format ends Fraction-display mode (UG p.5-2) */
        c->disp_n = op == AB_SETALL ? 0 : arg;
        return ABN_OK;
    case AB_PCT:
    case AB_PCTCHG:
        return percent_fn(c, op);
    case AB_PI:
        /* Pi arrives like a recalled number (UG p.4-3). */
        place_in_x(c, fn_pi());
        break;
    case AB_DEG:
    case AB_RAD:
    case AB_GRAD:
        /* Angle modes change no number and leave stack lift as it was (UG p.4-4, p.B-5). */
        c->angle = op == AB_DEG ? AB_ANGLE_DEG : (op == AB_RAD ? AB_ANGLE_RAD : AB_ANGLE_GRAD);
        return ABN_OK;
    case AB_EXP: case AB_LN: case AB_TENX: case AB_LOG:
    case AB_SQ: case AB_SQRT: case AB_INV:
    case AB_SIN: case AB_COS: case AB_TAN: case AB_ASIN: case AB_ACOS: case AB_ATAN:
    case AB_SINH: case AB_COSH: case AB_TANH: case AB_ASINH: case AB_ACOSH: case AB_ATANH:
        return one_number_fn(c, op);
    default:
        return ABN_OK;
    }
    /* Every operation not listed as disabling or neutral enables lift (UG p.B-4). */
    c->lift = true;
    return ABN_OK;
}
