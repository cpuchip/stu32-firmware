/* Included into the decimal library's cross builds (TARGET=esp32s3, esp32c6) in place of a bare
 * fenv.h. The library calls fegetround and fesetround only to make sure its binary floating point
 * runs in round-to-nearest, restoring the old mode afterwards (bid128_div.c, bid128_sqrt.c and
 * their kin). On these targets binary double is libgcc's software floating point, which always
 * rounds to nearest, so the calls are compiled out. (Newlib's xtensa libm also defines fesetround
 * in two members, which fails the link when both are pulled in.) The on-chip diff_test against
 * the reference engine is what shows this holds. */
#include <fenv.h>
#define fegetround() FE_TONEAREST
#define fesetround(mode) ((mode) == FE_TONEAREST ? 0 : 1)
