/* fn: the calculator's functions of one and two numbers (unit 003).
 *
 * The engine (abn.h) computes; this file adds the calculator's rules: exact decimal answers
 * where the answer is exact, angles in degrees and grads reduced exactly in decimal before any
 * conversion to radians, and domain errors instead of complex or infinite results. Rules from
 * the user's guide are cited "UG p.N"; rules this project set are "ours, unit 003".
 */
#include <string.h>

#include "fn.h"
#include "bigint.h"

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

/* x as an int32 when it is an integer of at most 9 digits. */
static bool small_int(const abn *x, int32_t *v)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead, n;
    abn_digits(x, &neg, d, &lead);
    n = (int)strlen(d);
    if (n == 0) {
        *v = 0;
        return true;
    }
    if (lead < 0 || lead > 8 || n > lead + 1) return false;
    int32_t r = 0;
    for (int i = 0; i <= lead; i++) r = r * 10 + (i < n ? d[i] - '0' : 0);
    *v = neg ? -r : r;
    return true;
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

/* y^e for y > 0. The engine's pow is accurate for non-integer exponents, but for integer
 * exponents it loses about one unit per power (measured: 247 units at n = 200, against its
 * documented bound of 8). So an integer power is exact by squaring when that is exact, and
 * otherwise taken as y^(n - 1/2) * sqrt(y): a non-integer power and a correctly rounded root. */
static abn_status pow_real(abn *out, const abn *y, const abn *e)
{
    int32_t n;
    abn r;
    if (!abn_is_integer(e) || !small_int(e, &n)) return abn_pow(out, y, e);
    if (n >= -POW_EXACT_MAX && n <= POW_EXACT_MAX &&
        pow_exact(&r, y, (uint32_t)(n < 0 ? -n : n)) == ABN_OK) {
        if (n < 0) {
            abn u = one();
            return abn_div(out, &u, &r);
        }
        *out = r;
        return ABN_OK;
    }
    if (n >= -1 && n <= 1) return abn_pow(out, y, e);
    abn half = K("0.5"), e2, p, s;
    abn_status st;
    if ((st = abn_sub(&e2, e, &half)) != ABN_OK) return st;
    if ((st = abn_pow(&p, y, &e2)) != ABN_OK) return st;
    if ((st = abn_sqrt(&s, y)) != ABN_OK) return st;
    return abn_mul(out, &p, &s);
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
    return abn_exp(out, x);
}

abn_status fn_ln(abn *out, const abn *x)
{
    abn u = one();
    if (abn_cmp(x, &u) == 0) {
        *out = abn_zero();
        return ABN_OK;
    }
    return abn_ln(out, x);
}

abn_status fn_tenx(abn *out, const abn *x)
{
    int32_t n;
    if (small_int(x, &n)) return abn_pow10(out, n);    /* 10^n exactly */
    return abn_exp10(out, x);
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
    return abn_log10(out, x);
}

/* y^x (UG p.4-2). A negative base needs an integer power, zero to a negative power is
 * division by zero (ours, unit 003); integer powers that fit 34 digits are exact. */
abn_status fn_pow(abn *out, const abn *y, const abn *x)
{
    bool xint = abn_is_integer(x);
    bool yneg = abn_is_negative(y);
    if (abn_is_zero(y)) {
        if (abn_is_negative(x)) return ABN_DIV_ZERO;
        *out = abn_is_zero(x) ? one() : abn_zero();
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
 * The root is y^(1/x), but 1/x is rarely exact and its rounding error is multiplied by
 * ln y. So 1/x = h + l, with h the rounded reciprocal and l = (1 - h*x)/x from a fused
 * multiply-add, and the root is y^h * (1 + l * ln y). Perfect powers come out exact. */
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
    abn ay = abn_abs(y), u = one(), h, nh, res, l, lny, corr, r;
    abn_status st;
    if ((st = abn_div(&h, &u, x)) != ABN_OK) return st;
    nh = abn_neg(&h);
    if ((st = abn_fma(&res, &nh, x, &u)) != ABN_OK) return st;      /* 1 - h*x */
    if ((st = pow_real(&r, &ay, &h)) != ABN_OK) return st;
    if (!abn_is_zero(&res)) {
        if ((st = abn_div(&l, &res, x)) != ABN_OK) return st;
        if ((st = abn_ln(&lny, &ay)) != ABN_OK) return st;
        if ((st = abn_mul(&corr, &l, &lny)) != ABN_OK) return st;
        if (!abn_is_zero(&corr) && (st = abn_fma(&r, &r, &corr, &r)) != ABN_OK) return st;
    }
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

abn_status fn_idiv(abn *out, const abn *y, const abn *x)
{
    return abn_idiv(out, y, x);     /* truncated toward zero (pending a ruling for negatives) */
}

abn_status fn_rmdr(abn *out, const abn *y, const abn *x)
{
    return abn_fmod(out, y, x);     /* sign of y (pending a ruling for negatives) */
}

abn_status fn_pct(abn *out, const abn *y, const abn *x)
{
    abn t, hundred = abn_from_int(100);
    abn_status st = abn_mul(&t, y, x);
    if (st != ABN_OK) return st;
    return abn_div(out, &t, &hundred);  /* dividing by 100 adds no rounding */
}

abn_status fn_pctchg(abn *out, const abn *y, const abn *x)
{
    abn d, hundred = abn_from_int(100);
    abn_status st;
    if (abn_is_zero(y)) return ABN_DIV_ZERO;
    if ((st = abn_sub(&d, x, y)) != ABN_OK) return st;
    if ((st = abn_mul(&d, &d, &hundred)) != ABN_OK) return st;
    return abn_div(out, &d, y);
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
static bool trigx(bigint *v, int *scale, const abn *b, const angle_unit *u, bool want_cos)
{
    static bigint th, th2, term, pos, neg, tmp;
    bool bneg;
    char d[ABN_DIGITS + 1];
    int lead, n, s;
    abn_digits(b, &bneg, d, &lead);
    n = (int)strlen(d);
    if (n == 0 || lead < -17) return false;
    s = TRIGX_DIGITS + (lead < 0 ? -lead : 0);
    /* theta * 10^s = digits * 10^(lead - n + 1) * PI_71 * 10^(s - 71) / half */
    if (!bi_from_digits(&th, d, 0) || !bi_from_digits(&tmp, PI_71, 0) || !bi_mul(&term, &th, &tmp))
        return false;
    int shift = (lead - n + 1) + s - 71;
    if (shift >= 0) {
        if (!bi_mul_pow10(&term, shift)) return false;
    } else {
        bi_div_pow10(&term, -shift);
    }
    {
        uint32_t half = (uint32_t)(u->half[0] - '0') * 100 + (uint32_t)(u->half[1] - '0') * 10 +
                        (uint32_t)(u->half[2] - '0');   /* "180" or "200" */
        bi_divmod_u32(&term, half);
    }
    th = term;
    /* theta^2 at the same scale */
    if (!bi_mul(&th2, &th, &th)) return false;
    bi_div_pow10(&th2, s);
    /* sum the alternating series: sin = theta - theta^3/3! + ..., cos = 1 - theta^2/2! + ... */
    if (want_cos) {
        bi_set(&term, 1);
        if (!bi_mul_pow10(&term, s)) return false;
    } else {
        term = th;
    }
    pos = term;
    neg.n = 0;
    for (uint32_t k = want_cos ? 1 : 2, i = 1; !bi_is_zero(&term); k += 2, i++) {
        if (!bi_mul(&tmp, &term, &th2)) return false;
        bi_div_pow10(&tmp, s);
        bi_divmod_u32(&tmp, k);
        bi_divmod_u32(&tmp, k + 1);
        term = tmp;
        if (i % 2) {
            if (!bi_add(&neg, &neg, &term)) return false;
        } else {
            if (!bi_add(&pos, &pos, &term)) return false;
        }
    }
    bi_sub(v, &pos, &neg);      /* pos > neg: the sum is positive on [0, pi/4] */
    *scale = s;
    return true;
}

/* sin or cos of a reduced angle, correctly rounded, optionally times r (both exact first). */
static abn_status trig_exactish(abn *out, const abn *a, const angle_unit *u, bool want_cos, const abn *r)
{
    static bigint v, rb, prod;
    int scale;
    if (!trigx(&v, &scale, a, u, want_cos)) return ABN_DOMAIN;  /* caller's tiny-angle path */
    if (!r) return bi_to_abn(out, &v, false, -scale, true);
    bool rneg;
    char d[ABN_DIGITS + 1];
    int lead;
    abn_digits(r, &rneg, d, &lead);
    if (d[0] == '\0') {
        *out = abn_zero();
        return ABN_OK;
    }
    if (!bi_from_digits(&rb, d, 0) || !bi_mul(&prod, &v, &rb)) return ABN_RANGE;
    return bi_to_abn(out, &prod, rneg, (long)lead - (long)strlen(d) + 1 - scale, true);
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
    if ((st = to_rad_pair(&hi, &lo, a, u->to_rad, u->to_rad_lo)) != ABN_OK) return st;
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
    if (m == FN_RAD) return abn_sin(out, x);    /* no special cases in radians */
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
    if (m == FN_RAD) return abn_cos(out, x);
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
    static bigint sv, cv, num, q;
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
        const bigint *den = cot ? &sv : &cv;
        num = cot ? cv : sv;
        /* scale the numerator so the quotient carries at least 45 digits, however small it is */
        int extra = 45 + (bi_ndigits(den) > bi_ndigits(&num) ? bi_ndigits(den) - bi_ndigits(&num) : 0);
        if (!bi_mul_pow10(&num, extra) || !bi_div(&q, &num, den, &inexact)) return ABN_RANGE;
        return bi_to_abn(out, &q, false, -extra, true);
    }
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
    if (m == FN_RAD) return abn_tan(out, x);
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
                          const exact_angle *table, int count)
{
    abn r, k;
    abn_status st;
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
    if ((st = f(&r, x)) != ABN_OK) return st;
    if (m == FN_RAD) {
        *out = r;
        return ABN_OK;
    }
    k = K(unit_of(m)->from_rad);
    return abn_mul(out, &r, &k);
}

abn_status fn_asin(abn *out, const abn *x, fn_angle m)
{
    return inverse(out, x, m, abn_asin, ASIN_EXACT, (int)(sizeof ASIN_EXACT / sizeof ASIN_EXACT[0]));
}

abn_status fn_acos(abn *out, const abn *x, fn_angle m)
{
    return inverse(out, x, m, abn_acos, ACOS_EXACT, (int)(sizeof ACOS_EXACT / sizeof ACOS_EXACT[0]));
}

abn_status fn_atan(abn *out, const abn *x, fn_angle m)
{
    return inverse(out, x, m, abn_atan, ATAN_EXACT, (int)(sizeof ATAN_EXACT / sizeof ATAN_EXACT[0]));
}

/* ---- hyperbolic (UG p.4-6) ---- */

abn_status fn_sinh(abn *out, const abn *x) { return abn_sinh(out, x); }
abn_status fn_cosh(abn *out, const abn *x) { return abn_cosh(out, x); }
abn_status fn_tanh(abn *out, const abn *x) { return abn_tanh(out, x); }
abn_status fn_asinh(abn *out, const abn *x) { return abn_asinh(out, x); }
abn_status fn_acosh(abn *out, const abn *x) { return abn_acosh(out, x); }
abn_status fn_atanh(abn *out, const abn *x) { return abn_atanh(out, x); }

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
    *out = round_sig(x, n + 1);
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
    return abn_mul(out, x, &k);
}

abn_status fn_torad(abn *out, const abn *x)
{
    abn k = K(DEG_UNIT.to_rad);
    return abn_mul(out, x, &k);
}

/* (a*x + b) / c, correctly rounded, for small integers a, b, c: computed exactly as integers
 * and rounded once (the temperature conversions, whose offset defeats a single operation). */
static abn_status affine_exact(abn *out, const abn *x, uint32_t a, long b, uint32_t c)
{
    static bigint A, B;     /* static: two 3 KB numbers kept off the stack */
    bool neg, bneg = b < 0;
    char d[ABN_DIGITS + 1];
    int lead, n;
    abn_digits(x, &neg, d, &lead);
    n = (int)strlen(d);
    long q = (long)lead - n + 1;            /* x = (-1)^neg * digits * 10^q */
    unsigned long ub = (unsigned long)(bneg ? -b : b);
    if (n == 0) {
        A.n = 0;
        q = 0;
    } else if (!bi_from_digits(&A, d, q > 0 ? (int)q : 0) || !bi_mul_u32(&A, a)) {
        return ABN_RANGE;
    }
    if (q > 0) q = 0;
    bi_set(&B, (uint32_t)ub);
    if (!bi_mul_pow10(&B, (int)-q)) return ABN_RANGE;   /* b on the same scale as a*x */
    bool rneg;
    if (neg == bneg || A.n == 0) {
        if (!bi_add(&A, &A, &B)) return ABN_RANGE;
        rneg = A.n == 0 ? bneg : neg;
    } else if (bi_cmp(&A, &B) >= 0) {
        bi_sub(&A, &A, &B);
        rneg = neg;
    } else {
        bi_sub(&A, &B, &A);
        rneg = bneg;
    }
    if (bi_is_zero(&A)) {
        *out = abn_zero();
        return ABN_OK;
    }
    if (!bi_mul_pow10(&A, 45)) return ABN_RANGE;        /* 45 more digits before dividing */
    uint32_t rem = bi_divmod_u32(&A, c);
    return bi_to_abn(out, &A, rneg, q - 45, rem != 0);
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

/* n! exactly, rounded once. The guide defines x! for non-integers through gamma (UG p.4-15);
 * that is deferred to a later unit, so a non-integer is an error here, as is a negative. */
abn_status fn_fact(abn *out, const abn *x)
{
    static bigint f;
    int32_t n;
    if (!abn_is_integer(x) || abn_is_negative(x)) return ABN_DOMAIN;
    if (!small_int(x, &n) || n > FACT_MAX) return ABN_RANGE;
    bi_set(&f, 1);
    for (int32_t i = 2; i <= n; i++)
        if (!bi_mul_u32(&f, (uint32_t)i)) return ABN_RANGE;
    return bi_to_abn(out, &f, false, 0, false);
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
abn_status fn_npr(abn *out, const abn *xn, const abn *xr)
{
    static bigint n, rb, p, t, tmp;
    abn_status st = prob_args(&n, &rb, xn, xr);
    if (st != ABN_OK) return st;
    if (bi_cmp(&rb, &n) > 0) {
        *out = abn_zero();
        return ABN_OK;
    }
    int32_t r = small_bi(&rb);
    if (r < 0 || r > FACT_MAX) return ABN_RANGE;     /* nPr >= r! */
    bi_set(&p, 1);
    t = n;
    for (int32_t i = 0; i < r; i++) {
        if (!bi_mul(&tmp, &p, &t)) return ABN_RANGE;
        p = tmp;
        if (bi_ndigits(&p) > RESULT_DIGITS_MAX) return ABN_RANGE;
        bi_sub_u32(&t, 1);
    }
    return bi_to_abn(out, &p, false, 0, false);
}

/* Combinations: with r' = min(r, n - r), the product over i of (n - r' + i) / i, each division
 * exact, then rounded once. r > n gives 0 (ours, unit 004). r' is taken before r is required to
 * be small, so C(10^30, 10^30 - 2) is C(10^30, 2). */
abn_status fn_ncr(abn *out, const abn *xn, const abn *xr)
{
    static bigint n, rb, c, base, t, tmp;
    abn_status st = prob_args(&n, &rb, xn, xr);
    if (st != ABN_OK) return st;
    if (bi_cmp(&rb, &n) > 0) {
        *out = abn_zero();
        return ABN_OK;
    }
    bi_sub(&base, &n, &rb);                         /* n - r */
    if (bi_cmp(&base, &rb) < 0) {                   /* use r' = n - r, base = r */
        tmp = base;
        base = rb;
        rb = tmp;
    }
    int32_t r = small_bi(&rb);
    if (r < 0 || r > 20500) return ABN_RANGE;   /* C(n, r') >= 2^r' when r' <= n / 2: beyond range */
    bi_set(&c, 1);
    for (int32_t i = 1; i <= r; i++) {
        t = base;
        if (!bi_add_u32(&t, (uint32_t)i) || !bi_mul(&tmp, &c, &t)) return ABN_RANGE;
        c = tmp;
        bi_divmod_u32(&c, (uint32_t)i);             /* exact */
        if (bi_ndigits(&c) > RESULT_DIGITS_MAX) return ABN_RANGE;
    }
    return bi_to_abn(out, &c, false, 0, false);
}

/* ---- rectangular and polar on X and Y (ours, unit 004) ---- */

/* theta of (x, y) in the angle mode, exact on the axes and diagonals in DEG and GRAD. */
abn_status fn_topol(abn *r, abn *theta, const abn *x, const abn *y, fn_angle m)
{
    abn rr, th, ax = abn_abs(x), ay = abn_abs(y);
    abn_status st;
    if ((st = abn_hypot(&rr, x, y)) != ABN_OK) return st;
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
            return ABN_OK;
        }
    }
    if ((st = abn_atan2(&th, y, x)) != ABN_OK) return st;
    if (m != FN_RAD) {
        abn k = K(unit_of(m)->from_rad);
        if ((st = abn_mul(&th, &th, &k)) != ABN_OK) return st;
    }
    *r = rr;
    *theta = th;
    return ABN_OK;
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
    if (abn_is_zero(&rd.a)) {
        v = use_cos ? *r : abn_zero();              /* cos 0 = 1, sin 0 = 0 exactly */
    } else if (!use_cos && u->sixth_of_half && (v = K(u->sixth_of_half), abn_cmp(&rd.a, &v) == 0)) {
        abn half = K("0.5");
        if ((st = abn_mul(&v, r, &half)) != ABN_OK) return st;     /* r sin 30 = r / 2 */
    } else if ((st = trig_exactish(&v, &rd.a, u, use_cos, r)) == ABN_DOMAIN) {
        abn f;      /* below 1E-17 of a unit */
        st = use_cos ? cos_reduced(&f, &rd.a, u) : sin_reduced(&f, &rd.a, u);
        if (st != ABN_OK) return st;
        if ((st = abn_mul(&v, &f, r)) != ABN_OK) return st;
    } else if (st != ABN_OK) {
        return st;
    }
    *out = signed_(&v, sign);
    return ABN_OK;
}

abn_status fn_torec(abn *x, abn *y, const abn *r, const abn *theta, fn_angle m)
{
    abn c, s;
    abn_status st;
    if (m == FN_RAD) {
        if ((st = abn_cos(&c, theta)) != ABN_OK) return st;
        if ((st = abn_sin(&s, theta)) != ABN_OK) return st;
        if ((st = abn_mul(&c, &c, r)) != ABN_OK) return st;
        if ((st = abn_mul(&s, &s, r)) != ABN_OK) return st;
    } else {
        if ((st = rect_part(&c, r, theta, m, false)) != ABN_OK) return st;
        if ((st = rect_part(&s, r, theta, m, true)) != ABN_OK) return st;
    }
    *x = c;
    *y = s;
    return ABN_OK;
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
