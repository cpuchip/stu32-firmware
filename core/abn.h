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
    ABN_BAD_TEXT            /* text is not a number */
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
