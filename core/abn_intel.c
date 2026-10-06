/* abn_intel: the number interface (abn.h) on IEEE 754 decimal128 from the Intel Decimal
 * Floating-Point Math Library (BSD-3-Clause; see third_party/).
 *
 * The only file in the core that sees the library. Every operation rounds half to even
 * (the library's default mode) and reports through the status flags of that one call.
 * Results outside the normal decimal128 range, and negative zeros, never leave this file.
 */
#include <string.h>

#include "bid_conf.h"
#include "bid_functions.h"

#include "abn.h"

_Static_assert(sizeof(abn) == sizeof(BID_UINT128), "abn holds one decimal128");

static const _IDEC_round RND = BID_ROUNDING_TO_NEAREST;

static BID_UINT128 to_bid(const abn *a)
{
    BID_UINT128 b;
    memcpy(&b, a, sizeof b);
    return b;
}

static abn from_bid(BID_UINT128 b)
{
    abn a;
    memcpy(&a, &b, sizeof a);
    return a;
}

abn abn_zero(void)
{
    abn z;
    memset(&z, 0, sizeof z);
    return z;
}

bool abn_is_zero(const abn *a)
{
    return bid128_isZero(to_bid(a)) != 0;
}

int abn_cmp(const abn *a, const abn *b)
{
    _IDEC_flags fl = 0;
    BID_UINT128 x = to_bid(a), y = to_bid(b);
    if (bid128_quiet_less(x, y, &fl)) return -1;
    if (bid128_quiet_greater(x, y, &fl)) return 1;
    return 0;
}

abn abn_neg(const abn *a)
{
    if (abn_is_zero(a)) return abn_zero();
    return from_bid(bid128_negate(to_bid(a)));
}

/* Accepts the result of one library call when it is in the normal range and finite; otherwise says
 * which way it left (unit 026): ABN_OVERFLOW beyond the largest finite value, ABN_UNDERFLOW a nonzero
 * result below the smallest normal one (or rounded to zero from there), ABN_DOMAIN not a number. What
 * each means is the caller's. A zero result (of either sign, any exponent) becomes the one zero. */
static bool ovf_neg;        /* the sign of the last overflow (abn_overflow_negative) */

static abn_status accept(abn *out, BID_UINT128 r, _IDEC_flags fl)
{
    if ((fl & BID_INVALID_EXCEPTION) || bid128_isNaN(r)) return ABN_DOMAIN;
    if ((fl & BID_OVERFLOW_EXCEPTION) || bid128_isInf(r)) {
        ovf_neg = bid128_isSigned(r);
        return ABN_OVERFLOW;
    }
    /* the flag alone is not an underflow: a result tiny before rounding may round to the smallest
       normal number, which is in range (sin 1E-6143 is 1E-6143) */
    if (bid128_isSubnormal(r) || ((fl & BID_UNDERFLOW_EXCEPTION) && bid128_isZero(r))) return ABN_UNDERFLOW;
    if (bid128_isZero(r)) {
        *out = abn_zero();
        return ABN_OK;
    }
    *out = from_bid(r);
    return ABN_OK;
}

abn_status abn_add(abn *out, const abn *a, const abn *b)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_add(to_bid(a), to_bid(b), RND, &fl);
    return accept(out, r, fl);
}

abn_status abn_sub(abn *out, const abn *a, const abn *b)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_sub(to_bid(a), to_bid(b), RND, &fl);
    return accept(out, r, fl);
}

abn_status abn_mul(abn *out, const abn *a, const abn *b)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_mul(to_bid(a), to_bid(b), RND, &fl);
    return accept(out, r, fl);
}

abn_status abn_div(abn *out, const abn *a, const abn *b)
{
    _IDEC_flags fl = 0;
    if (abn_is_zero(b)) return ABN_DIV_ZERO;
    BID_UINT128 r = bid128_div(to_bid(a), to_bid(b), RND, &fl);
    return accept(out, r, fl);
}

/* The library also reads words such as "inf" and "nan"; only plain numbers pass here. */
static bool plain_number(const char *s)
{
    bool digit = false, point = false;
    if (*s == '-' || *s == '+') s++;
    for (; *s && *s != 'E' && *s != 'e'; s++) {
        if (*s == '.' && !point) point = true;
        else if (*s >= '0' && *s <= '9') digit = true;
        else return false;
    }
    if (!digit) return false;
    if (!*s) return true;
    s++;
    if (*s == '-' || *s == '+') s++;
    if (!*s) return false;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return false;
    return true;
}

bool abn_overflow_negative(void)
{
    return ovf_neg;
}

abn abn_largest(void)
{
    abn m;
    (void)abn_from_text(&m, "9.999999999999999999999999999999999E6144");
    return m;
}

abn_status abn_from_text(abn *out, const char *s)
{
    _IDEC_flags fl = 0;
    char buf[256];
    size_t n = strlen(s);
    if (!plain_number(s) || n >= sizeof buf) return ABN_BAD_TEXT;
    memcpy(buf, s, n + 1);  /* the library takes a non-const pointer */
    BID_UINT128 r = bid128_from_string(buf, RND, &fl);
    return accept(out, r, fl);
}

abn_status abn_from_text_exact(abn *out, const char *s)
{
    _IDEC_flags fl = 0;
    char buf[256];
    size_t n = strlen(s);
    if (!plain_number(s) || n >= sizeof buf) return ABN_BAD_TEXT;
    memcpy(buf, s, n + 1);
    BID_UINT128 r = bid128_from_string(buf, RND, &fl);
    if (bid128_isZero(r) && bid128_isFinite(r) && !(fl & BID_INVALID_EXCEPTION)) {
        *out = from_bid(r);
        return ABN_OK;
    }
    return accept(out, r, fl);
}

int abn_to_text(const abn *a, char *buf, int size)
{
    _IDEC_flags fl = 0;
    char t[128];
    bid128_to_string(t, to_bid(a), &fl);
    int n = (int)strlen(t);
    if (size > 0) {
        int c = n < size - 1 ? n : size - 1;
        memcpy(buf, t, (size_t)c);
        buf[c] = '\0';
    }
    return n;
}

void abn_digits(const abn *a, bool *neg, char *digits, int *lead)
{
    /* The library writes a finite value as [+|-]coefficient E[+|-]exponent with no
     * leading zeros in the coefficient: "+1490987E-4" is 149.0987. */
    _IDEC_flags fl = 0;
    char t[128];
    const char *p = t;
    int n = 0;
    long e = 0;
    bool eneg = false;
    bid128_to_string(t, to_bid(a), &fl);
    *neg = (*p == '-');
    if (*p == '-' || *p == '+') p++;
    while (*p == '0') p++;                  /* only a zero coefficient starts with 0 */
    for (; *p >= '0' && *p <= '9'; p++) {
        if (n < ABN_DIGITS) digits[n++] = *p;
    }
    if (*p == 'E') {
        p++;
        if (*p == '-' || *p == '+') eneg = (*p++ == '-');
        for (; *p >= '0' && *p <= '9'; p++) e = e * 10 + (*p - '0');
    }
    if (eneg) e = -e;
    int len = n;
    while (n > 0 && digits[n - 1] == '0') n--;
    digits[n] = '\0';
    if (n == 0) {
        *neg = false;
        *lead = 0;
        return;
    }
    *lead = (int)(e + len - 1);
}

/* ---- unit 003: exact helpers and functions ---- */

bool abn_is_negative(const abn *a)
{
    return !abn_is_zero(a) && bid128_isSigned(to_bid(a));
}

abn abn_abs(const abn *a)
{
    return abn_is_negative(a) ? abn_neg(a) : *a;
}

abn abn_from_int(int32_t v)
{
    return from_bid(bid128_from_int32(v));
}

bool abn_is_integer(const abn *a)
{
    _IDEC_flags fl = 0;
    BID_UINT128 x = to_bid(a);
    BID_UINT128 r = bid128_round_integral_zero(x, &fl);
    return bid128_quiet_equal(x, r, &fl) != 0;
}

abn_status abn_mul_exact(abn *out, const abn *a, const abn *b)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_mul(to_bid(a), to_bid(b), RND, &fl);
    if (fl & BID_INEXACT_EXCEPTION) return ABN_INEXACT;
    return accept(out, r, fl);
}

abn_status abn_idiv(abn *out, const abn *a, const abn *b)
{
    /* Dividing with rounding toward zero never carries the quotient past an integer, so
       truncating that is the true integer quotient whenever it fits 34 digits. */
    _IDEC_flags fl = 0;
    if (abn_is_zero(b)) return ABN_DIV_ZERO;
    BID_UINT128 q = bid128_div(to_bid(a), to_bid(b), BID_ROUNDING_TO_ZERO, &fl);
    q = bid128_round_integral_zero(q, &fl);
    return accept(out, q, fl & ~BID_INEXACT_EXCEPTION);
}

abn_status abn_fmod(abn *out, const abn *a, const abn *b)
{
    _IDEC_flags fl = 0;
    if (abn_is_zero(b)) return ABN_DIV_ZERO;
    BID_UINT128 r = bid128_fmod(to_bid(a), to_bid(b), &fl);
    if (bid128_isZero(r)) {
        *out = abn_zero();
        return ABN_OK;
    }
    if (bid128_isSubnormal(r)) return ABN_UNDERFLOW;
    if (!bid128_isFinite(r)) return ABN_DOMAIN;
    *out = from_bid(r);
    return ABN_OK;
}

abn_status abn_pow10(abn *out, int32_t n)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_scalbn(bid128_from_int32(1), n, RND, &fl);
    return accept(out, r, fl);
}

abn_status abn_fma(abn *out, const abn *a, const abn *b, const abn *c)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_fma(to_bid(a), to_bid(b), to_bid(c), RND, &fl);
    return accept(out, r, fl);
}

typedef BID_UINT128 (*bid_fn1)(BID_UINT128, _IDEC_round, _IDEC_flags *);

static abn_status call1(abn *out, bid_fn1 f, const abn *a)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = f(to_bid(a), RND, &fl);
    return accept(out, r, fl);
}

/* Compares a with a small integer. */
static int cmp_int(const abn *a, int v)
{
    abn b = abn_from_int(v);
    return abn_cmp(a, &b);
}

abn_status abn_sqrt(abn *out, const abn *a)
{
    if (abn_is_negative(a)) return ABN_DOMAIN;
    return call1(out, bid128_sqrt, a);
}

abn_status abn_exp(abn *out, const abn *a)
{
    return call1(out, bid128_exp, a);
}

abn_status abn_ln(abn *out, const abn *a)
{
    if (abn_is_zero(a) || abn_is_negative(a)) return ABN_DOMAIN;
    return call1(out, bid128_log, a);
}

abn_status abn_log1p(abn *out, const abn *a)
{
    if (cmp_int(a, -1) <= 0) return ABN_DOMAIN;
    return call1(out, bid128_log1p, a);
}

abn_status abn_exp10(abn *out, const abn *a)
{
    return call1(out, bid128_exp10, a);
}

abn_status abn_log10(abn *out, const abn *a)
{
    if (abn_is_zero(a) || abn_is_negative(a)) return ABN_DOMAIN;
    return call1(out, bid128_log10, a);
}

abn_status abn_pow(abn *out, const abn *base, const abn *expo)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_pow(to_bid(base), to_bid(expo), RND, &fl);
    if (fl & BID_ZERO_DIVIDE_EXCEPTION) return ABN_DIV_ZERO;
    if (fl & BID_INVALID_EXCEPTION) return ABN_DOMAIN;
    return accept(out, r, fl);
}

abn_status abn_sin(abn *out, const abn *a) { return call1(out, bid128_sin, a); }
abn_status abn_cos(abn *out, const abn *a) { return call1(out, bid128_cos, a); }
abn_status abn_tan(abn *out, const abn *a) { return call1(out, bid128_tan, a); }

abn_status abn_asin(abn *out, const abn *a)
{
    if (cmp_int(a, 1) > 0 || cmp_int(a, -1) < 0) return ABN_DOMAIN;
    return call1(out, bid128_asin, a);
}

abn_status abn_acos(abn *out, const abn *a)
{
    if (cmp_int(a, 1) > 0 || cmp_int(a, -1) < 0) return ABN_DOMAIN;
    return call1(out, bid128_acos, a);
}

abn_status abn_atan(abn *out, const abn *a) { return call1(out, bid128_atan, a); }
abn_status abn_sinh(abn *out, const abn *a) { return call1(out, bid128_sinh, a); }
abn_status abn_cosh(abn *out, const abn *a) { return call1(out, bid128_cosh, a); }
abn_status abn_tanh(abn *out, const abn *a) { return call1(out, bid128_tanh, a); }
abn_status abn_asinh(abn *out, const abn *a) { return call1(out, bid128_asinh, a); }

abn_status abn_acosh(abn *out, const abn *a)
{
    if (cmp_int(a, 1) < 0) return ABN_DOMAIN;
    return call1(out, bid128_acosh, a);
}

abn_status abn_atanh(abn *out, const abn *a)
{
    if (cmp_int(a, 1) >= 0 || cmp_int(a, -1) <= 0) return ABN_DOMAIN;
    return call1(out, bid128_atanh, a);
}

/* ---- unit 004 ---- */

abn_status abn_trunc(abn *out, const abn *a)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_round_integral_zero(to_bid(a), &fl);
    return accept(out, r, fl & ~BID_INEXACT_EXCEPTION);
}

abn_status abn_floor(abn *out, const abn *a)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_round_integral_negative(to_bid(a), &fl);
    return accept(out, r, fl & ~BID_INEXACT_EXCEPTION);
}

abn_status abn_hypot(abn *out, const abn *x, const abn *y)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_hypot(to_bid(x), to_bid(y), RND, &fl);
    return accept(out, r, fl);
}

abn_status abn_atan2(abn *out, const abn *y, const abn *x)
{
    _IDEC_flags fl = 0;
    BID_UINT128 r = bid128_atan2(to_bid(y), to_bid(x), RND, &fl);
    return accept(out, r, fl);
}
