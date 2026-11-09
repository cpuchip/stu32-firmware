/* THROWAWAY: Phi(z) = erfc(-z/sqrt2)/2 and Q(z) = erfc(z/sqrt2)/2 in the library's decimal128, each step rounded */
#include <stdio.h>
#include "bid_conf.h"
#include "bid_functions.h"
int main(void)
{
    char s[128];
    _IDEC_flags f = 0;
    const _IDEC_round N = BID_ROUNDING_TO_NEAREST;
    BID_UINT128 two = bid128_from_string("2", N, &f), r2 = bid128_sqrt(two, N, &f);
    while (scanf("%127s", s) == 1) {
        BID_UINT128 z = bid128_from_string(s, N, &f), t = bid128_div(z, r2, N, &f);
        BID_UINT128 phi = bid128_div(bid128_erfc(bid128_negate(t), N, &f), two, N, &f);
        BID_UINT128 q = bid128_div(bid128_erfc(t, N, &f), two, N, &f);
        char a[64], b[64];
        bid128_to_string(a, phi, &f);
        bid128_to_string(b, q, &f);
        printf("%s %s %s\n", s, a, b);
    }
    return 0;
}
