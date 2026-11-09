/* THROWAWAY (abacus #14270's fix, measured): Phi(z) = erfc(-x)/2 and Q(z) = erfc(x)/2, x = z/sqrt2 formed in hp at 64
   digits; x0 its 34-digit rounding, r = x - x0; erfc(u) = erfc(u0) - r*(2/sqrt(pi))e^(-u0^2) + r^2*(2u0/sqrt(pi))e^(-u0^2)
   for u = +-x (the sign of the derivative follows u), all in hp, rounded once. */
#include <stdio.h>
#include <string.h>
#include "bid_conf.h"
#include "bid_functions.h"
#include "abn.h"
#include "hp.h"
#define P 64
static abn lib_erfc(const abn *a)
{
    BID_UINT128 x, r;
    _IDEC_flags f = 0;
    memcpy(&x, a, sizeof x);
    r = bid128_erfc(x, BID_ROUNDING_TO_NEAREST, &f);
    abn o;
    memcpy(&o, &r, sizeof o);
    return o;
}
/* erfc(u) for u = sign * z/sqrt2, to the engine's digits */
static abn_status corrected(abn *out, const abn *z, int sign)
{
    static hp hz, two, s2, x, hx0, r, x02, e, t, k, pi, spi, c, c2, base, sum, tmp;
    hp_from_abn(&hz, z);
    hp_set_int(&two, 2);
    hp_sqrt(&s2, &two, P);
    hp_div(&x, &hz, &s2, P);
    if (sign < 0) hp_neg(&x);
    abn x0;
    hp_round(&x0, &x);
    hp_from_abn(&hx0, &x0);
    hp_add(&r, &x, &hx0, true, P);                  /* r = x - x0 */
    hp_mul(&x02, &hx0, &hx0, P);
    hp_neg(&x02);
    hp_exp(&e, &x02, P);                            /* e^(-x0^2) */
    hp_pi(&pi, P);
    hp_sqrt(&spi, &pi, P);
    hp_div(&k, &two, &spi, P);                      /* 2/sqrt(pi) */
    hp_mul(&t, &k, &e, P);                          /* (2/sqrt pi) e^(-x0^2) */
    hp_mul(&c, &r, &t, P);                          /* first term: r * that */
    hp_mul(&tmp, &r, &r, P);
    hp_mul(&c2, &tmp, &t, P);
    hp_mul(&tmp, &c2, &hx0, P);                     /* second term: r^2 x0 (2/sqrt pi) e^(-x0^2) */
    abn ec = lib_erfc(&x0);
    hp_from_abn(&base, &ec);
    hp_add(&sum, &base, &c, true, P);               /* erfc(x0) - r t */
    hp_add(&base, &sum, &tmp, false, P);            /* + r^2 x0 t */
    hp_div(&sum, &base, &two, P);                   /* the halving: Phi or Q */
    return hp_round(out, &sum);
}
int main(void)
{
    char s[128];
    while (scanf("%127s", s) == 1) {
        abn z, phi, q;
        abn_from_text(&z, s);
        corrected(&phi, &z, -1);
        corrected(&q, &z, +1);
        char a[64], b[64];
        abn_to_text(&phi, a, sizeof a);
        abn_to_text(&q, b, sizeof b);
        printf("%s %s %s\n", s, a, b);
    }
    return 0;
}
