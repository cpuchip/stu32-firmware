/* sbig: the bigint and xnum operations (core/bigint.h, core/xnum.h) on short numbers of up to
 * 648 digits, about 300 bytes each instead of 3 KB, for working values that are truncated to a
 * set precision (core/hp.c). Same algorithms (bigint_impl.h, xnum_impl.h); each operation returns
 * false when a result would not fit. */
#ifndef SBIG_H
#define SBIG_H

#include <stdbool.h>
#include <stdint.h>

#include "abn.h"

#define SB_LIMBS 72

typedef struct {
    int n;
    uint32_t l[SB_LIMBS];
} sbig;

typedef struct {
    bool neg;
    long e;
    sbig m;
} sxnum;

void sb_set(sbig *a, uint32_t v);
bool sb_is_zero(const sbig *a);
int sb_cmp(const sbig *a, const sbig *b);
int sb_ndigits(const sbig *a);
bool sb_mul_u32(sbig *a, uint32_t m);
bool sb_mul(sbig *r, const sbig *a, const sbig *b);
bool sb_add(sbig *r, const sbig *a, const sbig *b);
bool sb_add_u32(sbig *a, uint32_t v);
void sb_sub(sbig *r, const sbig *a, const sbig *b);
bool sb_sub_u32(sbig *a, uint32_t v);
bool sb_mul_pow10(sbig *a, int k);
uint32_t sb_divmod_u32(sbig *a, uint32_t d);
void sb_div_pow10(sbig *a, int k);
bool sb_div(sbig *q, const sbig *n, const sbig *d, bool *inexact);
bool sb_from_digits(sbig *a, const char *digits, int zeros);
bool sb_from_abn(sbig *a, const abn *x);
abn_status sb_to_abn(abn *out, const sbig *a, bool neg, long e10, bool sticky);

bool sxn_from_abn(sxnum *a, const abn *x);
void sxn_set_int(sxnum *a, int v);
int sxn_sign(const sxnum *a);
bool sxn_mul(sxnum *r, const sxnum *a, const sxnum *b);
bool sxn_add(sxnum *r, const sxnum *a, const sxnum *b, bool subtract);
abn_status sxn_div_to_abn(abn *out, const sxnum *a, const sxnum *b);
abn_status sxn_sqrt_to_abn(abn *out, const sxnum *a);
/* sqrt(a / b) correctly rounded; a / b >= 0, b nonzero (unit 010b). */
abn_status sxn_sqrt_ratio_to_abn(abn *out, const sxnum *a, const sxnum *b);

#endif
