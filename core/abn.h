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
#define ABN_EXP_MIN (-6143) /* (the decimal128 normal range; beyond it, ABN_OVERFLOW and
                               ABN_UNDERFLOW, unit 026) */
#define ABN_TEXT_MAX 64

/* Opaque: 128 bits of engine-defined content. All bits zero is the value 0. */
typedef struct {
    uint64_t w[2];
} abn;

typedef enum {
    ABN_OK = 0,
    ABN_DIV_ZERO,           /* division by zero */
    ABN_RANGE,              /* a number out of range that is an error: a typed number, a working
                               store exhausted (unit 026 moved overflow and underflow out) */
    ABN_BAD_TEXT,           /* text is not a number */
    ABN_DOMAIN,             /* argument outside the function's real domain */
    ABN_INEXACT,            /* abn_mul_exact only: the product needed rounding */
    ABN_CLAMPED             /* base modes: the result was replaced by the nearest end of the word
                               range and the operation completed (unit 008 rule 3) */,
    /* calculator conditions (unit 011): the engine never returns these */
    ABN_NONEXISTENT,        /* GTO or XEQ to a label or line that does not exist (UG p.F-3) */
    ABN_XEQ_OVERFLOW,       /* a 21st nested XEQ (UG p.14-3, p.F-5) */
    ABN_DUP_LABEL,          /* a label that already exists (UG p.13-4, p.F-1) */
    ABN_MEMORY_FULL,        /* program memory full (UG p.F-3; capacity ours) */
    ABN_NONPROGRAMMABLE,    /* a key refused in program entry (UG p.13-24) */
    ABN_BUDGET,             /* a program ran AB_RUN_BUDGET lines without stopping (ours) */
    ABN_NO_ROOT,            /* SOLVE found no root: the stack holds its report (UG p.7-8; unit 013) */
    ABN_SELECT_FN,          /* SOLVE or integrate with no program chosen by FN= (UG p.F-4) */
    ABN_NESTED,             /* SOLVE, integrate or FN= while a SOLVE or integral runs (UG p.15-11) */
    ABN_SYNTAX,             /* an equation that does not parse (SYNTAX ERROR, UG p.6-19; unit 014) */
    ABN_INVALID_VAR,        /* SOLVE for a variable the equation does not have (INVALID VAR, p.F-2) */
    ABN_NO_SOLUTION,        /* a linear system with no solution (NO SOLUTION, p.F-3) */
    ABN_MULT_SOLUTION,      /* a linear system with many solutions (MULT SOLUTION, p.F-3) */
    ABN_INTERRUPTED,        /* a SOLVE or integral stopped by C or R/S (INTERRUPTED, unit 016) */
    ABN_BAD_IMAGE,          /* a state image that cannot be loaded: the device clears memory
                               (MEMORY CLEAR, UG p.B-3; unit 016) */
    /* the engine's again (unit 026; last, so the numbers above keep their meaning in a state image) */
    ABN_OVERFLOW,           /* a result beyond the largest finite value: the calculator gives the
                               largest number, sets flag 6 and warns OVERFLOW (33s p.1-16, F-3) */
    ABN_UNDERFLOW,          /* a nonzero result below the smallest normal value: the calculator
                               gives 0 (33s p.1-16) */
    ABN_RPN_ONLY,           /* a key with no meaning on ALG's expression line: shown briefly, nothing
                               done, not an error (unit 029 rule 7) */
    ABN_STU_ONLY,           /* unit 032: an STU feature asked in 33s or 35s mode (TABLE): shown briefly */
    ABN_NO_EQN,             /* unit 032: TABLE with no equation shown (EQN LIST TOP, a built-in solver) */
    ABN_NO_VAR              /* unit 032: TABLE of an equation with no variable (abacus's answer 3) */
} abn_status;

abn abn_zero(void);
abn abn_largest(void);                      /* 9.99...9E6144, 34 nines: what an overflow gives (unit 026) */
/* The sign of the infinite result behind the last ABN_OVERFLOW the engine returned: the true sign
   where that rounding was the result's only one (an exact value rounded once); a function that forms
   |result| first, or returns ABN_OVERFLOW itself, has its sign from its own rule (unit 026). */
bool abn_overflow_negative(void);
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
abn_status abn_log1p(abn *out, const abn *a);  /* ln(1 + a), a > -1 (unit 009) */
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

/* unit 004 */
abn_status abn_trunc(abn *out, const abn *a);           /* integer part, toward zero (exact) */
abn_status abn_floor(abn *out, const abn *a);           /* greatest integer <= a (exact) */
abn_status abn_hypot(abn *out, const abn *x, const abn *y);
abn_status abn_atan2(abn *out, const abn *y, const abn *x);    /* radians, in (-pi, pi] */

/* Text form: [-|+]digits[.digits][E|e[-|+]digits], at least one mantissa digit. More than
   ABN_DIGITS significant digits round half to even. */
abn_status abn_from_text(abn *out, const char *s);
/* As abn_from_text, but a zero keeps the sign and exponent written (abn_from_text gives every zero
   as abn_zero()): for state images, which must give back exactly the number saved (unit 016). */
abn_status abn_from_text_exact(abn *out, const char *s);
/* Writes a NUL-terminated text form readable by abn_from_text; returns its length. */
int abn_to_text(const abn *a, char *buf, int size);

/* The value as a sign, its significant digits and where they sit: digits[0] is the digit
   at 10^lead, trailing zeros removed, NUL-terminated (room for ABN_DIGITS + 1 bytes).
   Zero gives an empty string, neg false and lead 0. */
void abn_digits(const abn *a, bool *neg, char *digits, int *lead);

#endif
