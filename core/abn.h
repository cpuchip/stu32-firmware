/* abn: the number interface the calculator core is written against.
 *
 * The core never looks inside an `abn`; it only calls these functions, so the engine
 * behind them can be replaced. The shipped engine is abn_intel.c: IEEE 754 decimal128
 * (34 significant digits) from the Intel Decimal Floating-Point Math Library, rounding
 * half to even, no binary floating point in any result.
 */
#ifndef ABN_H
#define ABN_H

#include <stdbool.h>
#include <stdint.h>

#define ABN_DIGITS 34       /* significant digits kept in every result */
#define ABN_EXP_MAX 6144    /* exponent of the leading digit lies in [ABN_EXP_MIN, ABN_EXP_MAX] */
#define ABN_EXP_MIN (-6143) /* (the decimal128 normal range; overflow and underflow behaviour
                               for the calculator arrive with a later unit) */
#define ABN_TEXT_MAX 64

/* Opaque: 128 bits of engine-defined content. All bits zero is the value 0. */
typedef struct {
    uint64_t w[2];
} abn;

typedef enum {
    ABN_OK = 0,
    ABN_DIV_ZERO,           /* division by zero */
    ABN_RANGE,              /* result outside the normal range above */
    ABN_BAD_TEXT,           /* text is not a number */
    ABN_DOMAIN,             /* argument outside the function's real domain */
    ABN_INEXACT             /* abn_mul_exact only: the product needed rounding */
} abn_status;

abn abn_zero(void);
bool abn_is_zero(const abn *a);
int abn_cmp(const abn *a, const abn *b);    /* -1, 0, 1 by value */
abn abn_neg(const abn *a);                  /* zero stays zero, never negative */

/* On any status but ABN_OK, *out is left unchanged. Results are never negative zero. */
abn_status abn_add(abn *out, const abn *a, const abn *b);
abn_status abn_sub(abn *out, const abn *a, const abn *b);   /* a - b */
abn_status abn_mul(abn *out, const abn *a, const abn *b);
abn_status abn_div(abn *out, const abn *a, const abn *b);   /* a / b */

/* Exact helpers. abn_mul_exact succeeds only when the product needs no rounding.
   abn_idiv is the integer quotient a/b truncated toward zero; abn_fmod is the exact
   remainder a - b*trunc(a/b), with the sign of a. abn_pow10 is 10^n exactly. */
abn_status abn_mul_exact(abn *out, const abn *a, const abn *b);
abn_status abn_idiv(abn *out, const abn *a, const abn *b);
abn_status abn_fmod(abn *out, const abn *a, const abn *b);
abn_status abn_pow10(abn *out, int32_t n);
abn_status abn_fma(abn *out, const abn *a, const abn *b, const abn *c);   /* a*b + c, rounded once */
abn abn_abs(const abn *a);
bool abn_is_negative(const abn *a);
bool abn_is_integer(const abn *a);
abn abn_from_int(int32_t v);

/* Functions, each within the engine's documented accuracy (sqrt correctly rounded).
   Angles are in radians. Domain errors are reported, never computed as NaN. */
abn_status abn_sqrt(abn *out, const abn *a);
abn_status abn_exp(abn *out, const abn *a);
abn_status abn_ln(abn *out, const abn *a);
abn_status abn_exp10(abn *out, const abn *a);
abn_status abn_log10(abn *out, const abn *a);
abn_status abn_pow(abn *out, const abn *base, const abn *expo);
abn_status abn_sin(abn *out, const abn *a);
abn_status abn_cos(abn *out, const abn *a);
abn_status abn_tan(abn *out, const abn *a);
abn_status abn_asin(abn *out, const abn *a);
abn_status abn_acos(abn *out, const abn *a);
abn_status abn_atan(abn *out, const abn *a);
abn_status abn_sinh(abn *out, const abn *a);
abn_status abn_cosh(abn *out, const abn *a);
abn_status abn_tanh(abn *out, const abn *a);
abn_status abn_asinh(abn *out, const abn *a);
abn_status abn_acosh(abn *out, const abn *a);
abn_status abn_atanh(abn *out, const abn *a);

/* Text form: [-|+]digits[.digits][E|e[-|+]digits], at least one mantissa digit. More than
   ABN_DIGITS significant digits round half to even. */
abn_status abn_from_text(abn *out, const char *s);
/* Writes a NUL-terminated text form readable by abn_from_text; returns its length. */
int abn_to_text(const abn *a, char *buf, int size);

/* The value as a sign, its significant digits and where they sit: digits[0] is the digit
   at 10^lead, trailing zeros removed, NUL-terminated (room for ABN_DIGITS + 1 bytes).
   Zero gives an empty string, neg false and lead 0. */
void abn_digits(const abn *a, bool *neg, char *digits, int *lead);

#endif
