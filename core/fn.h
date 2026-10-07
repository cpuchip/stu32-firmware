/* fn: the calculator's functions of one and two numbers (unit 003), on top of abn.h.
 *
 * Adds what the engine alone does not give: exact results where the decimal answer is exact,
 * angles in degrees and grads reduced exactly in decimal, and the calculator's domain rules.
 * Every function returns ABN_OK or an error status and leaves *out alone on error; ABN_OVERFLOW and
 * ABN_UNDERFLOW say the result lies beyond the range (unit 026), except where a function's own
 * comment says its outputs are written with it (->POL, ->REC).
 */
#ifndef FN_H
#define FN_H

#include "abn.h"
#include "fmt.h"

typedef enum { FN_DEG = 0, FN_RAD, FN_GRAD } fn_angle;

abn fn_pi(void);

abn_status fn_sqrt(abn *out, const abn *x);
abn_status fn_sq(abn *out, const abn *x);
abn_status fn_inv(abn *out, const abn *x);
abn_status fn_exp(abn *out, const abn *x);
abn_status fn_ln(abn *out, const abn *x);
abn_status fn_tenx(abn *out, const abn *x);
abn_status fn_log(abn *out, const abn *x);
abn_status fn_pow(abn *out, const abn *y, const abn *x);    /* y^x */
extern unsigned long fn_pow_hp_calls;   /* unit 038: the high-precision y^x's uses (a test's probe) */
abn_status fn_expm1(abn *out, const abn *x);                 /* e^x - 1 (unit 021) */
abn_status fn_ln1p(abn *out, const abn *x);                  /* ln(1 + x), x > -1 */
abn_status fn_twox(abn *out, const abn *x);                  /* 2^x */
abn_status fn_log2(abn *out, const abn *x);                  /* log2 x, x > 0 */
abn_status fn_xroot(abn *out, const abn *y, const abn *x);  /* the x-th root of y */
abn_status fn_idiv(abn *out, const abn *y, const abn *x);
abn_status fn_rmdr(abn *out, const abn *y, const abn *x);
abn_status fn_pct(abn *out, const abn *y, const abn *x);    /* x percent of y */
abn_status fn_pctchg(abn *out, const abn *y, const abn *x); /* percent change, y to x */

abn_status fn_sin(abn *out, const abn *x, fn_angle m);
abn_status fn_cos(abn *out, const abn *x, fn_angle m);
abn_status fn_tan(abn *out, const abn *x, fn_angle m);
abn_status fn_asin(abn *out, const abn *x, fn_angle m);
abn_status fn_acos(abn *out, const abn *x, fn_angle m);
abn_status fn_atan(abn *out, const abn *x, fn_angle m);

abn_status fn_sinh(abn *out, const abn *x);
abn_status fn_cosh(abn *out, const abn *x);
abn_status fn_tanh(abn *out, const abn *x);
abn_status fn_asinh(abn *out, const abn *x);
abn_status fn_acosh(abn *out, const abn *x);
abn_status fn_atanh(abn *out, const abn *x);

/* unit 004 */
abn_status fn_ip(abn *out, const abn *x);
abn_status fn_fp(abn *out, const abn *x);
abn_status fn_intg(abn *out, const abn *x);
abn_status fn_abs(abn *out, const abn *x);
abn_status fn_sgn(abn *out, const abn *x);
abn_status fn_rnd(abn *out, const abn *x, ab_mode mode, int n);
abn_status fn_hms(abn *out, const abn *x);
abn_status fn_hmsinv(abn *out, const abn *x);
abn_status fn_todeg(abn *out, const abn *x);
abn_status fn_torad(abn *out, const abn *x);

typedef enum { FN_TOKG, FN_TOLB, FN_TOC, FN_TOF, FN_TOCM, FN_TOIN, FN_TOL, FN_TOGAL, FN_TOKM, FN_TOMILE } fn_unit;
abn_status fn_convert(abn *out, const abn *x, fn_unit u);

abn_status fn_fact(abn *out, const abn *x);
abn_status fn_ncr(abn *out, const abn *n, const abn *r);
abn_status fn_npr(abn *out, const abn *n, const abn *r);

/* x, y to r, theta (theta in the angle mode, in (-half turn, half turn]) and back. */
/* ABN_OVERFLOW from these two: a result beyond the range, written as the largest number with its
   sign, both outputs written (unit 026); a part below the range is 0. */
abn_status fn_topol(abn *r, abn *theta, const abn *x, const abn *y, fn_angle m);
abn_status fn_torec(abn *x, abn *y, const abn *r, const abn *theta, fn_angle m);

void fn_seed(uint64_t *state, const abn *x);
abn fn_rand(uint64_t *state);

#endif
