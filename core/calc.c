/* calc: the four-level RPN stack, number entry and the operations around them.
 *
 * Behaviour is written from the user's guide's description of the stack, cited by page
 * (UG p.N), in our own words.
 */
#include <string.h>

#include "calc.h"
#include "fn.h"
#include "const.h"

void ab_init(ab_calc *c)
{
    memset(c, 0, sizeof *c);
    c->x = c->y = c->z = c->t = c->lastx = abn_zero();
    c->lift = true;             /* a cleared calculator has stack lift enabled (UG p.B-4) */
    c->angle = AB_ANGLE_DEG;    /* and is in degrees (UG p.B-4) */
    c->disp_mode = AB_ALL;      /* ALL is the default display (ours, unit 002) */
    c->disp_n = 0;
    c->ind_top = -1;
    {
        abn zero = abn_zero();
        fn_seed(&c->rng, &zero);    /* a fixed start: the core has no clock */
    }
}

/* The stack lifts: T is lost, Z to T, Y to Z, X to Y; X keeps its value (UG p.2-5). */
static void stack_lift(ab_calc *c)
{
    c->t = c->z;
    c->z = c->y;
    c->y = c->x;
}

/* The stack drops after a two-number operation: Z to Y, T to Z, and T keeps its value
 * (UG p.2-5, p.2-6). */
static void stack_drop(ab_calc *c)
{
    c->y = c->z;
    c->z = c->t;
}

/* A number arrives in X: if stack lift is enabled the stack lifts first; otherwise the
 * number writes over X (UG p.B-4, p.B-5). */
static void place_in_x(ab_calc *c, abn v)
{
    if (c->lift) stack_lift(c);
    c->x = v;
}

/* ---- number entry (UG p.1-15 to p.1-17) ---- */

static bool has_char(const ab_entry *e, char ch)
{
    return memchr(e->text, ch, (size_t)e->len) != NULL;
}

static int count_digits(const ab_entry *e, bool exponent)
{
    int n = 0;
    bool in_exp = false;
    for (int i = 0; i < e->len; i++) {
        if (e->text[i] == 'E') in_exp = true;
        else if (e->text[i] != '.' && in_exp == exponent) n++;
    }
    return n;
}

/* The value the keys typed so far stand for. A mantissa with no digit yet reads as 0, and
 * an exponent with no digit yet as E0. */
static abn_status entry_value(const ab_entry *e, abn *out)
{
    char s[sizeof e->text + 8];
    int n = 0;
    if (e->mant_neg) s[n++] = '-';
    if (count_digits(e, false) == 0) s[n++] = '0';
    for (int i = 0; i < e->len; i++) {
        if (e->text[i] == 'E') {
            s[n++] = 'E';
            if (e->exp_neg) s[n++] = '-';
            if (count_digits(e, true) == 0) s[n++] = '0';
            continue;
        }
        if (count_digits(e, false) == 0 && e->text[i] == '.') continue;
        s[n++] = e->text[i];
    }
    s[n] = '\0';
    return abn_from_text(out, s);
}

/* A key that builds the number being entered. Keys the entry cannot take (a second
 * point, a digit past the limit, E before any mantissa digit) are ignored. */
static abn_status entry_key(ab_calc *c, ab_op op)
{
    ab_entry e = c->entry;
    if (!e.active) {
        memset(&e, 0, sizeof e);
    }
    bool in_exp = has_char(&e, 'E');
    if (op <= AB_K9) {
        int limit = in_exp ? AB_ENTRY_EXP : AB_ENTRY_MANT;
        if (count_digits(&e, in_exp) >= limit) return ABN_OK;
        e.text[e.len++] = (char)('0' + (op - AB_K0));
    } else if (op == AB_KPOINT) {
        if (in_exp || has_char(&e, '.')) return ABN_OK;
        e.text[e.len++] = '.';
    } else if (op == AB_KE) {
        /* E starts the exponent of the mantissa typed so far (UG p.1-16). With no
         * mantissa the guide gives no meaning (it keys 1 E 6 for a million): ignored. */
        if (in_exp || count_digits(&e, false) == 0) return ABN_OK;
        e.text[e.len++] = 'E';
    } else if (op == AB_KCHS) {
        /* During entry, change sign applies to the part being typed: the exponent once E
         * has been pressed, the mantissa before (UG p.1-15, p.1-16). */
        if (in_exp) e.exp_neg = !e.exp_neg;
        else e.mant_neg = !e.mant_neg;
    } else if (op == AB_KBKSP) {
        /* Backspace erases the last key typed (UG p.1-17). Erasing the E also drops the
         * exponent's sign. Erasing everything leaves 0 with lift disabled, as clear X
         * does (our choice; the guide does not say). */
        if (e.len > 0 && e.text[--e.len] == 'E') e.exp_neg = false;
        if (e.len == 0) {
            c->entry.active = false;
            c->x = abn_zero();
            c->lift = false;
            return ABN_OK;
        }
    }
    abn v;
    if (entry_value(&e, &v) != ABN_OK) return ABN_OK;  /* out of range: key ignored */
    if (!c->entry.active) {
        /* The first key of a number places it in X, lifting if lift is enabled. Digit
         * entry itself leaves the lift flag alone (UG p.B-5). */
        place_in_x(c, v);
    } else {
        c->x = v;
    }
    e.active = true;
    c->entry = e;
    return ABN_OK;
}

/* ---- operations ---- */

static fn_angle angle_of(const ab_calc *c)
{
    return c->angle == AB_ANGLE_RAD ? FN_RAD : (c->angle == AB_ANGLE_GRAD ? FN_GRAD : FN_DEG);
}

/* A function of X alone replaces X and saves the old X in LAST X (UG p.B-6); an error leaves
 * the stack and LAST X as they were (ours, unit 003). */
static abn_status one_number_fn(ab_calc *c, ab_op op)
{
    abn r;
    abn_status st;
    fn_angle m = angle_of(c);
    switch (op) {
    case AB_EXP: st = fn_exp(&r, &c->x); break;
    case AB_LN: st = fn_ln(&r, &c->x); break;
    case AB_TENX: st = fn_tenx(&r, &c->x); break;
    case AB_LOG: st = fn_log(&r, &c->x); break;
    case AB_SQ: st = fn_sq(&r, &c->x); break;
    case AB_SQRT: st = fn_sqrt(&r, &c->x); break;
    case AB_INV: st = fn_inv(&r, &c->x); break;
    case AB_SIN: st = fn_sin(&r, &c->x, m); break;
    case AB_COS: st = fn_cos(&r, &c->x, m); break;
    case AB_TAN: st = fn_tan(&r, &c->x, m); break;
    case AB_ASIN: st = fn_asin(&r, &c->x, m); break;
    case AB_ACOS: st = fn_acos(&r, &c->x, m); break;
    case AB_ATAN: st = fn_atan(&r, &c->x, m); break;
    case AB_SINH: st = fn_sinh(&r, &c->x); break;
    case AB_COSH: st = fn_cosh(&r, &c->x); break;
    case AB_TANH: st = fn_tanh(&r, &c->x); break;
    case AB_ASINH: st = fn_asinh(&r, &c->x); break;
    case AB_ACOSH: st = fn_acosh(&r, &c->x); break;
    case AB_ATANH: st = fn_atanh(&r, &c->x); break;
    case AB_HMS: st = fn_hms(&r, &c->x); break;           /* UG p.4-13 */
    case AB_HMSINV: st = fn_hmsinv(&r, &c->x); break;
    case AB_TODEG: st = fn_todeg(&r, &c->x); break;       /* UG p.4-13, p.4-14 */
    case AB_TORAD: st = fn_torad(&r, &c->x); break;
    case AB_TOKG: st = fn_convert(&r, &c->x, FN_TOKG); break;     /* UG p.4-14 */
    case AB_TOLB: st = fn_convert(&r, &c->x, FN_TOLB); break;
    case AB_TOC: st = fn_convert(&r, &c->x, FN_TOC); break;
    case AB_TOF: st = fn_convert(&r, &c->x, FN_TOF); break;
    case AB_TOCM: st = fn_convert(&r, &c->x, FN_TOCM); break;
    case AB_TOIN: st = fn_convert(&r, &c->x, FN_TOIN); break;
    case AB_TOL: st = fn_convert(&r, &c->x, FN_TOL); break;
    case AB_TOGAL: st = fn_convert(&r, &c->x, FN_TOGAL); break;
    case AB_TOKM: st = fn_convert(&r, &c->x, FN_TOKM); break;
    case AB_TOMILE: st = fn_convert(&r, &c->x, FN_TOMILE); break;
    case AB_FACT: st = fn_fact(&r, &c->x); break;         /* UG p.4-15 */
    case AB_IP: st = fn_ip(&r, &c->x); break;             /* UG p.4-17, p.4-18 */
    case AB_FP: st = fn_fp(&r, &c->x); break;
    case AB_INTG: st = fn_intg(&r, &c->x); break;
    case AB_ABS: st = fn_abs(&r, &c->x); break;
    case AB_SGN: st = fn_sgn(&r, &c->x); break;
    default: st = fn_rnd(&r, &c->x, c->disp_mode, c->disp_n); break;     /* AB_RND */
    }
    if (st != ABN_OK) return st;
    c->lastx = c->x;
    c->x = r;
    c->lift = true;
    return ABN_OK;
}

/* Percent and percent change give their result in X and keep the base number in Y: the stack
 * does not drop (UG p.4-6). Both save x in LAST X (UG p.B-6). */
static abn_status percent_fn(ab_calc *c, ab_op op)
{
    abn r;
    abn_status st = op == AB_PCT ? fn_pct(&r, &c->y, &c->x) : fn_pctchg(&r, &c->y, &c->x);
    if (st != ABN_OK) return st;
    c->lastx = c->x;
    c->x = r;
    c->lift = true;
    return ABN_OK;
}

static abn_status two_number_op(ab_calc *c, ab_op op)
{
    abn r;
    abn_status st;
    /* Y op X: the earlier number is on the left (UG p.2-5). */
    switch (op) {
    case AB_ADD: st = abn_add(&r, &c->y, &c->x); break;
    case AB_SUB: st = abn_sub(&r, &c->y, &c->x); break;
    case AB_MUL: st = abn_mul(&r, &c->y, &c->x); break;
    case AB_DIV: st = abn_div(&r, &c->y, &c->x); break;
    case AB_POW: st = fn_pow(&r, &c->y, &c->x); break;        /* y^x (UG p.4-2) */
    case AB_XROOT: st = fn_xroot(&r, &c->y, &c->x); break;    /* x-th root of y (UG p.4-3) */
    case AB_IDIV: st = fn_idiv(&r, &c->y, &c->x); break;      /* INT÷ (UG p.4-2) */
    case AB_NCR: st = fn_ncr(&r, &c->y, &c->x); break;        /* n in Y, r in X (UG p.4-16) */
    case AB_NPR: st = fn_npr(&r, &c->y, &c->x); break;
    default: st = fn_rmdr(&r, &c->y, &c->x); break;           /* Rmdr (UG p.4-2) */
    }
    if (st != ABN_OK) return st;    /* the stack stays as it was */
    c->lastx = c->x;                /* these operations save x in LAST X (UG p.B-6) */
    c->x = r;
    stack_drop(c);
    c->lift = true;
    return ABN_OK;
}

abn_status ab_do(ab_calc *c, ab_op op)
{
    return ab_do_arg(c, op, 0);
}

abn_status ab_do_arg(ab_calc *c, ab_op op, int arg)
{
    if (op >= AB_STO) return ABN_OK;    /* STUB: unit 005 lands after the oracle */

    if (op <= AB_KE) return entry_key(c, op);
    if ((op == AB_KCHS || op == AB_KBKSP) && c->entry.active) return entry_key(c, op);

    /* Every other key completes the number being entered (UG p.1-17). A completed typed
     * number enables lift before the operation acts, so a recall after `1 ENTER 2` keeps
     * the 2 (work unit 001, amendment 2, ruling 1; the guide does not show this case). */
    if (c->entry.active) {
        c->entry.active = false;
        c->lift = true;
    }

    switch (op) {
    case AB_KBKSP:  /* with no entry in progress backspace acts as clear X (UG p.1-17) */
    case AB_CLX:
        /* Clear X puts 0 in X and disables lift, so the next number replaces it
         * (UG p.2-7, p.B-5). */
        c->x = abn_zero();
        c->lift = false;
        return ABN_OK;
    case AB_CLSTK:
        /* Clear the stack: all four registers to 0, lift disabled (UG p.2-7, p.B-5). */
        c->x = c->y = c->z = c->t = abn_zero();
        c->lift = false;
        return ABN_OK;
    case AB_ENTER:
        /* ENTER lifts the stack, copying X into Y, and disables lift so the next number
         * writes over the copy left in X (UG p.2-6, p.B-5). */
        stack_lift(c);
        c->lift = false;
        return ABN_OK;
    case AB_RDN: {
        /* Roll down: Y to X, Z to Y, T to Z, X to T (UG p.2-3). */
        abn x = c->x;
        c->x = c->y;
        c->y = c->z;
        c->z = c->t;
        c->t = x;
        break;
    }
    case AB_RUP: {
        /* Roll up: X to Y, Y to Z, Z to T, T to X (UG p.2-4). */
        abn t = c->t;
        c->t = c->z;
        c->z = c->y;
        c->y = c->x;
        c->x = t;
        break;
    }
    case AB_XY: {
        /* Exchange X and Y; Z and T are untouched (UG p.2-4). */
        abn x = c->x;
        c->x = c->y;
        c->y = x;
        break;
    }
    case AB_LASTX:
        /* LAST X brings back the number X held before the last numeric operation; it
         * arrives like any new number, and the LAST X register keeps it (UG p.2-8, p.2-11). */
        place_in_x(c, c->lastx);
        break;
    case AB_KCHS:
        /* Change sign of a completed X (UG p.1-15). */
        c->x = abn_neg(&c->x);
        break;
    case AB_ADD:
    case AB_SUB:
    case AB_MUL:
    case AB_DIV:
    case AB_POW:
    case AB_XROOT:
    case AB_IDIV:
    case AB_RMDR:
    case AB_NCR:
    case AB_NPR:
        return two_number_op(c, op);
    case AB_HMS: case AB_HMSINV: case AB_TODEG: case AB_TORAD:
    case AB_TOKG: case AB_TOLB: case AB_TOC: case AB_TOF: case AB_TOCM: case AB_TOIN:
    case AB_TOL: case AB_TOGAL: case AB_TOKM: case AB_TOMILE:
    case AB_FACT: case AB_IP: case AB_FP: case AB_INTG: case AB_ABS: case AB_SGN: case AB_RND:
        return one_number_fn(c, op);
    case AB_TOPOL:
    case AB_TOREC: {
        /* Two numbers in, two out: x and y in X and Y become r and theta, or back (ours, unit
           004). LAST X keeps the old x, as for the other conversions (UG p.B-6). */
        abn a, b;
        abn_status st = op == AB_TOPOL ? fn_topol(&a, &b, &c->x, &c->y, angle_of(c))
                                       : fn_torec(&a, &b, &c->x, &c->y, angle_of(c));
        if (st != ABN_OK) return st;
        c->lastx = c->x;
        c->x = a;
        c->y = b;
        break;
    }
    case AB_RAND:
        /* RAND arrives like a recalled number (UG p.4-15). */
        place_in_x(c, fn_rand(&c->rng));
        break;
    case AB_SEED:
        /* SEED takes x as the seed and leaves the stack as it is (UG p.4-15; ours: X stays). */
        fn_seed(&c->rng, &c->x);
        break;
    case AB_CONST: {
        abn v;
        abn_status st = ab_const_value(&v, arg);
        if (st != ABN_OK) return st;
        place_in_x(c, v);   /* a constant arrives like a recalled number (UG p.4-8) */
        break;
    }
    case AB_SETFIX:
    case AB_SETSCI:
    case AB_SETENG:
    case AB_SETALL:
        /* Display settings change no number and leave stack lift alone (UG p.1-21, p.B-5). */
        if (arg < 0 || arg > AB_FMT_N_MAX) return ABN_DOMAIN;
        c->disp_mode = op == AB_SETFIX ? AB_FIX : op == AB_SETSCI ? AB_SCI : op == AB_SETENG ? AB_ENG : AB_ALL;
        c->disp_n = op == AB_SETALL ? 0 : arg;
        return ABN_OK;
    case AB_PCT:
    case AB_PCTCHG:
        return percent_fn(c, op);
    case AB_PI:
        /* Pi arrives like a recalled number (UG p.4-3). */
        place_in_x(c, fn_pi());
        break;
    case AB_DEG:
    case AB_RAD:
    case AB_GRAD:
        /* Angle modes change no number and leave stack lift as it was (UG p.4-4, p.B-5). */
        c->angle = op == AB_DEG ? AB_ANGLE_DEG : (op == AB_RAD ? AB_ANGLE_RAD : AB_ANGLE_GRAD);
        return ABN_OK;
    case AB_EXP: case AB_LN: case AB_TENX: case AB_LOG:
    case AB_SQ: case AB_SQRT: case AB_INV:
    case AB_SIN: case AB_COS: case AB_TAN: case AB_ASIN: case AB_ACOS: case AB_ATAN:
    case AB_SINH: case AB_COSH: case AB_TANH: case AB_ASINH: case AB_ACOSH: case AB_ATANH:
        return one_number_fn(c, op);
    default:
        return ABN_OK;
    }
    /* Every operation not listed as disabling or neutral enables lift (UG p.B-4). */
    c->lift = true;
    return ABN_OK;
}
