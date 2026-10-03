/* xnum_impl: the xnum algorithms, written once for each capacity (see bigint_impl.h): the
 * including file defines XN_T and XN_(name) besides BI_T and BI_(name). */

bool XN_(from_abn)(XN_T *a, const abn *x)
{
    char d[ABN_DIGITS + 1];
    int lead;
    abn_digits(x, &a->neg, d, &lead);
    a->e = (long)lead - (long)strlen(d) + 1;
    if (d[0] == '\0') {
        a->neg = false;
        a->e = 0;
    }
    return BI_(from_digits)(&a->m, d, 0);
}

void XN_(set_int)(XN_T *a, int v)
{
    a->neg = v < 0;
    a->e = 0;
    BI_(set)(&a->m, (uint32_t)(v < 0 ? -v : v));
}

int XN_(sign)(const XN_T *a)
{
    return BI_(is_zero)(&a->m) ? 0 : (a->neg ? -1 : 1);
}

bool XN_(mul)(XN_T *r, const XN_T *a, const XN_T *b)
{
    if (!BI_(mul)(&r->m, &a->m, &b->m)) return false;
    r->e = a->e + b->e;
    r->neg = BI_(is_zero)(&r->m) ? false : (a->neg != b->neg);
    return true;
}

#define ALIGN_MAX 6000      /* exponent gap beyond which the exact sum would not fit */

bool XN_(add)(XN_T *r, const XN_T *a, const XN_T *b, bool subtract)
{
    static XN_T sa, sb;
    bool bneg = b->neg != subtract;
    if (BI_(is_zero)(&b->m)) {
        *r = *a;
        return true;
    }
    if (BI_(is_zero)(&a->m)) {
        *r = *b;
        r->neg = bneg;
        return true;
    }
    /* bring both to the smaller exponent */
    sa = *a;
    sb = *b;
    long e = a->e < b->e ? a->e : b->e;
    if (a->e - e > ALIGN_MAX || b->e - e > ALIGN_MAX) return false;
    if (!BI_(mul_pow10)(&sa.m, (int)(a->e - e)) || !BI_(mul_pow10)(&sb.m, (int)(b->e - e))) return false;
    r->e = e;
    if (a->neg == bneg) {
        if (!BI_(add)(&r->m, &sa.m, &sb.m)) return false;
        r->neg = a->neg;
    } else if (BI_(cmp)(&sa.m, &sb.m) >= 0) {
        BI_(sub)(&r->m, &sa.m, &sb.m);
        r->neg = a->neg;
    } else {
        BI_(sub)(&r->m, &sb.m, &sa.m);
        r->neg = bneg;
    }
    if (BI_(is_zero)(&r->m)) r->neg = false;
    return true;
}

abn_status XN_(div_to_abn)(abn *out, const XN_T *a, const XN_T *b)
{
    static BI_T num, q;
    bool inexact;
    if (BI_(is_zero)(&b->m)) return ABN_DIV_ZERO;
    if (BI_(is_zero)(&a->m)) {
        *out = abn_zero();
        return ABN_OK;
    }
    /* scale the numerator so the quotient carries at least 45 digits */
    num = a->m;
    int extra = 45 + (BI_(ndigits)(&b->m) > BI_(ndigits)(&num) ? BI_(ndigits)(&b->m) - BI_(ndigits)(&num) : 0);
    if (!BI_(mul_pow10)(&num, extra) || !BI_(div)(&q, &num, &b->m, &inexact)) return ABN_RANGE;
    return BI_(to_abn)(out, &q, a->neg != b->neg, a->e - b->e - extra, inexact);
}

/* floor(sqrt(n)) by Newton's method on integers, from a start above the root: each step
 * x <- (x + n / x) / 2 decreases until it would not, and then x is the floor (Cohen, A Course in
 * Computational Algebraic Number Theory, algorithm 1.7.1). */
static bool XN_(isqrt_)(BI_T *r, const BI_T *n)
{
    static BI_T x, q;
    bool inexact;
    if (BI_(is_zero)(n)) {
        r->n = 0;
        return true;
    }
    BI_(set)(&x, 1);
    if (!BI_(mul_pow10)(&x, (BI_(ndigits)(n) + 1) / 2)) return false;     /* 10^ceil(d/2) > sqrt(n) */
    for (;;) {
        if (!BI_(div)(&q, n, &x, &inexact) || !BI_(add)(&q, &q, &x)) return false;
        BI_(divmod_u32)(&q, 2);
        if (BI_(cmp)(&q, &x) >= 0) break;
        x = q;
    }
    *r = x;
    return true;
}

abn_status XN_(sqrt_to_abn)(abn *out, const XN_T *a)
{
    static BI_T n, r, sq;
    if (a->neg && !BI_(is_zero)(&a->m)) return ABN_DOMAIN;
    if (BI_(is_zero)(&a->m)) {
        *out = abn_zero();
        return ABN_OK;
    }
    /* n = m 10^k with e - k even and at least 76 digits, so the root has 38 or more */
    int k = 76 - BI_(ndigits)(&a->m);
    if (k < 0) k = 0;
    if (((a->e - k) % 2 + 2) % 2) k++;
    n = a->m;
    if (!BI_(mul_pow10)(&n, k) || !XN_(isqrt_)(&r, &n) || !BI_(mul)(&sq, &r, &r)) return ABN_RANGE;
    return BI_(to_abn)(out, &r, false, (a->e - k) / 2, BI_(cmp)(&sq, &n) != 0);
}
