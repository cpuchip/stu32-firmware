/* dref: our decimal reference engine (test aid; see dref.h).
 *
 * A value is a sign, a coefficient of at most DREF_DIGITS decimal digits and a power of
 * ten. Every operation computes enough digits to know the exact result's position
 * relative to the rounding point, then rounds once, half to even, to DREF_DIGITS
 * significant digits. Trailing zeros are stripped, so each value has one representation.
 *
 * The algorithms are the schoolbook ones (aligned addition with a sticky digit, digit-by-
 * digit multiplication and long division); see Knuth, TAOCP vol. 2, 4.3.1, and
 * IEEE 754-2008 for half-to-even rounding.
 */
#include <string.h>

#include "dref.h"

#define WORK (2 * DREF_DIGITS + 8)   /* digits in a working buffer */

dref dref_zero(void)
{
    dref z;
    memset(&z, 0, sizeof z);
    return z;
}

bool dref_is_zero(const dref *a)
{
    return a->len == 0;
}

/* Rounds the little-endian digits w[0..n) times 10^exp to DREF_DIGITS significant
 * digits, half to even. `sticky` says nonzero digits existed below w[0]; it matters only
 * when n exceeds DREF_DIGITS, which every caller that sets it guarantees. */
static dref_status finish(dref *out, int neg, uint8_t *w, int n, long exp, int sticky)
{
    while (n > 0 && w[n - 1] == 0) n--;
    if (n == 0) {
        *out = dref_zero();
        return DREF_OK;
    }
    if (n > DREF_DIGITS) {
        int drop = n - DREF_DIGITS;
        int guard = w[drop - 1];
        int rest = sticky;
        for (int i = 0; i < drop - 1 && !rest; i++) rest = (w[i] != 0);
        int up = guard > 5 || (guard == 5 && (rest || (w[drop] & 1)));
        memmove(w, w + drop, DREF_DIGITS);
        n = DREF_DIGITS;
        exp += drop;
        if (up) {
            int i = 0;
            while (i < n && w[i] == 9) w[i++] = 0;
            if (i < n) {
                w[i]++;
            } else {            /* all nines rounded up: the value is a power of ten */
                w[0] = 1;
                n = 1;
                exp += DREF_DIGITS;
            }
        }
    }
    int lo = 0;
    while (w[lo] == 0) lo++;
    n -= lo;
    exp += lo;
    long lead = exp + n - 1;
    if (lead > DREF_EXP_MAX || lead < -DREF_EXP_MAX) return DREF_RANGE;
    dref r = dref_zero();
    r.neg = (uint8_t)(neg ? 1 : 0);
    r.len = (uint8_t)n;
    r.exp = (int32_t)exp;
    memcpy(r.d, w + lo, (size_t)n);
    *out = r;
    return DREF_OK;
}

/* Compares magnitudes. */
static int cmp_mag(const dref *a, const dref *b)
{
    if (a->len == 0 || b->len == 0) return (a->len != 0) - (b->len != 0);
    long ta = (long)a->exp + a->len, tb = (long)b->exp + b->len;
    if (ta != tb) return ta > tb ? 1 : -1;
    int ia = a->len - 1, ib = b->len - 1;
    for (; ia >= 0 && ib >= 0; ia--, ib--)
        if (a->d[ia] != b->d[ib]) return a->d[ia] > b->d[ib] ? 1 : -1;
    /* Equal so far; the longer one still holds a nonzero digit (no trailing zeros). */
    return (ia >= 0) - (ib >= 0);
}

int dref_cmp(const dref *a, const dref *b)
{
    int sa = a->len ? (a->neg ? -1 : 1) : 0;
    int sb = b->len ? (b->neg ? -1 : 1) : 0;
    if (sa != sb) return sa < sb ? -1 : 1;
    if (sa == 0) return 0;
    int m = cmp_mag(a, b);
    return sa > 0 ? m : -m;
}

dref dref_neg(const dref *a)
{
    dref r = *a;
    if (r.len) r.neg ^= 1;
    return r;
}

/* a + (-1)^flip * b. */
static dref_status add_signed(dref *out, const dref *a, const dref *b, int flip)
{
    int bneg = b->neg ^ flip;
    if (b->len == 0) {
        *out = *a;
        return DREF_OK;
    }
    if (a->len == 0) {
        *out = *b;
        out->neg = (uint8_t)bneg;
        return DREF_OK;
    }
    /* x is the operand of larger magnitude; the result takes its sign. */
    const dref *x = a, *y = b;
    int xneg = a->neg, yneg = bneg;
    int c = cmp_mag(a, b);
    if (c < 0) {
        x = b;
        y = a;
        xneg = bneg;
        yneg = a->neg;
    }
    if (c == 0 && xneg != yneg) {
        *out = dref_zero();
        return DREF_OK;
    }
    /* Keep digits from x's leading digit down to DREF_DIGITS + 3 places; any of y's digits
     * below that become a single sticky unit one place lower. That unit cannot move the
     * result across a rounding boundary, which lie on multiples of the kept places. */
    long top = (long)x->exp + x->len;
    long floor_ = top - (DREF_DIGITS + 3);
    long base = x->exp < y->exp ? x->exp : y->exp;
    int sticky = 0;
    if (y->exp < floor_) {
        sticky = 1;
        base = floor_ - 1;
    }
    uint8_t X[WORK], Y[WORK], R[WORK];
    memset(X, 0, sizeof X);
    memset(Y, 0, sizeof Y);
    int n = (int)(top - base);
    for (int i = 0; i < x->len; i++) X[x->exp - base + i] = x->d[i];
    for (int i = 0; i < y->len; i++) {
        long pos = (long)y->exp + i - base;
        if (!sticky || pos >= 1) Y[pos] = y->d[i];
    }
    if (sticky) Y[0] = 1;
    if (xneg == yneg) {
        int carry = 0;
        for (int i = 0; i < n; i++) {
            int v = X[i] + Y[i] + carry;
            R[i] = (uint8_t)(v % 10);
            carry = v / 10;
        }
        R[n++] = (uint8_t)carry;
    } else {
        int borrow = 0;
        for (int i = 0; i < n; i++) {
            int v = X[i] - Y[i] - borrow;
            borrow = v < 0;
            R[i] = (uint8_t)(v + (borrow ? 10 : 0));
        }
    }
    return finish(out, xneg, R, n, base, 0);
}

dref_status dref_add(dref *out, const dref *a, const dref *b)
{
    return add_signed(out, a, b, 0);
}

dref_status dref_sub(dref *out, const dref *a, const dref *b)
{
    return add_signed(out, a, b, 1);
}

dref_status dref_mul(dref *out, const dref *a, const dref *b)
{
    if (a->len == 0 || b->len == 0) {
        *out = dref_zero();
        return DREF_OK;
    }
    unsigned acc[2 * DREF_DIGITS];
    uint8_t w[WORK];
    int n = a->len + b->len;
    memset(acc, 0, sizeof acc);
    for (int i = 0; i < a->len; i++)
        for (int j = 0; j < b->len; j++) acc[i + j] += (unsigned)a->d[i] * b->d[j];
    unsigned carry = 0;
    for (int k = 0; k < n; k++) {
        unsigned v = acc[k] + carry;
        w[k] = (uint8_t)(v % 10);
        carry = v / 10;
    }
    return finish(out, a->neg ^ b->neg, w, n, (long)a->exp + b->exp, 0);
}

/* Little-endian digit strings without leading zeros: compare, and subtract in place. */
static int cmp_digits(const uint8_t *r, int rn, const uint8_t *b, int bn)
{
    if (rn != bn) return rn > bn ? 1 : -1;
    for (int i = rn - 1; i >= 0; i--)
        if (r[i] != b[i]) return r[i] > b[i] ? 1 : -1;
    return 0;
}

static void sub_digits(uint8_t *r, int *rn, const uint8_t *b, int bn)
{
    int borrow = 0;
    for (int i = 0; i < *rn; i++) {
        int v = r[i] - (i < bn ? b[i] : 0) - borrow;
        borrow = v < 0;
        r[i] = (uint8_t)(v + (borrow ? 10 : 0));
    }
    while (*rn > 0 && r[*rn - 1] == 0) (*rn)--;
}

dref_status dref_div(dref *out, const dref *a, const dref *b)
{
    if (b->len == 0) return DREF_DIV_ZERO;
    if (a->len == 0) {
        *out = dref_zero();
        return DREF_OK;
    }
    /* Scale the dividend by 10^k so the integer quotient has at least DREF_DIGITS + 2
     * digits; the remainder then only decides the sticky bit. */
    int k = DREF_DIGITS + 2 + b->len - a->len;
    int nn = a->len + k;
    uint8_t rem[DREF_DIGITS + 2];
    uint8_t q[WORK], w[WORK];
    int rn = 0;
    for (int i = 0; i < nn; i++) {
        int digit = i < a->len ? a->d[a->len - 1 - i] : 0;
        memmove(rem + 1, rem, (size_t)rn);
        rem[0] = (uint8_t)digit;
        rn++;
        while (rn > 0 && rem[rn - 1] == 0) rn--;
        int qd = 0;
        while (cmp_digits(rem, rn, b->d, b->len) >= 0) {
            sub_digits(rem, &rn, b->d, b->len);
            qd++;
        }
        q[i] = (uint8_t)qd;
    }
    for (int i = 0; i < nn; i++) w[i] = q[nn - 1 - i];
    return finish(out, a->neg ^ b->neg, w, nn, (long)a->exp - b->exp - k, rn > 0);
}

dref_status dref_from_text(dref *out, const char *s)
{
    int neg = 0, point = 0, any = 0, sticky = 0, mn = 0;
    uint8_t m[DREF_DIGITS + 2];  /* significant digits kept, most significant first */
    long shift = 0, e = 0;
    if (*s == '-' || *s == '+') neg = (*s++ == '-');
    for (; *s && *s != 'E' && *s != 'e'; s++) {
        if (*s == '.') {
            if (point) return DREF_BAD_TEXT;
            point = 1;
            continue;
        }
        if (*s < '0' || *s > '9') return DREF_BAD_TEXT;
        int dg = *s - '0';
        any = 1;
        if (mn == 0 && dg == 0) {           /* a leading zero */
            if (point) shift--;
        } else if (mn < (int)sizeof m) {
            m[mn++] = (uint8_t)dg;
            if (point) shift--;
        } else {                            /* beyond what rounding can use */
            if (dg) sticky = 1;
            if (!point) shift++;
        }
    }
    if (!any) return DREF_BAD_TEXT;
    if (*s == 'E' || *s == 'e') {
        int eneg = 0, ed = 0;
        s++;
        if (*s == '-' || *s == '+') eneg = (*s++ == '-');
        for (; *s >= '0' && *s <= '9'; s++, ed++)
            if (e < 100000000L) e = e * 10 + (*s - '0');
        if (!ed) return DREF_BAD_TEXT;
        if (eneg) e = -e;
    }
    if (*s) return DREF_BAD_TEXT;
    if (mn == 0) {
        *out = dref_zero();
        return DREF_OK;
    }
    uint8_t w[WORK];
    for (int i = 0; i < mn; i++) w[i] = m[mn - 1 - i];
    return finish(out, neg, w, mn, e + shift, sticky);
}

int dref_to_text(const dref *a, char *buf, int size)
{
    char t[DREF_TEXT_MAX];
    int n = 0;
    if (a->len == 0) {
        t[n++] = '0';
    } else {
        long lead = (long)a->exp + a->len - 1;
        if (a->neg) t[n++] = '-';
        if (lead >= -6 && lead < DREF_DIGITS) {      /* plain: 149.0987, 0.000062 */
            if (lead < 0) {
                t[n++] = '0';
                t[n++] = '.';
                for (long p = -1; p > lead; p--) t[n++] = '0';
            }
            for (long p = lead; p >= 0 || p >= a->exp; p--) {
                if (p == -1 && lead >= 0) t[n++] = '.';
                long i = p - a->exp;
                t[n++] = (char)('0' + ((i >= 0 && i < a->len) ? a->d[i] : 0));
            }
        } else {                                    /* scientific: 6.6261E-34 */
            char ex[12];
            int en = 0;
            long e = lead < 0 ? -lead : lead;
            t[n++] = (char)('0' + a->d[a->len - 1]);
            if (a->len > 1) {
                t[n++] = '.';
                for (int i = a->len - 2; i >= 0; i--) t[n++] = (char)('0' + a->d[i]);
            }
            t[n++] = 'E';
            if (lead < 0) t[n++] = '-';
            do {
                ex[en++] = (char)('0' + e % 10);
                e /= 10;
            } while (e);
            while (en) t[n++] = ex[--en];
        }
    }
    if (size > 0) {
        int c = n < size - 1 ? n : size - 1;
        memcpy(buf, t, (size_t)c);
        buf[c] = '\0';
    }
    return n;
}
