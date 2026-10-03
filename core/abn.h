/* abn: the number interface the calculator core is written against.
 *
 * The core never touches the fields of `abn`; it only calls these functions, so the
 * engine behind them can be replaced. The engine in this tree is abn_dec.c: decimal,
 * ABN_DIGITS significant digits, every result rounded half-to-even, no binary floating
 * point anywhere.
 */
#ifndef ABN_H
#define ABN_H

#include <stdbool.h>
#include <stdint.h>

#define ABN_DIGITS 34       /* significant digits kept in every result */
#define ABN_EXP_MAX 9999    /* largest |exponent| of the leading digit (provisional; range
                               and overflow behaviour arrive with a later unit) */
#define ABN_TEXT_MAX (ABN_DIGITS + 16)

typedef struct {
    uint8_t neg;            /* 1 when negative; zero is never negative */
    uint8_t len;            /* coefficient digits in use; 0 means the value is zero */
    int32_t exp;            /* value = (-1)^neg * coefficient * 10^exp */
    uint8_t d[ABN_DIGITS];  /* coefficient digits, least significant first; d[0] != 0 */
} abn;

typedef enum {
    ABN_OK = 0,
    ABN_DIV_ZERO,           /* division by zero */
    ABN_RANGE,              /* result exponent beyond ABN_EXP_MAX */
    ABN_BAD_TEXT            /* text is not a number */
} abn_status;

abn abn_zero(void);
bool abn_is_zero(const abn *a);
int abn_cmp(const abn *a, const abn *b);    /* -1, 0, 1 by value */
abn abn_neg(const abn *a);

abn_status abn_add(abn *out, const abn *a, const abn *b);
abn_status abn_sub(abn *out, const abn *a, const abn *b);   /* a - b */
abn_status abn_mul(abn *out, const abn *a, const abn *b);
abn_status abn_div(abn *out, const abn *a, const abn *b);   /* a / b */

/* Text form: [-]digits[.digits][E[-|+]digits], at least one mantissa digit. */
abn_status abn_from_text(abn *out, const char *s);
/* Writes a NUL-terminated text form readable by abn_from_text; returns its length. */
int abn_to_text(const abn *a, char *buf, int size);

#endif
