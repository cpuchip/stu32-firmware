/* cplx: complex arithmetic and functions on (re, im) pairs (unit 009).
 *
 * + and - are one rounding per part. x, / and 1/z form each part exactly (core/xnum.c) and round
 * it once, so their parts are correctly rounded. The transcendental functions are held to within
 * 8 units in the 34th digit of |result| (unit 009 rule 4, normwise).
 */
#ifndef CPLX_H
#define CPLX_H

#include "abn.h"

typedef struct {
    abn re, im;
} cplx;

abn_status cx_add(cplx *out, const cplx *a, const cplx *b);
abn_status cx_sub(cplx *out, const cplx *a, const cplx *b);
abn_status cx_mul(cplx *out, const cplx *a, const cplx *b);
abn_status cx_div(cplx *out, const cplx *a, const cplx *b);
abn_status cx_inv(cplx *out, const cplx *a);
cplx cx_neg(const cplx *a);
abn_status cx_abs(abn *out, const cplx *a);

#endif
