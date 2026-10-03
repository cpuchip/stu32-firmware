/* frac: the most accurate fraction for a value (see frac.h).
 *
 * The value is taken exactly: |v| = M * 10^e, so its fractional part is F / Q with Q = 10^-e.
 * FR_PRECISE uses continued fractions: the closest fraction with denominator <= c is the last
 * convergent within the bound or the largest semiconvergent (a classical result; see e.g.
 * Khinchin, Continued Fractions, ch. II), and the two are compared exactly. FR_FIXED and
 * FR_FACTORS round F * c / Q to the nearest integer, halves away from zero. */
#include <string.h>

#include "bigint.h"
#include "frac.h"

static uint32_t gcd32(uint32_t a, uint32_t b)
{
    while (b) {
        uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/* |F*d - n*Q| into r and its sign (F*d - n*Q). */
static int err_of(bigint *r, const bigint *F, const bigint *Q, uint32_t n, uint32_t d)
{
    static bigint a, b;
    a = *F;
    b = *Q;
    bi_mul_u32(&a, d);
    bi_mul_u32(&b, n);
    int s = bi_cmp(&a, &b);
    if (s >= 0) bi_sub(r, &a, &b);
    else bi_sub(r, &b, &a);
    return s;
}

/* Is n1/d1 strictly closer to F/Q than n2/d2? Ties: smaller denominator, then larger n/d. */
static bool closer(const bigint *F, const bigint *Q, uint32_t n1, uint32_t d1, uint32_t n2, uint32_t d2)
{
    static bigint e1, e2;
    err_of(&e1, F, Q, n1, d1);
    err_of(&e2, F, Q, n2, d2);
    bi_mul_u32(&e1, d2);       /* |x - n1/d1| = e1 / (Q d1): compare e1 d2 with e2 d1 */
    bi_mul_u32(&e2, d1);
    int c = bi_cmp(&e1, &e2);
    if (c != 0) return c < 0;
    if (d1 != d2) return d1 < d2;
    return (uint64_t)n1 * d2 > (uint64_t)n2 * d1;
}

/* The closest fraction to F/Q (0 <= F < Q) with denominator <= c. */
static void best_bounded(const bigint *F, const bigint *Q, uint32_t c, uint32_t *n, uint32_t *d)
{
    static bigint p, q, r, a;
    uint64_t h2 = 0, h1 = 1, k2 = 1, k1 = 0;   /* convergents h/k, two back and one back */
    bool inexact;
    p = *F;
    q = *Q;
    if (bi_is_zero(&p)) {
        *n = 0;
        *d = 1;
        return;
    }
    for (;;) {
        /* a = floor(p / q), capped above c (any larger a overshoots the bound the same way) */
        uint64_t av;
        bi_div(&a, &p, &q, &inexact);
        av = (a.n > 1 || (a.n == 1 && a.l[0] > c)) ? (uint64_t)c + 1 : (a.n ? a.l[0] : 0);
        uint64_t h = av * h1 + h2, k = av * k1 + k2;
        if (k > c) {
            /* last convergent h1/k1 against the largest semiconvergent within the bound */
            uint64_t t = (c - k2) / k1;
            uint32_t sn = (uint32_t)(t * h1 + h2), sd = (uint32_t)(t * k1 + k2);
            if (t > 0 && closer(F, Q, sn, sd, (uint32_t)h1, (uint32_t)k1)) {
                *n = sn;
                *d = sd;
            } else {
                *n = (uint32_t)h1;
                *d = (uint32_t)k1;
            }
            return;
        }
        /* (p, q) <- (q, p - a q) */
        if (!inexact) {
            *n = (uint32_t)h;
            *d = (uint32_t)k;
            return;
        }
        r = a;
        {
            static bigint aq;
            bi_mul(&aq, &r, &q);
            bi_sub(&r, &p, &aq);
        }
        p = q;
        q = r;
        h2 = h1;
        h1 = h;
        k2 = k1;
        k1 = k;
    }
}

abn_status fr_approx(fr_result *out, const abn *v, uint32_t c, fr_format f)
{
    static bigint M, Q, I, F, tmp;
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead;
    uint32_t n, den;
    bool inexact;
    if (c < 1) c = 1;
    abn_digits(v, &neg, d, &lead);
    out->neg = neg && d[0] != '\0';
    long e = (long)lead - (long)strlen(d) + 1;
    if (lead >= ABN_DIGITS) return ABN_RANGE;          /* more integer digits than the value has */
    if (!bi_from_digits(&M, d, e > 0 ? (int)e : 0)) return ABN_RANGE;
    bi_set(&Q, 1);
    if (e < 0 && !bi_mul_pow10(&Q, (int)-e)) return ABN_RANGE;
    if (!bi_div(&I, &M, &Q, &inexact)) return ABN_RANGE;
    if (!bi_mul(&tmp, &I, &Q)) return ABN_RANGE;
    bi_sub(&F, &M, &tmp);                              /* fraction F / Q in [0, 1) */
    if (f == FR_PRECISE) {
        best_bounded(&F, &Q, c, &n, &den);
    } else {
        /* k = floor((2 F c + Q) / (2 Q)): the nearest integer to F c / Q, halves up */
        static bigint num, q2, k;
        num = F;
        bi_mul_u32(&num, 2 * c);
        bi_add(&num, &num, &Q);
        q2 = Q;
        bi_mul_u32(&q2, 2);
        bi_div(&k, &num, &q2, &inexact);
        n = k.n ? k.l[0] : 0;
        den = c;
        if (f == FR_FACTORS && n != 0) {
            uint32_t g = gcd32(n, den);
            n /= g;
            den /= g;
        }
    }
    /* indicator: sign of x - n/den, x = F / Q */
    {
        static bigint r;
        out->indicator = err_of(&r, &F, &Q, n, den) > 0 ? 1 : (bi_is_zero(&r) ? 0 : -1);
    }
    if (n == den) {                                    /* rounded up to a whole: carry */
        bi_add_u32(&I, 1);
        n = 0;
        if (f != FR_FIXED) den = 1;
    }
    out->n = n;
    out->d = den;
    /* integer part as text */
    {
        int t = 0;
        if (I.n == 0) {
            out->ip[t++] = '0';
        } else {
            for (int i = I.n - 1; i >= 0; i--) {
                char limb[10];
                uint32_t val = I.l[i];
                for (int kk = 8; kk >= 0; kk--) {
                    limb[kk] = (char)('0' + val % 10);
                    val /= 10;
                }
                int s = 0;
                if (i == I.n - 1)
                    while (s < 8 && limb[s] == '0') s++;
                for (; s < 9; s++) out->ip[t++] = limb[s];
            }
        }
        out->ip[t] = '\0';
    }
    if (out->n == 0 && out->ip[0] == '0' && out->ip[1] == '\0' && out->indicator == 0) out->neg = false;
    return ABN_OK;
}

abn_status fr_from_parts(abn *out, bool neg, const char *int_digits, const char *num_digits,
                         const char *den_digits)
{
    static bigint I, N, Dn, t;
    static bigint q, num;
    bool inexact;
    if (!bi_from_digits(&I, int_digits, 0) || !bi_from_digits(&N, num_digits, 0) ||
        !bi_from_digits(&Dn, den_digits, 0))
        return ABN_RANGE;
    if (bi_is_zero(&Dn)) return ABN_DOMAIN;
    if (!bi_mul(&t, &I, &Dn) || !bi_add(&num, &t, &N)) return ABN_RANGE;   /* I*D + N */
    if (bi_is_zero(&num)) {
        *out = abn_zero();
        return ABN_OK;
    }
    /* scale so the quotient carries at least 45 digits, then round once */
    int extra = 45 + (bi_ndigits(&Dn) > bi_ndigits(&num) ? bi_ndigits(&Dn) - bi_ndigits(&num) : 0);
    if (!bi_mul_pow10(&num, extra) || !bi_div(&q, &num, &Dn, &inexact)) return ABN_RANGE;
    return bi_to_abn(out, &q, neg, -extra, inexact);
}

abn_status fr_value(abn *out, const fr_result *r)
{
    char num[12], den[12];
    int k = 0;
    uint32_t v = r->n;
    char tmp[12];
    do {
        tmp[k++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    for (int i = 0; i < k; i++) num[i] = tmp[k - 1 - i];
    num[k] = '\0';
    k = 0;
    v = r->d ? r->d : 1;
    do {
        tmp[k++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    for (int i = 0; i < k; i++) den[i] = tmp[k - 1 - i];
    den[k] = '\0';
    return fr_from_parts(out, r->neg, r->ip, num, den);
}
