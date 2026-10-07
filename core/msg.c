/* msg: the 35s's messages, appendix F (pp. F-1 to F-5), exact texts (unit 018). The page of each
 * is beside it. Statuses with no message of the guide's carry ours, marked. */
#include <stddef.h>

#include "calc.h"

const char *ab_status_message(abn_status st)
{
    switch (st) {
    case ABN_OK: return "";
    case ABN_DIV_ZERO: return "DIVIDE BY 0";                    /* F-1 */
    case ABN_RANGE: return "OVERFLOW";                          /* F-4 */
    case ABN_BAD_TEXT: return "SYNTAX ERROR";                   /* F-5 (a number's text: ours) */
    case ABN_DOMAIN: return "INVALID DATA";                     /* F-2 */
    case ABN_INEXACT: return "";                                /* not a message (internal) */
    case ABN_CLAMPED: return "OVERFLOW";                        /* a base result beyond the word: OVERFLOW
                                                                   briefly, the largest kept (33s and 32SII
                                                                   p.10-3, 35s p.11-5; unit 024 rule 3) */
    case ABN_NONEXISTENT: return "NONEXISTENT";                 /* F-3 */
    case ABN_XEQ_OVERFLOW: return "XEQ OVERFLOW";               /* F-5 */
    case ABN_DUP_LABEL: return "DUPLICAT.LBL";                  /* F-1 */
    case ABN_MEMORY_FULL: return "MEMORY FULL";                 /* F-3 */
    case ABN_NONPROGRAMMABLE: return "NOT PROGRAMMABLE";        /* ours: the guide has none */
    case ABN_BUDGET: return "RUN LIMIT";                        /* ours: AB_RUN_BUDGET lines */
    case ABN_NO_ROOT: return "NO ROOT FND";                     /* F-4 */
    case ABN_SELECT_FN: return "SELECT FN";                     /* F-4 */
    case ABN_NESTED: return "SOLVE(SOLVE)";                     /* F-4 (made specific below) */
    case ABN_SYNTAX: return "SYNTAX ERROR";                     /* F-5: one line (its column wraps; #3822) */
    case ABN_INVALID_VAR: return "INVALID VAR";                 /* F-2 */
    case ABN_NO_SOLUTION: return "NO SOLUTION";                 /* F-3 */
    case ABN_MULT_SOLUTION: return "MULT SOLUTION";             /* F-3 */
    case ABN_INTERRUPTED: return "INTERRUPTED";                 /* F-2 */
    case ABN_BAD_IMAGE: return "MEMORY CLEAR";                  /* F-3, B-3 */
    case ABN_OVERFLOW: return "OVERFLOW";                       /* F-3: the warning; an error only for a
                                                                   complex or vector result (unit 026) */
    case ABN_UNDERFLOW: return "OVERFLOW";                      /* ours: a complex or vector result whose
                                                                   parts underflow stays the range error
                                                                   it was (unit 026) */
    case ABN_RPN_ONLY: return "RPN ONLY";                       /* ours: a stack key in ALG (unit 029) */
    case ABN_STU_ONLY: return "STU ONLY";                       /* ours: TABLE outside STU (unit 032) */
    case ABN_NO_EQN: return "NO EQUATION";                      /* ours: TABLE with none shown (unit 032) */
    }
    return "";
}

static bool is_zero_val(const ab_val *v)
{
    return v->kind != AB_VECTOR && abn_is_zero(&v->re) && abn_is_zero(&v->im);
}

static bool takes_var(int op)
{
    switch (op) {
    case AB_STO: case AB_RCL: case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:
    case AB_RCLADD: case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV: case AB_XVAR: case AB_VIEW:
    case AB_INPUT: case AB_ISG: case AB_DSE: case AB_SOLVE: case AB_INTEG:
        return true;
    default:
        return false;
    }
}

const char *ab_error_message(const ab_calc *c)
{
    int op = c->err_op, arg = c->err_arg;
    const ab_val *x = &c->err_x;
    if (!c->msg) return NULL;
    /* the failing function decides where its message is not the status's own: 0 to a negative
       power is DIVIDE BY 0 in the engine but INVALID yˣ in the guide (F-3); a statistic with n = 0
       divides by zero, and is STAT ERROR (F-5) */
    if (c->err_status == ABN_DIV_ZERO || c->err_status == ABN_DOMAIN) {
        switch (op) {
        case AB_POW: case AB_XROOT:
            return "INVALID y\xcb\xa3";
        case AB_XBAR: case AB_YBAR: case AB_XWBAR: case AB_SX: case AB_SY: case AB_PSX: case AB_PSY:
        case AB_XHAT: case AB_YHAT: case AB_CORR: case AB_SLOPE: case AB_YINT:
            return "STAT ERROR";
        default:
            break;
        }
    }
    switch (c->err_status) {
    case ABN_DOMAIN:
        /* the indirect variable first: whatever took (I) or (J) (F-3) */
        if ((takes_var(op) && (arg == AB_VAR_IND_I || arg == AB_VAR_IND_J)) ||
            ((op == AB_XEQ || op == AB_GTO || op == AB_FN) && (arg == AB_TGT_IND_I || arg == AB_TGT_IND_J)))
            return arg == AB_VAR_IND_I || arg == AB_TGT_IND_I ? (c->err_m35 ? "INVALID (I)" : "INVALID (i)")   /* 33s F-2 */
                                                               : "INVALID (J)";
        switch (op) {
        case AB_SQRT:
            return x->kind == AB_REAL && abn_is_negative(&x->re) ? "SQRT(NEG)" : "INVALID DATA";   /* F-4 */
        case AB_LN: case AB_LOG: case AB_LOG2:
            if (is_zero_val(x)) return "LOG(0)";                                        /* F-3 */
            return x->kind == AB_REAL && abn_is_negative(&x->re) ? "LOG(NEG)" : "INVALID DATA";
        case AB_LN1P: {                         /* ln(1 + x): as LN of 1 + x (ours, unit 021) */
            abn m1 = abn_from_int(-1);
            if (x->kind != AB_REAL) return "INVALID DATA";
            return abn_cmp(&x->re, &m1) == 0 ? "LOG(0)" : "LOG(NEG)";
        }
        case AB_POW: case AB_XROOT:
            return "INVALID y\xcb\xa3";                                                 /* F-3: INVALID yˣ */
        case AB_FACT:
            return "INVALID x!";                                                        /* F-2 */
        case AB_XBAR: case AB_YBAR: case AB_XWBAR: case AB_SX: case AB_SY: case AB_PSX: case AB_PSY:
        case AB_XHAT: case AB_YHAT: case AB_CORR: case AB_SLOPE: case AB_YINT:
            return "STAT ERROR";                                                        /* F-5 */
        default:
            return "INVALID DATA";                                                      /* F-2 */
        }
    case ABN_NESTED:
        /* what was running decides the text (F-1, F-4) */
        if (op == AB_FN) return c->err_active == AB_INTEG ? "\xe2\x88\xab" "FN ACTIVE" : "SOLVE ACTIVE";
        if (c->err_active == AB_INTEG)
            return op == AB_INTEG ? "\xe2\x88\xab(\xe2\x88\xab" "FN)" : "\xe2\x88\xab(SOLVE)";
        return op == AB_INTEG ? "SOLVE(\xe2\x88\xab" "FN)" : "SOLVE(SOLVE)";
    default:
        return ab_status_message(c->err_status);
    }
}

const char *ab_busy_message(ab_op op)
{
    switch (op) {
    case AB_SOLVE: return "SOLVING";                            /* F-4 */
    case AB_INTEG: return "INTEGRATING";                        /* F-2 */
    case AB_XEQ: case AB_RS: case AB_SST: return "RUNNING";     /* F-4 (with its boxed B) */
    default: return NULL;
    }
}
