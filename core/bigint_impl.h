/* bigint_impl: the bigint algorithms, written once for each capacity. Not a header to include
 * directly: a .c file defines BI_T (the type), BI_CAP (its limbs) and BI_(name) (the function
 * name for that type) and includes this (core/bigint.c, the wide numbers; core/sbig.c, the short
 * ones). Schoolbook arithmetic in base 10^9 (Knuth, TAOCP vol. 2, 4.3.1). */

/* t / 10^9, with t % 10^9 into *r, for any 64-bit t, without a 64-bit division (unit 045): a 32-bit core does one in
 * software (1.1 us on the S3), and every limb step took one. 10^9 = 2^9 1953125, so the quotient is (t >> 9) / 1953125,
 * which is the high bits of (t >> 9) m, m = ceil(2^76 / 1953125): exact for every (t >> 9) < 2^55, since
 * m 1953125 - 2^76 = 799614 <= 2^21 (Granlund and Montgomery, "Division by Invariant Integers using Multiplication",
 * 1994, theorem 4.2). The product's high half from four 32-bit products; the remainder in 32 bits, being below 10^9. */
static inline uint64_t BI_(qr9_)(uint64_t t, uint32_t *r)
{
    const uint64_t m = 0x89705f4136b4a6ull;
    uint64_t u = t >> 9;
    uint32_t ul = (uint32_t)u, uh = (uint32_t)(u >> 32), ml = (uint32_t)m, mh = (uint32_t)(m >> 32);
    uint64_t ll = (uint64_t)ul * ml, lh = (uint64_t)ul * mh, hl = (uint64_t)uh * ml, hh = (uint64_t)uh * mh;
    uint64_t mid = (ll >> 32) + (uint32_t)lh + (uint32_t)hl;
    uint64_t q = (hh + (lh >> 32) + (hl >> 32) + (mid >> 32)) >> 12;
    *r = (uint32_t)t - (uint32_t)q * BI_BASE;
    return q;
}

static void BI_(trim_)(BI_T *a)
{
    while (a->n > 0 && a->l[a->n - 1] == 0) a->n--;
}

void BI_(set)(BI_T *a, uint32_t v)
{
    a->n = 0;
    while (v) {
        a->l[a->n++] = v % BI_BASE;
        v /= BI_BASE;
    }
}

bool BI_(is_zero)(const BI_T *a)
{
    return a->n == 0;
}

int BI_(cmp)(const BI_T *a, const BI_T *b)
{
    if (a->n != b->n) return a->n > b->n ? 1 : -1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->l[i] != b->l[i]) return a->l[i] > b->l[i] ? 1 : -1;
    return 0;
}

int BI_(ndigits)(const BI_T *a)
{
    if (a->n == 0) return 0;
    int d = 9 * (a->n - 1);
    for (uint32_t top = a->l[a->n - 1]; top; top /= 10) d++;
    return d;
}

bool BI_(mul_u32)(BI_T *a, uint32_t m)
{
    uint64_t carry = 0;
    for (int i = 0; i < a->n; i++) carry = BI_(qr9_)((uint64_t)a->l[i] * m + carry, &a->l[i]);
    while (carry) {
        if (a->n >= BI_CAP) return false;
        carry = BI_(qr9_)(carry, &a->l[a->n]);
        a->n++;
    }
    BI_(trim_)(a);
    return true;
}

bool BI_(mul)(BI_T *r, const BI_T *a, const BI_T *b)
{
    if (a->n == 0 || b->n == 0) {
        r->n = 0;
        return true;
    }
    if (a->n + b->n > BI_CAP) return false;
    /* column by column (comba): a column's products summed in 64 bits, its digit and carry taken once (unit 045). A
       sum reaching 2^63 is folded first: each product is below 10^18 < 2^60, so the sum never wraps. The top
       column's carry is the product's top limb, below 10^9. */
    int n = a->n + b->n;
    uint64_t carry = 0;
    for (int k = 0; k < n - 1; k++) {
        uint64_t acc = carry, folded = 0;
        int i1 = k < a->n ? k : a->n - 1;
        for (int i = k < b->n ? 0 : k - b->n + 1; i <= i1; i++) {
            acc += (uint64_t)a->l[i] * b->l[k - i];
            if (acc >> 63) {
                uint32_t low;
                folded += BI_(qr9_)(acc, &low);
                acc = low;
            }
        }
        carry = folded + BI_(qr9_)(acc, &r->l[k]);
    }
    r->l[n - 1] = (uint32_t)carry;
    r->n = n;
    BI_(trim_)(r);
    return true;
}

bool BI_(add)(BI_T *r, const BI_T *a, const BI_T *b)
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
        if (n >= BI_CAP) return false;
        r->l[r->n++] = 1;
    }
    return true;
}

/* a += v, in place (no temporary: a 3 KB local would not fit a small task's stack). */
bool BI_(add_u32)(BI_T *a, uint32_t v)
{
    uint64_t carry = v;
    for (int i = 0; carry && i < a->n; i++) carry = BI_(qr9_)((uint64_t)a->l[i] + carry, &a->l[i]);
    while (carry) {
        if (a->n >= BI_CAP) return false;
        carry = BI_(qr9_)(carry, &a->l[a->n]);
        a->n++;
    }
    return true;
}

void BI_(sub)(BI_T *r, const BI_T *a, const BI_T *b)
{
    int64_t borrow = 0;
    for (int i = 0; i < a->n; i++) {
        int64_t t = (int64_t)a->l[i] - (i < b->n ? b->l[i] : 0) - borrow;
        borrow = t < 0;
        r->l[i] = (uint32_t)(borrow ? t + BI_BASE : t);
    }
    r->n = a->n;
    BI_(trim_)(r);
}

/* a -= v in place; false (a unchanged) if a < v. */
bool BI_(sub_u32)(BI_T *a, uint32_t v)
{
    uint64_t low = a->n == 0 ? 0 : a->n == 1 ? a->l[0] : (uint64_t)a->l[1] * BI_BASE + a->l[0];
    if (a->n <= 2 && low < v) return false;
    int64_t borrow = v;
    for (int i = 0; borrow && i < a->n; i++) {
        int64_t t = (int64_t)a->l[i] - borrow;
        borrow = 0;
        while (t < 0) {
            t += BI_BASE;
            borrow++;
        }
        a->l[i] = (uint32_t)t;
    }
    BI_(trim_)(a);
    return true;
}

static const uint32_t BI_(p10_)[10] = {1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u, 100000000u,
                                        1000000000u};

bool BI_(mul_pow10)(BI_T *a, int k)
{
    if (a->n == 0 || k <= 0) return true;      /* (k < 0: nothing, as before unit 045; its review) */
    int limbs = k / 9;
    if (a->n + limbs > BI_CAP) return false;
    memmove(a->l + limbs, a->l, sizeof(uint32_t) * (size_t)a->n);
    memset(a->l, 0, sizeof(uint32_t) * (size_t)limbs);
    a->n += limbs;
    return k % 9 == 0 || BI_(mul_u32)(a, BI_(p10_)[k % 9]);    /* one pass, not k % 9 (unit 045) */
}

uint32_t BI_(divmod_u32)(BI_T *a, uint32_t d)
{
    /* With 10^9 = A d + B, a limb step's (rem 10^9 + l) / d is rem A + (rem B + l) / d, its remainder (rem B + l) mod
       d; rem < d, so all of it is in 32 bits when (d - 1) B + 10^9 - 1 < 2^32: every d up to 57358, and every d that
       divides 10^9 (B is 0: the powers of ten that trim a value). A 32-bit core divides those in hardware and needs
       no 64-bit division (unit 045); any other d takes the 64-bit step. */
    uint32_t A = BI_BASE / d, B = BI_BASE % d;
    if ((uint64_t)(d - 1) * B + BI_BASE - 1 <= UINT32_MAX) {
        uint32_t rem = 0;
        for (int i = a->n - 1; i >= 0; i--) {
            uint32_t x = rem * B + a->l[i];
            a->l[i] = rem * A + x / d;
            rem = x % d;
        }
        BI_(trim_)(a);
        return rem;
    }
    uint64_t rem = 0;
    for (int i = a->n - 1; i >= 0; i--) {
        uint64_t t = rem * BI_BASE + a->l[i];
        a->l[i] = (uint32_t)(t / d);
        rem = t % d;
    }
    BI_(trim_)(a);
    return (uint32_t)rem;
}

void BI_(div_pow10)(BI_T *a, int k)
{
    if (k <= 0) return;                         /* (k < 0: nothing, as before unit 045; its review) */
    int limbs = k / 9;
    if (limbs >= a->n) {
        a->n = 0;
        return;
    }
    memmove(a->l, a->l + limbs, sizeof(uint32_t) * (size_t)(a->n - limbs));
    a->n -= limbs;
    if (k % 9) BI_(divmod_u32)(a, BI_(p10_)[k % 9]);
}

/* Long division one decimal digit at a time: each quotient digit is found by subtracting the
 * divisor at most nine times. Slow for big quotients, but the callers ask for about 45 digits. */
static bool BI_(div_)(BI_T *q, const BI_T *n, const BI_T *d, bool *inexact, BI_T *r)
{
    q->n = 0;
    r->n = 0;
    for (int i = n->n - 1; i >= 0; i--) {
        uint32_t limb = n->l[i];
        uint32_t pw = 100000000u;
        for (int k = 0; k < 9; k++, pw /= 10) {
            uint32_t digit = (limb / pw) % 10;
            if (!BI_(mul_u32)(r, 10) || !BI_(add_u32)(r, digit)) return false;
            uint32_t qd = 0;
            while (BI_(cmp)(r, d) >= 0) {
                BI_(sub)(r, r, d);
                qd++;
            }
            if (!BI_(mul_u32)(q, 10) || !BI_(add_u32)(q, qd)) return false;
        }
    }
    *inexact = !BI_(is_zero)(r);
    return true;
}

bool BI_(div)(BI_T *q, const BI_T *n, const BI_T *d, bool *inexact)
{
    int m = AR_MARK();
    BI_T *r;
    bool ok = BI_TAKE(1, &r) && BI_(div_)(q, n, d, inexact, r);
    AR_RELEASE(m);
    return ok;
}

bool BI_(from_digits)(BI_T *a, const char *digits, int zeros)
{
    a->n = 0;
    for (const char *p = digits; *p;) {         /* nine digits a pass, not one (unit 045) */
        uint32_t v = 0;
        int len = 0;
        for (; *p && len < 9; p++, len++) v = v * 10 + (uint32_t)(*p - '0');
        if (!BI_(mul_u32)(a, BI_(p10_)[len]) || !BI_(add_u32)(a, v)) return false;
    }
    return BI_(mul_pow10)(a, zeros);
}

bool BI_(from_abn)(BI_T *a, const abn *x)
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
    if (n > lead + 1 || lead + 1 > 9 * BI_CAP) return false;   /* not an integer, or too big */
    return BI_(from_digits)(a, d, lead + 1 - n);
}

/* Appends the decimal text of v (>= 0 or < 0) to s at t; returns the new end. */
static int BI_(put_long_)(char *s, int t, long v)
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

abn_status BI_(to_abn)(abn *out, const BI_T *a, bool neg, long e10, bool sticky)
{
    /* Keep 39 digits and a 40th that is 1 if anything nonzero lies below them, so the engine's
       half-to-even rounding to 34 digits sees exactly where the value falls. */
    enum { KEEP = 39 };
    char text[KEEP + 40];
    int t = 0, total = BI_(ndigits)(a), kept = 0;
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
    t = BI_(put_long_)(text, t, e10 + (long)(total - real) - appended);
    text[t] = '\0';
    return abn_from_text(out, text);
}
