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

/* Accepts the result of one library call: in range and finite, or ABN_RANGE. A zero
 * result (of either sign, any exponent) becomes the one zero. */
static abn_status accept(abn *out, BID_UINT128 r, _IDEC_flags fl)
{
    if (fl & (BID_OVERFLOW_EXCEPTION | BID_UNDERFLOW_EXCEPTION | BID_INVALID_EXCEPTION))
        return ABN_RANGE;
    if (!bid128_isFinite(r)) return ABN_RANGE;
    if (bid128_isZero(r)) {
        *out = abn_zero();
        return ABN_OK;
    }
    if (bid128_isSubnormal(r)) return ABN_RANGE;
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
    (void)a;
    *neg = false;
    digits[0] = '\0';
    *lead = 0;
}
