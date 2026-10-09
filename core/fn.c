/* fn: the calculator's functions of one and two numbers (unit 003).
 *
 * The engine (abn.h) computes; this file adds the calculator's rules: exact decimal answers
 * where the answer is exact, angles in degrees and grads reduced exactly in decimal before any
 * conversion to radians, and domain errors instead of complex or infinite results. Rules from
 * the user's guide are cited "UG p.N"; rules this project set are "ours, unit 003".
 */
#include <string.h>

#include "fn.h"
#include "arena.h"
#include "sbig.h"
#include "bigint.h"
#include "hp.h"

/* unit 044 (below): the one-input functions correctly rounded */
typedef bool (*hp_f1)(hp *r, const hp *x, int p, int arg);
static abn_status sure(abn *out, abn_status st0, const abn *x, hp_f1 f, int arg);
static bool f_exp(hp *r, const hp *x, int p, int arg);
static bool f_ln(hp *r, const hp *x, int p, int arg);
static bool f_tenx(hp *r, const hp *x, int p, int arg);
static bool f_trig(hp *r, const hp *x, int p, int arg);
static bool f_inv(hp *r, const hp *x, int p, int arg);
static bool f_hyp(hp *r, const hp *x, int p, int arg);
static bool f_ahyp(hp *r, const hp *x, int p, int arg);
static bool f_angle(hp *r, const hp *x, int p, int arg);
#define UNIT3(m) (3 * ((m) == FN_RAD ? 0 : (m) == FN_DEG ? 1 : 2))  /* f_inv's unit */

/* Constants to 34 significant digits, from mpmath at 80 digits (pi also checked against
 * Machin's formula in Python's decimal module). */
#define PI_34 "3.141592653589793238462643383279503"

static abn K(const char *text)
{
    abn v = abn_zero();
    abn_from_text(&v, text);    /* constants in this file are well formed and in range */
    return v;
}

abn fn_pi(void)
{
    /* The guide's pi is its 12-digit approximation (UG p.4-3); ours carries 34 digits. */
    return K(PI_34);
}

/* ---- small exact helpers ---- */

static abn one(void)
{
    return abn_from_int(1);
}

/* x as an int32 when it is an integer of at most 9 digits (false for any fraction). Unit 054: read from the number
 * itself, not its text (yˣ's 34-digit path spent about 33 us a call here on the S3). */
static bool small_int(const abn *x, int32_t *v)
{
    return abn_small_int(x, v);
}

/* Below 1E-40 a function's series has its first term alone to far beyond 34 digits (the next term
 * is x squared smaller), and the engine underflows inside below about 1E-3000 (x squared), so the
 * functions below take that term themselves (found in unit 021's review). */
static bool tiny(const abn *x)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead;
    if (abn_is_zero(x)) return false;
    abn_digits(x, &neg, d, &lead);
    return lead < -40;
}

/* An odd integer: x mod 2 is +-1. */
static bool is_odd_integer(const abn *x)
{
    abn two = abn_from_int(2), r;
    if (!abn_is_integer(x) || abn_fmod(&r, x, &two) != ABN_OK) return false;
    return !abn_is_zero(&r);
}

/* y^n by repeated squaring, for n >= 0; ABN_OK only when every product is exact. */
static abn_status pow_exact(abn *out, const abn *y, uint32_t n)
{
    abn result = one(), b = *y;
    while (n) {
        if ((n & 1) && abn_mul_exact(&result, &result, &b) != ABN_OK) return ABN_INEXACT;
        n >>= 1;
        if (n && abn_mul_exact(&b, &b, &b) != ABN_OK) return ABN_INEXACT;
    }
    *out = result;
    return ABN_OK;
}

#define POW_EXACT_MAX 4096  /* exponents tried by exact squaring before the engine's pow */

/* x rounded to k significant digits (half away from zero), for exactness probes. */
static abn round_sig(const abn *x, int k)
{
    bool neg;
    char d[ABN_DIGITS + 1], text[ABN_DIGITS + 16];
    int lead, n, t = 0;
    abn_digits(x, &neg, d, &lead);
    n = (int)strlen(d);
    if (n == 0) return abn_zero();
    if (n > k) {
        bool up = d[k] >= '5';
        n = k;
        for (int i = k - 1; up && i >= 0; i--) {
            if (d[i] == '9') {
                d[i] = '0';
            } else {
                d[i]++;
                up = false;
            }
        }
        if (up) {   /* all nines: a power of ten */
            d[0] = '1';
            n = 1;
            lead++;
        }
    }
    if (neg) text[t++] = '-';
    text[t++] = d[0];
    text[t++] = '.';
    for (int i = 1; i < n; i++) text[t++] = d[i];
    text[t++] = '0';
    text[t++] = 'E';
    {
        char e[12];
        int en = 0;
        long v = lead < 0 ? -(long)lead : lead;
        if (lead < 0) text[t++] = '-';
        do {
            e[en++] = (char)('0' + v % 10);
            v /= 10;
        } while (v);
        while (en) text[t++] = e[--en];
    }
    text[t] = '\0';
    return K(text);
}

#define FN_HP 50        /* the digits these are formed to before their one rounding */

/* y^e for y > 0 as e^(e ln y) in hp, rounded once (units 026, 028). The logarithm and the product to
 * FN_HP digits: |e ln y| < 14200 in range, so the exponent carries 45 digits after its point, 11 beyond
 * the result's 34 (measured on the S3, unit 028: 50 digits cost 45% less than 70, with no case of
 * 24000 rounded differently). */
unsigned long fn_pow_hp_calls;

/* The precisions y^x and the root are formed to in turn until their rounding is sure (Ziv; unit 034's review
   showed two inputs can put a result within about 1E-68 of a rounding midpoint, beyond any fixed 50 digits). */
#ifndef POW_FIRST
#define POW_FIRST FN_HP     /* tests build one at 37 to show the loop at work (make accuracy) */
#endif
static const int POW_P[] = {POW_FIRST, 90, 150};

static abn_status pow_hp(abn *out, const abn *y, const abn *e)
{
    fn_pow_hp_calls++;
    static hp hy, he, one, t, l, w, r;
    abn_status st = ABN_RANGE;
    for (int i = 0; i < 3; i++) {
        int p = POW_P[i];
        hp_set_int(&one, 1);
        if (!hp_from_abn(&hy, y) || !hp_from_abn(&he, e) || !hp_add(&t, &hy, &one, true, HP_PMAX) ||
            !hp_ln(&l, &hy, &t, p) || !hp_mul(&w, &he, &l, p))
            return i ? st : ABN_RANGE;
        if (!hp_exp(&r, &w, p))         /* |e ln y| >= 10^7: beyond the range */
            return w.neg ? ABN_UNDERFLOW : ABN_OVERFLOW;
        if (hp_round_sure(out, &st, &r, p, &t)) break;      /* t free: the scratch */
    }
    return st;
}

/* The x-th root of y > 0 as e^(ln y / x) in hp, rounded once its rounding is sure (units 028, 034). */
static abn_status root_hp(abn *out, const abn *y, const abn *x)
{
    static hp hy, hx, one, t, l, w, r;
    abn_status st = ABN_RANGE;
    for (int i = 0; i < 3; i++) {
        int p = POW_P[i];
        hp_set_int(&one, 1);
        if (!hp_from_abn(&hy, y) || !hp_from_abn(&hx, x) || !hp_add(&t, &hy, &one, true, HP_PMAX) ||
            !hp_ln(&l, &hy, &t, p) || !hp_div(&w, &l, &hx, p))
            return i ? st : ABN_RANGE;
        if (!hp_exp(&r, &w, p))         /* |ln y / x| >= 10^7: beyond the range */
            return w.neg ? ABN_UNDERFLOW : ABN_OVERFLOW;
        if (hp_round_sure(out, &st, &r, p, &t)) break;
    }
    return st;
}

/* y^e for y > 0 (unit 028): exact by squaring where the integer power is exact (2^10 = 1024), and
 * otherwise e^(e ln y) in hp rounded once (pow_hp), so within a unit of the 34th digit everywhere. The
 * engine's pow missed by thousands of units for a base near 1 with a large exponent (6914 at
 * (1 - 2.8E-30)^-1.8E33, unit 026's review) and lost about one unit per power on integer powers. */
#define POW_XNUM_MAX 64     /* unit 038: integer exponents done exactly in the bigint code, then rounded once */

/* y^n (or y^-n) for 2 <= n <= POW_XNUM_MAX: the exact power by repeated squaring of y's coefficient, then
   one rounding, half even (a negative n: 1 over the exact power, one rounding): correctly rounded by
   construction, and in microseconds where the high-precision route takes ~12 ms on the S3. ABN_RANGE
   when the arena or a bigint would not hold it (the caller takes the high-precision route). */
static abn_status pow_xnum(abn *out, const abn *y, uint32_t n, bool recip)
{
    int m = ar_mark();
    xnum *r, *b, *t, *u;
    abn_status st = ABN_RANGE;
    if (!ar_xn(4, &r, &b, &t, &u) || !xn_from_abn(b, y)) goto done;
    xn_set_int(r, 1);
    xn_set_int(u, 1);
    while (n) {
        if (n & 1) {
            if (!xn_mul(t, r, b)) goto done;
            xnum *s = r;
            r = t;
            t = s;
        }
        n >>= 1;
        if (n) {
            if (!xn_mul(t, b, b)) goto done;
            xnum *s = b;
            b = t;
            t = s;
        }
    }
    st = recip ? xn_div_to_abn(out, u, r) : xn_div_to_abn(out, r, u);
done:
    ar_release(m);
    return st;
}

static abn_status pow_real(abn *out, const abn *y, const abn *e)
{
    int32_t n;
    abn r;
    bool si = small_int(e, &n);
    /* Unit 054 (abacus #5546): y with d significant digits is c x 10^k, c of d digits and not a multiple of 10, so
       y^|n| has c^|n| >= 10^(|n|(d-1)) for its coefficient, never a multiple of 10: at least |n|(d-1)+1 significant
       digits. Past 34 the exact path must fail, so it is not tried (about 143 us on the S3 at 34 digits). */
    if (si && n >= -POW_EXACT_MAX && n <= POW_EXACT_MAX &&
        (n < 0 ? -n : n) * (abn_sig_digits(y) - 1) + 1 <= ABN_DIGITS &&
        pow_exact(&r, y, (uint32_t)(n < 0 ? -n : n)) == ABN_OK) {
        if (n < 0) {
            abn u = one();
            return abn_div(out, &u, &r);        /* one rounding of an exact value */
        }
        *out = r;
        return ABN_OK;
    }
    if (si && n == 2)  /* one multiplication: the exact square rounded once
                                                            (038; x^-1 is the exact path's one division above) */
        return abn_mul(out, y, y);
    if (si && n >= -POW_XNUM_MAX && n <= POW_XNUM_MAX && n != 0 && n != 1) {
        abn_status st = pow_xnum(out, y, (uint32_t)(n < 0 ? -n : n), n < 0);
        if (st != ABN_RANGE) return st;         /* (it did not fit: the high-precision route) */
    }
    return pow_hp(out, y, e);
}

/* ---- powers and logarithms (UG p.4-2, p.4-3) ---- */

abn_status fn_sqrt(abn *out, const abn *x)
{
    return abn_sqrt(out, x);    /* correctly rounded, so exact for perfect squares */
}

abn_status fn_sq(abn *out, const abn *x)
{
    return abn_mul(out, x, x);
}

abn_status fn_inv(abn *out, const abn *x)
{
    abn u = one();
    return abn_div(out, &u, x);
}

abn_status fn_exp(abn *out, const abn *x)
{
    if (abn_is_zero(x)) {   /* exact cases exact (ours, unit 003) */
        *out = one();
        return ABN_OK;
    }
    return sure(out, abn_exp(out, x), x, f_exp, 0);
}

abn_status fn_ln(abn *out, const abn *x)
{
    abn u = one();
    if (abn_cmp(x, &u) == 0) {
        *out = abn_zero();
        return ABN_OK;
    }
    return sure(out, abn_ln(out, x), x, f_ln, 0);
}

abn_status fn_tenx(abn *out, const abn *x)
{
    int32_t n;
    if (small_int(x, &n)) return abn_pow10(out, n);    /* 10^n exactly */
    if (tiny(x)) {                                      /* 1 + x ln 10: 1 */
        *out = one();
        return ABN_OK;
    }
    return sure(out, abn_exp10(out, x), x, f_tenx, 0);
}

abn_status fn_log(abn *out, const abn *x)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead;
    if (abn_is_zero(x) || abn_is_negative(x)) return ABN_DOMAIN;
    abn_digits(x, &neg, d, &lead);
    if (strcmp(d, "1") == 0) {  /* LOG of 10^n is n exactly */
        *out = abn_from_int(lead);
        return ABN_OK;
    }
    return sure(out, abn_log10(out, x), x, f_ln, 1);
}

/* ---- e^x - 1, ln(1 + x), 2^x, log2 x (ours, unit 021) ---- */


/* e^x - 1, its digits kept where x is tiny (core/hp.c). */
abn_status fn_expm1(abn *out, const abn *x)
{
    static hp hx, r;
    if (abn_is_zero(x)) {
        *out = abn_zero();
        return ABN_OK;
    }
    if (!hp_from_abn(&hx, x)) return ABN_RANGE;
    if (hp_lead(&hx) >= 7) {    /* |x| >= 10^7: e^x overflows, or is far below a unit of 1's 34th digit */
        if (!abn_is_negative(x)) return ABN_OVERFLOW;
        *out = abn_from_int(-1);
        return ABN_OK;
    }
    if (!hp_expm1(&r, &hx, FN_HP)) return ABN_RANGE;
    return hp_round(out, &r);
}

/* ln(1 + x), x > -1; formed from x itself, so a tiny x keeps its digits. */
abn_status fn_ln1p(abn *out, const abn *x)
{
    static hp t, s, one, r;
    abn m1 = abn_from_int(-1);
    if (abn_cmp(x, &m1) <= 0) return ABN_DOMAIN;
    if (abn_is_zero(x) || tiny(x)) {
        *out = *x;                      /* ln(1 + x) = x - x^2/2: x itself below 1E-40 (see tiny) */
        return ABN_OK;
    }
    hp_set_int(&one, 1);
    if (!hp_from_abn(&t, x) || !hp_add(&s, &one, &t, false, HP_PMAX) || !hp_ln(&r, &s, &t, FN_HP))
        return ABN_RANGE;
    return hp_round(out, &r);
}

/* 2^x: exact on the integer powers that fit; otherwise e^(x ln 2) in hp, rounded once (the engine's
 * pow underflowed inside for a tiny x, unit 021's review). */
abn_status fn_twox(abn *out, const abn *x)
{
    static hp hx, one, two, l2, t, r;
    abn two_ = abn_from_int(2);
    if (abn_is_integer(x)) return fn_pow(out, &two_, x);
    hp_set_int(&one, 1);
    hp_set_int(&two, 2);
    if (!hp_from_abn(&hx, x) || !hp_ln(&l2, &two, &one, FN_HP + 5) || !hp_mul(&t, &hx, &l2, FN_HP + 5) ||
        !hp_exp(&r, &t, FN_HP))
        return abn_is_negative(x) ? ABN_UNDERFLOW : ABN_OVERFLOW;  /* |x ln 2| >= 10^7: beyond the range */
    return hp_round(out, &r);
}

/* log2 x, x > 0 (as LOG). A power of two comes out as its power exactly: ln x / ln 2 is within
 * 10^-48 of the integer, which rounds to it. */
abn_status fn_log2(abn *out, const abn *x)
{
    static hp hx, t, one, two, l, l2, r;
    if (abn_is_zero(x) || abn_is_negative(x)) return ABN_DOMAIN;
    hp_set_int(&one, 1);
    hp_set_int(&two, 2);
    if (!hp_from_abn(&hx, x) || !hp_add(&t, &hx, &one, true, HP_PMAX) || !hp_ln(&l, &hx, &t, FN_HP + 5) ||
        !hp_ln(&l2, &two, &one, FN_HP + 5) || !hp_div(&r, &l, &l2, FN_HP))
        return ABN_RANGE;
    return hp_round(out, &r);
}

/* y^x (UG p.4-2). A negative base needs an integer power, zero to a negative power is
 * division by zero (ours, unit 003); integer powers that fit 34 digits are exact. */
abn_status fn_pow(abn *out, const abn *y, const abn *x)
{
    bool xint = abn_is_integer(x);
    bool yneg = abn_is_negative(y);
    if (abn_is_zero(y)) {
        /* 0 to the 0th power or to a negative power: INVALID yˣ (UG p.F-3; ruled by abacus #3778,
           which replaces unit 003's 0^0 = 1) */
        if (abn_is_negative(x)) return ABN_DIV_ZERO;
        if (abn_is_zero(x)) return ABN_DOMAIN;
        *out = abn_zero();
        return ABN_OK;
    }
    if (yneg && !xint) return ABN_DOMAIN;
    abn ay = abn_abs(y), r;
    abn_status st = pow_real(&r, &ay, x);
    if (st != ABN_OK) return st;
    *out = (yneg && is_odd_integer(x)) ? abn_neg(&r) : r;
    return ABN_OK;
}

/* The x-th root of y (UG p.4-3): for y < 0, x must be an odd integer (the guide says an
 * integer; an even root of a negative is complex, an error here, ours, unit 003).
 *
 * The root is e^(ln y / x) formed in hp, the division exact to far beyond 34 digits, and rounded once
 * (unit 028; it was y^h with h the rounded 1/x and a correction for h's error). Perfect powers come
 * out exact. */
abn_status fn_xroot(abn *out, const abn *y, const abn *x)
{
    if (abn_is_zero(x)) return ABN_DOMAIN;
    bool yneg = abn_is_negative(y);
    if (yneg && !is_odd_integer(x)) return ABN_DOMAIN;
    if (abn_is_zero(y)) {
        if (abn_is_negative(x)) return ABN_DIV_ZERO;
        *out = abn_zero();
        return ABN_OK;
    }
    abn ay = abn_abs(y), u = one(), h, r;
    abn_status st;
    if ((st = abn_div(&h, &u, x)) == ABN_UNDERFLOW) {
        /* |1/x| below the range: y^(1/x) is 1 to far beyond the 34th digit (unit 026) */
        *out = yneg ? abn_neg(&u) : u;
        return ABN_OK;
    }
    if (st != ABN_OK) return st;
    if (tiny(&h)) {
        /* |1/x| < 1E-40: y^(1/x) = 1 + ln(y)/x rounds to 1 (|ln y| < 14200), and the engine's pow
           underflows inside (unit 026) */
        *out = yneg ? abn_neg(&u) : u;
        return ABN_OK;
    }
    if ((st = root_hp(&r, &ay, x)) != ABN_OK) return st;      /* e^(ln y / x), rounded once (unit 028) */
    /* Exact roots exact: if a short neighbour of r, raised to x, gives y back exactly. */
    int32_t n;
    if (small_int(x, &n) && n >= -POW_EXACT_MAX && n <= POW_EXACT_MAX) {
        abn c = round_sig(&r, 30), p;
        if (pow_exact(&p, &c, (uint32_t)(n < 0 ? -n : n)) == ABN_OK) {
            abn chk;
            bool exact = n > 0 ? abn_cmp(&p, &ay) == 0
                               : (abn_mul_exact(&chk, &p, &ay) == ABN_OK && abn_cmp(&chk, &u) == 0);
            if (exact) r = c;
        }
    }
    *out = yneg ? abn_neg(&r) : r;
    return ABN_OK;
}

/* ---- quotient, remainder, percent (UG p.4-2, p.4-6, p.4-7) ---- */

/* INT÷ and Rmdr are floored (unit 017 rule 5: Michael's 35s, 33s and 50g give -17 INT÷ 5 = -4,
 * -17 Rmdr 5 = 3, 17 Rmdr -5 = -3; the guide shows only 58 ÷ 9, p.4-2): the quotient is
 * floor(y / x) and the remainder y - x floor(y / x) takes x's sign. The floor is of the exact
 * quotient: the truncated remainder is exact, and when it is not 0 and its sign is not x's, the
 * quotient is one less and the remainder is x more. */
static bool floor_adjust(const abn *r, const abn *x)
{
    return !abn_is_zero(r) && abn_is_negative(r) != abn_is_negative(x);
}

/* The truncated remainder of y by x; one below the range (|y| tiny beside x, or x tiny) is kept as
 * its sign alone, a nonzero smaller than any number (unit 026: it is not the result, only its floor
 * correction). False when below the range. */
static abn_status trunc_rem(abn *r, bool *tiny_rem, const abn *y, const abn *x)
{
    abn_status st = abn_fmod(r, y, x);
    *tiny_rem = st == ABN_UNDERFLOW;
    if (*tiny_rem) {
        abn m;
        abn_from_text(&m, "1E-6143");
        *r = abn_is_negative(y) ? abn_neg(&m) : m;     /* stands for its sign */
        return ABN_OK;
    }
    return st;
}

abn_status fn_idiv(abn *out, const abn *y, const abn *x)
{
    abn q, r, one = abn_from_int(1);
    abn_status st;
    bool tr;
    if ((st = abn_idiv(&q, y, x)) == ABN_UNDERFLOW) q = abn_zero();    /* |y / x| < 1: 0 (unit 026) */
    else if (st != ABN_OK) return st;
    if ((st = trunc_rem(&r, &tr, y, x)) != ABN_OK) return st;
    if (floor_adjust(&r, x)) return abn_sub(out, &q, &one);
    *out = q;
    return ABN_OK;
}

abn_status fn_rmdr(abn *out, const abn *y, const abn *x)
{
    abn r;
    abn_status st;
    bool tr;
    if ((st = trunc_rem(&r, &tr, y, x)) != ABN_OK) return st;
    if (tr) {
        /* the truncated remainder below the range (unit 026). Beside an x not that small, x plus it
           is x; else both scaled by 10^200 (exact), the remainder taken there and scaled back */
        abn ax = abn_abs(x), small, k, ys, xs, rs, ik;
        abn_from_text(&small, "1E-6000");
        if (abn_cmp(&ax, &small) >= 0) {
            if (!floor_adjust(&r, x)) return ABN_UNDERFLOW;
            *out = *x;
            return ABN_OK;
        }
        abn_from_text(&k, "1E200");
        abn_from_text(&ik, "1E-200");
        if ((st = abn_mul(&ys, y, &k)) != ABN_OK || (st = abn_mul(&xs, x, &k)) != ABN_OK)
            return st == ABN_OVERFLOW ? ABN_RANGE : st;    /* y huge beside a tiny x: an error */
        if ((st = fn_rmdr(&rs, &ys, &xs)) != ABN_OK) return st;
        return abn_mul(out, &rs, &ik);
    }
    if (floor_adjust(&r, x)) return abn_add(out, &r, x);
    *out = r;
    return ABN_OK;
}

abn_status fn_pct(abn *out, const abn *y, const abn *x)
{
    abn t, hundred = abn_from_int(100);
    abn_status st = abn_mul(&t, y, x);
    if (st == ABN_OVERFLOW) {
        /* y x beyond the range, y x / 100 perhaps not: y / 100 first, exact (unit 026) */
        if ((st = abn_div(&t, y, &hundred)) != ABN_OK) return st;
        return abn_mul(out, &t, x);
    }
    if (st != ABN_OK) return st;
    return abn_div(out, &t, &hundred);  /* dividing by 100 adds no rounding */
}

abn_status fn_pctchg(abn *out, const abn *y, const abn *x)
{
    abn d, hundred = abn_from_int(100), ys = *y, xs = *x;
    abn_status st;
    if (abn_is_zero(y)) return ABN_DIV_ZERO;
    if ((st = abn_sub(&d, x, y)) == ABN_UNDERFLOW) {
        /* x - y below the range: both are tiny; scaled by 10^200 (exact) the change is the same */
        abn k;
        abn_from_text(&k, "1E200");
        if ((st = abn_mul(&xs, x, &k)) != ABN_OK || (st = abn_mul(&ys, y, &k)) != ABN_OK ||
            (st = abn_sub(&d, &xs, &ys)) != ABN_OK)
            return st;
    } else if (st == ABN_OVERFLOW) {
        /* x - y beyond the range: x and y huge, of opposite signs; (x / y - 1) 100 (unit 026) */
        abn q, u = one();
        if ((st = abn_div(&q, x, y)) != ABN_OK || (st = abn_sub(&q, &q, &u)) != ABN_OK) return st;
        return abn_mul(out, &q, &hundred);
    } else if (st != ABN_OK) {
        return st;
    }
    if ((st = abn_mul(&d, &d, &hundred)) == ABN_OVERFLOW) {
        /* (x - y) 100 beyond the range: divide first; times 100 adds no rounding (unit 026) */
        if ((st = abn_div(&d, &d, &ys)) != ABN_OK) return st;
        return abn_mul(out, &d, &hundred);
    }
    if (st != ABN_OK) return st;
    return abn_div(out, &d, &ys);
}

/* ---- trigonometry (UG p.4-3 to p.4-5; exact reduction ours, unit 003 amendment 1) ---- */

typedef struct {
    const char *full, *half, *quarter, *eighth;
    const char *to_rad, *from_rad;  /* pi/180 and 180/pi (or the grad pair), 34 digits */
    const char *to_rad_lo;          /* (pi/180) - to_rad, the constant's own rounding error */
    const char *sixth_of_half;      /* 30 degrees, where sin is exactly 1/2; NULL in grads */
} angle_unit;

/* The reduced angle b in radians as hi + lo: hi = b * to_rad rounded once, lo = what that
 * rounding and the constant's own error left out (an fma gives the product's residual exactly).
 * Without lo, the half unit lost in hi can grow to about 5 units of a sine near 0.1. */
static abn_status to_rad_pair(abn *hi, abn *lo, const abn *b, const char *to_rad, const char *to_rad_lo)
{
    abn k = K(to_rad), klo = K(to_rad_lo), nhi, res;
    abn_status st;
    if ((st = abn_mul(hi, b, &k)) != ABN_OK) return st;
    nhi = abn_neg(hi);
    if (abn_fma(&res, b, &k, &nhi) != ABN_OK) res = abn_zero();     /* underflow: negligible */
    if (abn_fma(lo, b, &klo, &res) != ABN_OK) *lo = res;
    return ABN_OK;
}

static const angle_unit DEG_UNIT = {"360", "180", "90", "45", "0.01745329251994329576923690768488613",
                                    "57.29577951308232087679815481410517",
                                    "-2.86557128111458274543902808559829E-36", "30"};
static const angle_unit GRAD_UNIT = {"400", "200", "100", "50", "0.01570796326794896619231321691639751",
                                     "63.66197723675813430755350534900574",
                                     "4.420985846996875529104874722961539E-36", NULL};

/* An angle reduced to [0, eighth turn]: the wanted value is sign * (cofn ? the cofunction :
 * the function) of a. Every step is exact: fmod, and subtractions whose result needs no more
 * digits than the operand had. */
typedef struct {
    abn a;
    int sin_sign, cos_sign;
    bool swap;      /* a was taken as quarter - a: sine and cosine trade places */
} reduced;

static abn_status reduce(reduced *rd, const abn *x, const angle_unit *u)
{
    abn a = abn_abs(x), full = K(u->full), half = K(u->half), quarter = K(u->quarter), t;
    abn_status st;
    rd->sin_sign = abn_is_negative(x) ? -1 : 1;     /* sine is odd, cosine even */
    rd->cos_sign = 1;
    if ((st = abn_fmod(&a, &a, &full)) != ABN_OK) return st;
    if (abn_cmp(&a, &half) >= 0) {                  /* a + half turn: both change sign */
        if ((st = abn_sub(&a, &a, &half)) != ABN_OK) return st;
        rd->sin_sign = -rd->sin_sign;
        rd->cos_sign = -rd->cos_sign;
    }
    if (abn_cmp(&a, &quarter) > 0) {                /* half turn - a: cosine changes sign */
        if ((st = abn_sub(&a, &half, &a)) != ABN_OK) return st;
        rd->cos_sign = -rd->cos_sign;
    }
    t = K(u->eighth);
    rd->swap = abn_cmp(&a, &t) > 0;
    if (rd->swap && (st = abn_sub(&a, &quarter, &a)) != ABN_OK) return st;
    rd->a = a;
    return ABN_OK;
}

/* ---- sine and cosine of an exact decimal angle, to about 60 digits (unit 004) ----
 *
 * For a reduced angle b in degrees or grads (0 < b <= eighth turn) the angle in radians is formed
 * in fixed point, theta * 10^S = b * pi * 10^S / half, with pi to 71 decimals, and the Taylor
 * series is summed exactly in integers (Knuth, TAOCP vol. 2, 4.3.1 for the arithmetic). The value
 * is good to about 1E-58 relative, so rounding it once gives the correctly rounded sine or cosine,
 * and multiplying it by r exactly before rounding gives r sin and r cos correctly rounded (the
 * rectangular conversion, whose result can carry a unit ten times finer than the cosine's). */

/* pi * 10^71, truncated (mpmath at 120 digits; Machin's formula agrees to 74 digits). */
static const char PI_71[] = "314159265358979323846264338327950288419716939937510582097494459230781640";

#define TRIGX_DIGITS 60     /* fixed-point scale above the angle's own magnitude */

/* Returns the scale s and fills v with sin (or cos) of b * 10^... as v * 10^-s, or false when b is
 * below 1E-17 of a unit (the caller handles those directly). */
static bool trigx(sbig *v, int *scale, const abn *b, const angle_unit *u, bool want_cos)
{
    static sbig th, th2, term, pos, neg, tmp;    /* at most about 200 digits: short numbers */
    bool bneg;
    char d[ABN_DIGITS + 1];
    int lead, n, s;
    abn_digits(b, &bneg, d, &lead);
    n = (int)strlen(d);
    if (n == 0 || lead < -17) return false;
    s = TRIGX_DIGITS + (lead < 0 ? -lead : 0);
    /* theta * 10^s = digits * 10^(lead - n + 1) * PI_71 * 10^(s - 71) / half */
    if (!sb_from_digits(&th, d, 0) || !sb_from_digits(&tmp, PI_71, 0) || !sb_mul(&term, &th, &tmp))
        return false;
    int shift = (lead - n + 1) + s - 71;
    if (shift >= 0) {
        if (!sb_mul_pow10(&term, shift)) return false;
    } else {
        sb_div_pow10(&term, -shift);
    }
    {
        uint32_t half = (uint32_t)(u->half[0] - '0') * 100 + (uint32_t)(u->half[1] - '0') * 10 +
                        (uint32_t)(u->half[2] - '0');   /* "180" or "200" */
        sb_divmod_u32(&term, half);
    }
    th = term;
    /* theta^2 at the same scale */
    if (!sb_mul(&th2, &th, &th)) return false;
    sb_div_pow10(&th2, s);
    /* sum the alternating series: sin = theta - theta^3/3! + ..., cos = 1 - theta^2/2! + ... */
    if (want_cos) {
        sb_set(&term, 1);
        if (!sb_mul_pow10(&term, s)) return false;
    } else {
        term = th;
    }
    pos = term;
    neg.n = 0;
    for (uint32_t k = want_cos ? 1 : 2, i = 1; !sb_is_zero(&term); k += 2, i++) {
        if (!sb_mul(&tmp, &term, &th2)) return false;
        sb_div_pow10(&tmp, s);
        sb_divmod_u32(&tmp, k);
        sb_divmod_u32(&tmp, k + 1);
        term = tmp;
        if (i % 2) {
            if (!sb_add(&neg, &neg, &term)) return false;
        } else {
            if (!sb_add(&pos, &pos, &term)) return false;
        }
    }
    sb_sub(v, &pos, &neg);      /* pos > neg: the sum is positive on [0, pi/4] */
    *scale = s;
    return true;
}

/* Working values for the polar conversions and their trigonometry (unit 034): fn_topol, fn_torec, rect_part and
   trig_hp use them in turn; none calls another while holding them. Shared to keep the internal RAM they take as it
   was before (the stand-in's boot needs it). */
static hp fw[6];

/* sin or cos of an angle a (in degrees or grads, a >= 0), times r (NULL: 1), correctly rounded: the angle in
   radians a pi / half, the function and the product in hp, each value formed to 60 digits, then 100, then 180
   until its rounding is sure (unit 034, Ziv). */
static abn_status trig_hp(abn *out, const abn *a, const angle_unit *u, bool want_cos, const abn *r)
{
    static const int P[] = {60, 100, 180};
    hp *ha = &fw[0], *t1 = &fw[1], *t2 = &fw[2], *hs = &fw[3], *hc = &fw[4], *hr = &fw[5];
    abn_status st = ABN_RANGE;
    abn v = abn_zero();
    int half = (u->half[0] - '0') * 100 + (u->half[1] - '0') * 10 + (u->half[2] - '0');
    for (int i = 0; i < 3; i++) {
        int p = P[i];
        if (!hp_from_abn(ha, a) || !hp_pi(t1, p + 6) || !hp_mul(t2, ha, t1, p + 6)) return ABN_RANGE;
        hp_set_int(t1, half);
        if (!hp_div(ha, t2, t1, p + 6) || !hp_sincos(hs, hc, ha, p)) return i ? (*out = v, st) : ABN_RANGE;
        if (r) {
            if (!hp_from_abn(hr, r) || !hp_mul(t1, want_cos ? hc : hs, hr, p)) return ABN_RANGE;
        } else {
            *t1 = want_cos ? *hc : *hs;
        }
        if (hp_round_sure(&v, &st, t1, p, t2)) break;
    }
    *out = v;
    return st;
}

/* ---- unit 044: the one-input functions correctly rounded ----
   Each formed in hp to 60 digits, then 100, then 180, until its rounding is sure (034's route, Ziv), and rounded once.
   The library's own result is kept for its status (a domain error, a range end, the exact cases and their shortcuts);
   only an ordinary value is replaced by the sure one. The driver's working values are fw[0..2] (the polar
   conversions and trig_hp hold fw only while calling none of these); the formulas use u44[0..3]. */
static hp u44[4];

#ifndef ZIV_FIRST
#define ZIV_FIRST 60        /* make accuracy builds one at 37: every rounding then needs the loop (fn_drive_p37) */
#endif

unsigned long fn_ziv_calls;

static abn_status ziv1(abn *out, const abn *x, hp_f1 f, int arg, bool *is_sure)
{
    static const int P[] = {ZIV_FIRST, 100, 180};
    fn_ziv_calls++;
    hp *hx = &fw[0], *r = &fw[1], *sc = &fw[2];
    abn v = abn_zero();
    abn_status st = ABN_RANGE;
    *is_sure = false;
    for (int i = 0; i < 3; i++) {
        int p = P[i];
        if (!hp_from_abn(hx, x) || !f(r, hx, p, arg)) {
            if (!i) return ABN_RANGE;
            break;                              /* the last pass's value: within 10^6 units of its p-th digit */
        }
        if (hp_round_sure(&v, &st, r, p, sc)) {
            *is_sure = true;
            break;
        }
    }
    *out = v;
    return st;
}

static bool fast;          /* fn_set_fast */

bool fn_set_fast(bool on)
{
    bool was = fast;
    fast = on;
    return was;
}

/* The engine's value and status st0 replaced by the correctly rounded value; at the range's edges a sure rounding's
   status too, either way (a value the engine kept that rounds past the range, or one it overflowed or underflowed
   that rounds within it: review). The engine's other errors stand. */
static abn_status sure(abn *out, abn_status st0, const abn *x, hp_f1 f, int arg)
{
    abn v;
    bool is_sure;
    bool edge = st0 == ABN_OVERFLOW || st0 == ABN_UNDERFLOW;
    if (fast || (st0 != ABN_OK && !edge) || (st0 == ABN_OK && abn_is_zero(out))) return st0;   /* fast: as before 044 */
    abn_status st = ziv1(&v, x, f, arg, &is_sure);
    if (st == ABN_OK && !abn_is_zero(&v) && (is_sure || !edge)) {
        *out = v;                               /* (unsure only past a pass's reach: the last pass's, within a unit) */
        return ABN_OK;
    }
    if (is_sure && (st == ABN_OVERFLOW || st == ABN_UNDERFLOW)) {
        *out = v;
        return st;
    }
    return st0;                                 /* (hp past its reach: the library's) */
}

static bool one_minus(hp *r, const hp *x, bool plus)       /* 1 - x or 1 + x, exactly */
{
    hp one;
    hp_set_int(&one, 1);
    return hp_add(r, &one, x, !plus, HP_PMAX);
}

static bool f_exp(hp *r, const hp *x, int p, int arg) { (void)arg; return hp_exp(r, x, p); }

static bool f_ln(hp *r, const hp *x, int p, int arg)        /* arg 1: log10 */
{
    hp *t = &u44[0], *l = &u44[1], *k = &u44[2];
    hp_set_int(k, 1);
    if (!hp_add(t, x, k, true, HP_PMAX) || !hp_ln(arg ? l : r, x, t, arg ? p + 5 : p)) return false;
    return !arg || (hp_ln10(k, p + 5) && hp_div(r, l, k, p));
}

static bool f_tenx(hp *r, const hp *x, int p, int arg)
{
    (void)arg;
    hp *k = &u44[0], *t = &u44[1];
    long l = hp_lead(x);
    return hp_ln10(k, p + 8 + (l > 0 ? (int)l : 0)) && hp_mul(t, x, k, p + 8 + (l > 0 ? (int)l : 0)) && hp_exp(r, t, p);
}

/* Rule 2's reach: hp_sincos at p + 5 needs p + L + 15 of pi's 241 digits, L the exponent of x's leading digit, so
   the 60-digit pass reaches L 163, the 100-digit L 123, the 180-digit L 43 (fewer when x lies near a multiple of
   pi/2: its second reduction takes more). Below 1E44 every result is correctly rounded. Below 1E164 every result
   whose first pass is sure is (all but values within about 1E-20 units of a rounding boundary); one that needs a pass
   past its reach keeps the last pass's rounding, within one unit. From 1E164 on, the engine's, within 8 units
   (measured, 100 arguments a decade by SIN, COS and TAN: all correctly rounded at 1E161..1E163; about 40% not at
   1E164..1E167, none worse). */
static bool f_trig(hp *r, const hp *x, int p, int arg)      /* radians; arg 0 sin, 1 cos, 2 tan */
{
    hp *s = &u44[0], *c = &u44[1];
    if (!hp_sincos(s, c, x, p + 5)) return false;           /* (false past hp's pi: rule 2's reach) */
    if (arg == 2) return hp_div(r, s, c, p);
    *r = arg ? *c : *s;
    hp_trim(r, p);
    return true;
}

/* the radian angle a in the unit (0 rad, 1 deg, 2 grad): a half / pi */
static bool in_unit(hp *r, const hp *a, int p, int unit)
{
    hp *k = &u44[2], *t = &u44[3];
    if (!unit) {
        *r = *a;
        hp_trim(r, p);
        return true;
    }
    hp_set_int(k, unit == 1 ? 180 : 200);
    if (!hp_mul(t, a, k, p + 5)) return false;
    return hp_pi(k, p + 8) && hp_div(r, t, k, p);
}

/* Unit 045: the engine's angle, in radians, that fn_asin, fn_acos and fn_atan formed before the correctly rounded one:
   it starts f_inv's Newton step in place of a second engine atan2 (about 1 ms on the S3). */
static abn inv_start;
static bool inv_start_on;

static void inv_hint(const abn *v, abn_status st, fn_angle m)
{
    abn k;
    inv_start_on = false;
    if (st != ABN_OK) return;
    if (m == FN_RAD) inv_start = *v;
    else if (k = K(m == FN_GRAD ? GRAD_UNIT.to_rad : DEG_UNIT.to_rad), abn_mul(&inv_start, v, &k) != ABN_OK) return;
    /* an angle below 10^-30 keeps the engine's atan2 start, as before: there the Newton step's correction falls below
       the last kept digit and the result is the start itself, so the start's decimal128 cohort is the result's (the
       hint's, the engine's full-length asin or atan, differed from atan2's: same value, other bytes; 045's
       differential, tiny ATAN) */
    static abn tiny_angle;
    static bool tiny_set;
    if (!tiny_set) abn_from_text(&tiny_angle, "1E-30"), tiny_set = true;     /* (parsed once: a software conversion) */
    k = abn_abs(&inv_start);
    if (abn_cmp(&k, &tiny_angle) < 0) return;
    inv_start_on = true;
}

static abn_status inv_done(abn_status st)
{
    inv_start_on = false;
    return st;
}

static bool f_inv(hp *r, const hp *x, int p, int arg)       /* arg: 0 asin, 1 acos, 2 atan; plus 3 x the unit */
{
    hp *a = &u44[0], *b = &u44[1], *t = &u44[2];
    int fn = arg % 3, unit = arg / 3;
    const abn *s = inv_start_on ? &inv_start : NULL;
    if (fn == 2) {
        hp_set_int(t, 1);
        if (!hp_atan2s(a, x, t, s, p + 5)) return false;
    } else {
        /* sqrt((1 - x)(1 + x)), the product of two exact values: no cancellation near |x| = 1 */
        if (!one_minus(a, x, false) || !one_minus(b, x, true) || !hp_mul(t, a, b, p + 10) || !hp_sqrt(b, t, p + 8))
            return false;
        if (!(fn == 0 ? hp_atan2s(a, x, b, s, p + 5) : hp_atan2s(a, b, x, s, p + 5))) return false;
    }
    return in_unit(r, a, p, unit);
}

static bool f_hyp(hp *r, const hp *x, int p, int arg)       /* arg 0 sinh, 1 cosh, 2 tanh */
{
    hp *s = &u44[0], *c = &u44[1];
    if (!hp_sinhcosh(s, c, x, p + 5)) return false;
    if (arg == 2) return hp_div(r, s, c, p);
    *r = arg ? *c : *s;
    hp_trim(r, p);
    return true;
}

/* ln(2 |x|) for a large |x| (asinh, acosh: the rest lies below 1/(4 x^2)) */
static bool ln_two_x(hp *r, const hp *ax, int p)
{
    hp *t = &u44[0], *u = &u44[1], *k = &u44[2];
    hp_set_int(k, 2);
    if (!hp_mul(u, ax, k, HP_PMAX)) return false;
    hp_set_int(k, 1);
    return hp_add(t, u, k, true, HP_PMAX) && hp_ln(r, u, t, p);
}

static bool f_ahyp(hp *r, const hp *x, int p, int arg)      /* arg 0 asinh, 1 acosh, 2 atanh */
{
    hp *a = &u44[0], *s = &u44[1], *t = &u44[2], *q = &u44[3], one;
    hp ax = *x;
    int w = p + 10;
    hp_set_int(&one, 1);
    ax.neg = false;
    if (arg == 2) {                             /* atanh x = ln((1 + x) / (1 - x)) / 2, s - 1 = 2x / (1 - x) */
        if (!one_minus(a, &ax, false) || !one_minus(t, &ax, true) || !hp_div(s, t, a, w)) return false;
        hp_set_int(q, 2);
        if (!hp_mul(t, &ax, q, w) || !hp_div(q, t, a, w) || !hp_ln(a, s, q, w)) return false;
        hp_set_int(q, 2);
        if (!hp_div(r, a, q, p)) return false;
    } else if (hp_lead(&ax) > (p + 5) / 2) {    /* past this, 1/(4 x^2) is below the p-th digit */
        if (!ln_two_x(r, &ax, p)) return false;
    } else if (arg == 0) {                      /* asinh: s = |x| + sqrt(x^2 + 1), s - 1 = |x| + x^2/(sqrt(x^2 + 1) + 1) */
        if (!hp_mul(q, &ax, &ax, w) || !hp_add(t, q, &one, false, w) || !hp_sqrt(a, t, w)) return false;
        if (!hp_add(s, &ax, a, false, w) || !hp_add(t, a, &one, false, w) || !hp_div(a, q, t, w) ||
            !hp_add(t, &ax, a, false, w))
            return false;
        if (!hp_ln(r, s, t, p)) return false;
    } else {                                    /* acosh, x >= 1: d = x - 1, s - 1 = d + sqrt(d (x + 1)) */
        if (!hp_add(a, &ax, &one, true, HP_PMAX) || !hp_add(t, &ax, &one, false, HP_PMAX) || !hp_mul(q, a, t, w) ||
            !hp_sqrt(t, q, w))
            return false;
        if (!hp_add(s, &ax, t, false, w) || !hp_add(q, a, t, false, w) || !hp_ln(r, s, q, p)) return false;
    }
    if (x->neg && arg != 1) hp_neg(r);
    return true;
}

static bool f_angle(hp *r, const hp *x, int p, int arg)     /* arg 0: x pi / 180 (->RAD), 1: x 180 / pi (->DEG) */
{
    hp *k = &u44[0], *t = &u44[1];
    long l = hp_lead(x);
    int w = p + 8 + (l > 0 ? (int)l : 0);
    if (!hp_pi(k, w)) return false;
    if (arg == 0) {
        if (!hp_mul(t, x, k, w)) return false;
        hp_set_int(k, 180);
        return hp_div(r, t, k, p);
    }
    hp_set_int(t, 180);
    return hp_mul(r, x, t, w) && (*t = *r, hp_div(r, t, k, p));
}

/* Sin (kind 0), tan (2) or cot (3) of a pi / den, a in degrees (den 180) or grads (200), for arg = kind 1000 + den. */
static bool f_utrig(hp *r, const hp *x, int p, int arg)
{
    hp *s = &u44[0], *c = &u44[1], *t = &u44[2], *k = &u44[3];
    int kind = arg / 1000, w = p + 8;
    if (!hp_pi(k, w) || !hp_mul(s, x, k, w)) return false;
    hp_set_int(k, arg % 1000);
    if (!hp_div(t, s, k, w) || !hp_sincos(s, c, t, p + 5)) return false;
    if (kind == 0) {
        *r = *s;
        hp_trim(r, p);
        return true;
    }
    return kind == 2 ? hp_div(r, s, c, p) : hp_div(r, c, s, p);
}

/* A reduced angle below 1E-17 of a unit (trigx's tiny path): sin, tan or cot (kind 0, 2, 3) of a in u, correctly
   rounded, with a range status when the rounding is past the range. 034 formed these from the 34-digit pi/180 (or
   pi/200) and its error term, and near the range's bottom that term had lost its digits: SIN of 5E-6124 grads was
   2.2 units off (unit 044's review fuzz). ABN_RANGE: hp could not, the caller's path stands; so too with fast (044
   rule 5), which keeps 034's path. */
static abn_status tiny_trig(abn *out, const abn *a, const angle_unit *u, int kind)
{
    abn v;
    bool is_sure;
    if (fast) return ABN_RANGE;
    abn_status st = ziv1(&v, a, f_utrig, kind * 1000 + (strcmp(u->half, "180") == 0 ? 180 : 200), &is_sure);
    if (st == ABN_OK ? abn_is_zero(&v) : !(is_sure && (st == ABN_OVERFLOW || st == ABN_UNDERFLOW))) return ABN_RANGE;
    *out = v;
    return st;
}

/* The rounding of m 10^e (good to about 1E-55 relative) is sure: m moved by 10^-52 of itself either way rounds the
   same. d is scratch; m is restored. */
static bool sb_round_sure(sbig *m, bool neg, long e, sbig *d)
{
    abn a1, a2;
    int k = sb_ndigits(m) - 52;
    if (k <= 0) return true;
    sb_set(d, 1);
    if (!sb_mul_pow10(d, k)) return false;
    sb_sub(m, m, d);
    abn_status s1 = sb_to_abn(&a1, m, neg, e, true);
    if (!sb_add(m, m, d) || !sb_add(m, m, d)) return false;
    abn_status s2 = sb_to_abn(&a2, m, neg, e, true);
    sb_sub(m, m, d);
    return s1 == s2 && (s1 != ABN_OK || abn_cmp(&a1, &a2) == 0);
}

/* sin or cos of a reduced angle, correctly rounded, optionally times r (both exact first); when the value is too
   near a rounding's midpoint for its 58 digits, formed again in hp (unit 034: products from two inputs can lie
   far nearer a midpoint than one input's function). */
static abn_status trig_exactish(abn *out, const abn *a, const angle_unit *u, bool want_cos, const abn *r)
{
    static sbig v, rb, prod;
    int scale;
    if (!trigx(&v, &scale, a, u, want_cos)) return ABN_DOMAIN;  /* caller's tiny-angle path */
    if (!r) {
        if (!sb_round_sure(&v, false, -scale, &rb)) return trig_hp(out, a, u, want_cos, NULL);
        return sb_to_abn(out, &v, false, -scale, true);
    }
    bool rneg;
    char d[ABN_DIGITS + 1];
    int lead;
    abn_digits(r, &rneg, d, &lead);
    if (d[0] == '\0') {
        *out = abn_zero();
        return ABN_OK;
    }
    if (!sb_from_digits(&rb, d, 0) || !sb_mul(&prod, &v, &rb)) return ABN_RANGE;
    long e = (long)lead - (long)strlen(d) + 1 - scale;
    if (!sb_round_sure(&prod, rneg, e, &rb)) return trig_hp(out, a, u, want_cos, r);
    return sb_to_abn(out, &prod, rneg, e, true);
}

/* sin or cos of a in [0, eighth turn], exact at 0 and (degrees) at 30. With the angle as
 * hi + lo radians: sin = sin hi + lo cos hi, cos = cos hi - lo sin hi (lo is below a unit of hi,
 * so the next terms are far below the 34th digit). */
static abn_status sin_reduced(abn *out, const abn *a, const angle_unit *u)
{
    abn hi, lo, sv, cv, k;
    abn_status st;
    if (abn_is_zero(a)) {
        *out = abn_zero();
        return ABN_OK;
    }
    if (u->sixth_of_half) {
        k = K(u->sixth_of_half);
        if (abn_cmp(a, &k) == 0) {
            *out = K("0.5");
            return ABN_OK;
        }
    }
    if ((st = trig_exactish(out, a, u, false, NULL)) != ABN_DOMAIN) return st;
    if ((st = tiny_trig(out, a, u, 0)) != ABN_RANGE) return st;    /* below 1E-17 of a unit (044 review) */
    /* below 1E-17 of a unit: sin theta = theta to far below the 34th digit */
    if ((st = to_rad_pair(&hi, &lo, a, u->to_rad, u->to_rad_lo)) != ABN_OK) return st;
    if ((st = abn_sin(&sv, &hi)) != ABN_OK) return st;
    if (abn_is_zero(&lo) || abn_cos(&cv, &hi) != ABN_OK) {
        *out = sv;
        return ABN_OK;
    }
    return abn_fma(out, &cv, &lo, &sv);
}

static abn_status cos_reduced(abn *out, const abn *a, const angle_unit *u)
{
    abn hi, lo, sv, cv, nlo;
    abn_status st;
    if (abn_is_zero(a)) {
        *out = one();
        return ABN_OK;
    }
    if ((st = trig_exactish(out, a, u, true, NULL)) != ABN_DOMAIN) return st;
    if ((st = to_rad_pair(&hi, &lo, a, u->to_rad, u->to_rad_lo)) == ABN_UNDERFLOW) {
        *out = one();                   /* an angle below the range in radians: cos 1 (unit 026) */
        return ABN_OK;
    }
    if (st != ABN_OK) return st;
    if ((st = abn_cos(&cv, &hi)) != ABN_OK) return st;
    if (abn_is_zero(&lo) || abn_sin(&sv, &hi) != ABN_OK) {
        *out = cv;
        return ABN_OK;
    }
    nlo = abn_neg(&lo);
    return abn_fma(out, &sv, &nlo, &cv);
}

static const angle_unit *unit_of(fn_angle m)
{
    return m == FN_GRAD ? &GRAD_UNIT : &DEG_UNIT;
}

static abn signed_(const abn *v, int sign)
{
    return sign < 0 ? abn_neg(v) : *v;
}

abn_status fn_sin(abn *out, const abn *x, fn_angle m)
{
    if (m == FN_RAD) return sure(out, abn_sin(out, x), x, f_trig, 0);  /* no special cases in radians; unit 044 */
    const angle_unit *u = unit_of(m);
    reduced rd;
    abn v;
    abn_status st;
    if ((st = reduce(&rd, x, u)) != ABN_OK) return st;
    st = rd.swap ? cos_reduced(&v, &rd.a, u) : sin_reduced(&v, &rd.a, u);
    if (st != ABN_OK) return st;
    *out = signed_(&v, rd.sin_sign);
    return ABN_OK;
}

abn_status fn_cos(abn *out, const abn *x, fn_angle m)
{
    if (m == FN_RAD) return sure(out, abn_cos(out, x), x, f_trig, 1);
    const angle_unit *u = unit_of(m);
    reduced rd;
    abn v;
    abn_status st;
    if ((st = reduce(&rd, x, u)) != ABN_OK) return st;
    st = rd.swap ? sin_reduced(&v, &rd.a, u) : cos_reduced(&v, &rd.a, u);
    if (st != ABN_OK) return st;
    *out = signed_(&v, rd.cos_sign);
    return ABN_OK;
}

/* tan of b in [0, eighth turn]: the correctly rounded sine over the correctly rounded cosine (on
 * this range neither has a mantissa that can magnify the other's half unit: within about 2 units).
 * Below 1E-17 of a unit, through the radian engine with the angle as hi + lo:
 * tan = tan hi + lo (1 + tan^2 hi). */
static abn_status tan_small(abn *out, const abn *b, const angle_unit *u)
{
    abn hi, lo, t, w = one(), d, sv, cv;
    abn_status st;
    if (abn_is_zero(b)) {
        *out = abn_zero();
        return ABN_OK;
    }
    if ((st = trig_exactish(&sv, b, u, false, NULL)) != ABN_DOMAIN) {
        if (st != ABN_OK) return st;
        if ((st = trig_exactish(&cv, b, u, true, NULL)) != ABN_OK) return st;
        return abn_div(out, &sv, &cv);
    }
    if ((st = to_rad_pair(&hi, &lo, b, u->to_rad, u->to_rad_lo)) != ABN_OK) return st;
    if ((st = abn_tan(&t, &hi)) != ABN_OK) return st;
    if (abn_is_zero(&lo) || abn_fma(&d, &t, &t, &w) != ABN_OK) {
        *out = t;
        return ABN_OK;
    }
    return abn_fma(out, &d, &lo, &t);
}

/* tan (or, with cot, 1/tan) of a reduced angle a in [0, eighth turn]: the 60-digit sine and
 * cosine divided exactly and rounded once, so correctly rounded. (A ratio of two rounded values,
 * or 1/tan, can lose up to ten of the result's units where its mantissa is smaller than theirs:
 * measured 11.8 units near the odd quarter turns before this.) Exact at 0 and at the eighth turn.
 * Below 1E-17 of a unit, tan is the angle in radians and cot its reciprocal, to far below the
 * 34th digit. */
static abn_status tan_reduced(abn *out, const abn *a, const angle_unit *u, bool cot)
{
    static sbig sv, cv, num, q;
    abn eighth = K(u->eighth);
    int s1, s2;
    bool inexact;
    if (abn_is_zero(a)) {
        if (cot) return ABN_DOMAIN;     /* tan of an odd quarter turn (ours, unit 003) */
        *out = abn_zero();
        return ABN_OK;
    }
    if (abn_cmp(a, &eighth) == 0) {     /* tan 45 = 1 exactly */
        *out = one();
        return ABN_OK;
    }
    if (trigx(&sv, &s1, a, u, false) && trigx(&cv, &s2, a, u, true)) {
        const sbig *den = cot ? &sv : &cv;
        num = cot ? cv : sv;
        /* scale the numerator so the quotient carries at least 45 digits, however small it is */
        int extra = 45 + (sb_ndigits(den) > sb_ndigits(&num) ? sb_ndigits(den) - sb_ndigits(&num) : 0);
        if (!sb_mul_pow10(&num, extra) || !sb_div(&q, &num, den, &inexact)) return ABN_RANGE;
        return sb_to_abn(out, &q, false, -extra, true);
    }
    abn_status st = tiny_trig(out, a, u, cot ? 3 : 2);              /* below 1E-17 of a unit (044 review) */
    if (st != ABN_RANGE) return st;
    if (cot) {
        abn f = K(u->from_rad);
        return abn_div(out, &f, a);
    }
    return tan_small(out, a, u);
}

/* Tangent: reduced as sine and cosine are, then tan of the reduced angle, or its reciprocal
 * when sine and cosine traded places, so an odd quarter turn is an error (ours, unit 003:
 * tan 90 degrees is an error). Exact at 0 and at the eighth turn (tan 45 = 1). */
abn_status fn_tan(abn *out, const abn *x, fn_angle m)
{
    if (m == FN_RAD) return sure(out, abn_tan(out, x), x, f_trig, 2);
    const angle_unit *u = unit_of(m);
    reduced rd;
    abn v;
    abn_status st;
    if ((st = reduce(&rd, x, u)) != ABN_OK) return st;
    if ((st = tan_reduced(&v, &rd.a, u, rd.swap)) != ABN_OK) return st;
    *out = signed_(&v, rd.sin_sign * rd.cos_sign);
    return ABN_OK;
}

/* Inverse functions in degrees and grads: the exact angles exact (ASIN 0.5 = 30, ATAN 1 = 45,
 * ACOS -1 = 180 ...; ours, unit 003), otherwise the radian result converted. */
typedef struct {
    const char *arg, *deg, *grad;   /* grad NULL where the angle is not exact in grads */
} exact_angle;

static const exact_angle ASIN_EXACT[] = {
    {"0", "0", "0"}, {"0.5", "30", NULL}, {"1", "90", "100"}, {"-0.5", "-30", NULL}, {"-1", "-90", "-100"}};
static const exact_angle ACOS_EXACT[] = {
    {"1", "0", "0"}, {"0.5", "60", NULL}, {"0", "90", "100"}, {"-0.5", "120", NULL}, {"-1", "180", "200"}};
static const exact_angle ATAN_EXACT[] = {{"0", "0", "0"}, {"1", "45", "50"}, {"-1", "-45", "-50"}};

static abn_status inverse(abn *out, const abn *x, fn_angle m, abn_status (*f)(abn *, const abn *),
                          const exact_angle *table, int count, bool odd)
{
    abn r, k, z = abn_zero();
    abn_status st;
    if (tiny(x) && !odd) x = &z;        /* acos of a tiny x: acos 0 (pi/2 - x rounds to it) */
    if (m != FN_RAD) {
        for (int i = 0; i < count; i++) {
            const char *v = m == FN_DEG ? table[i].deg : table[i].grad;
            k = K(table[i].arg);
            if (v && abn_cmp(x, &k) == 0) {
                *out = K(v);
                return ABN_OK;
            }
        }
    }
    if (tiny(x)) r = *x;                /* asin, atan: x itself */
    else if ((st = f(&r, x)) != ABN_OK) return st;
    if (m == FN_RAD) {
        *out = r;
        return ABN_OK;
    }
    k = K(unit_of(m)->from_rad);
    return abn_mul(out, &r, &k);
}

abn_status fn_asin(abn *out, const abn *x, fn_angle m)
{
    abn_status st0 = inverse(out, x, m, abn_asin, ASIN_EXACT, (int)(sizeof ASIN_EXACT / sizeof ASIN_EXACT[0]), true);
    inv_hint(out, st0, m);
    return inv_done(sure(out, st0, x, f_inv, 0 + UNIT3(m)));
}

abn_status fn_acos(abn *out, const abn *x, fn_angle m)
{
    abn_status st0 = inverse(out, x, m, abn_acos, ACOS_EXACT, (int)(sizeof ACOS_EXACT / sizeof ACOS_EXACT[0]), false);
    inv_hint(out, st0, m);
    return inv_done(sure(out, st0, x, f_inv, 1 + UNIT3(m)));
}

abn_status fn_atan(abn *out, const abn *x, fn_angle m)
{
    abn big, ax = abn_abs(x);
    abn_from_text(&big, "1E40");
    if (abn_cmp(&ax, &big) >= 0) {
        /* atan x = +-(pi/2 - 1/x), 1/x far below the 34th digit (the engine's atan leaves the range
           inside for the largest x, unit 026) */
        *out = K(m == FN_DEG ? "90" : m == FN_GRAD ? "100" : "1.570796326794896619231321691639751");
        if (abn_is_negative(x)) *out = abn_neg(out);
        inv_hint(out, ABN_OK, m);
        return inv_done(sure(out, ABN_OK, x, f_inv, 2 + UNIT3(m)));     /* (pi/2 - 1/x rounded once: unit 044) */
    }
    abn_status st0 = inverse(out, x, m, abn_atan, ATAN_EXACT, (int)(sizeof ATAN_EXACT / sizeof ATAN_EXACT[0]), true);
    inv_hint(out, st0, m);
    return inv_done(sure(out, st0, x, f_inv, 2 + UNIT3(m)));
}

/* ---- hyperbolic (UG p.4-6) ---- */

/* the odd ones of a tiny x are x, cosh is 1 (see tiny) */
static abn_status odd_fn(abn *out, const abn *x, abn_status (*f)(abn *, const abn *))
{
    if (tiny(x)) {
        *out = *x;
        return ABN_OK;
    }
    return f(out, x);
}

abn_status fn_sinh(abn *out, const abn *x) { return sure(out, odd_fn(out, x, abn_sinh), x, f_hyp, 0); }
abn_status fn_cosh(abn *out, const abn *x)
{
    if (tiny(x)) {
        *out = one();
        return ABN_OK;
    }
    return sure(out, abn_cosh(out, x), x, f_hyp, 1);
}
abn_status fn_tanh(abn *out, const abn *x)
{
    abn big = abn_from_int(100), ax = abn_abs(x);
    if (abn_cmp(&ax, &big) >= 0) {
        /* 1 - |tanh x| = 2e^-2|x| lies far below the 34th digit (and the engine's tanh leaves the
           range inside for the largest x, unit 026) */
        *out = abn_is_negative(x) ? abn_from_int(-1) : one();
        return ABN_OK;
    }
    return sure(out, odd_fn(out, x, abn_tanh), x, f_hyp, 2);
}
abn_status fn_asinh(abn *out, const abn *x) { return sure(out, odd_fn(out, x, abn_asinh), x, f_ahyp, 0); }
abn_status fn_acosh(abn *out, const abn *x) { return sure(out, abn_acosh(out, x), x, f_ahyp, 1); }
abn_status fn_atanh(abn *out, const abn *x) { return sure(out, odd_fn(out, x, abn_atanh), x, f_ahyp, 2); }

/* ======================================================================================== */
/* unit 004: parts of numbers, RND, H.MMSS, conversions, probability, rect/polar, RAND        */

/* ---- parts of numbers (UG p.4-17, p.4-18) ---- */

abn_status fn_ip(abn *out, const abn *x)
{
    return abn_trunc(out, x);       /* the integer part, toward zero */
}

/* FP is x - IP(x): it keeps the sign of x (ours, unit 004: the guide shows no negative case). */
abn_status fn_fp(abn *out, const abn *x)
{
    abn ip;
    abn_status st = abn_trunc(&ip, x);
    if (st != ABN_OK) return st;
    return abn_sub(out, x, &ip);    /* exact: the fraction has fewer digits than x */
}

abn_status fn_intg(abn *out, const abn *x)
{
    return abn_floor(out, x);       /* the greatest integer <= x (UG p.4-18) */
}

abn_status fn_abs(abn *out, const abn *x)
{
    *out = abn_abs(x);
    return ABN_OK;
}

abn_status fn_sgn(abn *out, const abn *x)
{
    *out = abn_from_int(abn_is_zero(x) ? 0 : (abn_is_negative(x) ? -1 : 1));
    return ABN_OK;
}

/* x rounded at 10^p, half away from zero. */
static abn round_pos(const abn *x, int p)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead;
    abn_digits(x, &neg, d, &lead);
    if (d[0] == '\0') return abn_zero();
    int k = lead - p + 1;           /* significant digits kept */
    if (k >= (int)strlen(d)) return *x;
    if (k > 0) return round_sig(x, k);
    if (k == 0 && d[0] >= '5') {    /* rounds up to one unit at 10^p */
        abn r;
        if (abn_pow10(&r, p) != ABN_OK) return abn_zero();
        return neg ? abn_neg(&r) : r;
    }
    return abn_zero();
}

/* RND rounds x to what the display setting shows (UG p.4-18; rules ours, unit 004): FIX n to n
 * decimals, SCI and ENG n to n + 1 significant digits, ALL unchanged; half away from zero like
 * the display. Where FIX would show scientific notation (a nonzero value that rounds to zero, or
 * more than 34 digits; unit 002), RND rounds to the n + 1 digits that are shown. */
abn_status fn_rnd(abn *out, const abn *x, ab_mode mode, int n)
{
    if (abn_is_zero(x) || mode == AB_ALL) {
        *out = *x;
        return ABN_OK;
    }
    if (mode == AB_FIX) {
        abn r = round_pos(x, -n);
        bool neg;
        char d[ABN_DIGITS + 1];
        int lead;
        abn_digits(&r, &neg, d, &lead);
        if (!abn_is_zero(&r) && lead + n + 1 <= ABN_DIGITS) {
            *out = r;
            return ABN_OK;
        }
    }
    abn r = round_sig(x, n + 1);
    if (abn_is_zero(&r)) return ABN_OVERFLOW;   /* all nines rounded up past the largest number (unit 026) */
    *out = r;
    return ABN_OK;
}

/* ---- hours, minutes, seconds (UG p.4-12, p.4-13) ---- */

/* Decimal hours (or degrees) to H.MMSSss. The total in seconds is formed once (exact for any
 * short input); hours, minutes and seconds then come from exact integer division and remainder,
 * and are packed as (h*10000 + m*100 + s) / 10000, where the division only moves the point. */
abn_status fn_hms(abn *out, const abn *x)
{
    abn ax = abn_abs(x), s, h, rest, m, sec, v, k3600 = abn_from_int(3600), k60 = abn_from_int(60),
        k100 = abn_from_int(100), k10000 = abn_from_int(10000);
    abn_status st;
    if (abn_is_integer(x)) {
        *out = *x;                      /* whole hours: H.MMSS is x (and x 3600 may leave the range) */
        return ABN_OK;
    }
    if ((st = abn_mul(&s, &ax, &k3600)) != ABN_OK) return st;
    if ((st = abn_idiv(&h, &s, &k3600)) != ABN_OK) return st;
    if ((st = abn_fmod(&rest, &s, &k3600)) != ABN_OK) return st;
    if ((st = abn_idiv(&m, &rest, &k60)) != ABN_OK) return st;
    if ((st = abn_fmod(&sec, &rest, &k60)) != ABN_OK) return st;
    if ((st = abn_mul(&v, &m, &k100)) != ABN_OK) return st;
    if ((st = abn_add(&v, &v, &sec)) != ABN_OK) return st;
    if ((st = abn_fma(&v, &h, &k10000, &v)) != ABN_OK) return st;
    if ((st = abn_div(&v, &v, &k10000)) != ABN_OK) return st;
    *out = abn_is_negative(x) ? abn_neg(&v) : v;
    return ABN_OK;
}

/* H.MMSSss to decimal hours: the seconds h*3600 + mm*60 + ss.ss are exact for any short input,
 * then one division by 3600. */
abn_status fn_hmsinv(abn *out, const abn *x)
{
    abn ax = abn_abs(x), h, f, mm, ss, t, k3600 = abn_from_int(3600), k60 = abn_from_int(60),
        k100 = abn_from_int(100);
    abn_status st;
    if (abn_is_integer(x)) {
        *out = *x;                      /* whole hours (and h 3600 may leave the range, unit 026) */
        return ABN_OK;
    }
    if ((st = abn_trunc(&h, &ax)) != ABN_OK) return st;
    if ((st = abn_sub(&f, &ax, &h)) != ABN_OK) return st;
    if ((st = abn_mul(&f, &f, &k100)) != ABN_OK) return st;          /* MM.SSss: exact */
    if ((st = abn_trunc(&mm, &f)) != ABN_OK) return st;
    if ((st = abn_sub(&ss, &f, &mm)) != ABN_OK) return st;
    if ((st = abn_mul(&ss, &ss, &k100)) != ABN_OK) return st;        /* SS.ss: exact */
    if ((st = abn_fma(&t, &mm, &k60, &ss)) != ABN_OK) return st;
    if ((st = abn_fma(&t, &h, &k3600, &t)) != ABN_OK) return st;
    if ((st = abn_div(&t, &t, &k3600)) != ABN_OK) return st;
    *out = abn_is_negative(x) ? abn_neg(&t) : t;
    return ABN_OK;
}

/* ---- angle and unit conversions (UG p.4-13, p.4-14) ---- */

abn_status fn_todeg(abn *out, const abn *x)
{
    abn k = K(DEG_UNIT.from_rad);
    return sure(out, abn_mul(out, x, &k), x, f_angle, 1);     /* x 180 / pi rounded once (unit 044) */
}

abn_status fn_torad(abn *out, const abn *x)
{
    abn k = K(DEG_UNIT.to_rad);
    return sure(out, abn_mul(out, x, &k), x, f_angle, 0);     /* x pi / 180 rounded once (unit 044) */
}

/* (a*x + b) / c, correctly rounded, for small integers a, b, c: computed exactly as integers
 * and rounded once (the temperature conversions, whose offset defeats a single operation). */
static abn_status affine_exact_(abn *out, const abn *x, uint32_t a, long b, uint32_t c)
{
    bigint *A, *B;
    bool neg, bneg = b < 0;
    char d[ABN_DIGITS + 1];
    int lead, n;
    abn_digits(x, &neg, d, &lead);
    n = (int)strlen(d);
    long q = (long)lead - n + 1;            /* x = (-1)^neg * digits * 10^q */
    unsigned long ub = (unsigned long)(bneg ? -b : b);
    if (!ar_bi(2, &A, &B)) return ABN_RANGE;
    if (n == 0) {
        A->n = 0;
        q = 0;
    } else if (!bi_from_digits(A, d, q > 0 ? (int)q : 0) || !bi_mul_u32(A, a)) {
        return ABN_RANGE;
    }
    if (q > 0) q = 0;
    bi_set(B, (uint32_t)ub);
    if (!bi_mul_pow10(B, (int)-q)) return ABN_RANGE;   /* b on the same scale as a*x */
    bool rneg;
    if (neg == bneg || A->n == 0) {
        if (!bi_add(A, A, B)) return ABN_RANGE;
        rneg = A->n == 0 ? bneg : neg;
    } else if (bi_cmp(A, B) >= 0) {
        bi_sub(A, A, B);
        rneg = neg;
    } else {
        bi_sub(A, B, A);
        rneg = bneg;
    }
    if (bi_is_zero(A)) {
        *out = abn_zero();
        return ABN_OK;
    }
    if (!bi_mul_pow10(A, 45)) return ABN_RANGE;        /* 45 more digits before dividing */
    uint32_t rem = bi_divmod_u32(A, c);
    return bi_to_abn(out, A, rneg, q - 45, rem != 0);
}

static abn_status affine_exact(abn *out, const abn *x, uint32_t a, long b, uint32_t c)
{
    int m = ar_mark();
    abn_status st = affine_exact_(out, x, a, b, c);
    ar_release(m);
    return st;
}

/* Exact definitional factors (ours, unit 004): 1 lb = 0.45359237 kg, 1 in = 2.54 cm, 1 US gal =
 * 3.785411784 l (the guide's example 3.7854 is the US gallon, UG p.4-14), 1 mile = 1.609344 km.
 * One multiplication or division each: correctly rounded. F = C * 9/5 + 32. */
abn_status fn_convert(abn *out, const abn *x, fn_unit u)
{
    static const char *const FACTOR[] = {"0.45359237", "0.45359237", NULL, NULL, "2.54", "2.54",
                                         "3.785411784", "3.785411784", "1.609344", "1.609344"};
    if (u == FN_TOC) return affine_exact(out, x, 5, -160, 9);  /* (5x - 160) / 9 */
    if (u == FN_TOF) return affine_exact(out, x, 9, 160, 5);   /* (9x + 160) / 5 */
    abn k = K(FACTOR[u]);
    bool to_metric = u == FN_TOKG || u == FN_TOCM || u == FN_TOL || u == FN_TOKM;
    return to_metric ? abn_mul(out, x, &k) : abn_div(out, x, &k);
}

/* ---- factorial, combinations, permutations (UG p.4-15) ---- */

#define FACT_MAX 2123   /* the largest n with n! inside the decimal128 range (ours, unit 004) */

#define GAMMA_P 64      /* the digits gamma is formed to before its one rounding (unit 021) */

/* gamma(z) for 0 < z < 10^5 (beyond that the result is outside the range either way): up to
 * w = z + n >= 40 by gamma(z) = gamma(w) / (z (z + 1) ... (w - 1)), then the Stirling series. */
static bool gamma_pos(hp *g, const hp *z)
{
    static hp one, forty, w, prod, d, t, lg, e;
    if (hp_lead(z) >= 5) return false;
    hp_set_int(&one, 1);
    hp_set_int(&forty, 40);
    hp_set_int(&prod, 1);
    w = *z;
    for (;;) {
        if (!hp_add(&d, &w, &forty, true, GAMMA_P + 40)) return false;
        if (sxn_sign(&d) >= 0) break;
        if (!hp_mul(&t, &prod, &w, GAMMA_P + 5)) return false;
        prod = t;
        if (!hp_add(&t, &w, &one, false, GAMMA_P + 40)) return false;
        w = t;
    }
    return hp_lgamma_big(&lg, &w, GAMMA_P) && hp_exp(&e, &lg, GAMMA_P) && hp_div(g, &e, &prod, GAMMA_P);
}

/* x! of a non-integer x is gamma(x + 1) (UG p.4-15). Below x = -1, the reflection
 * gamma(x + 1) gamma(-x) = pi / sin(pi (x + 1)), with the sine from the exact fraction of x
 * (the nearer of f and 1 - f) so it keeps its digits next to the poles. A result outside the
 * range is an overflow or an underflow (unit 026; it was unit 003 rule 4's range error). */
static abn_status gamma_(abn *out, const abn *x)
{
    static hp hx, h1, z, g, pi, a, s, c, t, u;
    abn m1 = abn_from_int(-1), u1 = one(), fl, f, f1;
    hp_set_int(&h1, 1);
    if (!hp_from_abn(&hx, x)) return ABN_RANGE;
    if (abn_cmp(x, &m1) > 0) {
        if (!hp_add(&z, &hx, &h1, false, 120) || !gamma_pos(&g, &z)) return ABN_OVERFLOW;
        return hp_round(out, &g);
    }
    /* x = fl + f, 0 < f < 1, all exact; sin(pi (x + 1)) = -(-1)^fl sin(pi f) */
    if (fn_intg(&fl, x) != ABN_OK || abn_sub(&f, x, &fl) != ABN_OK || abn_sub(&f1, &u1, &f) != ABN_OK)
        return ABN_RANGE;
    if (abn_cmp(&f1, &f) < 0) f = f1;
    hp_neg(&hx);
    if (!gamma_pos(&g, &hx)) return ABN_UNDERFLOW;  /* gamma(-x) beyond the range: x! below it */
    if (!hp_from_abn(&a, &f) || !hp_pi(&pi, GAMMA_P + 5) || !hp_mul(&t, &pi, &a, GAMMA_P + 5) ||
        !hp_sincos(&s, &c, &t, GAMMA_P) || !hp_mul(&u, &s, &g, GAMMA_P) || !hp_div(&t, &pi, &u, GAMMA_P))
        return ABN_RANGE;
    if (!is_odd_integer(&fl)) hp_neg(&t);
    return hp_round(out, &t);
}

/* n! exactly, rounded once; a negative integer is an error (UG p.4-15). */
static abn_status fn_fact_(abn *out, const abn *x)
{
    bigint *f;
    int32_t n;
    if (!ar_bi(1, &f)) return ABN_RANGE;
    if (abn_is_negative(x)) return ABN_DOMAIN;
    if (!small_int(x, &n) || n > FACT_MAX) return ABN_OVERFLOW;
    bi_set(f, 1);
    for (int32_t i = 2; i <= n; i++)
        if (!bi_mul_u32(f, (uint32_t)i)) return ABN_RANGE;
    return bi_to_abn(out, f, false, 0, false);
}

abn_status fn_fact(abn *out, const abn *x)
{
    if (!abn_is_integer(x)) return gamma_(out, x);
    int m = ar_mark();
    abn_status st = fn_fact_(out, x);
    ar_release(m);
    return st;
}

/* n and r must be non-negative integers (UG p.4-15); both as exact bigints. */
static abn_status prob_args(bigint *n, bigint *r, const abn *xn, const abn *xr)
{
    if (!abn_is_integer(xn) || !abn_is_integer(xr) || abn_is_negative(xn) || abn_is_negative(xr))
        return ABN_DOMAIN;
    if (!bi_from_abn(n, xn) || !bi_from_abn(r, xr)) return ABN_RANGE;
    return ABN_OK;
}

/* A bigint known to be small, as an int32 (or -1 if it has more than 9 digits). */
static int32_t small_bi(const bigint *a)
{
    if (a->n == 0) return 0;
    if (a->n > 1) return -1;
    return (int32_t)a->l[0];
}

#define RESULT_DIGITS_MAX 6150  /* past this the result is surely beyond the decimal128 range */

/* Permutations: n (n-1) ... (n-r+1), exactly, rounded once. r > n gives 0 (no arrangements;
 * ours, unit 004). */
static abn_status fn_npr_(abn *out, const abn *xn, const abn *xr)
{
    bigint *n, *rb, *p, *t, *tmp;
    if (!ar_bi(5, &n, &rb, &p, &t, &tmp)) return ABN_RANGE;
    abn_status st = prob_args(n, rb, xn, xr);
    if (st != ABN_OK) return st;
    /* r > n, or n of 10^16 or more: INVALID DATA (UG p.F-2; ruled by abacus #3778, which replaces
       unit 004's 0 for r > n and its large n) */
    if (bi_cmp(rb, n) > 0 || bi_ndigits(n) >= 17) return ABN_DOMAIN;
    int32_t r = small_bi(rb);
    if (r < 0 || r > FACT_MAX) return ABN_OVERFLOW;  /* nPr >= r! */
    bi_set(p, 1);
    *t = *n;
    for (int32_t i = 0; i < r; i++) {
        if (!bi_mul(tmp, p, t)) return ABN_RANGE;
        *p = *tmp;
        if (bi_ndigits(p) > RESULT_DIGITS_MAX) return ABN_OVERFLOW;
        bi_sub_u32(t, 1);
    }
    return bi_to_abn(out, p, false, 0, false);
}

abn_status fn_npr(abn *out, const abn *xn, const abn *xr)
{
    int m = ar_mark();
    abn_status st = fn_npr_(out, xn, xr);
    ar_release(m);
    return st;
}

/* Combinations: with r' = min(r, n - r), the product over i of (n - r' + i) / i, each division
 * exact, then rounded once. r > n and n >= 10^16 are INVALID DATA (p.F-2, #3778). */
static abn_status fn_ncr_(abn *out, const abn *xn, const abn *xr)
{
    bigint *n, *rb, *c, *base, *t, *tmp;
    if (!ar_bi(6, &n, &rb, &c, &base, &t, &tmp)) return ABN_RANGE;
    abn_status st = prob_args(n, rb, xn, xr);
    if (st != ABN_OK) return st;
    if (bi_cmp(rb, n) > 0 || bi_ndigits(n) >= 17) return ABN_DOMAIN;   /* as nPr (p.F-2) */
    bi_sub(base, n, rb);                         /* n - r */
    if (bi_cmp(base, rb) < 0) {                   /* use r' = n - r, base = r */
        *tmp = *base;
        *base = *rb;
        *rb = *tmp;
    }
    int32_t r = small_bi(rb);
    if (r < 0 || r > 20500) return ABN_OVERFLOW;    /* C(n, r') >= 2^r' when r' <= n / 2: beyond range */
    bi_set(c, 1);
    for (int32_t i = 1; i <= r; i++) {
        *t = *base;
        if (!bi_add_u32(t, (uint32_t)i) || !bi_mul(tmp, c, t)) return ABN_RANGE;
        *c = *tmp;
        bi_divmod_u32(c, (uint32_t)i);             /* exact */
        if (bi_ndigits(c) > RESULT_DIGITS_MAX) return ABN_OVERFLOW;
    }
    return bi_to_abn(out, c, false, 0, false);
}

abn_status fn_ncr(abn *out, const abn *xn, const abn *xr)
{
    int m = ar_mark();
    abn_status st = fn_ncr_(out, xn, xr);
    ar_release(m);
    return st;
}

/* ---- rectangular and polar on X and Y (ours, unit 004) ---- */

/* theta of (x, y) in the angle mode, exact on the axes and diagonals in DEG and GRAD. */
abn_status fn_topol(abn *r, abn *theta, const abn *x, const abn *y, fn_angle m)
{
    abn rr, th, ax = abn_abs(x), ay = abn_abs(y);
    abn_status st, rst = ABN_OK;
    /* r beyond the range: the largest number, theta as ever, reported ABN_OVERFLOW with both written;
       below it 0 (unit 026) */
    if ((st = hp_hypot(&rr, x, y)) == ABN_OVERFLOW) {     /* correctly rounded (unit 034) */
        rr = abn_largest();
        rst = ABN_OVERFLOW;
    } else if (st == ABN_UNDERFLOW) {
        rr = abn_zero();
    } else if (st != ABN_OK) {
        return st;
    }
    if (m != FN_RAD) {
        const angle_unit *u = unit_of(m);
        const char *v = NULL;
        bool yneg = abn_is_negative(y);
        if (abn_is_zero(y)) {
            v = abn_is_negative(x) ? u->half : "0";
        } else if (abn_is_zero(x)) {
            v = u->quarter;
        } else if (abn_cmp(&ax, &ay) == 0) {
            v = abn_is_negative(x) ? (m == FN_DEG ? "135" : "150") : u->eighth;
        }
        if (v) {
            th = K(v);
            if (yneg) th = abn_neg(&th);
            *r = rr;
            *theta = th;
            return rst;
        }
    }
    if (abn_is_zero(x) || abn_is_zero(y)) {
        /* on an axis (radians only here): the engine's atan2, exact to the rounding of pi, and its sign of
           zero as the engine gives it (the engine keeps no -0, so the negative real axis is +pi, 180 or 200) */
        if ((st = abn_atan2(&th, y, x)) != ABN_OK) return st;
    } else {
        /* theta correctly rounded (unit 034): atan2 (hp_atan2) into the angle unit, formed to 56 digits, then 100,
           then 180 until its rounding is sure (Ziv); below the range it is 0 */
        static const int P[] = {56, 100, 180};
        hp *t = &fw[0], *k = &fw[1], *u = &fw[2], *sc = &fw[3];
        for (int i = 0; i < 3; i++) {
            int p = P[i];
            if (!hp_atan2(t, y, x, p)) return ABN_RANGE;
            if (m != FN_RAD) {
                hp_set_int(k, m == FN_DEG ? 180 : 200);
                if (!hp_mul(u, t, k, p + 4) || !hp_pi(k, p + 6) || !hp_div(t, u, k, p)) return ABN_RANGE;
            }
            if (hp_round_sure(&th, &st, t, p, sc)) break;
        }
        if (st == ABN_UNDERFLOW) th = abn_zero();
        else if (st != ABN_OK) return st;
    }
    *r = rr;
    *theta = th;
    return rst;
}

/* r cos theta and r sin theta. In degrees and grads the products are formed exactly from the
 * 60-digit sine and cosine and rounded once, so both are correctly rounded; in radians the
 * engine's cosine and sine are multiplied by r. */
static abn_status rect_part(abn *out, const abn *r, const abn *theta, fn_angle m, bool want_y)
{
    const angle_unit *u = unit_of(m);
    reduced rd;
    abn v;
    abn_status st;
    if ((st = reduce(&rd, theta, u)) != ABN_OK) return st;
    bool use_cos = want_y ? rd.swap : !rd.swap;     /* the function the reduced angle needs */
    int sign = want_y ? rd.sin_sign : rd.cos_sign;
    abn_status vs = ABN_OK;                         /* the product's: its one rounding */
    if (abn_is_zero(&rd.a)) {
        v = use_cos ? *r : abn_zero();              /* cos 0 = 1, sin 0 = 0 exactly */
    } else if (!use_cos && u->sixth_of_half && (v = K(u->sixth_of_half), abn_cmp(&rd.a, &v) == 0)) {
        abn half = K("0.5");
        vs = abn_mul(&v, r, &half);                 /* r sin 30 = r / 2 */
    } else if ((st = trig_exactish(&v, &rd.a, u, use_cos, r)) == ABN_DOMAIN) {
        /* below 1E-17 of a unit: in hp, correctly rounded (trig_hp; unit 034: it was the function rounded,
           then the product; below the range, unit 026's a k r, now 0 by the rounding) */
        vs = trig_hp(&v, &rd.a, u, use_cos, r);
    } else {
        vs = st;
    }
    /* the part beyond the range: the largest number with r's sign (the function's here is +), or
       below it 0 (unit 026); the caller learns of an overflow by the status, *out written */
    if (vs == ABN_UNDERFLOW) {
        v = abn_zero();
        vs = ABN_OK;
    } else if (vs == ABN_OVERFLOW) {
        v = abn_largest();
        if (abn_is_negative(r)) v = abn_neg(&v);
    } else if (vs != ABN_OK) {
        return vs;
    }
    *out = signed_(&v, sign);
    return vs;
}

/* A part r f with its status: beyond the range the largest number with the product's sign, below it
 * 0 (unit 026); *over set for the first. Other statuses pass. */
static abn_status rad_part(abn *out, const abn *f, const abn *r, bool *over)
{
    abn_status st = abn_mul(out, f, r);
    if (st == ABN_UNDERFLOW) {
        *out = abn_zero();
        return ABN_OK;
    }
    if (st == ABN_OVERFLOW) {
        *out = abn_largest();
        if (abn_is_negative(f) != abn_is_negative(r)) *out = abn_neg(out);
        *over = true;
        return ABN_OK;
    }
    return st;
}

abn_status fn_torec(abn *x, abn *y, const abn *r, const abn *theta, fn_angle m)
{
    abn c, s;
    abn_status st;
    bool over = false;
    if (m == FN_RAD) {
        /* r cos theta and r sin theta in hp, formed to 60 digits, then 100, then 180 until each rounding is sure:
           correctly rounded (unit 034, Ziv) while hp's pi reaches (|theta| below about 1E169, less at the higher
           precisions, where the last sure-enough result stands); past it, the engine's cosine and sine times r */
        static const int P[] = {60, 100, 180};
        hp *ht = &fw[0], *hs = &fw[1], *hc = &fw[2], *hr = &fw[3], *t = &fw[4], *sc = &fw[5];
        abn_status sp[2] = {ABN_OK, ABN_OK};
        bool formed = false, pneg[2] = {false, false};
        for (int i = 0; i < 3; i++) {
            int p = P[i];
            if (!hp_from_abn(ht, theta) || !hp_sincos(hs, hc, ht, p) || !hp_from_abn(hr, r)) break;
            bool sure = true;
            for (int part = 0; part < 2; part++) {
                if (!hp_mul(t, part ? hs : hc, hr, p)) return ABN_RANGE;
                sure = hp_round_sure(part ? &s : &c, &sp[part], t, p, sc) && sure;
                pneg[part] = t->neg;            /* an overflow's sign */
            }
            formed = true;
            if (sure) break;
        }
        if (formed) {
            for (int part = 0; part < 2; part++) {
                abn *o = part ? &s : &c;
                if (sp[part] == ABN_UNDERFLOW) {
                    *o = abn_zero();
                } else if (sp[part] == ABN_OVERFLOW) {
                    *o = abn_largest();
                    if (pneg[part]) *o = abn_neg(o);
                    over = true;
                } else if (sp[part] != ABN_OK) {
                    return sp[part];
                }
            }
            *x = c;
            *y = s;
            return over ? ABN_OVERFLOW : ABN_OK;
        }
        if ((st = abn_cos(&c, theta)) != ABN_OK) return st;
        if ((st = abn_sin(&s, theta)) == ABN_UNDERFLOW) s = *theta;  /* a tiny theta: sin theta = theta */
        else if (st != ABN_OK) return st;
        if ((st = rad_part(&c, &c, r, &over)) != ABN_OK) return st;
        if ((st = rad_part(&s, &s, r, &over)) != ABN_OK) return st;
    } else {
        if ((st = rect_part(&c, r, theta, m, false)) == ABN_OVERFLOW) over = true;
        else if (st != ABN_OK) return st;
        if ((st = rect_part(&s, r, theta, m, true)) == ABN_OVERFLOW) over = true;
        else if (st != ABN_OK) return st;
    }
    *x = c;
    *y = s;
    return over ? ABN_OVERFLOW : ABN_OK;
}

/* ---- RAND and SEED (ours, unit 004) ---- */

/* SplitMix64: G. L. Steele Jr., D. Lea, C. H. Flood, "Fast Splittable Pseudorandom Number
 * Generators", OOPSLA 2014; the constants are the golden-ratio increment and the two finalizer
 * multipliers published there. 64-bit state, period 2^64. */
static uint64_t mix64(uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static uint64_t next64(uint64_t *state)
{
    *state += 0x9E3779B97F4A7C15ull;
    return mix64(*state);
}

/* SEED: the state is a hash of x's canonical digits and exponent, so equal values seed alike
 * (1 and 1.0 included). A seed of 0 is a seed like any other: the core has no clock (the guide's
 * calculator picks its own seed for 0, UG p.4-16). */
void fn_seed(uint64_t *state, const abn *x)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead;
    uint64_t h = 0x9E3779B97F4A7C15ull;
    abn_digits(x, &neg, d, &lead);
    h = mix64(h ^ (neg ? 1u : 0u));
    for (const char *p = d; *p; p++) h = mix64(h ^ (uint64_t)(unsigned char)*p);
    h = mix64(h ^ (uint64_t)(uint32_t)lead);
    *state = h;
}

/* 18 uniform decimal digits from one 64-bit output, by rejecting the top partial block. */
static uint64_t digits18(uint64_t *state)
{
    const uint64_t LIMIT = 18000000000000000000ull;     /* 18 * 10^18 <= 2^64 - 1 */
    uint64_t v;
    do {
        v = next64(state);
    } while (v >= LIMIT);
    return v % 1000000000000000000ull;
}

/* A number uniform on the 34-digit grid k / 10^34, 0 < k < 10^34 (the guide: 0 < x < 1). */
abn fn_rand(uint64_t *state)
{
    char text[40] = "0.";
    for (;;) {
        uint64_t a = digits18(state), b = digits18(state) / 100;    /* 18 + 16 digits */
        if (a == 0 && b == 0) continue;
        for (int i = 17; i >= 0; i--) {
            text[2 + i] = (char)('0' + a % 10);
            a /= 10;
        }
        for (int i = 15; i >= 0; i--) {
            text[20 + i] = (char)('0' + b % 10);
            b /= 10;
        }
        text[36] = '\0';
        return K(text);
    }
}
