/* dref: our own decimal reference engine, a TEST AID only.
 *
 * Written before the shipped engine was chosen; the core uses abn.h (backed by the Intel
 * decimal library). dref is an independent implementation of the same arithmetic,
 * DREF_DIGITS significant digits, every result rounded half-to-even, so the tests can
 * compare the shipped engine against a second one that shares no code with it.
 */
#ifndef DREF_H
#define DREF_H

#include <stdbool.h>
#include <stdint.h>

#define DREF_DIGITS 34       /* significant digits kept in every result */
#define DREF_EXP_MAX 9999   /* largest |exponent| of the leading digit */
#define DREF_TEXT_MAX (DREF_DIGITS + 16)

typedef struct {
    uint8_t neg;            /* 1 when negative; zero is never negative */
    uint8_t len;            /* coefficient digits in use; 0 means the value is zero */
    int32_t exp;            /* value = (-1)^neg * coefficient * 10^exp */
    uint8_t d[DREF_DIGITS];  /* coefficient digits, least significant first; d[0] != 0 */
} dref;

typedef enum {
    DREF_OK = 0,
    DREF_DIV_ZERO,           /* division by zero */
    DREF_RANGE,              /* result exponent beyond DREF_EXP_MAX */
    DREF_BAD_TEXT            /* text is not a number */
} dref_status;

dref dref_zero(void);
bool dref_is_zero(const dref *a);
int dref_cmp(const dref *a, const dref *b);    /* -1, 0, 1 by value */
dref dref_neg(const dref *a);

dref_status dref_add(dref *out, const dref *a, const dref *b);
dref_status dref_sub(dref *out, const dref *a, const dref *b);   /* a - b */
dref_status dref_mul(dref *out, const dref *a, const dref *b);
dref_status dref_div(dref *out, const dref *a, const dref *b);   /* a / b */

/* Text form: [-]digits[.digits][E[-|+]digits], at least one mantissa digit. */
dref_status dref_from_text(dref *out, const char *s);
/* Writes a NUL-terminated text form readable by dref_from_text; returns its length. */
int dref_to_text(const dref *a, char *buf, int size);

#endif
