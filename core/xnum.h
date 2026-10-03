/* xnum: exact decimal numbers, (-1)^neg * m * 10^e with m a bigint, for formulas that must be
 * evaluated exactly and rounded once (the statistics, unit 006). Each is about 3 KB; callers keep
 * them static. Operations return false when a result would not fit (the caller reports a range
 * error). */
#ifndef XNUM_H
#define XNUM_H

#include <stdbool.h>

#include "abn.h"
#include "bigint.h"

typedef struct {
    bool neg;
    long e;
    bigint m;
} xnum;

bool xn_from_abn(xnum *a, const abn *x);
void xn_set_int(xnum *a, int v);
int xn_sign(const xnum *a);                                 /* -1, 0, 1 */
bool xn_mul(xnum *r, const xnum *a, const xnum *b);         /* r must not be a or b */
bool xn_add(xnum *r, const xnum *a, const xnum *b, bool subtract);  /* r must not be a or b */
/* a / b rounded once, half to even, to the engine's 34 digits; b nonzero. */
abn_status xn_div_to_abn(abn *out, const xnum *a, const xnum *b);

#endif
