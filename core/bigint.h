/* bigint: exact non-negative integers, for results that must be exact before they are rounded
 * once: factorials, combinations, permutations, the affine temperature conversions.
 *
 * Base 10^9 limbs, least significant first. Room for 6840 decimal digits, more than any value in
 * the decimal128 range needs (its largest has 6145 digits). Each bigint is about 3 KB: callers
 * keep few alive at once (a device unit must give the core a stack sized for this).
 */
#ifndef BIGINT_H
#define BIGINT_H

#include <stdbool.h>
#include <stdint.h>

#include "abn.h"

#define BI_BASE 1000000000u
#define BI_LIMBS 760

typedef struct {
    int n;                  /* limbs in use; 0 is the value zero */
    uint32_t l[BI_LIMBS];
} bigint;

void bi_set(bigint *a, uint32_t v);
bool bi_is_zero(const bigint *a);
int bi_cmp(const bigint *a, const bigint *b);
int bi_ndigits(const bigint *a);

/* Each returns false when the result would not fit (the caller reports a range error). */
bool bi_mul_u32(bigint *a, uint32_t m);
bool bi_mul(bigint *r, const bigint *a, const bigint *b);       /* r must not be a or b */
bool bi_add(bigint *r, const bigint *a, const bigint *b);       /* r may be a or b */
bool bi_add_u32(bigint *a, uint32_t v);
void bi_sub(bigint *r, const bigint *a, const bigint *b);       /* a >= b; r may be a or b */
bool bi_sub_u32(bigint *a, uint32_t v);                         /* false if a < v */
bool bi_mul_pow10(bigint *a, int k);
uint32_t bi_divmod_u32(bigint *a, uint32_t d);                  /* a /= d; returns the remainder */
void bi_div_pow10(bigint *a, int k);                            /* a /= 10^k, truncating */
/* q = floor(n / d), d nonzero; *inexact says the remainder was nonzero. q must not be n or d. */
bool bi_div(bigint *q, const bigint *n, const bigint *d, bool *inexact);

/* digits (most significant first, '0'..'9') followed by `zeros` zeros. */
bool bi_from_digits(bigint *a, const char *digits, int zeros);
/* An integer abn (non-negative or not; the sign is the caller's) as a bigint. */
bool bi_from_abn(bigint *a, const abn *x);

/* (-1)^neg * a * 10^e10 rounded once, half to even, to the engine's 34 digits; `sticky` says
   the true value lies strictly above a * 10^e10 in magnitude (a nonzero remainder was dropped). */
abn_status bi_to_abn(abn *out, const bigint *a, bool neg, long e10, bool sticky);

#endif
