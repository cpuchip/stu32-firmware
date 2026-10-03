/* frac: the most accurate fraction for a value (unit 007).
 *
 * A value v = I + n/d with I its integer part (toward zero) and n/d chosen, exactly against v's
 * 34 digits, by the format: FR_PRECISE the closest fraction with denominator <= c, reduced;
 * FR_FACTORS the closest whose denominator divides c, reduced; FR_FIXED the closest k/c, not
 * reduced. Ties (exactly halfway): the smaller denominator, then the larger magnitude.
 */
#ifndef FRAC_H
#define FRAC_H

#include <stdbool.h>
#include <stdint.h>

#include "abn.h"

#define FR_C_MAX 4095

typedef enum { FR_PRECISE = 0, FR_FACTORS, FR_FIXED } fr_format;

typedef struct {
    bool neg;                   /* the value is negative */
    char ip[ABN_DIGITS + 2];    /* integer part of |v| (after any carry), decimal digits */
    uint32_t n, d;              /* the fraction of |v|: 0 <= n < d, or n == 0 */
    int indicator;              /* +1: |v| lies above the shown value; -1: below; 0: exact */
} fr_result;

/* Fails (ABN_RANGE) when the integer part of |v| has more than 34 digits. */
abn_status fr_approx(fr_result *out, const abn *v, uint32_t c, fr_format f);

/* The value of the fraction fr_approx shows, (-1)^neg (ip + n/d), rounded once to 34 digits. */
abn_status fr_value(abn *out, const fr_result *r);

/* (-1)^neg (int + num/den) for decimal digit strings (a typed fraction), rounded once. den must
   not be all zeros. */
abn_status fr_from_parts(abn *out, bool neg, const char *int_digits, const char *num_digits,
                         const char *den_digits);

#endif
