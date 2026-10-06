/* stat: one- and two-variable statistics on the six statistics registers (unit 006).
 *
 * r[0] n, r[1] sum x, r[2] sum y, r[3] sum x^2, r[4] sum y^2, r[5] sum xy: the order of the
 * indirect addresses -27..-32 (UG p.14-22).
 */
#ifndef STAT_H
#define STAT_H

#include <stdbool.h>

#include "abn.h"

#define ST_N 0
#define ST_SX 1
#define ST_SY 2
#define ST_SX2 3
#define ST_SY2 4
#define ST_SXY 5

/* Adds (or with remove, takes away) the data point (x, y): one correctly rounded operation per
   register, all or nothing for an error; a register that overflows is the largest number with
   *over set, one that underflows 0 (unit 026). */
abn_status st_accumulate(abn r[6], const abn *x, const abn *y, bool remove, bool *over);

abn_status st_mean(abn *out, const abn r[6], bool of_y);
abn_status st_wmean(abn *out, const abn r[6]);
abn_status st_sdev(abn *out, const abn r[6], bool of_y, bool population);
abn_status st_slope(abn *out, const abn r[6]);
abn_status st_yint(abn *out, const abn r[6]);
abn_status st_corr(abn *out, const abn r[6]);
abn_status st_yhat(abn *out, const abn r[6], const abn *x);
abn_status st_xhat(abn *out, const abn r[6], const abn *y);

#endif
