/* fn: the calculator's functions of one and two numbers (unit 003).
 *
 * The engine (abn.h) computes; this file adds the calculator's rules: exact decimal answers
 * where the answer is exact, angles in degrees and grads reduced exactly in decimal before any
 * conversion to radians, and domain errors instead of complex or infinite results. Rules from
 * the user's guide are cited "UG p.N"; rules this project set are "ours, unit 003".
 */
#include <string.h>

#include "fn.h"

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
    const char *sixth_of_half;      /* 30 degrees, where sin is exactly 1/2; NULL in grads */
} angle_unit;

static const angle_unit DEG_UNIT = {"360", "180", "90", "45", "0.01745329251994329576923690768488613",
                                    "57.29577951308232087679815481410517", "30"};
static const angle_unit GRAD_UNIT = {"400", "200", "100", "50", "0.01570796326794896619231321691639751",
                                     "63.66197723675813430755350534900574", NULL};

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

/* sin or cos of a in [0, eighth turn], exact at 0 and (degrees) at 30. */
static abn_status sin_reduced(abn *out, const abn *a, const angle_unit *u)
{
    abn rad, k;
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
    k = K(u->to_rad);
    if ((st = abn_mul(&rad, a, &k)) != ABN_OK) return st;
    return abn_sin(out, &rad);
}

static abn_status cos_reduced(abn *out, const abn *a, const angle_unit *u)
{
    abn rad, k;
    abn_status st;
    if (abn_is_zero(a)) {
        *out = one();
        return ABN_OK;
    }
    k = K(u->to_rad);
    if ((st = abn_mul(&rad, a, &k)) != ABN_OK) return st;
    return abn_cos(out, &rad);
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

/* tan of b in [0, eighth turn] through the radian engine. */
static abn_status tan_small(abn *out, const abn *b, const angle_unit *u)
{
    abn rad, k = K(u->to_rad);
    abn_status st;
    if (abn_is_zero(b)) {
        *out = abn_zero();
        return ABN_OK;
    }
    if ((st = abn_mul(&rad, b, &k)) != ABN_OK) return st;
    return abn_tan(out, &rad);
}

/* cot of a small reduced angle b in (0, eighth / 2], without a reciprocal of tan: near an odd
 * quarter turn 1/tan magnifies the engine's error up to tenfold (measured 11.8 units). Instead
 * cot(theta) = 1/theta - sum c_k theta^(2k-1), c_k = 2^(2k) |B_2k| / (2k)!, where 1/theta is the
 * single correctly rounded quotient (180/pi) / b of the exact decimal b, and the series, at most
 * 5% of the result, needs only modest relative accuracy. Coefficients from mpmath at 80 digits;
 * 22 terms leave 1.5E-41 at 22.5 degrees. */
static const char *const COT_C[] = {
    "0.3333333333333333333333333333333333",  "0.02222222222222222222222222222222222",
    "0.002116402116402116402116402116402116", "0.0002116402116402116402116402116402116",
    "0.00002137779915557693335471113248891027", "0.000002164404280806397208513610630012746",
    "0.0000002192594785187377779970372562965156", "0.00000002221460878997967907605836851609235",
    "0.000000002250784651680899285420842798396376", "0.0000000002280515120459218286586381125671629",
    "0.00000000002310643259900262409653254664812916", "0.000000000002341170681982488395920944453124272",
    "0.0000000000002372101740023365429502848203011582", "0.00000000000002403441533330770617909188752768355",
    "0.000000000000002435195402918336873112182253526452", "0.0000000000000002467368804517207470589960322677562",
    "0.00000000000000002499967277122080897989542788241656", "0.000000000000000002532996435740634831517870159070956",
    "0.0000000000000000002566461970282628661122487694625954", "0.00000000000000000002600369646013727358856256852505342",
    "0.000000000000000000002634725304415380134174108072608297", "0.0000000000000000000002669534864157394953454888316152288",
};

static abn_status cot_small(abn *out, const abn *b, const angle_unit *u)
{
    abn inv, theta, x2, sum, corr, k;
    abn_status st;
    k = K(u->from_rad);
    if ((st = abn_div(&inv, &k, b)) != ABN_OK) return st;
    k = K(u->to_rad);
    if ((st = abn_mul(&theta, b, &k)) != ABN_OK) return st;
    if ((st = abn_mul(&x2, &theta, &theta)) != ABN_OK) {
        if (st != ABN_RANGE) return st;
        x2 = abn_zero();                    /* theta^2 below the range: no correction */
    }
    int n = (int)(sizeof COT_C / sizeof COT_C[0]);
    sum = K(COT_C[n - 1]);
    for (int i = n - 2; i >= 0; i--) {
        abn c = K(COT_C[i]);
        if ((st = abn_fma(&sum, &sum, &x2, &c)) != ABN_OK) return st;
    }
    if (abn_mul(&corr, &sum, &theta) != ABN_OK) corr = abn_zero();  /* underflow: negligible */
    return abn_sub(out, &inv, &corr);
}

/* tan (or, with cot, 1/tan) of a reduced angle a in [0, eighth turn]. Near the eighth turn the
 * value is near 1, where the unit in the 34th digit changes tenfold, so it is built from the
 * small angle b = eighth - a: tan(45 - b) = 1 - 2t/(1 + t) and cot(45 - b) = 1 + 2t/(1 - t)
 * with t = tan b. Below that, cot comes from its series (cot_small). */
static abn_status tan_reduced(abn *out, const abn *a, const angle_unit *u, bool cot)
{
    abn eighth = K(u->eighth), quarter_of_eighth = K(u->eighth), two = abn_from_int(2), w = one();
    abn t, num, den, q, b;
    abn_status st;
    if (abn_is_zero(a)) {
        if (cot) return ABN_DOMAIN;     /* tan of an odd quarter turn (ours, unit 003) */
        *out = abn_zero();
        return ABN_OK;
    }
    if (abn_cmp(a, &eighth) == 0) {     /* tan 45 = 1 exactly */
        *out = one();
        return ABN_OK;
    }
    {
        abn halfk = K("0.5");
        if ((st = abn_mul(&quarter_of_eighth, &eighth, &halfk)) != ABN_OK) return st;
    }
    if (abn_cmp(a, &quarter_of_eighth) > 0) {
        if ((st = abn_sub(&b, &eighth, a)) != ABN_OK) return st;          /* exact */
        if ((st = tan_small(&t, &b, u)) != ABN_OK) return st;
        if ((st = abn_mul(&num, &two, &t)) != ABN_OK) return st;
        st = cot ? abn_sub(&den, &w, &t) : abn_add(&den, &w, &t);
        if (st != ABN_OK) return st;
        if ((st = abn_div(&q, &num, &den)) != ABN_OK) return st;
        return cot ? abn_add(out, &w, &q) : abn_sub(out, &w, &q);
    }
    if (cot) return cot_small(out, a, u);
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
