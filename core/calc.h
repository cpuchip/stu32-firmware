/* calc: the four-level RPN stack, number entry and the operations around it.
 *
 * Page references are to the user's guide named in the work unit and the commit that
 * introduced each behaviour ("UG p.2-6"). The behaviour is described in our own words.
 */
#ifndef CALC_H
#define CALC_H

#include <stdbool.h>
#include "abn.h"

typedef enum {
    AB_K0, AB_K1, AB_K2, AB_K3, AB_K4, AB_K5, AB_K6, AB_K7, AB_K8, AB_K9,
    AB_KPOINT,              /* decimal point */
    AB_KE,                  /* start the exponent of ten */
    AB_KCHS,                /* change sign */
    AB_KBKSP,               /* backspace */
    AB_ENTER,
    AB_RDN, AB_RUP,         /* roll down, roll up */
    AB_XY,                  /* exchange X and Y */
    AB_LASTX,
    AB_CLX, AB_CLSTK,       /* clear X, clear the stack */
    AB_ADD, AB_SUB, AB_MUL, AB_DIV
} ab_op;

#define AB_ENTRY_MANT ABN_DIGITS    /* most mantissa digits accepted in entry */
#define AB_ENTRY_EXP 4              /* most exponent digits accepted (provisional) */

typedef struct {
    bool active;                        /* a number is being keyed in (cursor present) */
    char text[AB_ENTRY_MANT + AB_ENTRY_EXP + 4];  /* keys typed: digits, '.', 'E' */
    int len;
    bool mant_neg, exp_neg;
} ab_entry;

typedef struct {
    abn x, y, z, t;
    abn lastx;
    bool lift;              /* stack lift enabled */
    ab_entry entry;
} ab_calc;

void ab_init(ab_calc *c);

/* Performs one operation. On an arithmetic error (divide by zero, out of range) the four
   registers and LAST X are left as they were and the error is returned; a number being
   keyed in is still completed. ABN_OK otherwise. */
abn_status ab_do(ab_calc *c, ab_op op);

#endif
