/* THROWAWAY (decision 95's normal distribution, measured first): the library's erf and erfc on each x read, printed
   as the library gives them (34 digits), with the flags */
#include <stdio.h>
#include "bid_conf.h"
#include "bid_functions.h"
int main(void)
{
    char s[128];
    while (scanf("%127s", s) == 1) {
        _IDEC_flags f = 0, g = 0, h = 0;
        BID_UINT128 x = bid128_from_string(s, BID_ROUNDING_TO_NEAREST, &f);
        BID_UINT128 e = bid128_erf(x, BID_ROUNDING_TO_NEAREST, &g), c = bid128_erfc(x, BID_ROUNDING_TO_NEAREST, &h);
        char a[64], b[64];
        bid128_to_string(a, e, &f);
        bid128_to_string(b, c, &f);
        printf("%s %s %s %u %u\n", s, a, b, g, h);
    }
    return 0;
}
