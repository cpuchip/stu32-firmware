/* xnum: exact decimal numbers on bigint (see xnum.h). */
#include <string.h>

#include "xnum.h"

bool xn_from_abn(xnum *a, const abn *x)
{
    char d[ABN_DIGITS + 1];
    int lead;
    abn_digits(x, &a->neg, d, &lead);
    a->e = (long)lead - (long)strlen(d) + 1;
    if (d[0] == '\0') {
        a->neg = false;
        a->e = 0;
    }
    return bi_from_digits(&a->m, d, 0);
}

void xn_set_int(xnum *a, int v)
{
    a->neg = v < 0;
    a->e = 0;
    bi_set(&a->m, (uint32_t)(v < 0 ? -v : v));
}

int xn_sign(const xnum *a)
{
    return bi_is_zero(&a->m) ? 0 : (a->neg ? -1 : 1);
}

bool xn_mul(xnum *r, const xnum *a, const xnum *b)
{
    if (!bi_mul(&r->m, &a->m, &b->m)) return false;
    r->e = a->e + b->e;
    r->neg = bi_is_zero(&r->m) ? false : (a->neg != b->neg);
    return true;
}

#define ALIGN_MAX 6000      /* exponent gap beyond which the exact sum would not fit */

bool xn_add(xnum *r, const xnum *a, const xnum *b, bool subtract)
{
    static xnum sa, sb;
    bool bneg = b->neg != subtract;
    if (bi_is_zero(&b->m)) {
        *r = *a;
        return true;
    }
    if (bi_is_zero(&a->m)) {
        *r = *b;
        r->neg = bneg;
        return true;
    }
    /* bring both to the smaller exponent */
    sa = *a;
    sb = *b;
    long e = a->e < b->e ? a->e : b->e;
    if (a->e - e > ALIGN_MAX || b->e - e > ALIGN_MAX) return false;
    if (!bi_mul_pow10(&sa.m, (int)(a->e - e)) || !bi_mul_pow10(&sb.m, (int)(b->e - e))) return false;
    r->e = e;
    if (a->neg == bneg) {
        if (!bi_add(&r->m, &sa.m, &sb.m)) return false;
        r->neg = a->neg;
    } else if (bi_cmp(&sa.m, &sb.m) >= 0) {
        bi_sub(&r->m, &sa.m, &sb.m);
        r->neg = a->neg;
    } else {
        bi_sub(&r->m, &sb.m, &sa.m);
        r->neg = bneg;
    }
    if (bi_is_zero(&r->m)) r->neg = false;
    return true;
}

abn_status xn_div_to_abn(abn *out, const xnum *a, const xnum *b)
{
    static bigint num, q;
    bool inexact;
    if (bi_is_zero(&b->m)) return ABN_DIV_ZERO;
    if (bi_is_zero(&a->m)) {
        *out = abn_zero();
        return ABN_OK;
    }
    /* scale the numerator so the quotient carries at least 45 digits */
    num = a->m;
    int extra = 45 + (bi_ndigits(&b->m) > bi_ndigits(&num) ? bi_ndigits(&b->m) - bi_ndigits(&num) : 0);
    if (!bi_mul_pow10(&num, extra) || !bi_div(&q, &num, &b->m, &inexact)) return ABN_RANGE;
    return bi_to_abn(out, &q, a->neg != b->neg, a->e - b->e - extra, inexact);
}
