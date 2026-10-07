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
    AB_CANCEL,              /* the C key: clears a number being typed, else cancels a prompt */
    /* unit 013: SOLVE and integrate on programmed functions */
    AB_FN,                  /* FN= label: arg 0..25, or AB_TGT_IND_I / J */
    AB_SOLVE,               /* arg: a variable code (the unknown) */
    AB_INTEG,               /* integrate: arg the variable of integration; limits in Y, X */
    /* unit 014: equations */
    AB_EQN,                 /* Equation mode on or off */
    AB_KEQ,                 /* the = key (types = in an equation) */
    AB_KPAREN,              /* the ( ) key (types a pair, the cursor inside) */
    AB_KLEFT, AB_KRIGHT,    /* the cursor keys < and > in an equation */
    /* unit 015 */
    AB_EQLINE,              /* an equation as a program line: arg indexes peqn[] (UG p.13-7) */
    AB_KSPACE,              /* SPACE: a blank in an equation (UG p.G-13) */
    AB_KREG,                /* in an equation, R-down's menu choice: arg 0..3 types REGX..REGT
                               (UG p.B-7) */
    /* unit 017 */
    AB_MODEL,               /* the 33s/35s mode: arg 33 or 35 (a device menu; not a program line) */
    /* unit 021 */
    AB_EXPM1, AB_LN1P,      /* e^x - 1 and ln(1 + x), accurate where x is tiny (ours) */
    AB_TWOX, AB_LOG2,       /* 2^x and log2 x (ours) */
    AB_CLVARX,              /* arg: an address 0..800; clears the indirect variables above it
                               (UG p.1-5) */
    AB_CLALL,               /* CLEAR ALL (UG p.1-29, B-3): data cleared, modes kept; nonprogrammable */
    /* unit 019 */
    AB_FRACFMT,             /* the fraction format, flags 8 and 9 at once (UG p.5-6): arg 0 precise
                               (8 clear), 1 factors (8 set, 9 clear), 2 fixed (both set); a device
                               menu setting, nonprogrammable */
    AB_CLEQN,               /* clear the equations, the built-in solvers kept (UG p.1-5: CLEAR's
                               EQN in Equation mode); nonprogrammable (unit 021, review) */
    AB_EQTOP,               /* in Equation mode, the list to EQN LIST TOP (unit 020's NEW, ours);
                               nothing elsewhere, and not a program line */
    AB_BEEP,                /* one short tone, the stack untouched; programmable, in both modes
                               (ours, unit 027 rule 6: none of the three machines has one). The core
                               counts it in beeps; the device sounds it */
    /* unit 029: STU's ALG (abacus decision 51) */
    AB_ENTRYM,              /* arg 0 RPN, 1 ALG (MODE's F4, F5): ALG in STU only; nonprogrammable */
    AB_ANS,                 /* types ANS on the expression line (LASTx's key in ALG) */
    AB_HUP, AB_HDOWN,       /* the expression line's history, back and forward (the bar's up, down) */
    AB_SHOW,                /* unit 030: SHOW, held: X's full mantissa (ab_show_text); neutral, changes
                               nothing, nonprogrammable */
    /* unit 032: TABLE (STU): GO opens the table of the equation shown; its variable (arg a variable code),
       start and step (from X); a row up, down. Nonprogrammable. */
    AB_TABLE, AB_TVAR, AB_TSTART, AB_TSTEP, AB_TUP, AB_TDOWN,
    /* unit 033: GRAPH (STU): GO; the window's XMIN XMAX YMIN YMAX (from X); AUTO; the trace's left, right */
    AB_GRAPH, AB_XMIN, AB_XMAX, AB_YMIN, AB_YMAX, AB_GAUTO, AB_GLEFT, AB_GRIGHT,
    AB_CMPLX,               /* unit 039: the 33s's CMPLX operation on stack pairs, arg AB_CXP_* */
    /* unit 040: the open graph's tools (after the 50g's plot menus): a zoom (arg AB_Z_*), the cursor moved (arg a
       numpad digit 1..9, plus 10 for ten pixels), TRACE on or off, MARK the cursor, the zoom factor (arg 1..100),
       a function tool at the cursor (arg AB_F_*) */
    AB_GZOOM, AB_GMOVE, AB_GTRACE, AB_GMARK, AB_GFACT, AB_GFCN
} ab_op;

#define AB_OP_LAST AB_GFCN     /* the last op: loops over every op end here */

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
    AB_STOP_BUDGET,         /* AB_RUN_BUDGET lines without stopping */
    AB_STOP_MSG,            /* showing an equation line as a message, flag 10 set: the line at
                               stop_line (R/S resumes after it; UG p.13-16, 14-11) */
    AB_STOP_SLICE           /* the device's slice of lines ran out (unit 016): ab_continue goes
                               on as if nothing had happened; ab_interrupt makes it a STOP */
} ab_stop;

enum { AB_TEST_NE = 0, AB_TEST_LE, AB_TEST_LT, AB_TEST_GT, AB_TEST_GE, AB_TEST_EQ };   /* UG p.14-7 order */

#define AB_VAR_A 0                  /* AB_VAR_A + k is the k-th letter; I and J are letters too */
#define AB_VAR_IND_I 26             /* (I): the variable or register the number in I names */
#define AB_VAR_IND_J 27             /* (J) */
#define AB_VAR_SI 28                /* i: the 33s's 27th variable, separate from the letter I (33s
                                       p.13-20; unit 025); it lives at var[26] */
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
#define AB_TGT_EQN (-5)             /* XEQ in Equation mode: evaluate the equation shown (unit 014) */
/* unit 039: AB_CMPLX's operations (33s p.9-3): z1 = Z + iT, z2 = X + iY; the result real in X, imaginary in Y */
#define AB_CXP_ADD 0
#define AB_CXP_SUB 1
#define AB_CXP_MUL 2
#define AB_CXP_DIV 3
#define AB_CXP_POW 4                /* z1^z2 */
#define AB_CXP_CHS 5                /* the one-number operations from here */
#define AB_CXP_INV 6
#define AB_CXP_LN 7
#define AB_CXP_EXP 8
#define AB_CXP_SIN 9
#define AB_CXP_COS 10
#define AB_CXP_TAN 11
#define AB_CXP_N 12
/* unit 040: AB_GZOOM's zooms and AB_GFCN's tools */
enum { AB_Z_IN, AB_Z_OUT, AB_Z_BOX, AB_Z_CNTR, AB_Z_AUTO, AB_Z_HIN, AB_Z_HOUT, AB_Z_VIN, AB_Z_VOUT, AB_Z_LAST,
       AB_Z_DFLT, AB_Z_DECI, AB_Z_INTG, AB_Z_SQR, AB_Z_TRIG, AB_Z_N };
enum { AB_F_ROOT, AB_F_SLOPE, AB_F_EXTR, AB_F_AREA, AB_F_FX, AB_F_TANL, AB_F_N };
#define AB_GRAPH_CROW 92            /* the cursor's home row, the plot's middle (184 / 2) */
#define AB_EQN_MAX 64               /* equations in the list (ours, unit 014 rule 5) */
#define AB_EQN_LEN 192              /* bytes of one equation's text (UTF-8, as the guide shows it) */
#define AB_HIST 16                  /* the expression line's history, at least 16 (unit 029 rule 5) */
#define AB_MODEL_STU 1              /* AB_MODEL's arg for STU (unit 029); a label stamped STU runs as 35 */
#define AB_PEQN_MAX 64              /* equation lines in programs, a store of their own (ours, unit
                                       015 rule 4: they are not in the list, UG p.13-7) */

typedef enum { AB_ANGLE_DEG = 0, AB_ANGLE_RAD, AB_ANGLE_GRAD } ab_angle;

#define AB_ENTRY_MANT ABN_DIGITS    /* most mantissa digits accepted in entry */
#define AB_ENTRY_TEXT 80            /* room for a fraction: integer, numerator and denominator */
#define AB_ENTRY_EXP 4              /* most exponent digits accepted (provisional) */

typedef struct {
    bool active;                        /* a number is being keyed in (cursor present) */
    char text[AB_ENTRY_TEXT];   /* keys typed: digits, '.', 'E' */
    int len;
    bool mant_neg, exp_neg;
    int back;                   /* keys after the insertion point: 0 (typing at the end) but after
                                   the 35s's SYNTAX ERROR for E with no mantissa, which puts the
                                   point at the start (unit 017) */
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
    ab_val var[27];         /* A..Z (unit 005), each may hold a complex number (UG p.9-2, unit 009);
                               and i at 26 (unit 025) */
    uint8_t lmode[26];      /* each label's mode, 33 or 35, stamped when its LBL line was entered
                               (unit 025 rule 3); 0 for a label not in memory */
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
    int stop_var;           /* INPUT / VIEW / PSE: the variable (A..Z 0..25, i 26, or -27..-32, or
                               1000 + an unnamed address); -1 for none */
    bool viewing;           /* a keyboard VIEW's display, Variable=Value (UG p.3-4), until the next key:
                               C or backspace only clears it, any other key clears it and acts
                               (abacus #3979); not in the state image */
    int view_id;            /* its variable, as stop_var's ids (A..Z 0..25, i 26, 1000 + an indirect
                               address, -27..-32 the statistics) */
    int stop_line;          /* the line an error stopped at */
    int fn;                 /* FN=: the label chosen (0..25), -1 none (unit 013) */
    int active;             /* 0, or AB_SOLVE / AB_INTEG while its function is being evaluated */
    int solve_var;          /* the unknown (or variable of integration) as a stop_var id */
    int rbase;              /* return depth that counts as the top level of the current run */
    long evals;             /* function evaluations used by the last SOLVE or integral */
    /* unit 014: the equation list and Equation mode */
    char eqn[AB_EQN_MAX + 2][AB_EQN_LEN];   /* the list, with the two built-in linear solvers
                                               (unit 015: text "\x01" then their LCD name) */
    int eqn_count;          /* entries, the built-ins included */
    /* unit 032: TABLE's settings (in the image): the variable (a variable code, -1 the equation's first),
       the start (set by START since the variable changed, else the variable's value at GO), the step;
       and the open table: its equation and the selected row k (rows k-3..k shown, row k = start + k x step) */
    int tbl_var;
    int tbl_v;              /* the open table's variable (0..25) */
    bool tbl_start_set;
    abn tbl_start, tbl_step;
    bool tbl_on;
    int tbl_eq;
    int32_t tbl_k;
    /* unit 033: GRAPH's window (in the image): X from gr_xmin to gr_xmax, Y set (gr_ymin..gr_ymax) or fitted
       (gr_auto); the open graph: its equation, variable and trace column */
    abn gr_xmin, gr_xmax, gr_ymin, gr_ymax;
    bool gr_auto;
    bool gr_on;
    int gr_eq, gr_v;
    int gr_cur;
    /* unit 040 (in the image): the cursor's row (gr_cur its column), TRACE (the cursor on the curve), the mark,
       the zoom factors, ZLAST's window, and the last tool's result (gr_res_kind AB_F_* + 1, 0 none; TANL's line
       y = gr_res x + gr_res_b) */
    int gr_row;
    bool gr_trace;
    bool gr_mark;
    int gr_mcol, gr_mrow;
    abn gr_hf, gr_vf;
    bool gr_has_last, gr_last_auto;
    abn gr_lxmin, gr_lxmax, gr_lymin, gr_lymax;
    int gr_res_kind;
    abn gr_res, gr_res_b;
    uint8_t eqn_model[AB_EQN_MAX + 2];      /* unit 031: the model each was typed in (33, 35, AB_MODEL_STU):
                                               its meaning wherever it is evaluated (rule 3); in the image */
    int eqn_cur;            /* the equation shown, -1 at EQN LIST TOP */
    bool eqn_mode;          /* Equation mode */
    bool eqn_typing;        /* an equation is being typed or edited */
    int eqn_edit;           /* the index being edited, or -1 for a new equation */
    char ebuf[AB_EQN_LEN];  /* the text being typed */
    int ecur;               /* the cursor: a byte offset into ebuf */
    int eqn_errpos;         /* SYNTAX ERROR: the character offset of the first error */
    int eqn_pending;        /* prompting for: 1 ENTER, 2 XEQ, 3 SOLVE, 4 integrate, 5 the 2*2 and
                               6 the 3*3 solver (unit 015); 0 none */
    int eqn_vars[27];       /* the variables to prompt for, in order (26: i, unit 025) */
    int eqn_nvars, eqn_next;
    /* unit 015: equations in programs; SOLVE and integrate of equations */
    char peqn[AB_PEQN_MAX][AB_EQN_LEN];     /* the texts of AB_EQLINE program lines */
    bool peqn_used[AB_PEQN_MAX];
    uint8_t peqn_model[AB_PEQN_MAX];        /* unit 031: each line's model, as eqn_model */
    int peq_line;           /* the equation line whose flag-11 prompts are pending, -1 none */
    bool peq_ready;         /* its prompts are answered: evaluate it when the line runs again */
    int fn_eqn;             /* the function SOLVE / integrate evaluate is equation fn_eqn of the
                               list, -1 for the FN= program */
    int solve_phase;        /* 1 while the first evaluation runs (it may prompt), 2 after */
    int running;            /* prog_run calls in progress (a SOLVE line is in a program) */
    bool ev_cached;         /* the first evaluation's result, used by the search's first call */
    abn ev_cx, ev_cf;
    abn_status ev_cst;
    struct {                /* a SOLVE or integral waiting on its first evaluation's prompts, or
                               on an equation's prompts (kind 0: none) */
        int kind;           /* AB_SOLVE or AB_INTEG */
        int var;            /* the unknown, a stop_var id */
        int code;           /* its target code as keyed */
        ab_val x, y, z, t, lastx, var0;
        bool lift;
        int pc, rbase, rdepth;  /* where the run that keyed it stood */
        bool from_prog;     /* keyed by a program line at pc */
    } susp;
    ab_val ev_y, ev_z, ev_t;    /* the stack each SOLVE / integrate evaluation starts from */
    /* unit 016: the device's view of a run */
    long slice;             /* lines a program run may take per call before AB_STOP_SLICE; 0 none */
    long run_lines;         /* lines the current run has taken, against budget, across slices */
    volatile bool intr;     /* set by ab_interrupt while a run is in progress (C or R/S pressed) */
    long intr_after;        /* for tests: an interrupt arriving after this many more lines or
                               equation evaluations (0: none), as a device's would mid-call */
    /* unit 017: where Michael's 33s and 35s differ, both are modes, the 33s the default */
    bool m35;               /* the 35s's behaviour (abacus decisions section 22) */
    bool msg;               /* an error message is showing: the key after it acts (33s) or only
                               clears it (35s) */
    /* unit 018: what the message says (ab_error_message): the operation that failed, innermost
       first (a function inside an equation, a program line, else the key), its argument, its X
       operand and the SOLVE / integrate in progress then */
    int err_op, err_arg, err_active;
    ab_val err_x;
    bool err_set;           /* recorded already in this call (the innermost wins) */
    int ovf;                /* overflows since the key began (unit 026): flag 5's stop, the device's
                               OVERFLOW notice; not in the state image */
    uint16_t beeps;         /* BEEP instructions done, counting up (unit 027): the device sounds one
                               tone per new one; not in the state image */
    /* unit 029: STU, a personality that behaves as 35s mode (m35 is set with it), and its ALG: an
       expression line in ebuf (the equation editor's), = evaluating it into ans */
    bool stu;               /* STU mode (MODE's F3); in the state image */
    bool alg;               /* ALG entry (MODE's F5, STU only); in the state image */
    ab_val ans;             /* ANS: the last result, 0 to begin; in the state image */
    bool alg_eval;          /* the line is being evaluated: the parser knows ANS (not in the image) */
    bool alg_shown;         /* the line shows its result: the next digit or function starts a new
                               line, an operator a new line after ANS */
    bool err_m35;           /* the mode in force when it was recorded (unit 025: INVALID (I) or (i)) */
    int exec_at;            /* the program line being executed while running (unit 025: its label's
                               mode is in force), -1 none */
    abn_status err_status;  /* the status the message is for */
    /* unit 029: the line's history, in RAM only: not in the state image (rule 5). The struct's tail,
       so that an image loaded keeps it by clearing only what is before it (state.c) */
    char hist[AB_HIST][AB_EQN_LEN];
    ab_val hist_val[AB_HIST];
    int hist_n;             /* entries kept, the newest at hist_n - 1 */
    int hist_at;            /* the entry shown by up and down, -1 none */
} ab_calc;

void ab_init(ab_calc *c);

/* Unit 018: line i's text as program entry shows it (UG Appendix G's names; list.c), a typed
   number in the display d gives (NULL: ALL in the current base). Returns its length in bytes, or
   -1 and "?" for no such line. */
int ab_prog_line(const ab_calc *c, int i, const ab_display *d, char *out, int size);

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
/* Adds an equation's text after the one shown and shows it (for tests and a device's editor; the
   keys build the same text, unit 014). Returns ABN_MEMORY_FULL when the list is full. */
abn_status ab_eqn_add(ab_calc *c, const char *text);

/* The value of the variable or register that (I) or (J) names (code AB_VAR_IND_I / J), for an
   equation (unit 015); INVALID (I) as ABN_DOMAIN, as for RCL (I). */
abn_status ab_indirect(ab_calc *c, int code, ab_val *out);
/* For an equation (unit 015): one keyboard operation op on y (in Y; NULL to keep Y) and x (in X),
   its X the result; the stack, LAST X and lift as they were afterwards. And digits typed with a
   base mark (2, 8, 10 or 16) read as the keyboard reads typed digits (UG p.11-3). */
abn_status ab_eqn_op(ab_calc *c, ab_op op, const ab_val *y, const ab_val *x, ab_val *out);
abn_status ab_eqn_based(const char *digits, int n, int base, abn *out);

/* Unit 018: the 35s's messages (appendix F, pp. F-1 to F-5), exact texts. ab_status_message is a
   status's own text; ab_error_message the message showing now (NULL if none), made specific from
   the operation that failed (SQRT(NEG), LOG(0), INVALID yˣ, SOLVE(SOLVE), INVALID (I)...);
   ab_busy_message what shows while op runs (RUNNING, SOLVING, INTEGRATING; NULL for a quick one). */
const char *ab_status_message(abn_status st);
const char *ab_error_message(const ab_calc *c);
const char *ab_busy_message(ab_op op);

/* STU's ALG (unit 029): whether op is one the expression line refuses with RPN ONLY (the stack keys,
   programs, Equation mode, SOLVE and integration, statistics entry, STO arithmetic, x<>); false
   outside ALG. The device shows the notice before prompting for such an op's argument. */
bool ab_rpn_only(const ab_calc *c, ab_op op);

/* Unit 030: what SHOW shows, as one or two parts, each a sign (- only) and all ABN_DIGITS significant
   digits, trailing zeros kept, no point, no exponent (33s p.1-21, 35s p.1-26): X's, a complex X's real
   then imaginary part, or in program entry the number on the line shown. 0 parts: nothing to show (a
   vector, a number being typed, Equation mode, a base mode, another program line). */
#define AB_SHOW_LEN (ABN_DIGITS + 2)
int ab_show_text(const ab_calc *c, char out[2][AB_SHOW_LEN]);

/* Unit 031 rule 3, answer C: the mark after an equation's text when it is shown under the other
   grouping and its chain (a^b^c) would read differently: " ·STU" for one typed in STU shown in 33s or
   35s, " ·35" or " ·33" for one typed there shown in STU; "" otherwise. */
const char *ab_eqn_mark(const ab_calc *c, const char *text, int model);

/* Unit 032: row k of the open table: *x = start + k x step, *y the equation's value there (L-R for an
   L=R), with the variable set for the evaluation alone and restored after (rule 3); the status of the
   row's evaluation (an error is that row's, the table goes on). */
abn_status ab_table_row(ab_calc *c, int32_t k, abn *x, ab_val *y);

/* Unit 033: the graph model. The plot area is AB_GRAPH_W columns by AB_GRAPH_H rows (screen pixels 0..399,
   21..205). Column c samples x = XMIN + c x (XMAX - XMIN) / (W - 1) and the value there (rule 1's form) with
   the variable set for it alone (pure: nothing changed). The window: Y as set, or fitted to the finite
   samples (not errors, not overflows) plus 5% each way, a constant +-1, none -10..10. A value's row: rule
   3's formula, clamped to -1 (above) and H (below). Column c's drawn rows: its row and every row strictly
   between it and the previous column's, clipped to the area; none after an error or across -1 and H. */
#define AB_GRAPH_W 400
#define AB_GRAPH_H 185
abn_status ab_graph_sample(ab_calc *c, int col, abn *x, ab_val *y);
void ab_graph_fit(const abn *y, const bool *ok, int n, abn *lo, abn *hi);
void ab_graph_window(ab_calc *c, abn *ymin, abn *ymax);
/* Unit 040: the cursor's x and y: with TRACE, the column's x and the curve's value there (an erring column: y 0
   and the status); free, the pixel's (x as graph_x; y = YMAX - row (YMAX - YMIN) / 184, each step rounded once). */
abn_status ab_graph_cursor(ab_calc *c, abn *x, abn *y);
/* Unit 040's review: the curve's samples taken (a test's probe: a free cursor's readout must not refit the window). */
extern unsigned long ab_graph_sample_calls;

/* Unit 040: column col's x (033's pinned order). */
abn_status ab_graph_x(ab_calc *c, int col, abn *x);
int ab_graph_row(const abn *y, const abn *ymin, const abn *ymax);
bool ab_graph_join(bool prev_ok, int prev_row, int row, int *lo, int *hi);
bool ab_graph_column(ab_calc *c, int col, const abn *ymin, const abn *ymax, int *lo, int *hi);
/* The axes (rule 5, answer 3): the x axis's row (y = 0) and the y axis's column (x = 0), -1 when outside. */
void ab_graph_axes(const ab_calc *c, const abn *ymin, const abn *ymax, int *xrow, int *ycol);
/* Rule 1's form of equation e: 0 an expression, V + 1 for V=expr (V = 0..25), -1 L-R. */
int ab_graph_form(const ab_calc *c, int e);

/* Unit 025: the mode in force (33s or 35s): while a program line runs, the mode of the label that
   owns it; otherwise (the keyboard, lines before the first label) the calculator's. Every
   mode-dependent behaviour asks here. */
bool ab_mode35(const ab_calc *c);
/* Whether a label's lines use an indirect address ((I), (J), (i), or an equation's), so a restamp
   changes their storage (unit 025 rule 5). */
bool ab_label_indirect(const ab_calc *c, int label);

/* Unit 016: CLEAR ALL and MEMORY CLEAR as UG p.B-4's table has them. ab_init is MEMORY CLEAR,
   with the device fields (budget, slice) at their defaults. */
void ab_clear_all(ab_calc *c);
void ab_memory_clear(ab_calc *c);

/* MEMORY CLEAR of the state alone: the expression line's history (RAM, not state) kept; the state
   loader's (unit 029) */
void ab_memory_clear_state(ab_calc *c);

/* Unit 016, for the device. ab_continue goes on after AB_STOP_SLICE. ab_interrupt is C or R/S
   pressed during a run (UG p.13-19): safe to call from an interrupt handler while a call runs (it
   sets a flag the run reads between lines and between SOLVE / integrate evaluations); at a slice
   stop it makes the stop a STOP at once. */
abn_status ab_continue(ab_calc *c);
void ab_interrupt(ab_calc *c);

/* A key arriving while a keyboard VIEW shows (UG p.3-4; abacus #3979): the display goes, and true
   when the key is spent on that, C or backspace (p.3-6); any other op (or -1, a device key that is
   no op) then acts. ab_do_arg applies it; a device applies it to keys that never reach the core. */
bool ab_view_key(ab_calc *c, int op);

/* Unit 035: the views a keyboard SOLVE, integral or linear solver ends with (view_id beyond the variables
   and registers): the integral's result in X ("∫="), the 2x2 solver's X and Y, the 3x3's X, Y and Z
   (down and up step among them, 35s p.7-7). */
#define AB_VIEW_INTEG (-33)
#define AB_VIEW_LIN2 (-40)          /* -40 X, -41 Y */
#define AB_VIEW_LIN3 (-50)          /* -50 X, -51 Y, -52 Z */
bool ab_view_linear(const ab_calc *c);  /* a linear solution's view is showing (its bar: up, down) */

/* Rebuilds the label table after program memory is replaced (state loading). */
void ab_prog_relabel(ab_calc *c);

/* The index of the constant named `name` (see core/const.c), or -1. */
int ab_const_index(const char *name);

#endif
