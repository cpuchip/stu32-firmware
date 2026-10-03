/* fn: the calculator's functions of one and two numbers (unit 003), on top of abn.h.
 *
 * Adds what the engine alone does not give: exact results where the decimal answer is exact,
 * angles in degrees and grads reduced exactly in decimal, and the calculator's domain rules.
 * Every function returns ABN_OK or an error status and leaves *out alone on error.
 */
#ifndef FN_H
#define FN_H

#include "abn.h"

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

#endif
