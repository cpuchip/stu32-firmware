/* cplx: complex arithmetic (unit 009; see cplx.h). */
#include "cplx.h"
#include "xnum.h"

abn_status cx_add(cplx *out, const cplx *a, const cplx *b)
{
    cplx r;
    abn_status st;
    if ((st = abn_add(&r.re, &a->re, &b->re)) != ABN_OK) return st;
    if ((st = abn_add(&r.im, &a->im, &b->im)) != ABN_OK) return st;
    *out = r;
    return ABN_OK;
}

abn_status cx_sub(cplx *out, const cplx *a, const cplx *b)
{
    cplx r;
    abn_status st;
    if ((st = abn_sub(&r.re, &a->re, &b->re)) != ABN_OK) return st;
    if ((st = abn_sub(&r.im, &a->im, &b->im)) != ABN_OK) return st;
    *out = r;
    return ABN_OK;
}

cplx cx_neg(const cplx *a)
{
    cplx r;
    r.re = abn_neg(&a->re);
    r.im = abn_neg(&a->im);
    return r;
}

/* p*q +/- r*s exactly. */
static bool dot2(xnum *out, const xnum *p, const xnum *q, const xnum *r, const xnum *s, bool subtract)
{
    static xnum t1, t2;
    return xn_mul(&t1, p, q) && xn_mul(&t2, r, s) && xn_add(out, &t1, &t2, subtract);
}

static xnum A, B, C, D, N1, N2, DEN;

static abn_status load4(const cplx *a, const cplx *b)
{
    if (!xn_from_abn(&A, &a->re) || !xn_from_abn(&B, &a->im) || !xn_from_abn(&C, &b->re) ||
        !xn_from_abn(&D, &b->im))
        return ABN_RANGE;
    return ABN_OK;
}

static abn_status round_part(abn *out, const xnum *num, const xnum *den)
{
    return xn_div_to_abn(out, num, den);
}

/* (a + bi)(c + di) = (ac - bd) + (ad + bc)i, each part exact, then rounded once. */
abn_status cx_mul(cplx *out, const cplx *a, const cplx *b)
{
    static xnum one;
    cplx r;
    abn_status st;
    if ((st = load4(a, b)) != ABN_OK) return st;
    xn_set_int(&one, 1);
    if (!dot2(&N1, &A, &C, &B, &D, true) || !dot2(&N2, &A, &D, &B, &C, false)) return ABN_RANGE;
    if ((st = round_part(&r.re, &N1, &one)) != ABN_OK) return st;
    if ((st = round_part(&r.im, &N2, &one)) != ABN_OK) return st;
    *out = r;
    return ABN_OK;
}

/* (a + bi)/(c + di) = ((ac + bd) + (bc - ad)i) / (c^2 + d^2), each part exact, rounded once. */
abn_status cx_div(cplx *out, const cplx *a, const cplx *b)
{
    cplx r;
    abn_status st;
    if ((st = load4(a, b)) != ABN_OK) return st;
    if (!dot2(&DEN, &C, &C, &D, &D, false)) return ABN_RANGE;
    if (xn_sign(&DEN) == 0) return ABN_DIV_ZERO;
    if (!dot2(&N1, &A, &C, &B, &D, false) || !dot2(&N2, &B, &C, &A, &D, true)) return ABN_RANGE;
    if ((st = round_part(&r.re, &N1, &DEN)) != ABN_OK) return st;
    if ((st = round_part(&r.im, &N2, &DEN)) != ABN_OK) return st;
    *out = r;
    return ABN_OK;
}

abn_status cx_inv(cplx *out, const cplx *a)
{
    cplx one;
    one.re = abn_from_int(1);
    one.im = abn_zero();
    return cx_div(out, &one, a);
}

/* |a + bi| = sqrt(a^2 + b^2): the sum of squares exact, rounded once, then a correctly rounded
 * root (within about 1 unit). */
abn_status cx_abs(abn *out, const cplx *a)
{
    static xnum s, one;
    abn q;
    abn_status st;
    cplx z = {abn_zero(), abn_zero()};
    if ((st = load4(a, &z)) != ABN_OK) return st;
    xn_set_int(&one, 1);
    if (!dot2(&s, &A, &A, &B, &B, false)) return ABN_RANGE;
    if ((st = xn_div_to_abn(&q, &s, &one)) != ABN_OK) return st;
    return abn_sqrt(out, &q);
}
