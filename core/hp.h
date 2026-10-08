/* hp: decimal numbers to a chosen number of significant digits (up to HP_PMAX), for functions
 * that must be formed beyond the engine's 34 digits and rounded once (the complex functions,
 * unit 009). A value is a short xnum (core/sbig.h) truncated after each operation to the working
 * precision p; the exponent is unbounded, so intermediate values may lie far outside the
 * engine's range. Each operation returns false when a result would not fit; callers report a
 * range error. Results must not alias arguments. Each hp is about 300 bytes.
 */
#ifndef HP_H
#define HP_H

#include <stdbool.h>

#include "sbig.h"

#define HP_PMAX 220             /* pi and ln 10 are held to 241 digits */

typedef sxnum hp;

void hp_set_int(hp *r, int v);
bool hp_from_abn(hp *r, const abn *x);
bool hp_is_zero(const hp *x);
long hp_lead(const hp *x);      /* exponent of the leading digit; x nonzero */
void hp_trim(hp *x, int p);
bool hp_mul(hp *r, const hp *a, const hp *b, int p);
bool hp_add(hp *r, const hp *a, const hp *b, bool subtract, int p);
bool hp_div(hp *r, const hp *a, const hp *b, int p);
void hp_neg(hp *x);
abn_status hp_round(abn *out, const hp *x);     /* to the engine's 34 digits, half to even */

bool hp_pi(hp *r, int p);
bool hp_exp(hp *r, const hp *x, int p);         /* |x| < 10^7 */
bool hp_expm1(hp *r, const hp *x, int p);       /* e^x - 1, relative to itself for small x */
bool hp_sincos(hp *s, hp *c, const hp *x, int p);   /* radians; false past the stored pi */
bool hp_sinhcosh(hp *sh, hp *ch, const hp *x, int p);
/* ln s for s = 1 + t > 0, both given (t exact matters near s = 1). */
bool hp_ln(hp *r, const hp *s, const hp *t, int p);
/* ln gamma(w) for w >= 40 by the Stirling series, p <= 64 (unit 021). */
bool hp_lgamma_big(hp *r, const hp *w, int p);
/* The angle of (a, b) in (-pi, pi], radians; not both zero. */
bool hp_atan2(hp *r, const abn *b, const abn *a, int p);
/* sqrt(x^2 + y^2), and a vector's norm, correctly rounded (unit 034): the root of the exact sum of the squares
   of the parts within 40 digits of the largest (the rest are below half a unit of the result), in the arena's
   xnums. No intermediate overflow. */
abn_status hp_hypot(abn *out, const abn *x, const abn *y);
abn_status hp_norm(abn *out, const abn *el, int n);

/* Unit 044: sqrt x (x >= 0) by Newton from the engine's root; the angle of (a, b) for hp a and b (as hp_atan2), not
   both zero; ln 10 to p digits (at most 241). */
bool hp_sqrt(hp *r, const hp *x, int p);
bool hp_atan2h(hp *r, const hp *b, const hp *a, int p);
bool hp_ln10(hp *r, int p);

/* v (formed to p digits, good to a few units of its p-th) rounded to the engine's digits into out and *st; true
   when the rounding is sure: v moved by 10^6 units of its p-th digit either way rounds the same (Ziv: when false,
   form v again to more digits). s is scratch; v is restored. */
bool hp_round_sure(abn *out, abn_status *st, hp *v, int p, hp *s);

#endif
