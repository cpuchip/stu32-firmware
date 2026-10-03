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
    AB_AND, AB_XOR, AB_OR, AB_NOT, AB_NAND, AB_NOR,
    /* unit 009: complex numbers */
    AB_KI,                  /* the i between the parts of x i y */
    AB_KTHETA,              /* the theta between r and a of polar entry */
    AB_XIY, AB_RTA,         /* the complex display form */
    AB_ARG,
    /* unit 010: vectors */
    AB_KVEC,                /* [ ]: begins a vector */
    AB_KCOMMA,              /* , : the next element */
    /* unit 011: keystroke programs. A target (XEQ, GTO, GTOPOS) is label * 1000 + line, the label
       0..25 for A..Z, the line 1..999 (0: the label's first line); GTOPOS -1 is PRGM TOP. */
    AB_PRGM,                /* program entry on or off (nonprogrammable) */
    AB_LBL,                 /* arg: the label 0..25 */
    AB_RTN,
    AB_XEQ, AB_GTO,         /* arg: a target */
    AB_GTOPOS,              /* GTO . label nnn / GTO . . : moves the pointer (nonprogrammable) */
    AB_RS,                  /* R/S: runs from the pointer; in program entry it records STOP */
    AB_PSE,
    AB_SST, AB_BST,         /* one line down (outside program entry: executes it) / one line up */
    AB_CLPGM,               /* clear all programs (nonprogrammable) */
    AB_TEST,                /* arg: AB_TEST_* relation, + 6 for the tests against 0 */
    AB_FS,                  /* FS? arg: a flag 0..11 */
    AB_ISG, AB_DSE,         /* arg: a variable code */
    AB_LIT,                 /* internal: a program line holding a typed number (not a key) */
    /* unit 012: programs, part 2 */
    AB_INPUT,               /* arg: a variable code */
    AB_CANCEL               /* the C key: clears a number being typed, else cancels a prompt */
} ab_op;

/* Why the last run stopped (unit 012 rule 1): the device draws it; the core never waits. */
typedef enum {
    AB_STOP_NONE = 0,       /* no run yet, or a keyboard GTO/XEQ setup */
    AB_STOP_DONE,           /* finished: a top-level RTN or past the last line */
    AB_STOP_STOP,           /* at a STOP line (R/S resumes) */
    AB_STOP_INPUT,          /* at INPUT: prompting for stop_var (R/S stores X there and resumes) */
    AB_STOP_VIEW,           /* at VIEW: showing stop_var (R/S resumes) */
    AB_STOP_PSE,            /* at PSE: showing X, or stop_var after a VIEW; the device resumes
                               after its pause (about 1 s, UG p.13-19) */
    AB_STOP_CANCELLED,      /* a prompt cancelled with C: R/S prompts again (UG p.13-15) */
    AB_STOP_ERROR,          /* an error at stop_line (the pointer is there) */
    AB_STOP_BUDGET          /* AB_RUN_BUDGET lines without stopping */
} ab_stop;

enum { AB_TEST_NE = 0, AB_TEST_LE, AB_TEST_LT, AB_TEST_GT, AB_TEST_GE, AB_TEST_EQ };   /* UG p.14-7 order */

#define AB_VAR_A 0                  /* AB_VAR_A + k is the k-th letter; I and J are letters too */
#define AB_VAR_IND_I 26             /* (I): the variable or register the number in I names */
#define AB_VAR_IND_J 27             /* (J) */
#define AB_STAT_REGS 6              /* n, sum x, sum y, sum x^2, sum y^2, sum xy (UG p.14-22) */
#define AB_IND_MAX 800              /* unnamed indirect variables 0..800 (UG p.14-22) */
#define AB_PROG_LINES 4000          /* program memory, lines (ours, unit 011 rule 4) */
#define AB_PROG_LITS 500            /* of which typed numbers (each holds a whole stack value) */
#define AB_LABEL_LINES 999          /* lines a label may own: the address form A001..A999 (UG p.13-4) */
#define AB_RTN_DEPTH 20             /* nested subroutines below the top level (UG p.14-2) */
#define AB_RUN_BUDGET 10000000L     /* lines one run may execute (ours, unit 011 rule 2) */
#define AB_PC_TOP (-1)              /* the program pointer at PRGM TOP */
#define AB_TGT_IND_I (-2)           /* XEQ (I), GTO (I): the label numbered by I (unit 012) */
#define AB_TGT_IND_J (-3)

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

/* What a stack level holds: a real number, a complex number (unit 009) or a vector of 1 to 3
   elements (unit 010). A register has three parts (UG p.2-2): a real fills the first, a complex
   number the first two, a vector one per element. */
typedef enum { AB_REAL = 0, AB_COMPLEX, AB_VECTOR } ab_kind;

typedef struct {
    ab_kind kind;
    abn re, im;             /* parts 1 and 2: a real number is re, with im zero */
    abn p3;                 /* part 3: a 3-D vector's third element */
    int len;                /* a vector's element count, 1..3 (0 otherwise) */
} ab_val;

typedef struct {
    ab_val x, y, z, t;
    ab_val lastx;
    bool lift;              /* stack lift enabled */
    ab_angle angle;         /* angular mode for trigonometry */
    ab_mode disp_mode;      /* display setting, for RND (unit 004) */
    int disp_n;
    uint64_t rng;           /* random generator state (unit 004) */
    ab_val var[26];         /* A..Z (unit 005); each may hold a complex number (UG p.9-2, unit 009) */
    abn stat[AB_STAT_REGS]; /* the statistics registers, reached through (I)/(J) for now */
    abn ind[AB_IND_MAX + 1];/* unnamed indirect variables */
    int ind_top;            /* highest defined indirect address, -1 if none */
    bool fdisp;             /* Fraction-display mode (flag 7; unit 007) */
    uint32_t frac_c;        /* maximum denominator /c, 2..4095 */
    bool flag8, flag9;      /* fraction format flags (UG p.5-6) */
    int base;               /* 10, 16, 8 or 2 (unit 008) */
    bool cpolar;            /* complex numbers shown as r theta a (unit 009) */
    ab_entry entry;         /* the number being typed, or its second part after i or theta */
    ab_entry entry1;        /* the first part, once i or theta has been pressed */
    int cpart;              /* 0 a real entry, 1 typing y of x i y, 2 typing a of r theta a */
    ab_entry ventry[2];     /* a vector's finished elements while the next is typed (unit 010) */
    int vlen;               /* 0, or the elements of the vector being typed, 1..3 */
    /* unit 011: program memory (UG ch.13, 14) */
    struct { uint16_t op; int32_t arg; } prog[AB_PROG_LINES];   /* AB_LIT: arg indexes lit[] */
    int prog_len;
    ab_val lit[AB_PROG_LITS];
    bool lit_used[AB_PROG_LITS];
    int pc;                 /* the program pointer: a line index, or AB_PC_TOP */
    int rstack[AB_RTN_DEPTH];   /* return addresses of the subroutines being run */
    int rdepth;
    bool prgm;              /* program entry */
    ab_val plit;            /* in program entry, the number being keyed as a line */
    int labelpos[26];       /* the line of each label, -1 if none (rebuilt after each edit) */
    uint16_t flags;         /* flags 0..6, 10, 11 (7, 8, 9 are fdisp, flag8, flag9) */
    bool yes;               /* the last test's answer, shown as YES / NO from the keyboard */
    long budget;            /* lines a run may execute: AB_RUN_BUDGET unless a device changes it */
    ab_stop stop;           /* why the last run stopped (unit 012) */
    int stop_var;           /* INPUT / VIEW / PSE: the variable (A..Z 0..25, or -27..-32, or
                               1000 + an unnamed address); -1 for none */
    int stop_line;          /* the line an error stopped at */
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

/* Program memory, for displays and tests (unit 011). The address of line i ("A005", "0003" for a
   line before any label, "TOP" for AB_PC_TOP) into buf (at least 8 bytes); the index of the line
   with address `addr`, or -1; a flag's state. */
void ab_prog_addr(const ab_calc *c, int i, char *buf);
int ab_prog_find(const ab_calc *c, const char *addr);
bool ab_flag(const ab_calc *c, int n);
/* The stop state as text, for tests: DONE, STOP, INPUT:R, VIEW:V, PSE, PSE:V, CANCELLED, ERROR,
   BUDGET, NONE (an unnamed variable as #n). */
void ab_stop_text(const ab_calc *c, char *buf);

/* The index of the constant named `name` (see core/const.c), or -1. */
int ab_const_index(const char *name);

#endif
