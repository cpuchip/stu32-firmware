/* calc: the four-level RPN stack, number entry and the operations around it.
 *
 * Page references are to the user's guide named in the work unit and the commit that
 * introduced each behaviour ("UG p.2-6"). The behaviour is described in our own words.
 */
#ifndef CALC_H
#define CALC_H

#include <stdbool.h>
#include <stdint.h>
#include "abn.h"
#include "fmt.h"

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
    AB_ADD, AB_SUB, AB_MUL, AB_DIV,
    /* unit 003: one-argument functions, powers, trigonometry, percent */
    AB_EXP, AB_LN, AB_TENX, AB_LOG,
    AB_IDIV, AB_RMDR,
    AB_SQ, AB_SQRT, AB_INV, AB_POW, AB_XROOT,
    AB_PI, AB_DEG, AB_RAD, AB_GRAD,
    AB_SIN, AB_COS, AB_TAN, AB_ASIN, AB_ACOS, AB_ATAN,
    AB_SINH, AB_COSH, AB_TANH, AB_ASINH, AB_ACOSH, AB_ATANH,
    AB_PCT, AB_PCTCHG,
    /* unit 004: conversions, probability, parts of numbers, display setting */
    AB_HMS, AB_HMSINV, AB_TODEG, AB_TORAD,
    AB_TOKG, AB_TOLB, AB_TOC, AB_TOF, AB_TOCM, AB_TOIN, AB_TOL, AB_TOGAL, AB_TOKM, AB_TOMILE,
    AB_FACT, AB_NCR, AB_NPR, AB_RAND, AB_SEED,
    AB_IP, AB_FP, AB_INTG, AB_RND, AB_ABS, AB_SGN, AB_TOPOL, AB_TOREC,
    AB_CONST,               /* arg: index from ab_const_index */
    AB_SETFIX, AB_SETSCI, AB_SETENG, AB_SETALL,  /* arg: the digit setting, 0..33 (ALL takes none) */
    /* unit 005: storage; arg: a variable code (AB_VAR_A .. AB_VAR_Z, AB_VAR_IND_I, AB_VAR_IND_J) */
    AB_STO, AB_RCL,
    AB_STOADD, AB_STOSUB, AB_STOMUL, AB_STODIV,
    AB_RCLADD, AB_RCLSUB, AB_RCLMUL, AB_RCLDIV,
    AB_XVAR, AB_VIEW,
    AB_CLVARS,              /* no argument */
    /* unit 006: statistics */
    AB_SIGMAPLUS, AB_SIGMAMINUS, AB_CLSIGMA,
    AB_XBAR, AB_YBAR, AB_XWBAR, AB_SX, AB_SY, AB_PSX, AB_PSY,
    AB_XHAT, AB_YHAT, AB_CORR, AB_SLOPE, AB_YINT,
    AB_NSUM, AB_SUMX, AB_SUMY, AB_SUMX2, AB_SUMY2, AB_SUMXY,
    /* unit 007: fractions */
    AB_FDISP,               /* toggle Fraction-display mode */
    AB_SETC,                /* /c from x (1 recalls it) */
    AB_RCLC,                /* recall /c (ours: a separate token) */
    AB_SF, AB_CF,           /* set / clear flag arg (7, 8 or 9 in this unit) */
    /* unit 008: bases and logic */
    AB_DEC, AB_HEX, AB_OCT, AB_BIN,
    AB_KA, AB_KB, AB_KC, AB_KD, AB_KE_HEX, AB_KF,   /* hex digit keys A-F */
    AB_SUF,                 /* arg: 2, 8, 10 or 16, the base the typed digits are read in */
    AB_AND, AB_XOR, AB_OR, AB_NOT, AB_NAND, AB_NOR
} ab_op;

#define AB_VAR_A 0                  /* AB_VAR_A + k is the k-th letter; I and J are letters too */
#define AB_VAR_IND_I 26             /* (I): the variable or register the number in I names */
#define AB_VAR_IND_J 27             /* (J) */
#define AB_STAT_REGS 6              /* n, sum x, sum y, sum x^2, sum y^2, sum xy (UG p.14-22) */
#define AB_IND_MAX 800              /* unnamed indirect variables 0..800 (UG p.14-22) */

typedef enum { AB_ANGLE_DEG = 0, AB_ANGLE_RAD, AB_ANGLE_GRAD } ab_angle;

#define AB_ENTRY_MANT ABN_DIGITS    /* most mantissa digits accepted in entry */
#define AB_ENTRY_TEXT 80            /* room for a fraction: integer, numerator and denominator */
#define AB_ENTRY_EXP 4              /* most exponent digits accepted (provisional) */

typedef struct {
    bool active;                        /* a number is being keyed in (cursor present) */
    char text[AB_ENTRY_TEXT];   /* keys typed: digits, '.', 'E' */
    int len;
    bool mant_neg, exp_neg;
} ab_entry;

/* What a stack level holds (unit 009): a real number or a complex number; unit 010 adds vectors. */
typedef enum { AB_REAL = 0, AB_COMPLEX } ab_kind;

typedef struct {
    ab_kind kind;
    abn re, im;             /* a real number is re, with im zero */
} ab_val;

typedef struct {
    ab_val x, y, z, t;
    ab_val lastx;
    bool lift;              /* stack lift enabled */
    ab_angle angle;         /* angular mode for trigonometry */
    ab_mode disp_mode;      /* display setting, for RND (unit 004) */
    int disp_n;
    uint64_t rng;           /* random generator state (unit 004) */
    abn var[26];            /* A..Z (unit 005) */
    abn stat[AB_STAT_REGS]; /* the statistics registers, reached through (I)/(J) for now */
    abn ind[AB_IND_MAX + 1];/* unnamed indirect variables */
    int ind_top;            /* highest defined indirect address, -1 if none */
    bool fdisp;             /* Fraction-display mode (flag 7; unit 007) */
    uint32_t frac_c;        /* maximum denominator /c, 2..4095 */
    bool flag8, flag9;      /* fraction format flags (UG p.5-6) */
    int base;               /* 10, 16, 8 or 2 (unit 008) */
    ab_entry entry;
} ab_calc;

void ab_init(ab_calc *c);

/* Performs one operation. On an arithmetic error (divide by zero, out of range) the four
   registers and LAST X are left as they were and the error is returned; a number being
   keyed in is still completed. ABN_OK otherwise. The one exception is ABN_CLAMPED (base modes,
   unit 008): the result was replaced by the nearest end of the word range, and the operation
   completed with it. */
abn_status ab_do(ab_calc *c, ab_op op);

/* An operation that takes an argument: AB_CONST (a constant's index) and AB_FIX, AB_SCI, AB_ENG
   (the digit setting). ab_do(c, op) is ab_do_arg(c, op, 0). */
abn_status ab_do_arg(ab_calc *c, ab_op op, int arg);

/* The index of the constant named `name` (see core/const.c), or -1. */
int ab_const_index(const char *name);

#endif
