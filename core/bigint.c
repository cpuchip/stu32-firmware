/* bigint: exact non-negative integers (see bigint.h). Schoolbook arithmetic in base 10^9
 * (Knuth, TAOCP vol. 2, 4.3.1). */
#include <string.h>

#include "bigint.h"

static void trim(bigint *a)
{
    while (a->n > 0 && a->l[a->n - 1] == 0) a->n--;
}

void bi_set(bigint *a, uint32_t v)
{
    a->n = 0;
    while (v) {
        a->l[a->n++] = v % BI_BASE;
        v /= BI_BASE;
    }
}

bool bi_is_zero(const bigint *a)
{
    return a->n == 0;
}

int bi_cmp(const bigint *a, const bigint *b)
{
    if (a->n != b->n) return a->n > b->n ? 1 : -1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->l[i] != b->l[i]) return a->l[i] > b->l[i] ? 1 : -1;
    return 0;
}

int bi_ndigits(const bigint *a)
{
    if (a->n == 0) return 0;
    int d = 9 * (a->n - 1);
    for (uint32_t top = a->l[a->n - 1]; top; top /= 10) d++;
    return d;
}

bool bi_mul_u32(bigint *a, uint32_t m)
{
    uint64_t carry = 0;
    for (int i = 0; i < a->n; i++) {
        uint64_t t = (uint64_t)a->l[i] * m + carry;
        a->l[i] = (uint32_t)(t % BI_BASE);
        carry = t / BI_BASE;
    }
    while (carry) {
        if (a->n >= BI_LIMBS) return false;
        a->l[a->n++] = (uint32_t)(carry % BI_BASE);
        carry /= BI_BASE;
    }
    trim(a);
    return true;
}

bool bi_mul(bigint *r, const bigint *a, const bigint *b)
{
    if (a->n == 0 || b->n == 0) {
        r->n = 0;
        return true;
    }
    if (a->n + b->n > BI_LIMBS) return false;
    memset(r->l, 0, sizeof(uint32_t) * (size_t)(a->n + b->n));
    for (int i = 0; i < a->n; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < b->n; j++) {
            uint64_t t = (uint64_t)r->l[i + j] + (uint64_t)a->l[i] * b->l[j] + carry;
            r->l[i + j] = (uint32_t)(t % BI_BASE);
            carry = t / BI_BASE;
        }
        r->l[i + b->n] = (uint32_t)carry;
    }
    r->n = a->n + b->n;
    trim(r);
    return true;
}

bool bi_add(bigint *r, const bigint *a, const bigint *b)
{
    int n = a->n > b->n ? a->n : b->n;
    uint32_t carry = 0;
    for (int i = 0; i < n; i++) {
        uint32_t t = (i < a->n ? a->l[i] : 0) + (i < b->n ? b->l[i] : 0) + carry;
        carry = t >= BI_BASE;
        r->l[i] = carry ? t - BI_BASE : t;
    }
    r->n = n;
    if (carry) {
        if (n >= BI_LIMBS) return false;
        r->l[r->n++] = 1;
    }
    return true;
}

bool bi_add_u32(bigint *a, uint32_t v)
{
    bigint b;
    bi_set(&b, v);
    return bi_add(a, a, &b);
}

void bi_sub(bigint *r, const bigint *a, const bigint *b)
{
    int64_t borrow = 0;
    for (int i = 0; i < a->n; i++) {
        int64_t t = (int64_t)a->l[i] - (i < b->n ? b->l[i] : 0) - borrow;
        borrow = t < 0;
        r->l[i] = (uint32_t)(borrow ? t + BI_BASE : t);
    }
    r->n = a->n;
    trim(r);
}

bool bi_sub_u32(bigint *a, uint32_t v)
{
    bigint b;
    bi_set(&b, v);
    if (bi_cmp(a, &b) < 0) return false;
    bi_sub(a, a, &b);
    return true;
}

bool bi_mul_pow10(bigint *a, int k)
{
    if (a->n == 0) return true;
    int limbs = k / 9;
    if (a->n + limbs > BI_LIMBS) return false;
    memmove(a->l + limbs, a->l, sizeof(uint32_t) * (size_t)a->n);
    memset(a->l, 0, sizeof(uint32_t) * (size_t)limbs);
    a->n += limbs;
    for (int i = 0; i < k % 9; i++)
        if (!bi_mul_u32(a, 10)) return false;
    return true;
}

uint32_t bi_divmod_u32(bigint *a, uint32_t d)
{
    uint64_t rem = 0;
    for (int i = a->n - 1; i >= 0; i--) {
        uint64_t t = rem * BI_BASE + a->l[i];
        a->l[i] = (uint32_t)(t / d);
        rem = t % d;
    }
    trim(a);
    return (uint32_t)rem;
}

void bi_div_pow10(bigint *a, int k)
{
    int limbs = k / 9;
    if (limbs >= a->n) {
        a->n = 0;
        return;
    }
    memmove(a->l, a->l + limbs, sizeof(uint32_t) * (size_t)(a->n - limbs));
    a->n -= limbs;
    uint32_t d = 1;
    for (int i = 0; i < k % 9; i++) d *= 10;
    if (d > 1) bi_divmod_u32(a, d);
}

/* Long division one decimal digit at a time: each quotient digit is found by subtracting the
 * divisor at most nine times. Slow for big quotients, but the callers ask for about 45 digits. */
bool bi_div(bigint *q, const bigint *n, const bigint *d, bool *inexact)
{
    static bigint r;
    q->n = 0;
    r.n = 0;
    for (int i = n->n - 1; i >= 0; i--) {
        uint32_t limb = n->l[i];
        uint32_t pw = 100000000u;
        for (int k = 0; k < 9; k++, pw /= 10) {
            uint32_t digit = (limb / pw) % 10;
            if (!bi_mul_u32(&r, 10) || !bi_add_u32(&r, digit)) return false;
            uint32_t qd = 0;
            while (bi_cmp(&r, d) >= 0) {
                bi_sub(&r, &r, d);
                qd++;
            }
            if (!bi_mul_u32(q, 10) || !bi_add_u32(q, qd)) return false;
        }
    }
    *inexact = !bi_is_zero(&r);
    return true;
}

bool bi_from_digits(bigint *a, const char *digits, int zeros)
{
    a->n = 0;
    for (const char *p = digits; *p; p++) {
        if (!bi_mul_u32(a, 10) || !bi_add_u32(a, (uint32_t)(*p - '0'))) return false;
    }
    return bi_mul_pow10(a, zeros);
}

bool bi_from_abn(bigint *a, const abn *x)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead, n;
    abn_digits(x, &neg, d, &lead);
    n = (int)strlen(d);
    if (n == 0) {
        a->n = 0;
        return true;
    }
    if (n > lead + 1 || lead + 1 > 9 * BI_LIMBS) return false;   /* not an integer, or too big */
    return bi_from_digits(a, d, lead + 1 - n);
}

/* Appends the decimal text of v (>= 0 or < 0) to s at t; returns the new end. */
static int put_long(char *s, int t, long v)
{
    char buf[24];
    int n = 0;
    unsigned long u = v < 0 ? (unsigned long)(-v) : (unsigned long)v;
    if (v < 0) s[t++] = '-';
    do {
        buf[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (n) s[t++] = buf[--n];
    return t;
}

abn_status bi_to_abn(abn *out, const bigint *a, bool neg, long e10, bool sticky)
{
    /* Keep 39 digits and a 40th that is 1 if anything nonzero lies below them, so the engine's
       half-to-even rounding to 34 digits sees exactly where the value falls. */
    enum { KEEP = 39 };
    char text[KEEP + 40];
    int t = 0, total = bi_ndigits(a), kept = 0;
    bool below = sticky;
    if (total == 0) {
        *out = abn_zero();
        return ABN_OK;
    }
    if (neg) text[t++] = '-';
    for (int i = a->n - 1; i >= 0; i--) {
        char limb[10];
        uint32_t v = a->l[i];
        for (int k = 8; k >= 0; k--) {
            limb[k] = (char)('0' + v % 10);
            v /= 10;
        }
        int start = 0;
        if (i == a->n - 1)
            while (start < 8 && limb[start] == '0') start++;
        for (int k = start; k < 9; k++) {
            if (kept < KEEP) {
                text[t++] = limb[k];
                kept++;
            } else if (limb[k] != '0') {
                below = true;
            }
        }
    }
    int real = kept;                        /* digits of the value actually written */
    int appended = 0;
    if (total > KEEP || below) {
        text[t++] = below ? '1' : '0';      /* stands for everything below the kept digits */
        appended = 1;
    }
    text[t++] = 'E';
    t = put_long(text, t, e10 + (long)(total - real) - appended);
    text[t] = '\0';
    return abn_from_text(out, text);
}
