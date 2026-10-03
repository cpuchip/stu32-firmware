/* calc: the four-level RPN stack, number entry and the operations around them.
 *
 * Behaviour is written from the user's guide's description of the stack, cited by page
 * (UG p.N), in our own words.
 */
#include <string.h>

#include "calc.h"

void ab_init(ab_calc *c)
{
    memset(c, 0, sizeof *c);
    c->x = c->y = c->z = c->t = c->lastx = abn_zero();
    c->lift = true;             /* a cleared calculator has stack lift enabled (UG p.B-4) */
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

static abn_status two_number_op(ab_calc *c, ab_op op)
{
    abn r;
    abn_status st;
    /* Y op X: the earlier number is on the left (UG p.2-5). */
    switch (op) {
    case AB_ADD: st = abn_add(&r, &c->y, &c->x); break;
    case AB_SUB: st = abn_sub(&r, &c->y, &c->x); break;
    case AB_MUL: st = abn_mul(&r, &c->y, &c->x); break;
    default: st = abn_div(&r, &c->y, &c->x); break;
    }
    if (st != ABN_OK) return st;    /* the stack stays as it was */
    c->lastx = c->x;                /* the four operations save x in LAST X (UG p.B-6) */
    c->x = r;
    stack_drop(c);
    c->lift = true;
    return ABN_OK;
}

abn_status ab_do(ab_calc *c, ab_op op)
{
    if (op <= AB_KE) return entry_key(c, op);
    if ((op == AB_KCHS || op == AB_KBKSP) && c->entry.active) return entry_key(c, op);

    /* Every other key completes the number being entered (UG p.1-17). */
    c->entry.active = false;

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
        return two_number_op(c, op);
    default:
        return ABN_OK;
    }
    /* Every operation not listed as disabling or neutral enables lift (UG p.B-4). */
    c->lift = true;
    return ABN_OK;
}
