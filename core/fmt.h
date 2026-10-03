/* fmt: a number as the text of one display line (FIX, SCI, ENG, ALL).
 *
 * A pure function of the value and the settings; it asks the engine only for the value's
 * sign, digits and exponent (abn_digits) and does its rounding on those digits, half away
 * from zero (display rounding; the engine's arithmetic rounding is separate).
 */
#ifndef FMT_H
#define FMT_H

#include <stdbool.h>
#include "abn.h"

typedef enum { AB_FIX, AB_SCI, AB_ENG, AB_ALL } ab_mode;

#define AB_FMT_N_MAX 33     /* digit setting 0..33 (ours, unit 002) */

typedef struct {
    ab_mode mode;
    int n;                  /* digit setting; ignored for ALL */
    int width;              /* most characters the text may take; every character counts */
    char radix;             /* '.' or ',' */
    bool separator;         /* thousands separator in the integer part */
} ab_fmt;

/* Writes the text (NUL-terminated) and returns its length. Returns -1 and writes "" when
   not even the shortest form of the value fits the width. */
int ab_format(const abn *v, const ab_fmt *f, char *out, int size);

/* unit 007: the whole display setting, decimal or fraction. */
typedef struct {
    ab_fmt dec;             /* the decimal format: in effect, or the one Fraction-display mode restores */
    bool fdisp;             /* Fraction-display mode */
    unsigned c;             /* maximum denominator, 1..4095 */
    int frac_format;        /* 0 most precise, 1 factors of c, 2 fixed denominator c */
} ab_display;

/* v as the text of one line under the display setting; *indicator is +1 when |v| lies above the
   shown fraction, -1 below, 0 when exact or when decimal notation is shown. A fraction that does
   not fit the width, or whose integer part needs more than 34 digits, falls back to the decimal
   format with no indicator (ours, unit 007). Returns the length, or -1 and "" if nothing fits. */
int ab_format_display(const abn *v, const ab_display *d, char *out, int size, int *indicator);

#endif
