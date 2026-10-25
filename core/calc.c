/* calc: the four-level RPN stack, number entry and the operations around them.
 *
 * Behaviour is written from the user's guide's description of the stack, cited by page
 * (UG p.N), in our own words.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "calc.h"
#include "fn.h"
#include "const.h"
#include "stat.h"
#include "frac.h"
#include "base.h"
#include "arena.h"
#include "cplx.h"
#include "hp.h"
#include "state.h"
#include "eqn.h"
#ifdef AB_CASIM
#include "casim.h"
_Static_assert(CASIM_MODEL_STU == AB_MODEL_STU && CASIM_MODEL_33 == 33 && CASIM_MODEL_35 == 35, "casim's models");
_Static_assert(CASIM_DEG == AB_ANGLE_DEG && CASIM_RAD == AB_ANGLE_RAD && CASIM_GRAD == AB_ANGLE_GRAD, "casim's angles");
#endif
#include "lin.h"
#include "xnum.h"

/* A real number as a stack value. */
static ab_val ab_real(abn v)
{
    ab_val r;
    memset(&r, 0, sizeof r);
    r.kind = AB_REAL;
    r.re = v;
    return r;
}

/* The built-in linear solvers as list entries (UG p.6-6, 7-6): locked, never typed. */
#define EQN_LIN2 "\x01" "2*2 lin. solve"
#define EQN_LIN3 "\x01" "3*3 lin. solve"

/* the history is the struct's tail (calc.h): what is before it is the state */
_Static_assert(sizeof(ab_calc) - (offsetof(ab_calc, hist_at) + sizeof(int)) < _Alignof(ab_calc),
               "the history is ab_calc's tail");

/* Every field to its default; the history too unless keep_hist. */
static void init_fields(ab_calc *c, bool keep_hist)
{
    memset(c, 0, keep_hist ? offsetof(ab_calc, hist) : sizeof *c);
    if (!keep_hist) c->hist_at = -1;
    c->x = c->y = c->z = c->t = c->lastx = ab_real(abn_zero());
    c->lift = true;             /* a cleared calculator has stack lift enabled (UG p.B-4) */
    c->ans = ab_real(abn_zero());   /* ANS 0 to begin (unit 029 rule 4) */
    c->tbl_var = c->tbl_v = c->tbl_eq = -1;     /* TABLE: the first variable, start from it, step 1 (032) */
    c->tbl_start = abn_zero();
    c->tbl_step = abn_from_int(1);
    c->gr_xmin = c->gr_ymin = abn_from_int(-10);    /* GRAPH: X -10..10, Y fitted (033 rule 2) */
    c->gr_xmax = c->gr_ymax = abn_from_int(10);
    c->gr_auto = true;
    c->gr_eq = c->gr_v = -1;
    c->gr_cur = AB_GRAPH_W / 2;
    c->gr_row = AB_GRAPH_CROW;      /* unit 040: the cursor home, TRACE on, the factors 4, no ZLAST */
    c->gr_trace = true;
    c->gr_hf = c->gr_vf = abn_from_int(4);
    c->angle = AB_ANGLE_DEG;    /* and is in degrees (UG p.B-4) */
    /* FIX 4, the HP's original default (UG p.B-3, MEMORY CLEAR's (Default) column); unit 002's ALL start
       ruled out by unit 067 (abacus #6964) */
    c->disp_mode = AB_FIX;
    c->disp_n = 4;
    c->ind_top = -1;
    c->exec_at = -1;
    c->pc = AB_PC_TOP;          /* program memory empty, the pointer at PRGM TOP (unit 011) */
    c->budget = AB_RUN_BUDGET;
    c->stop_var = -1;
    c->fn = -1;
    c->eqn_cur = -1;
    strcpy(c->eqn[0], EQN_LIN2);    /* the list always holds the linear solvers (UG p.6-6, 7-3) */
    strcpy(c->eqn[1], EQN_LIN3);
    c->eqn_model[0] = c->eqn_model[1] = 35;
    c->eqn_count = 2;
    c->peq_line = -1;
    c->fn_eqn = -1;
    c->eqn_edit = -1;
    for (int l = 0; l < 26; l++) c->labelpos[l] = -1;
    c->frac_c = 4095;          /* the default maximum denominator (UG p.5-5) */
    c->base = 10;
    {
        abn zero = abn_zero();
        fn_seed(&c->rng, &zero);    /* a fixed start: the core has no clock */
    }
}

void ab_init(ab_calc *c)
{
    init_fields(c, false);
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
static void place_val(ab_calc *c, ab_val v)
{
    if (c->lift) stack_lift(c);
    c->x = v;
}

static void place_in_x(ab_calc *c, abn v)
{
    place_val(c, ab_real(v));
}

/* ---- complex values (unit 009) ---- */

static bool is_cx(const ab_val *v)
{
    return v->kind == AB_COMPLEX;
}

/* A stack value as a complex number: a real one has imaginary part 0 (unit 009 rule 3). */
static cplx cx_of(const ab_val *v)
{
    cplx z;
    z.re = v->re;
    z.im = v->im;
    return z;
}

static ab_val ab_cx(cplx z)
{
    ab_val r;
    memset(&r, 0, sizeof r);
    r.kind = AB_COMPLEX;
    r.re = z.re;
    r.im = z.im;
    return r;
}

/* The failing operation, for the message (unit 018): the first one recorded in a call is the
 * innermost (a function inside an equation, a program line, the key last). */
static void err_note(ab_calc *c, int op, int arg, const ab_val *x)
{
    if (c->err_set) return;
    c->err_set = true;
    c->err_op = op;
    c->err_arg = arg;
    c->err_active = c->active;
    c->err_x = x ? *x : c->x;
    c->err_m35 = ab_mode35(c);
}

/* ---- vectors (unit 010) ---- */

static bool is_vec(const ab_val *v)
{
    return v->kind == AB_VECTOR;
}

/* Element i (0..2) of a vector: parts 1 to 3 of the register (UG p.2-2). */
static abn *vel(ab_val *v, int i)
{
    return i == 0 ? &v->re : i == 1 ? &v->im : &v->p3;
}

static const abn *cvel(const ab_val *v, int i)
{
    return i == 0 ? &v->re : i == 1 ? &v->im : &v->p3;
}

static ab_val ab_vec(int n)
{
    ab_val r;
    memset(&r, 0, sizeof r);
    r.kind = AB_VECTOR;
    r.len = n;
    return r;
}

static fn_angle angle_of(const ab_calc *c);
static abn_status entry_value(const ab_entry *e, abn *out, int base, bool short_frac);
/* 5 . . 8 is 5/8: the 33s's fraction shorthand (FC2), which STU borrows (decision 83); the 35s
   reads 5 */
static bool short_frac(const ab_calc *c) { return !c->m35 || c->stu; }

/* The vector being typed: its finished elements and the value v of the one being typed. */
static abn_status vector_entry_value(const ab_calc *c, const abn *v, ab_val *out)
{
    ab_val r = ab_vec(c->vlen);
    abn_status st;
    for (int i = 0; i < c->vlen - 1; i++)
        if ((st = entry_value(&c->ventry[i], vel(&r, i), 10, short_frac(c))) != ABN_OK) return st;
    *vel(&r, c->vlen - 1) = *v;
    *out = r;
    return ABN_OK;
}

/* The complex number being typed, from its first part and the value v of its second: x i y, or
 * r theta a with a in the angle mode (UG p.9-2, p.4-11; polar entry exact as unit 004's
 * conversion is, so 1 theta 90 in degrees is 0 i 1). */
static abn_status complex_entry_value(const ab_calc *c, const abn *v, ab_val *out)
{
    abn v1;
    cplx z;
    abn_status st;
    if ((st = entry_value(&c->entry1, &v1, 10, short_frac(c))) != ABN_OK) return st;
    if (c->cpart == 1) {
        z.re = v1;
        z.im = *v;
    } else if ((st = fn_torec(&z.re, &z.im, &v1, v, angle_of(c))) != ABN_OK) {
        return st;
    }
    *out = ab_cx(z);
    return ABN_OK;
}

/* ---- number entry (UG p.1-15 to p.1-17) ---- */

/* Where a number being typed goes: into X, the first key placing it as a new number; in program
 * entry, into the line being keyed instead (unit 011). */
static void entry_put(ab_calc *c, ab_val nv, bool first)
{
    if (c->prgm) {
        c->plit = nv;
        return;
    }
    if (first) place_val(c, nv);
    else c->x = nv;
}

/* Backspace erased the whole number: X is 0 with lift disabled, as clear X does (our choice; the
 * guide does not say); in program entry nothing is keyed. */
static void entry_cancel(ab_calc *c)
{
    c->entry.active = false;
    c->vlen = 0;
    c->cpart = 0;
    if (c->prgm) return;
    c->x = ab_real(abn_zero());
    c->lift = false;
}

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
/* Puts a key's character at the insertion point (the end, but after the 35s's SYNTAX ERROR for E
 * with no mantissa: the start, unit 017). */
static void put_ch(ab_entry *e, char ch)
{
    int at = e->len - e->back;
    memmove(e->text + at + 1, e->text + at, (size_t)(e->len - at));
    e->text[at] = ch;
    e->len++;
}

/* E typed with no mantissa digit before it, in the 35s mode (unit 017 rule 3). */
static bool no_mantissa(const ab_entry *e)
{
    for (int i = 0; i < e->len; i++) {
        if (e->text[i] == 'E') return true;
        if (e->text[i] != '.') return false;
    }
    return false;
}

static int count_char(const ab_entry *e, char ch)
{
    int n = 0;
    for (int i = 0; i < e->len; i++) n += e->text[i] == ch;
    return n;
}

static abn_status entry_value(const ab_entry *e, abn *out, int base, bool short_frac)
{
    /* In HEX, OCT or BIN the keys are an integer's digits in that base (UG p.11-3); a value past
     * the largest word cannot be typed (UG p.11-7): the key is refused by the caller. */
    if (base != 10) {
        int64_t v;
        char d[AB_ENTRY_TEXT];
        memcpy(d, e->text, (size_t)e->len);
        d[e->len] = '\0';
        if (e->len == 0) {
            *out = abn_zero();
            return ABN_OK;
        }
        if (!base_parse(d, base, &v)) return ABN_RANGE;
        *out = base_from_word(e->mant_neg ? -v : v);
        return ABN_OK;
    }
    /* A second point makes the entry a fraction: integer . numerator . denominator (UG p.5-1),
     * the exact rational rounded once (unit 007 rule 2). With no denominator digit yet, the
     * number is the decimal typed so far (ours, unit 007). In 33s mode, with no numerator digit,
     * the integer part is the numerator: 5 . . 8 is 5/8, as Michael's 33s keys it (FC2, abacus
     * #5550); STU borrows it (decision 83, abacus #6765); the 35s reads 5 and no fraction. */
    if (count_char(e, '.') == 2) {
        char ip[AB_ENTRY_TEXT], nu[AB_ENTRY_TEXT], de[AB_ENTRY_TEXT];
        int part = 0, a = 0, b = 0, c2 = 0;
        for (int i = 0; i < e->len; i++) {
            char ch = e->text[i];
            if (ch == '.') part++;
            else if (part == 0) ip[a++] = ch;
            else if (part == 1) nu[b++] = ch;
            else de[c2++] = ch;
        }
        ip[a] = nu[b] = de[c2] = '\0';
        if (c2 > 0 && short_frac && b == 0) return fr_from_parts(out, e->mant_neg, "", ip, de);
        if (c2 > 0) return fr_from_parts(out, e->mant_neg, ip, nu, de);
        char s[AB_ENTRY_TEXT + 4];
        int n = 0;
        if (e->mant_neg) s[n++] = '-';
        s[n++] = '0';
        for (int i = 0; i < a; i++) s[n++] = ip[i];
        s[n++] = '.';
        for (int i = 0; i < b; i++) s[n++] = nu[i];
        s[n++] = '0';
        s[n] = '\0';
        return abn_from_text(out, s);
    }
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
    int points = count_char(&e, '.');
    if (c->base != 10 && (op == AB_KPOINT || op == AB_KE)) return ABN_OK;  /* integers only (ours) */
    if (op <= AB_K9 || (op >= AB_KA && op <= AB_KF)) {
        /* a digit outside the base is ignored, and A-F are digits only in HEX (ours, unit 008) */
        int dv = op <= AB_K9 ? (int)(op - AB_K0) : 10 + (int)(op - AB_KA);
        if (dv >= c->base) return ABN_OK;
        if (dv >= 10) {
            /* hex digits are kept in lower case so that 'E' always means the exponent */
            put_ch(&e, (char)('a' + dv - 10));
            goto value;
        }
    }
    if (op <= AB_K9) {
        /* in a base mode the range, not a digit count, limits entry: up to a word's binary
           digits (a 35-digit binary number is the largest positive word, UG p.11-7) */
        bool exp_part = in_exp && e.back == 0;  /* typing before the E: the mantissa (unit 017) */
        int limit = c->base != 10 ? AB_WORD_BITS : exp_part ? AB_ENTRY_EXP : AB_ENTRY_MANT;
        if (count_digits(&e, exp_part) >= limit) return ABN_OK;
        /* a denominator never starts with 0 (ours, unit 007): the key is ignored */
        if (op == AB_K0 && points == 2 && e.text[e.len - 1] == '.') return ABN_OK;
        /* in 33s mode a denominator takes 4 digits and a 5th does nothing, as Michael's 33s (FC3,
           abacus #5550); the 35s takes more */
        if (!c->m35 && points == 2 && e.len >= 4 && !memchr(e.text + e.len - 4, '.', 4)) return ABN_OK;
        put_ch(&e, (char)('0' + (op - AB_K0)));
    } else if (op == AB_KPOINT) {
        /* the second point starts a fraction's denominator (UG p.5-1); a third is ignored, as is
           a point in the exponent (ours, unit 007) */
        if (in_exp || points >= 2) return ABN_OK;
        put_ch(&e, '.');
    } else if (op == AB_KE) {
        /* E starts the exponent of the mantissa typed so far (UG p.1-16). With nothing typed
         * (unit 017 rule 3, Michael's machines): the 33s starts the number as if 1 had been
         * typed (E 3 is 1000); the 35s takes the E alone, and ending the entry is then SYNTAX
         * ERROR (do_key). A fraction has no exponent (ours, unit 007): ignored. */
        if (in_exp || points == 2) return ABN_OK;
        if (count_digits(&e, false) == 0) {
            if (e.len != 0) return ABN_OK;      /* a point alone: ignored, as before */
            if (!c->m35) put_ch(&e, '1');
        }
        put_ch(&e, 'E');
    } else if (op == AB_KCHS) {
        /* During entry, change sign applies to the part being typed: the exponent once E
         * has been pressed, the mantissa before (UG p.1-15, p.1-16). */
        if (in_exp) e.exp_neg = !e.exp_neg;
        else e.mant_neg = !e.mant_neg;
    } else if (op == AB_KBKSP) {
        /* Backspace erases the last key typed (UG p.1-17). Erasing the E also drops the
         * exponent's sign. Erasing everything leaves 0 with lift disabled, as clear X
         * does (our choice; the guide does not say). */
        if (c->vlen && e.len == 0) {
            /* with the element empty, backspace erases the comma before it, or with the first
               element empty the bracket: X is 0 and lift is disabled, as clearing an entry does
               (ours; the guide does not say) */
            if (c->vlen == 1) {
                entry_cancel(c);
                return ABN_OK;
            }
            c->vlen--;
            e = c->ventry[c->vlen - 1];
            e.active = true;
            c->entry = e;
            goto value;
        }
        if (c->cpart && e.len == 0) {
            /* with the second part empty, backspace erases the i (or theta) and the first part is
               being typed again (ours; the guide does not say) */
            c->cpart = 0;
            e = c->entry1;
            if (e.len == 0) {
                entry_cancel(c);
                return ABN_OK;
            }
            e.active = true;
            c->entry = e;
            goto value;
        }
        {
            /* the key before the insertion point goes (at the start, nothing does) */
            int at = e.len - e.back;
            bool removed = at > 0;
            if (removed) {
                if (e.text[at - 1] == 'E') e.exp_neg = false;
                memmove(e.text + at - 1, e.text + at, (size_t)(e.len - at));
                e.len--;
            }
            /* erasing the last digit of a negative number leaves its sign, and entry goes on
               (unit 017 rule 2, Michael's 33s and 35s: 1 ENTER 5 +/- backspace shows -); with
               nothing left at all, or the lone sign erased, entry ends with X 0 and lift off */
            if (e.len == 0 && !c->cpart && !c->vlen && !(removed && e.mant_neg)) {
                entry_cancel(c);
                return ABN_OK;
            }
        }
    }
    abn v;
    ab_val nv;
value:
    if (entry_value(&e, &v, c->base, short_frac(c)) != ABN_OK) return ABN_OK;  /* out of range: key ignored */
    nv = ab_real(v);
    if (c->cpart && complex_entry_value(c, &v, &nv) != ABN_OK) return ABN_OK;
    if (c->vlen && vector_entry_value(c, &v, &nv) != ABN_OK) return ABN_OK;
    /* The first key of a number places it in X, lifting if lift is enabled. Digit entry itself
     * leaves the lift flag alone (UG p.B-5). */
    entry_put(c, nv, !c->entry.active);
    e.active = true;
    c->entry = e;
    return ABN_OK;
}

/* The i of x i y or the theta of r theta a (UG p.9-2, p.4-11): what was typed becomes the first
 * part and the second part starts empty (0). With nothing typed the first part is 0 and the
 * number arrives in X as any new number does (ours). A second i or theta is ignored (ours). In
 * HEX, OCT and BIN a complex number cannot be typed (ours): an error, nothing changed. */
static abn_status complex_key(ab_calc *c, ab_op op)
{
    bool was_active = c->entry.active;
    abn zero = abn_zero();
    ab_val nv;
    ab_entry saved = c->entry, saved1 = c->entry1;
    int saved_part = c->cpart;
    if (c->base != 10 || c->vlen) return ABN_DOMAIN;    /* a vector holds no complex number (ours) */
    if (c->cpart) return ABN_OK;
    if (was_active) c->entry1 = c->entry;
    else memset(&c->entry1, 0, sizeof c->entry1);
    c->cpart = op == AB_KI ? 1 : 2;
    if (complex_entry_value(c, &zero, &nv) != ABN_OK) {
        c->entry = saved;
        c->entry1 = saved1;
        c->cpart = saved_part;
        return ABN_OK;
    }
    memset(&c->entry, 0, sizeof c->entry);
    c->entry.active = true;
    entry_put(c, nv, !was_active);
    return ABN_OK;
}

/* [ ] begins a vector and , starts its next element (UG p.10-1). [ during a number being typed
 * completes it, and the vector arrives as any new number does; a second [ is ignored, as is a
 * comma outside a vector or after the third element (the guide allows three, p.10-1) (ours).
 * Inside a complex number, or in HEX, OCT and BIN, [ is an error (ours). */
static abn_status vector_key(ab_calc *c, ab_op op)
{
    abn zero = abn_zero();
    ab_val nv;
    if (op == AB_KVEC) {
        if (c->vlen) return ABN_OK;
        if (c->cpart || c->base != 10) return ABN_DOMAIN;
        if (c->entry.active) {
            c->entry.active = false;
            c->lift = true;
        }
        memset(&c->entry, 0, sizeof c->entry);
        c->entry.active = true;
        c->vlen = 1;
        nv = ab_vec(1);
        entry_put(c, nv, true);
        return ABN_OK;
    }
    if (!c->vlen || c->vlen == 3) return ABN_OK;
    c->ventry[c->vlen - 1] = c->entry;
    memset(&c->entry, 0, sizeof c->entry);
    c->entry.active = true;
    c->vlen++;
    if (vector_entry_value(c, &zero, &nv) == ABN_OK) entry_put(c, nv, false);
    return ABN_OK;
}

/* ---- operations ---- */

/* Flags 8 and 9 choose the fraction format (UG p.5-6, p.14-10): 8 clear, most precise; 8 set,
 * factors of /c; 8 and 9 set, fixed denominator /c. */
static fr_format frac_format_of(const ab_calc *c)
{
    return !c->flag8 ? FR_PRECISE : (c->flag9 ? FR_FIXED : FR_FACTORS);
}

static fn_angle angle_of(const ab_calc *c)
{
    return c->angle == AB_ANGLE_RAD ? FN_RAD : (c->angle == AB_ANGLE_GRAD ? FN_GRAD : FN_DEG);
}

/* A function of X alone replaces X and saves the old X in LAST X (UG p.B-6); an error leaves
 * the stack and LAST X as they were (ours, unit 003). */
/* The operation a storage or recall arithmetic key performs. */
static ab_op plain_op(ab_op op)
{
    switch (op) {
    case AB_STOADD: case AB_RCLADD: return AB_ADD;
    case AB_STOSUB: case AB_RCLSUB: return AB_SUB;
    case AB_STOMUL: case AB_RCLMUL: return AB_MUL;
    case AB_STODIV: case AB_RCLDIV: return AB_DIV;
    default: return op;
    }
}

/* The dot product, exact, rounded once (unit 010 rule 3); its numbers from the wide pool. */
static abn_status dot_(abn *out, const ab_val *a, const ab_val *b)
{
    xnum *sum, *t, *p, *q, *u;
    if (!ar_xn(5, &sum, &t, &p, &q, &u)) return ABN_RANGE;
    xn_set_int(sum, 0);
    for (int i = 0; i < a->len; i++) {
        if (!xn_from_abn(p, cvel(a, i)) || !xn_from_abn(q, cvel(b, i)) || !xn_mul(t, p, q) ||
            !xn_add(u, sum, t, false))
            return ABN_RANGE;
        *sum = *u;
    }
    xn_set_int(u, 1);
    return xn_div_to_abn(out, sum, u);
}

static abn_status dot(abn *out, const ab_val *a, const ab_val *b)
{
    int m = ar_mark();
    abn_status st = dot_(out, a, b);
    ar_release(m);
    return st;
}

/* a op b where a or b is a vector (UG p.10-1 to 10-5): + and - of equal lengths, one rounding per
 * element; x of two equal lengths is the dot product (UG p.10-4), of a vector and a number scales
 * it (either order: ours for the number first); / of a vector by a number. Anything else, a
 * complex number included, is INVALID DATA (UG p.10-1, unit 010 rule 2); / by 0 is an error
 * (rule 4). An element out of range changes nothing (rule 4). */
static abn_status vec_arith(ab_val *r, ab_op op, const ab_val *a, const ab_val *b)
{
    ab_val v;
    abn_status st = ABN_OK;
    if (a->kind == AB_COMPLEX || b->kind == AB_COMPLEX) return ABN_DOMAIN;
    switch (op) {
    case AB_ADD:
    case AB_SUB:
        if (!is_vec(a) || !is_vec(b) || a->len != b->len) return ABN_DOMAIN;
        v = ab_vec(a->len);
        for (int i = 0; i < a->len && st == ABN_OK; i++)
            st = op == AB_ADD ? abn_add(vel(&v, i), cvel(a, i), cvel(b, i))
                              : abn_sub(vel(&v, i), cvel(a, i), cvel(b, i));
        break;
    case AB_MUL:
        if (is_vec(a) && is_vec(b)) {
            if (a->len != b->len) return ABN_DOMAIN;
            v = ab_real(abn_zero());
            st = dot(&v.re, a, b);
            break;
        }
        {
            const ab_val *w = is_vec(a) ? a : b, *k = is_vec(a) ? b : a;
            v = ab_vec(w->len);
            for (int i = 0; i < w->len && st == ABN_OK; i++) st = abn_mul(vel(&v, i), cvel(w, i), &k->re);
        }
        break;
    default:    /* AB_DIV */
        if (is_vec(b)) return ABN_DOMAIN;           /* so a is the vector */
        if (abn_is_zero(&b->re)) return ABN_DIV_ZERO;
        v = ab_vec(a->len);
        for (int i = 0; i < a->len && st == ABN_OK; i++) st = abn_div(vel(&v, i), cvel(a, i), &b->re);
        break;
    }
    if (st != ABN_OK) return st;
    *r = v;
    return ABN_OK;
}

static abn_status settle(ab_calc *c, abn_status st, abn *r, bool neg);

/* x op y with a vector in either; LAST X and the drop as for real arithmetic (UG p.B-6). */
static abn_status two_vector(ab_calc *c, ab_op op)
{
    ab_val r;
    abn_status st = vec_arith(&r, op, &c->y, &c->x);
    if ((st == ABN_OVERFLOW || st == ABN_UNDERFLOW) && op == AB_MUL && is_vec(&c->x) && is_vec(&c->y)) {
        /* the dot product is a real result: settled (unit 026); exact, then rounded once, so the
           engine's sign is its own */
        abn d;
        if ((st = settle(c, st, &d, abn_overflow_negative())) == ABN_OK) r = ab_real(d);
    }
    if (st != ABN_OK) return st;
    c->lastx = c->x;
    c->x = r;
    stack_drop(c);
    c->lift = true;
    return ABN_OK;
}

/* ABS of a vector: the correctly rounded root of the exact sum of squares (UG p.10-3, unit 010
 * rule 3), less parts too small to change it (hp_norm). */
static abn_status vec_abs_(abn *r, const ab_val *v)
{
    abn el[3];
    for (int i = 0; i < v->len; i++) el[i] = *cvel(v, i);
    return hp_norm(r, el, v->len);      /* unit 034: parts far apart too (they were a range error) */
}

static abn_status vec_abs(ab_calc *c)
{
    abn r;
    int m = ar_mark();
    abn_status st = vec_abs_(&r, &c->x);
    ar_release(m);
    if ((st = settle(c, st, &r, false)) != ABN_OK) return st;      /* a real result (unit 026) */
    c->lastx = c->x;
    c->x = ab_real(r);
    c->lift = true;
    return ABN_OK;
}

/* The functions of a complex x (UG p.9-3): 1/z, ln z, e^z, sin z, cos z, tan z (radians whatever
 * the angle mode: the guide's sin(2 i 3)) and ABS, which is real. LAST X as for real x (UG p.B-6). */
static abn_status one_complex(ab_calc *c, ab_op op)
{
    cplx z = cx_of(&c->x), r;
    ab_val nv;
    abn_status st;
    switch (op) {
    case AB_INV: st = cx_inv(&r, &z); break;
    case AB_LN: st = cx_ln(&r, &z); break;
    case AB_EXP: st = cx_exp(&r, &z); break;
    case AB_SIN: st = cx_sin(&r, &z); break;
    case AB_COS: st = cx_cos(&r, &z); break;
    case AB_TAN: st = cx_tan(&r, &z); break;
    default: st = cx_abs(&r.re, &z); break;    /* AB_ABS */
    }
    if (op == AB_ABS) st = settle(c, st, &r.re, false);     /* |z| is a real result (unit 026) */
    if (st != ABN_OK) return st;
    nv = op == AB_ABS ? ab_real(r.re) : ab_cx(r);
    c->lastx = c->x;
    c->x = nv;
    c->lift = true;
    return ABN_OK;
}

/* ---- overflow and underflow (unit 026; 33s p.1-16, p.13-9, F-3; 35s p.1-17) ---- */

/* An overflow: flag 6 set (33s p.13-9: "any time an overflow occurs") and counted, for flag 5's stop
 * and the device's OVERFLOW notice. */
static void note_overflow(ab_calc *c)
{
    c->flags = (uint16_t)(c->flags | (1u << 6));
    c->ovf++;
}

/* A real result out of the range: an overflow is the largest number, with the sign the true result
 * has (neg), noted; an underflow is 0, silently. Neither is an error: the operation completes as any
 * other does. Other statuses pass. */
static abn_status settle(ab_calc *c, abn_status st, abn *r, bool neg)
{
    if (st == ABN_UNDERFLOW) {
        *r = abn_zero();
        return ABN_OK;
    }
    if (st != ABN_OVERFLOW) return st;
    abn m = abn_largest();
    *r = neg ? abn_neg(&m) : m;
    note_overflow(c);
    return ABN_OK;
}

/* The sign of an overflowing one-number function's true result: from the function where it forms
 * |result| first or reports the overflow itself; otherwise the engine's last rounding, which was the
 * result's own. */
static bool overflow_neg1(ab_op op, const abn *x)
{
    switch (op) {
    case AB_EXP: case AB_TENX: case AB_TWOX: case AB_EXPM1: case AB_COSH: case AB_SQ: case AB_FACT:
        return false;
    case AB_SINH: case AB_HMS: case AB_HMSINV: case AB_TODEG: case AB_TORAD: case AB_TOKG: case AB_TOLB:
    case AB_TOC: case AB_TOF: case AB_TOCM: case AB_TOIN: case AB_TOL: case AB_TOGAL: case AB_TOKM:
    case AB_TOMILE: case AB_RND: case AB_CUBE:
        return abn_is_negative(x);              /* odd in x, or x times a positive factor; RND keeps x's sign */
    default:
        return abn_overflow_negative();
    }
}

static bool odd_integer(const abn *x)
{
    abn two = abn_from_int(2), r;
    return abn_is_integer(x) && abn_fmod(&r, x, &two) == ABN_OK && !abn_is_zero(&r);
}

/* The sign of an overflowing y op x (storage arithmetic: the variable's op as its plain one). A sum
 * overflows only with both terms of one sign, a difference with y and -x of one sign. */
static bool overflow_neg2(ab_op op, const abn *y, const abn *x)
{
    bool ny = abn_is_negative(y), nx = abn_is_negative(x);
    switch (op) {
    case AB_ADD: case AB_STOADD: case AB_RCLADD: return abn_is_zero(y) ? nx : ny;
    case AB_SUB: case AB_STOSUB: case AB_RCLSUB: return abn_is_zero(y) ? !nx : ny;
    case AB_MUL: case AB_STOMUL: case AB_RCLMUL: case AB_DIV: case AB_STODIV: case AB_RCLDIV: case AB_IDIV:
        return ny != nx;
    case AB_POW: return ny && odd_integer(x);   /* a negative base to an odd power */
    case AB_XROOT: return ny;                   /* a negative's odd root */
    default: return false;                      /* nCr, nPr */
    }
}

static abn_status one_number_fn(ab_calc *c, ab_op op)
{
    abn r;
    abn_status st;
    fn_angle m = angle_of(c);
    if (is_vec(&c->x)) return vec_abs(c);     /* the only function the guard lets through */
    if (is_cx(&c->x)) return one_complex(c, op);
    switch (op) {
    case AB_EXP: st = fn_exp(&r, &c->x.re); break;
    case AB_LN: st = fn_ln(&r, &c->x.re); break;
    case AB_TENX: st = fn_tenx(&r, &c->x.re); break;
    case AB_LOG: st = fn_log(&r, &c->x.re); break;
    case AB_SQ: st = fn_sq(&r, &c->x.re); break;
    case AB_CUBE: {                             /* unit 041 C: x³, unit 038's exact power rounded once */
        abn three = abn_from_int(3);
        st = fn_pow(&r, &c->x.re, &three);
        break;
    }
    case AB_CBRT: {                             /* ∛x: the real root, through the hp root (unit 028) */
        abn three = abn_from_int(3);
        st = fn_xroot(&r, &c->x.re, &three);
        break;
    }
    case AB_SQRT: st = fn_sqrt(&r, &c->x.re); break;
    case AB_INV: st = fn_inv(&r, &c->x.re); break;
    case AB_SIN: st = fn_sin(&r, &c->x.re, m); break;
    case AB_COS: st = fn_cos(&r, &c->x.re, m); break;
    case AB_TAN: st = fn_tan(&r, &c->x.re, m); break;
    case AB_ASIN: st = fn_asin(&r, &c->x.re, m); break;
    case AB_ACOS: st = fn_acos(&r, &c->x.re, m); break;
    case AB_ATAN: st = fn_atan(&r, &c->x.re, m); break;
    case AB_SINH: st = fn_sinh(&r, &c->x.re); break;
    case AB_COSH: st = fn_cosh(&r, &c->x.re); break;
    case AB_TANH: st = fn_tanh(&r, &c->x.re); break;
    case AB_ASINH: st = fn_asinh(&r, &c->x.re); break;
    case AB_ACOSH: st = fn_acosh(&r, &c->x.re); break;
    case AB_ATANH: st = fn_atanh(&r, &c->x.re); break;
    case AB_HMS: st = fn_hms(&r, &c->x.re); break;           /* UG p.4-13 */
    case AB_HMSINV: st = fn_hmsinv(&r, &c->x.re); break;
    case AB_TODEG: st = fn_todeg(&r, &c->x.re); break;       /* UG p.4-13, p.4-14 */
    case AB_TORAD: st = fn_torad(&r, &c->x.re); break;
    case AB_TOKG: st = fn_convert(&r, &c->x.re, FN_TOKG); break;     /* UG p.4-14 */
    case AB_TOLB: st = fn_convert(&r, &c->x.re, FN_TOLB); break;
    case AB_TOC: st = fn_convert(&r, &c->x.re, FN_TOC); break;
    case AB_TOF: st = fn_convert(&r, &c->x.re, FN_TOF); break;
    case AB_TOCM: st = fn_convert(&r, &c->x.re, FN_TOCM); break;
    case AB_TOIN: st = fn_convert(&r, &c->x.re, FN_TOIN); break;
    case AB_TOL: st = fn_convert(&r, &c->x.re, FN_TOL); break;
    case AB_TOGAL: st = fn_convert(&r, &c->x.re, FN_TOGAL); break;
    case AB_TOKM: st = fn_convert(&r, &c->x.re, FN_TOKM); break;
    case AB_TOMILE: st = fn_convert(&r, &c->x.re, FN_TOMILE); break;
    case AB_FACT: st = fn_fact(&r, &c->x.re); break;         /* UG p.4-15 */
    case AB_EXPM1: st = fn_expm1(&r, &c->x.re); break;       /* ours, unit 021 */
    case AB_LN1P: st = fn_ln1p(&r, &c->x.re); break;
    case AB_TWOX: st = fn_twox(&r, &c->x.re); break;
    case AB_LOG2: st = fn_log2(&r, &c->x.re); break;
    case AB_IP: st = fn_ip(&r, &c->x.re); break;             /* UG p.4-17, p.4-18 */
    case AB_FP: st = fn_fp(&r, &c->x.re); break;
    case AB_INTG: st = fn_intg(&r, &c->x.re); break;
    case AB_ABS: st = fn_abs(&r, &c->x.re); break;
    case AB_SGN: st = fn_sgn(&r, &c->x.re); break;
    default:    /* AB_RND */
        if (c->fdisp) {
            /* RND in Fraction-display mode: x becomes the shown fraction, rounded once (UG p.5-8,
               unit 007 rule 6); beyond fractions, as the decimal format */
            fr_result fr;
            if (fr_approx(&fr, &c->x.re, c->frac_c, frac_format_of(c)) == ABN_OK) {
                st = fr_value(&r, &fr);
                break;
            }
        }
        st = fn_rnd(&r, &c->x.re, c->disp_mode, c->disp_n);
        break;
    }
    if ((st = settle(c, st, &r, overflow_neg1(op, &c->x.re))) != ABN_OK) return st;
    c->lastx = c->x;
    c->x = ab_real(r);
    c->lift = true;
    return ABN_OK;
}

/* Percent and percent change give their result in X and keep the base number in Y: the stack
 * does not drop (UG p.4-6). Both save x in LAST X (UG p.B-6). */
static abn_status percent_fn(ab_calc *c, ab_op op)
{
    abn r;
    abn_status st = op == AB_PCT ? fn_pct(&r, &c->y.re, &c->x.re) : fn_pctchg(&r, &c->y.re, &c->x.re);
    bool ny = abn_is_negative(&c->y.re);
    /* y x / 100; (x - y) / y 100 */
    bool neg = op == AB_PCT ? ny != abn_is_negative(&c->x.re) : (abn_cmp(&c->x.re, &c->y.re) < 0) != ny;
    if ((st = settle(c, st, &r, neg)) != ABN_OK) return st;
    c->lastx = c->x;
    c->x = ab_real(r);
    c->lift = true;
    return ABN_OK;
}

/* z1 op z2 with z1 in Y and z2 in X, either complex, a real one with imaginary part 0; the result
 * is complex even when its imaginary part is 0 (unit 009 rule 3). */
static abn_status two_complex(ab_calc *c, ab_op op)
{
    cplx a = cx_of(&c->y), b = cx_of(&c->x), r;
    abn_status st;
    switch (op) {
    case AB_ADD: st = cx_add(&r, &a, &b); break;
    case AB_SUB: st = cx_sub(&r, &a, &b); break;
    case AB_MUL: st = cx_mul(&r, &a, &b); break;
    case AB_DIV: st = cx_div(&r, &a, &b); break;
    default: st = cx_pow(&r, &a, &b); break;   /* AB_POW: y^x */
    }
    if (st != ABN_OK) return st;
    c->lastx = c->x;
    c->x = ab_cx(r);
    stack_drop(c);
    c->lift = true;
    return ABN_OK;
}

/* The 33s's CMPLX operations on stack pairs (unit 039; 33s p.9-1 to 9-3): z1 = Z + iT, z2 = X + iY, each part a
 * real; the result real in X and imaginary in Y. Z and T keep z1 (p.9-2: "replicated"), and a one-number
 * operation leaves them. LAST X gets x (p.B-6). The arithmetic is the complex engine's (rule 2; trigonometry in
 * radians, as one_complex); its errors, overflow included, change nothing. Decimal only, as every complex
 * function; a complex or vector part is INVALID DATA. */
static abn_status cmplx_pair(ab_calc *c, int arg)
{
    bool two = arg <= AB_CXP_POW;
    cplx z1, z2, r;
    abn_status st;
    if (arg < 0 || arg >= AB_CXP_N || c->base != 10) return ABN_DOMAIN;
    if (c->x.kind != AB_REAL || c->y.kind != AB_REAL || (two && (c->z.kind != AB_REAL || c->t.kind != AB_REAL)))
        return ABN_DOMAIN;
    z1.re = c->z.re, z1.im = c->t.re;
    z2.re = c->x.re, z2.im = c->y.re;
    switch (arg) {
    case AB_CXP_ADD: st = cx_add(&r, &z1, &z2); break;
    case AB_CXP_SUB: st = cx_sub(&r, &z1, &z2); break;
    case AB_CXP_MUL: st = cx_mul(&r, &z1, &z2); break;
    case AB_CXP_DIV: st = cx_div(&r, &z1, &z2); break;
    case AB_CXP_POW: st = cx_pow(&r, &z1, &z2); break;
    case AB_CXP_CHS: r = cx_neg(&z2), st = ABN_OK; break;
    case AB_CXP_INV: st = cx_inv(&r, &z2); break;
    case AB_CXP_LN: st = cx_ln(&r, &z2); break;
    case AB_CXP_EXP: st = cx_exp(&r, &z2); break;
    case AB_CXP_SIN: st = cx_sin(&r, &z2); break;
    case AB_CXP_COS: st = cx_cos(&r, &z2); break;
    default: st = cx_tan(&r, &z2); break;
    }
    if (st != ABN_OK) return st;
    c->lastx = c->x;
    c->x = ab_real(r.re);
    c->y = ab_real(r.im);
    c->lift = true;
    return ABN_OK;
}

static abn_status two_number_op(ab_calc *c, ab_op op)
{
    abn r;
    abn_status st;
    if (is_vec(&c->x) || is_vec(&c->y)) return two_vector(c, op);
    if (is_cx(&c->x) || is_cx(&c->y)) return two_complex(c, op);
    /* Y op X: the earlier number is on the left (UG p.2-5). */
    switch (op) {
    case AB_ADD: st = abn_add(&r, &c->y.re, &c->x.re); break;
    case AB_SUB: st = abn_sub(&r, &c->y.re, &c->x.re); break;
    case AB_MUL: st = abn_mul(&r, &c->y.re, &c->x.re); break;
    case AB_DIV: st = abn_div(&r, &c->y.re, &c->x.re); break;
    case AB_POW: st = fn_pow(&r, &c->y.re, &c->x.re); break;        /* y^x (UG p.4-2) */
    case AB_XROOT: st = fn_xroot(&r, &c->y.re, &c->x.re); break;    /* x-th root of y (UG p.4-3) */
    case AB_IDIV: st = fn_idiv(&r, &c->y.re, &c->x.re); break;      /* INT÷ (UG p.4-2) */
    case AB_NCR: st = fn_ncr(&r, &c->y.re, &c->x.re); break;        /* n in Y, r in X (UG p.4-16) */
    case AB_NPR: st = fn_npr(&r, &c->y.re, &c->x.re); break;
    default: st = fn_rmdr(&r, &c->y.re, &c->x.re); break;           /* Rmdr (UG p.4-2) */
    }
    if ((st = settle(c, st, &r, overflow_neg2(op, &c->y.re, &c->x.re))) != ABN_OK)
        return st;                  /* the stack stays as it was */
    c->lastx = c->x;                /* these operations save x in LAST X (UG p.B-6) */
    c->x = ab_real(r);
    stack_drop(c);
    c->lift = true;
    return ABN_OK;
}

/* ---- storage (unit 005; UG p.3-1 to 3-9, p.14-20 to 14-24) ---- */

/* A storage location: a lettered variable (which may hold a complex number), or a register that
 * holds a real (a statistics register or an unnamed indirect variable, with its address). */
typedef struct {
    ab_val *val;            /* A..Z, or NULL */
    abn *reg;               /* otherwise */
    int ind;                /* 0..800 for an unnamed indirect variable, otherwise -1 */
} slot;

/* The integer part of x as an int when it lies in [lo, hi]. */
static bool int_in(const abn *x, int lo, int hi, int *v)
{
    abn t;
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead, n, r = 0;
    if (abn_trunc(&t, x) != ABN_OK) return false;
    abn_digits(&t, &neg, d, &lead);
    n = (int)strlen(d);
    if (n == 0) {
        *v = 0;
        return lo <= 0 && 0 <= hi;
    }
    if (lead > 4) return false;
    for (int i = 0; i <= lead; i++) r = r * 10 + (i < n ? d[i] - '0' : 0);
    r = neg ? -r : r;
    if (r < lo || r > hi) return false;
    *v = r;
    return true;
}

/* A variable code names A..Z directly, or (I)/(J): the number in I or J chooses (UG p.14-22):
 * -1..-26 are A..Z, -27..-32 the statistics registers (n, sum x, sum y, sum x^2, sum y^2,
 * sum xy), 0..800 the unnamed indirect variables. A non-integer index is truncated (ours, unit
 * 005; the guide does not say). An index outside -32..800 is INVALID (I) (UG p.14-22); so is
 * reading an unnamed variable above the defined range (UG p.14-24). */
static abn_status resolve(ab_calc *c, int code, bool for_read, slot *out)
{
    int k;
    out->ind = -1;
    out->val = NULL;
    out->reg = NULL;
    if (code >= AB_VAR_A && code < AB_VAR_A + 26) {
        out->val = &c->var[code - AB_VAR_A];
        return ABN_OK;
    }
    if (code == AB_VAR_SI) {                    /* i (unit 025) */
        out->val = &c->var[26];
        return ABN_OK;
    }
    if (code != AB_VAR_IND_I && code != AB_VAR_IND_J) return ABN_DOMAIN;
    if (!ab_mode35(c)) {
        /* the 33s's (i) (33s p.13-21; unit 025): only |integer part of i| counts: 1-26 are A-Z,
           27 is i, 28-33 the statistics registers n, Sx, Sy, Sx2, Sy2, Sxy; 0 or 34 up INVALID (i).
           The 33s has one index: (J) is INVALID there. */
        const ab_val *ix = &c->var[26];
        if (code == AB_VAR_IND_J || ix->kind != AB_REAL || !int_in(&ix->re, -33, 33, &k) || k == 0) return ABN_DOMAIN;
        if (k < 0) k = -k;
        if (k <= 27) out->val = &c->var[k - 1];
        else out->reg = &c->stat[k - 28];
        return ABN_OK;
    }
    const ab_val *idx = &c->var[(code == AB_VAR_IND_I ? 'I' : 'J') - 'A'];
    if (idx->kind != AB_REAL || !int_in(&idx->re, -32, AB_IND_MAX, &k)) return ABN_DOMAIN;  /* INVALID (I) */
    if (k <= -27) {
        out->reg = &c->stat[-k - 27];
    } else if (k < 0) {
        out->val = &c->var[-k - 1];
    } else {
        if (for_read && k > c->ind_top) return ABN_DOMAIN;          /* INVALID (I): undefined */
        out->reg = &c->ind[k];
        out->ind = k;
    }
    return ABN_OK;
}

/* Writes v to a location. The unnamed indirect variables are defined from address 0 up to the
 * last nonzero one: storing a nonzero value above extends the range, and storing 0 in the last
 * one releases it down to the next nonzero (UG p.14-24). */
static void write_slot(ab_calc *c, const slot *sl, abn v)
{
    *sl->reg = v;
    if (sl->ind < 0) return;
    if (!abn_is_zero(&v)) {
        if (sl->ind > c->ind_top) c->ind_top = sl->ind;
    } else if (sl->ind == c->ind_top) {
        while (c->ind_top >= 0 && abn_is_zero(&c->ind[c->ind_top])) c->ind_top--;
    }
}

/* The value at a location. */
static ab_val load_val(const slot *sl)
{
    return sl->val ? *sl->val : ab_real(*sl->reg);
}

abn_status ab_indirect(ab_calc *c, int code, ab_val *out)
{
    slot sl;
    abn_status st = resolve(c, code, true, &sl);
    if (st == ABN_OK) *out = load_val(&sl);
    return st;
}

abn_status ab_eqn_based(const char *digits, int n, int base, abn *out)
{
    char d[AB_EQN_LEN];
    int64_t v;
    if (n <= 0 || n >= (int)sizeof d) return ABN_SYNTAX;
    memcpy(d, digits, (size_t)n);
    d[n] = '\0';
    if (base == 10) return abn_from_text(out, d) == ABN_OK ? ABN_OK : ABN_SYNTAX;
    if (!base_parse(d, base, &v)) return ABN_RANGE;     /* past the largest word (UG p.11-7) */
    *out = base_from_word(v);
    return ABN_OK;
}

/* Stores v. The registers that hold reals refuse a complex number (ours, unit 009: the guide
 * shows complex numbers in the lettered variables, UG p.9-2): INVALID DATA, nothing changed. */
static abn_status store_val(ab_calc *c, const slot *sl, ab_val v)
{
    if (sl->val) {
        *sl->val = v;
        return ABN_OK;
    }
    if (v.kind != AB_REAL) return ABN_DOMAIN;
    write_slot(c, sl, v.re);
    return ABN_OK;
}

/* a op b for storage and recall arithmetic; complex when either is (unit 009 rule 3). */
static abn_status arith(ab_calc *c, ab_val *r, ab_op op, const ab_val *a, const ab_val *b)
{
    abn_status st;
    if (is_vec(a) || is_vec(b)) return vec_arith(r, plain_op(op), a, b);
    if (is_cx(a) || is_cx(b)) {
        cplx x = cx_of(a), y = cx_of(b), z;
        switch (op) {
        case AB_STOADD: case AB_RCLADD: st = cx_add(&z, &x, &y); break;
        case AB_STOSUB: case AB_RCLSUB: st = cx_sub(&z, &x, &y); break;
        case AB_STOMUL: case AB_RCLMUL: st = cx_mul(&z, &x, &y); break;
        default: st = cx_div(&z, &x, &y); break;
        }
        if (st == ABN_OK) *r = ab_cx(z);
        return st;
    }
    abn v;
    switch (op) {
    case AB_STOADD: case AB_RCLADD: st = abn_add(&v, &a->re, &b->re); break;
    case AB_STOSUB: case AB_RCLSUB: st = abn_sub(&v, &a->re, &b->re); break;
    case AB_STOMUL: case AB_RCLMUL: st = abn_mul(&v, &a->re, &b->re); break;
    default: st = abn_div(&v, &a->re, &b->re); break;
    }
    st = settle(c, st, &v, overflow_neg2(op, &a->re, &b->re));
    if (st == ABN_OK) *r = ab_real(v);
    return st;
}

static int slot_id(const ab_calc *c, const slot *sl);

static abn_status storage_op(ab_calc *c, ab_op op, int code)
{
    slot sl;
    ab_val r, v;
    abn_status st;
    bool reads = op != AB_STO;
    if ((st = resolve(c, code, reads, &sl)) != ABN_OK) return st;
    switch (op) {
    case AB_STO:
        /* STO copies x into the variable; X keeps its value (UG p.3-2). */
        if ((st = store_val(c, &sl, c->x)) != ABN_OK) return st;
        break;
    case AB_RCL:
        /* RCL brings a copy into X like any recalled number (UG p.3-2, p.3-4; unit 005 rule 2). */
        place_val(c, load_val(&sl));
        break;
    case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:
        /* Storage arithmetic: variable = variable op x; the stack is untouched (UG p.3-6) and so
           is LAST X (not in UG p.B-6's list). An error changes nothing (unit 005 rule 3). */
        v = load_val(&sl);
        if ((st = arith(c, &r, op, &v, &c->x)) != ABN_OK) return st;
        if ((st = store_val(c, &sl, r)) != ABN_OK) return st;
        break;
    case AB_RCLADD: case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV:
        /* Recall arithmetic: x = x op variable; only X changes (UG p.3-7), and x is saved in
           LAST X (UG p.B-6). */
        v = load_val(&sl);
        if ((st = arith(c, &r, op, &c->x, &v)) != ABN_OK) return st;
        c->lastx = c->x;
        c->x = r;
        break;
    case AB_XVAR:
        /* x<>variable swaps them; Y, Z, T are untouched (UG p.3-8). */
        v = load_val(&sl);
        if ((st = store_val(c, &sl, c->x)) != ABN_OK) return st;
        c->x = v;
        break;
    default:
        /* VIEW shows the variable and changes nothing; it is not among the neutral operations
           of UG p.B-5, so like any other it enables lift (ours, unit 005). The display, Variable=
           Value, stays until the next key (UG p.3-4; abacus #3979). */
        c->viewing = true;
        c->view_id = slot_id(c, &sl);
        break;
    }
    c->lift = true;
    return ABN_OK;
}

/* ---- bases and logic (unit 008; UG ch.11) ---- */

/* A suffix rereads the digits being typed in its base and ends digit entry without ENTER
 * (UG p.11-3). A digit too big for that base is INVALID DATA (UG p.F) and the entry stays as it
 * was. With no entry in progress it does nothing (ours). */
static abn_status suffix(ab_calc *c, int base)
{
    char d[AB_ENTRY_TEXT];
    int64_t v;
    abn val;
    if (!c->entry.active) return ABN_OK;
    if (c->cpart || c->vlen) return ABN_DOMAIN;    /* a suffix reads a real entry (ours, units 009, 010) */
    if (base != 16 && base != 8 && base != 2 && base != 10) return ABN_DOMAIN;
    memcpy(d, c->entry.text, (size_t)c->entry.len);
    d[c->entry.len] = '\0';
    if (base == 10) {
        for (int i = 0; i < c->entry.len; i++)
            if (d[i] >= 'a' && d[i] <= 'f') return ABN_DOMAIN;
        if (entry_value(&c->entry, &val, 10, short_frac(c)) != ABN_OK) return ABN_DOMAIN;
    } else {
        if (strchr(d, '.') || strchr(d, 'E') || !base_parse(d, base, &v)) return ABN_DOMAIN;
        val = base_from_word(c->entry.mant_neg ? -v : v);
    }
    c->x = ab_real(val);
    c->entry.active = false;
    c->lift = true;
    return ABN_OK;
}

/* a * b clamped to the word's range, without overflowing the 64-bit product. */
static int64_t mul_clamp(int64_t a, int64_t b, bool *clamped)
{
    if (a == 0 || b == 0) return 0;
    int64_t aa = a < 0 ? -a : a, bb = b < 0 ? -b : b;
    if (aa > (int64_t)1 << 37 || bb > ((int64_t)1 << 62) / aa) {
        *clamped = true;
        return (a < 0) != (b < 0) ? base_min() : base_max();
    }
    return base_clamp(a * b, clamped);
}

/* + - * / in HEX, OCT, BIN: on the integer parts, the result truncated toward zero (UG p.11-4,
 * 11-5; toward zero is ours). An operand or result beyond the range is replaced by the nearest
 * end and the operation completes, reported as ABN_CLAMPED (UG p.11-7; ours, unit 008 rule 3). */
static abn_status base_arith(ab_calc *c, ab_op op)
{
    int64_t a, b, r;
    bool clamped = false;
    abn_status st;
    if ((st = base_to_word(&c->y.re, &a, &clamped)) != ABN_OK) return st;
    if ((st = base_to_word(&c->x.re, &b, &clamped)) != ABN_OK) return st;
    switch (op) {
    case AB_ADD: r = base_clamp(a + b, &clamped); break;
    case AB_SUB: r = base_clamp(a - b, &clamped); break;
    case AB_MUL: r = mul_clamp(a, b, &clamped); break;
    default:
        if (b == 0) return ABN_DIV_ZERO;
        r = base_clamp(a / b, &clamped);
        break;
    }
    c->lastx = c->x;
    c->x = ab_real(base_from_word(r));
    stack_drop(c);
    c->lift = true;
    if (clamped) note_overflow(c);              /* an overflow too (33s p.13-9; unit 026) */
    return clamped ? ABN_CLAMPED : ABN_OK;
}

/* AND XOR OR NAND NOR on Y and X, NOT on X: the integer patterns of the word, the signed result
 * (UG p.11-4). A non-integer argument is INVALID DATA (UG p.F-2); one beyond the range is first
 * replaced as for arithmetic, and reported. LAST X is untouched (not in UG p.B-6). */
static abn_status logic(ab_calc *c, ab_op op)
{
    int64_t a = 0, b, r;
    bool clamped = false;
    abn_status st;
    if (!abn_is_integer(&c->x.re) || (op != AB_NOT && !abn_is_integer(&c->y.re))) return ABN_DOMAIN;
    if ((st = base_to_word(&c->x.re, &b, &clamped)) != ABN_OK) return st;
    if (op == AB_NOT) {
        r = base_not(b);
        c->x = ab_real(base_from_word(r));
    } else {
        if ((st = base_to_word(&c->y.re, &a, &clamped)) != ABN_OK) return st;
        base_logic bl = op == AB_AND ? BL_AND : op == AB_OR ? BL_OR : op == AB_XOR ? BL_XOR
                      : op == AB_NAND ? BL_NAND : BL_NOR;
        r = base_logic2(bl, a, b);
        c->x = ab_real(base_from_word(r));
        stack_drop(c);
    }
    c->lift = true;
    if (clamped) note_overflow(c);              /* an overflow too (33s p.13-9; unit 026) */
    return clamped ? ABN_CLAMPED : ABN_OK;
}

/* ---- keystroke programs (unit 011; UG ch.13, 14) ---- */

#define TGT_NONE (26 * 1000)    /* a GTO or XEQ whose target line was deleted (ours) */

static abn_status do_key(ab_calc *c, ab_op op, int arg);
static abn_status table_key(ab_calc *c, ab_op op);    /* unit 032 (at the end) */
static abn_status table_go(ab_calc *c);
static abn_status graph_go(ab_calc *c);         /* unit 033 (at the end) */
static abn_status graph_key(ab_calc *c, ab_op op, int arg);
static void num_text(const abn *a, char *out, int size);     /* unit 032b (at the end) */

static void prog_labels(ab_calc *c)
{
    for (int l = 0; l < 26; l++) c->labelpos[l] = -1;
    for (int i = 0; i < c->prog_len; i++)
        if (c->prog[i].op == AB_LBL) c->labelpos[c->prog[i].arg] = i;
}

/* The label owning line i, the last LBL at or above it (UG p.13-23), or -1 above every label. */
static int prog_owner(const ab_calc *c, int i)
{
    int best = -1, pos = -1;
    for (int l = 0; l < 26; l++)
        if (c->labelpos[l] >= 0 && c->labelpos[l] <= i && c->labelpos[l] > pos) {
            pos = c->labelpos[l];
            best = l;
        }
    return best;
}

/* One past the last line owned by the label (or the unlabeled lines) starting at pos. */
static int prog_label_end(const ab_calc *c, int pos)
{
    int end = c->prog_len;
    for (int l = 0; l < 26; l++)
        if (c->labelpos[l] > pos && c->labelpos[l] < end) end = c->labelpos[l];
    return end;
}

/* A target's line: label * 1000 + line (line 0: the label's first), or -1 when the label or that
 * line does not exist (NONEXISTENT, UG p.F-3). */
static int prog_target(const ab_calc *c, int t)
{
    int l = t / 1000, n = t % 1000;
    if (t < 0 || l >= 26 || c->labelpos[l] < 0) return -1;
    if (n == 0) n = 1;
    int i = c->labelpos[l] + n - 1;
    return i < prog_label_end(c, c->labelpos[l]) ? i : -1;
}

static bool is_jump(int op)
{
    return op == AB_XEQ || op == AB_GTO;
}

/* Edits keep each GTO and XEQ on the line it aims at (UG p.13-7): before the edit every target
 * that resolves becomes its line index (stored as ABS_BASE - index); after it, shifted by the edit,
 * it becomes label and line again. A target whose line was deleted has none (ours). A target that
 * did not resolve (a label not yet keyed) stays as written and resolves when it runs. */
#define ABS_BASE (-1000000)     /* far from the indirect targets AB_TGT_IND_I/J */

static void targets_absolute(ab_calc *c)
{
    for (int i = 0; i < c->prog_len; i++) {
        if (!is_jump(c->prog[i].op)) continue;
        int j = prog_target(c, c->prog[i].arg);
        if (j >= 0) c->prog[i].arg = ABS_BASE - j;
    }
}

static void targets_symbolic(ab_calc *c, int k, bool inserted)
{
    prog_labels(c);
    for (int i = 0; i < c->prog_len; i++) {
        if (!is_jump(c->prog[i].op) || c->prog[i].arg > ABS_BASE) continue;
        int j = ABS_BASE - c->prog[i].arg;
        if (inserted) {
            if (j >= k) j++;
        } else if (j == k) {
            c->prog[i].arg = TGT_NONE;
            continue;
        } else if (j > k) {
            j--;
        }
        int l = prog_owner(c, j);
        c->prog[i].arg = l < 0 ? TGT_NONE : l * 1000 + (j - c->labelpos[l] + 1);
    }
}

/* Records a line after the one shown, at PRGM TOP before all (UG p.13-6). A label owns at most
 * AB_LABEL_LINES lines, the address form A001..A999 (UG p.13-4; past it MEMORY FULL, ours). */
static abn_status prog_insert(ab_calc *c, int op, int32_t arg)
{
    int k = c->pc + 1;
    /* only a line the state image may hold, so a saved program always loads back (unit 018) */
    if (op != AB_LIT && op != AB_EQLINE && !ab_line_ok((ab_op)op, arg)) return ABN_DOMAIN;
    if (c->prog_len >= AB_PROG_LINES) return ABN_MEMORY_FULL;
    if (op != AB_LBL) {
        int l = c->pc < 0 ? -1 : prog_owner(c, c->pc);
        int pos = l < 0 ? 0 : c->labelpos[l];
        int end = l < 0 ? (c->prog_len ? prog_label_end(c, -1) : 0) : prog_label_end(c, pos);
        if (l < 0) {
            end = c->prog_len;
            for (int m = 0; m < 26; m++)
                if (c->labelpos[m] >= 0 && c->labelpos[m] < end) end = c->labelpos[m];
        }
        if (end - pos >= AB_LABEL_LINES) return ABN_MEMORY_FULL;
    }
    targets_absolute(c);
    memmove(&c->prog[k + 1], &c->prog[k], (size_t)(c->prog_len - k) * sizeof c->prog[0]);
    c->prog[k].op = (uint16_t)op;
    c->prog[k].arg = arg;
    c->prog_len++;
    targets_symbolic(c, k, true);
    c->pc = k;
    return ABN_OK;
}

/* A typed number as a line: its value in the literal pool. */
static abn_status prog_insert_lit(ab_calc *c, ab_val v)
{
    int slot = -1;
    for (int i = 0; i < AB_PROG_LITS && slot < 0; i++)
        if (!c->lit_used[i]) slot = i;
    if (slot < 0) return ABN_MEMORY_FULL;
    abn_status st = prog_insert(c, AB_LIT, slot);
    if (st != ABN_OK) return st;
    c->lit[slot] = v;
    c->lit_used[slot] = true;
    return ABN_OK;
}

/* An equation as a line (UG p.13-7): its text in the equation-line store. */
/* The model an equation typed now is stamped with (unit 031 rule 3, answer A): in program entry, the stamp of the
 * label the line joins (unit 041 C, abacus #4850: entry follows the label's stamp; its typed names follow too); an
 * equation being edited, its own stamp (041 D, proposed: an edit in the other mode no longer breaks its spellings). */
static int prog_owner(const ab_calc *c, int i);
static uint8_t typed_model(const ab_calc *c)
{
    if (c->prgm && c->running <= 0 && c->pc >= 0 && c->pc < c->prog_len) {
        int l = prog_owner(c, c->pc);
        if (l >= 0 && c->lmode[l]) return c->lmode[l];
    }
    if (c->eqn_typing && c->eqn_edit >= 0 && c->eqn_edit < c->eqn_count && c->eqn_model[c->eqn_edit])
        return c->eqn_model[c->eqn_edit];       /* an equation edited keeps its stamp (041 D, fresh eyes; proposed) */
    return c->stu ? AB_MODEL_STU : c->m35 ? 35 : 33;
}

/* An equation's text as stored (unit 031 rule 7): typed in STU, an explicit x at each implied
   product; as typed otherwise, and for a message that does not parse (answer J). MEMORY_FULL when
   the x's would not fit (answer I). */
static abn_status stored_text(ab_calc *c, const char *text, char *out, int model)
{
    if (strlen(text) >= AB_EQN_LEN) return ABN_MEMORY_FULL;
    if (eqn_too_deep(c, text, model)) return ABN_TOO_DEEP;     /* nothing deeper is stored (unit 047) */
    if (c->stu) {
        int w = eqn_write_implied(c, text, out, AB_EQN_LEN);
        if (w < 0) return ABN_MEMORY_FULL;
        if (w > 0) return ABN_OK;
    }
    strcpy(out, text);
    return ABN_OK;
}

static abn_status prog_insert_eqn(ab_calc *c, const char *text)
{
    int slot = -1;
    char t[AB_EQN_LEN];
    for (int i = 0; i < AB_PEQN_MAX && slot < 0; i++)
        if (!c->peqn_used[i]) slot = i;
    if (slot < 0 || strlen(text) >= AB_EQN_LEN) return ABN_MEMORY_FULL;
    if (eqn_too_deep(c, text, typed_model(c))) return ABN_TOO_DEEP;    /* a program line too (unit 047) */
    strcpy(t, text);                            /* as typed, in STU too: its stamp fixes its meaning and a
                                                   message stays intact (unit 031b, review F2 ruling (a)) */
    abn_status st = prog_insert(c, AB_EQLINE, slot);
    if (st != ABN_OK) return st;
    strcpy(c->peqn[slot], t);
    c->peqn_used[slot] = true;
    c->peqn_model[slot] = typed_model(c);
    return ABN_OK;
}

/* Deletes line k; the pointer moves to the line before (UG p.13-20). */
static void prog_delete(ab_calc *c, int k)
{
    if (c->prog[k].op == AB_LIT) c->lit_used[c->prog[k].arg] = false;
    if (c->prog[k].op == AB_EQLINE) c->peqn_used[c->prog[k].arg] = false;
    targets_absolute(c);
    memmove(&c->prog[k], &c->prog[k + 1], (size_t)(c->prog_len - k - 1) * sizeof c->prog[0]);
    c->prog_len--;
    targets_symbolic(c, k, false);
    c->pc = k - 1;
}

static void prog_clear(ab_calc *c)
{
    c->prog_len = 0;
    memset(c->lit_used, 0, sizeof c->lit_used);
    memset(c->peqn_used, 0, sizeof c->peqn_used);
    c->peq_line = -1;
    c->pc = AB_PC_TOP;
    c->rdepth = 0;
    prog_labels(c);
}

abn_status ab_prog_delete_label(ab_calc *c, int l)
{
    if (c->running) return ABN_OK;
    if (l < 0 || l >= 26 || c->labelpos[l] < 0) return ABN_NONEXISTENT;
    int start = c->labelpos[l], end = start + 1;
    while (end < c->prog_len && c->prog[end].op != AB_LBL) end++;
    for (int k = end - 1; k >= start; k--) prog_delete(c, k);
    c->pc = start - 1 >= 0 ? start - 1 : AB_PC_TOP;
    c->rdepth = 0;
    c->stop = AB_STOP_NONE;
    prog_labels(c);
    return ABN_OK;
}

abn_status ab_var_clear(ab_calc *c, int v)
{
    if (c->running || v < 0 || v > 26) return ABN_OK;     /* 26: the 33s's i */
    c->var[v] = ab_real(abn_zero());
    return ABN_OK;
}

/* GTO . label nnn and GTO . . : the pointer only (UG p.14-6; neutral, p.B-5). Like any GTO from the
 * keyboard it ends what R/S could resume (UG p.13-19), so the return stack is emptied (ours). */
static abn_status prog_goto_pos(ab_calc *c, int arg)
{
    int i = arg == AB_PC_TOP ? AB_PC_TOP : prog_target(c, arg);
    if (arg != AB_PC_TOP && i < 0) return ABN_NONEXISTENT;
    c->pc = i;
    c->rdepth = 0;
    c->stop = AB_STOP_NONE;
    return ABN_OK;
}

/* x?y, x?0 (UG p.14-7: the X and Y registers; complex or vector operands are INVALID DATA) and
 * FS? (UG p.14-12). */
static abn_status prog_test(ab_calc *c, int op, int arg, bool *t)
{
    if (arg < 0 || arg > 11) return ABN_DOMAIN;
    if (op == AB_FS) {
        *t = ab_flag(c, arg);
        return ABN_OK;
    }
    abn zero = abn_zero();
    bool vs0 = arg >= 6;
    if (c->x.kind != AB_REAL || (!vs0 && c->y.kind != AB_REAL)) return ABN_DOMAIN;
    int r = abn_cmp(&c->x.re, vs0 ? &zero : &c->y.re);
    switch (arg % 6) {
    case AB_TEST_NE: *t = r != 0; break;
    case AB_TEST_LE: *t = r <= 0; break;
    case AB_TEST_LT: *t = r < 0; break;
    case AB_TEST_GT: *t = r > 0; break;
    case AB_TEST_GE: *t = r >= 0; break;
    default: *t = r == 0; break;
    }
    return ABN_OK;
}

/* ISG and DSE on the loop-control number +-ccccccc.fffii (UG p.14-18, 14-19): the counter is the
 * integer part with the number's sign, fff the first three decimals (the final value), ii the
 * next two (the step; 00 or left out is 1). ISG adds ii and skips when the new counter exceeds
 * fff; DSE subtracts ii and skips when it is at most fff. The fraction is written back unchanged
 * (digits past ii too: ours), with the new counter's sign (a counter reaching 0 is positive:
 * ours). A complex or vector number uses its real part or first element (UG p.14-19), the other
 * parts kept (ours). All exact. */
static abn_status loop_op(ab_calc *c, int op, int code, bool *skip)
{
    slot sl;
    abn_status st;
    if ((st = resolve(c, code, true, &sl)) != ABN_OK) return st;
    ab_val v = load_val(&sl);
    abn *p = &v.re;
    bool neg = abn_is_negative(p);
    abn a = abn_abs(p), ip, frac, t, fff, f5, ii, cnt, nw, absn;
    abn k100 = abn_from_int(100), k1000 = abn_from_int(1000), k100000 = abn_from_int(100000);
    if ((st = abn_trunc(&ip, &a)) != ABN_OK || (st = abn_sub(&frac, &a, &ip)) != ABN_OK) return st;
    if ((st = abn_mul(&t, &frac, &k1000)) != ABN_OK || (st = abn_trunc(&fff, &t)) != ABN_OK) return st;
    if ((st = abn_mul(&t, &frac, &k100000)) != ABN_OK || (st = abn_trunc(&f5, &t)) != ABN_OK) return st;
    if ((st = abn_mul(&t, &fff, &k100)) != ABN_OK || (st = abn_sub(&ii, &f5, &t)) != ABN_OK) return st;
    if (abn_is_zero(&ii)) ii = abn_from_int(1);
    cnt = neg ? abn_neg(&ip) : ip;
    if ((st = op == AB_ISG ? abn_add(&nw, &cnt, &ii) : abn_sub(&nw, &cnt, &ii)) != ABN_OK) return st;
    *skip = op == AB_ISG ? abn_cmp(&nw, &fff) > 0 : abn_cmp(&nw, &fff) <= 0;
    absn = abn_abs(&nw);
    if ((st = abn_add(&t, &absn, &frac)) != ABN_OK) return st;
    *p = abn_is_negative(&nw) ? abn_neg(&t) : t;
    return store_val(c, &sl, v);
}

/* XEQ (I), GTO (I) and the J forms (ours: p.14-23 lists neither; on p.14-22's numbering -1..-26
 * are labels A..Z): the index outside -32..800 is INVALID (I) (p.14-22); one naming a register
 * or variable rather than a label is NONEXISTENT, the guide's error for a missing label (ours). */
static abn_status ind_target(const ab_calc *c, int arg, int *tgt)
{
    int k;
    if (arg != AB_TGT_IND_I && arg != AB_TGT_IND_J) {
        *tgt = arg;
        return ABN_OK;
    }
    if (!ab_mode35(c)) {
        /* XEQ (i), GTO (i), FN= (i): the label |i| names, 1-26 (33s p.13-21; unit 025) */
        const ab_val *ix = &c->var[26];
        if (arg == AB_TGT_IND_J || ix->kind != AB_REAL || !int_in(&ix->re, -33, 33, &k) || k == 0) return ABN_DOMAIN;
        if (k < 0) k = -k;
        if (k > 26) return ABN_NONEXISTENT;
        *tgt = (k - 1) * 1000 + 1;
        return ABN_OK;
    }
    const ab_val *idx = &c->var[(arg == AB_TGT_IND_I ? 'I' : 'J') - 'A'];
    if (idx->kind != AB_REAL || !int_in(&idx->re, -32, AB_IND_MAX, &k)) return ABN_DOMAIN;
    if (k < -26 || k > -1) return ABN_NONEXISTENT;
    *tgt = (-k - 1) * 1000 + 1;
    return ABN_OK;
}

/* The variable a code names, as a stop_var id: A..Z 0..25, the statistics registers -27..-32,
 * an unnamed variable 1000 + its address (so INPUT (I) and VIEW (I) show what I names, p.14-22). */
static int slot_id(const ab_calc *c, const slot *sl)
{
    if (sl->val) return (int)(sl->val - c->var);
    if (sl->ind >= 0) return 1000 + sl->ind;
    return -27 - (int)(sl->reg - c->stat);
}

static void slot_of_id(ab_calc *c, int id, slot *sl)
{
    sl->val = NULL;
    sl->reg = NULL;
    sl->ind = -1;
    if (id >= 0 && id <= 26) {                  /* 26: i (unit 025) */
        sl->val = &c->var[id];
    } else if (id >= 1000) {
        sl->ind = id - 1000;
        sl->reg = &c->ind[sl->ind];
    } else {
        sl->reg = &c->stat[-id - 27];
    }
}

/* Runs from the pointer for at most `limit` lines (ours, unit 011 rule 2). A line is one core
 * operation, done exactly as from the keyboard (rule 3); a typed-number line arrives as a typed
 * number does and enables lift (the stack as typed; unit 001 ruling 1). It ends at a top-level RTN
 * (pointer at PRGM TOP, UG p.13-4), a STOP (pointer after it, p.13-19), an error (pointer at the
 * failing line, p.13-19, so R/S retries it: ours), or the budget (ABN_BUDGET). Running off the last
 * line acts as RTN, and execution runs on through a LBL line into the next label (ours: the guide
 * does not say). LBL, RTN, GTO, XEQ, the tests and ISG/DSE enable lift (UG p.B-4, none of them is
 * listed as neutral); STOP and PSE are neutral (p.B-5); PSE's pause is the device's. */
/* The variables an evaluation of text prompts for, in order of first appearance (UG p.6-11): the
 * right side's only for an assignment's ENTER (rhs_only), never the unknown or the variable of
 * integration (p.7-2, 8-3, 15-4). */
static int eqn_prompt_vars(const ab_calc *c, const char *text, int model, bool rhs_only, int *out)
{
    int v[27], n = eqn_vars(text, rhs_only, model == 33, model, v), k = 0;    /* i a variable by its stamp (041 A) */
    for (int i = 0; i < n; i++)
        if (!(c->susp.kind && v[i] == c->susp.var)) out[k++] = v[i];   /* susp is set while SOLVE runs */
    return k;
}

/* Prompts for eqn_vars[eqn_next]: its value in X with lift off, an INPUT stop (UG p.6-14). */
static void eqn_prompt(ab_calc *c)
{
    int var = c->eqn_vars[c->eqn_next];
    place_val(c, c->var[var]);
    c->lift = false;
    c->stop = AB_STOP_INPUT;
    c->stop_var = var;
}

static abn_status prog_run_lines(ab_calc *c, long limit, bool sst);
static bool continuing;     /* ab_continue: the run goes on, its count of lines with it */

/* Runs the program from the pointer (UG ch.13, 14); counts the runs in progress, so a SOLVE or
 * integral knows whether a program line keyed it. A run the keys start (not one SOLVE makes)
 * begins a new count of lines against the budget and forgets an interrupt left from before; a
 * limit of 1 there is SST. */
static abn_status prog_run(ab_calc *c, long limit)
{
    bool top = c->running == 0, sst = top && limit == 1 && !continuing;
    if (top && !continuing) {
        c->run_lines = 0;
        if (!c->active) c->intr = false;        /* (inside a SOLVE or integral a C is for that: kept, 040b) */
    }
    continuing = false;
    c->running++;
    abn_status st = prog_run_lines(c, limit, sst);
    c->running--;
    return st;
}

static abn_status prog_run_lines(ab_calc *c, long limit, bool sst)
{
    long steps = 0;
    c->stop = AB_STOP_NONE;     /* running: a stop reported earlier no longer applies */
    if (c->pc == AB_PC_TOP) {
        if (c->prog_len == 0) return ABN_OK;
        c->pc = 0;
    }
    while (steps < limit) {
        if (c->intr) {
            /* C or R/S during the run (UG p.13-19): the instruction before has finished; a run
               the keys started stops at the next line, R/S resuming it; one SOLVE or integrate
               makes ends that SOLVE or integral (p.7-8, 8-2) */
            c->intr = false;
            if (c->running > 1 || c->active) return ABN_INTERRUPTED;
            c->stop = AB_STOP_STOP;
            return ABN_OK;
        }
        if (c->running == 1 && !c->active && c->slice > 0 && steps >= c->slice && !sst) {
            c->stop = AB_STOP_SLICE;            /* the device's slice (unit 016) */
            return ABN_OK;
        }
        if (c->pc < 0 || c->pc >= c->prog_len) {
            if (c->rdepth > c->rbase) {
                c->pc = c->rstack[--c->rdepth];
                continue;
            }
            c->pc = AB_PC_TOP;
            c->lift = true;
            c->stop = AB_STOP_DONE;
            return ABN_OK;
        }
        int at = c->pc, next = at + 1, op = c->prog[at].op;
        int32_t arg = c->prog[at].arg;
        abn_status st = ABN_OK;
        bool skip = false;
        c->exec_at = at;                        /* its label's mode is in force (unit 025) */
        int ovf0 = c->ovf;
        steps++;
        if (c->running == 1) c->run_lines++;
        if (c->intr_after > 0 && --c->intr_after == 0) c->intr = true;   /* the tests' interrupt */
        switch (op) {
        case AB_LIT:
            place_val(c, c->lit[arg]);
            c->lift = true;
            break;
        case AB_LBL:
            c->lift = true;
            break;
        case AB_RTN:
            c->lift = true;
            if (c->rdepth > c->rbase) {
                next = c->rstack[--c->rdepth];
                break;
            }
            c->pc = AB_PC_TOP;
            c->stop = AB_STOP_DONE;
            return ABN_OK;
        case AB_XEQ:
        case AB_GTO: {
            int t, i;
            if ((st = ind_target(c, arg, &t)) != ABN_OK) break;
            i = prog_target(c, t);
            if (i < 0) {
                st = ABN_NONEXISTENT;
                break;
            }
            if (op == AB_XEQ) {
                if (c->rdepth == AB_RTN_DEPTH) {
                    st = ABN_XEQ_OVERFLOW;      /* a 21st nested XEQ (UG p.14-3) */
                    break;
                }
                c->rstack[c->rdepth++] = next;
            }
            next = i;
            c->lift = true;
            break;
        }
        case AB_RS:                             /* STOP */
            if (c->active) break;               /* while SOLVE / integrate evaluate: ignored (ours) */
            c->pc = next;
            c->stop = AB_STOP_STOP;
            return ABN_OK;
        case AB_PSE:                            /* shows X for the device's pause (p.13-19) */
            if (c->active) break;
            c->pc = next;
            c->stop = sst ? AB_STOP_STOP : AB_STOP_PSE;
            c->stop_var = -1;
            return ABN_OK;
        case AB_INPUT: {
            /* recalls the variable to X and prompts; lift is off while waiting so a typed reply
               replaces it (UG p.13-13, p.B-5, G-9); R/S stores X and resumes (unit 012) */
            slot sl;
            if ((st = resolve(c, arg, false, &sl)) != ABN_OK) break;
            if (c->active && (c->solve_phase != 1 || slot_id(c, &sl) == c->solve_var)) {
                /* while SOLVE / integrate evaluate (UG p.15-2, 15-8): the unknown's INPUT is
                   skipped; any other prompts in the first evaluation only (15-A: each variable once
                   per SOLVE, unit 015) and later reads the stored value as RCL would */
                if (slot_id(c, &sl) != c->solve_var) {
                    place_val(c, load_val(&sl));
                    c->lift = true;
                }
                break;
            }
            place_val(c, load_val(&sl));
            c->lift = false;
            c->pc = at;
            c->stop = AB_STOP_INPUT;
            c->stop_var = slot_id(c, &sl);
            return ABN_OK;
        }
        case AB_VIEW: {
            /* shows the variable, X untouched, and stops; followed by PSE it is shown through the
               pause and the run goes on, one step when single-stepping (UG p.13-15, 13-18) */
            slot sl;
            if ((st = resolve(c, arg, true, &sl)) != ABN_OK) break;
            c->lift = true;
            if (c->active) break;               /* not shown while SOLVE / integrate evaluate */
            c->stop_var = slot_id(c, &sl);
            c->stop = AB_STOP_VIEW;
            if (next < c->prog_len && c->prog[next].op == AB_PSE) {
                next++;
                c->stop = sst ? AB_STOP_STOP : AB_STOP_PSE;
            }
            c->pc = next;
            return ABN_OK;
        }
        case AB_EQLINE: {
            /* an equation line (UG p.13-16, 14-11): with flag 10 set it is shown as a message, like
               VIEW, the stack untouched (not while SOLVE / integrate evaluate, as VIEW); else it
               is evaluated and its value put on the stack, after prompts for its variables when
               flag 11 is set (not the unknown; not in SOLVE's later evaluations), and flag 11 is
               cleared */
            const char *t = c->peqn[arg];
            ab_val v;
            int pos;
            if (ab_flag(c, 10)) {
                if (c->active) break;
                c->stop = AB_STOP_MSG;
                c->stop_line = at;
                c->stop_var = -2;
                if (next < c->prog_len && c->prog[next].op == AB_PSE) {
                    next++;
                    c->stop = sst ? AB_STOP_STOP : AB_STOP_PSE;
                }
                c->pc = next;
                return ABN_OK;
            }
            if (ab_flag(c, 11) && !c->peq_ready && !(c->active && c->solve_phase != 1)) {
                /* the prompts first, the syntax when the value is calculated (33s p.6-18; unit 024
                   rule 4, applied to a program's equation line as to the keyboard's) */
                c->eqn_nvars = eqn_prompt_vars(c, t, c->peqn_model[arg], false, c->eqn_vars);
                if (c->eqn_nvars > 0) {
                    c->peq_line = at;
                    c->eqn_next = 0;
                    c->pc = at;
                    eqn_prompt(c);
                    return ABN_OK;
                }
            }
            c->peq_ready = false;
            c->peq_line = -1;
            if ((st = eqn_eval(c, t, c->peqn_model[arg], false, false, &v, &pos)) != ABN_OK) {
                c->eqn_errpos = pos;
                break;
            }
            c->flags &= (uint16_t)~(1u << 11);
            place_val(c, v);
            c->lift = true;
            break;
        }
        case AB_TEST:
        case AB_FS: {
            bool t;
            st = prog_test(c, op, arg, &t);
            if (st == ABN_OK) {
                skip = !t;                      /* do if true (UG p.14-6) */
                c->lift = true;
            }
            break;
        }
        case AB_ISG:
        case AB_DSE:
            st = loop_op(c, op, arg, &skip);
            if (st == ABN_OK) c->lift = true;
            break;
        case AB_SOLVE:
        case AB_INTEG:
            st = do_key(c, (ab_op)op, arg);
            if (st == ABN_OK && c->susp.kind && c->stop == AB_STOP_INPUT)
                return ABN_OK;                  /* its first evaluation prompts (unit 015) */
            if (st == ABN_INTERRUPTED) {
                /* interrupted (unit 016, ours): the program stops at the line, R/S runs it again */
                c->pc = at;
                c->stop = AB_STOP_STOP;
                return st;
            }
            if (op == AB_SOLVE && st == ABN_NO_ROOT) {
                st = ABN_OK;                    /* no message: the next line is skipped (p.15-6) */
                skip = true;
            }
            break;
        default:
            st = do_key(c, (ab_op)op, arg);
            break;
        }
        if (st != ABN_OK && st != ABN_CLAMPED) {
            if (st == ABN_OVERFLOW) note_overflow(c);   /* a complex or vector one (unit 026) */
            err_note(c, op, arg, NULL);
            c->pc = at;
            c->stop = AB_STOP_ERROR;
            c->stop_line = at;
            return st;
        }
        c->pc = skip ? next + 1 : next;
        if (op == AB_SOLVE || op == AB_INTEG) ovf0 = c->ovf;    /* its evaluations are not steps (rule 6) */
        if (c->ovf != ovf0 && ab_flag(c, 5) && !c->active) {
            /* flag 5 set: an overflow stops the run at the line after, OVERFLOW held until a key
               (33s p.13-9, F-3; unit 026 rule 3); R/S runs on. Not while SOLVE or integrate
               evaluate (rule 6). */
            c->stop = AB_STOP_STOP;
            c->msg = true;
            c->err_status = ABN_OVERFLOW;
            c->err_op = op;
            c->err_arg = arg;
            return ABN_OK;
        }
    }
    if (sst) {
        c->stop = AB_STOP_STOP;
        return ABN_OK;
    }
    c->stop = AB_STOP_BUDGET;
    return ABN_BUDGET;
}

/* ---- SOLVE and integrate on programmed functions (unit 013; UG ch.7, 8, 15, App. D, E) ---- */

/* The stack each evaluation starts from: the trial value in X over the stack the SOLVE or integral
 * found (ours: the guide does not say), so an evaluation does not depend on the one before. */

static bool math_error(abn_status st)
{
    return st == ABN_DIV_ZERO || st == ABN_RANGE || st == ABN_DOMAIN || st == ABN_OVERFLOW || st == ABN_UNDERFLOW;
}

static abn_status fn_value(const ab_calc *c, const ab_val *v, abn *f);

/* f(t): the trial value stored in the variable and put in X, the function's label run as a
 * subroutine (it holds the return level SOLVE / integrate took), f the X it returns. */
/* Unit 040: while GRAPH's tools run SOLVE's or the integral's engine, the function is the plotted curve (1) or its
   slope (2), evaluated purely (033's eval_at), instead of SOLVE's own (0). */
static int ev_graph;
static abn_status eval_at(ab_calc *c, int e, int v, const abn *x, ab_val *y, bool pure);

static abn_status graph_f(ab_calc *c, const abn *x, abn *f)
{
    ab_val v;
    abn_status st = eval_at(c, c->gr_eq, c->gr_v, x, &v, true);
    if (st == ABN_OK && v.kind != AB_REAL) st = ABN_DOMAIN;    /* (a complex or vector value: not on the curve) */
    *f = v.re;
    return st;
}

/* The curve's slope at x: a 5-point central difference, (f(x-2h) - 8 f(x-h) + 8 f(x+h) - f(x+2h)) / 12h with
   h = 1E-7 max(1, |x|); its error about h^4 f^(5) / 30 + 1E-34 |f| / h, near 1E-26 relative on smooth curves
   (unit 040 rule 4: numeric now). */
static abn_status graph_slope(ab_calc *c, const abn *x, abn *m)
{
    abn h, ax = abn_abs(x), one = abn_from_int(1), e7, p, f[4], k8 = abn_from_int(8), k12 = abn_from_int(12), t, u;
    static const int K[4] = {-2, -1, 1, 2};
    abn_status st;
    abn_from_text(&e7, "1E-7");
    if ((st = abn_mul(&h, abn_cmp(&ax, &one) < 0 ? &one : &ax, &e7)) != ABN_OK) return st;
    for (int i = 0; i < 4; i++) {
        abn k = abn_from_int(K[i]);
        if ((st = abn_mul(&t, &k, &h)) != ABN_OK || (st = abn_add(&p, x, &t)) != ABN_OK ||
            (st = graph_f(c, &p, &f[i])) != ABN_OK)
            return st;
    }
    if ((st = abn_sub(&t, &f[2], &f[1])) != ABN_OK || (st = abn_mul(&t, &t, &k8)) != ABN_OK ||
        (st = abn_sub(&u, &f[0], &f[3])) != ABN_OK || (st = abn_add(&t, &t, &u)) != ABN_OK ||
        (st = abn_mul(&u, &h, &k12)) != ABN_OK)
        return st;
    return abn_div(m, &t, &u);
}

/* One evaluation for a graph tool, the curve (slope false) or its slope: C during the tool ends it (040b), as between
   SOLVE's evaluations, and each counts (unit 040; the extremum check's too, 040d). */
static abn_status graph_eval(ab_calc *c, bool slope, const abn *t, abn *f)
{
    if (c->intr) {
        c->intr = false;
        return ABN_INTERRUPTED;
    }
    c->evals++;
    if (c->intr_after > 0 && --c->intr_after == 0) c->intr = true;     /* the tests' interrupt */
    return slope ? graph_slope(c, t, f) : graph_f(c, t, f);
}

static abn_status eval_fn(ab_calc *c, const abn *t, abn *f)
{
    if (ev_graph) return graph_eval(c, ev_graph == 2, t, f);              /* unit 040 */
    slot sl;
    abn_status st;
    int pc0 = c->pc, rb0 = c->rbase;
    if (c->ev_cached && abn_cmp(t, &c->ev_cx) == 0) {
        c->ev_cached = false;                   /* the first evaluation's (unit 015) */
        *f = c->ev_cf;
        return c->ev_cst;
    }
    if (c->intr) {
        c->intr = false;                        /* C or R/S between evaluations (unit 016) */
        return ABN_INTERRUPTED;
    }
    slot_of_id(c, c->solve_var, &sl);
    if ((st = store_val(c, &sl, ab_real(*t))) != ABN_OK) return st;
    c->x = ab_real(*t);
    c->y = c->ev_y;
    c->z = c->ev_z;
    c->t = c->ev_t;
    c->lift = true;
    if (c->fn_eqn >= 0) {
        /* a listed equation as the function (UG p.7-2, 8-3): its value, "=" as "-" */
        int pos;
        ab_val v;
        c->evals++;
        if (c->intr_after > 0 && --c->intr_after == 0) c->intr = true;
        if ((st = eqn_eval(c, c->eqn[c->fn_eqn], c->eqn_model[c->fn_eqn], false, false, &v, &pos)) != ABN_OK) return st;
        return fn_value(c, &v, f);
    }
    c->rbase = c->rdepth;
    c->pc = c->labelpos[c->fn];
    st = prog_run(c, c->budget);
    c->rdepth = c->rbase;
    c->rbase = rb0;
    c->pc = pc0;
    c->evals++;
    if (st != ABN_OK) return st;
    return fn_value(c, &c->x, f);
    return ABN_OK;
}

/* f as SOLVE and integrate take it: a real number; for an integral also a one-element vector or
 * one whose other elements are 0, by its first element (UG p.10-6); else INVALID DATA. */
static bool ev_nonreal;     /* the last evaluation's value was not one SOLVE / integrate take */
static bool ev_best_set;    /* the evaluated point with the least |f| in the current SOLVE */
static abn ev_best_x, ev_best_f;

static abn_status fn_value(const ab_calc *c, const ab_val *v, abn *f)
{
    ev_nonreal = false;
    if (v->kind == AB_REAL ||
        (c->active == AB_INTEG && v->kind == AB_VECTOR &&
         (v->len == 1 || (abn_is_zero(&v->im) && (v->len < 3 || abn_is_zero(&v->p3)))))) {
        *f = v->re;
        return ABN_OK;
    }
    ev_nonreal = true;
    return ABN_DOMAIN;
}

static int sgn(const abn *a)
{
    return abn_is_zero(a) ? 0 : abn_is_negative(a) ? -1 : 1;
}

static int cmp_abs(const abn *a, const abn *b)
{
    abn x = abn_abs(a), y = abn_abs(b);
    return abn_cmp(&x, &y);
}

#define SOLVE_SEARCH 100    /* evaluations the search for a sign change may use (ours) */
#define SOLVE_MAX 400       /* evaluations one SOLVE may use (ours) */

/* The outcome of the root finder: a root (or, failing, the point of least |f|), the other end of
 * the last bracket or the previous estimate, and f at the first. */
typedef struct {
    abn x, prev, fx;
    bool found;
} solve_out;

/* f at x; a math error makes the point unusable (bad, no root: UG p.7-9, D-12 report NO ROOT FND
 * for an undefined region); other errors stop SOLVE. */
static abn_status ev(ab_calc *c, const abn *x, abn *f, bool *bad)
{
    abn_status st;
    ev_nonreal = false;
    st = eval_fn(c, x, f);
    *bad = math_error(st) && !ev_nonreal;  /* a complex or vector f is INVALID DATA, no point */
    if (st == ABN_OK) {
        /* the best estimate so far, for an interrupt (UG p.7-8: it is left in the unknown) */
        abn af = abn_abs(f), ab = abn_abs(&ev_best_f);
        if (!ev_best_set || abn_cmp(&af, &ab) < 0) {
            ev_best_set = true;
            ev_best_x = *x;
            ev_best_f = *f;
        }
    }
    return *bad ? ABN_OK : st;
}

/* Two guesses (UG p.7-8): if f has the same sign at both, a secant search (each step at most ten
 * times the last interval; doubling outward where f is flat) looks for a sign change; then
 * Brent's method (R. P. Brent, Algorithms for Minimization without Derivatives, 1973, ch.4,
 * procedure zero), then bisection until the bracket's ends are adjacent 34-digit numbers. The
 * result is the end with the smaller |f| (ties: the lower), the other end as the previous estimate
 * (UG p.D-3). No sign change within SOLVE_SEARCH evaluations, a search that closes in without
 * one, an unusable point, or a converged bracket whose |f| exceeds both starting values (a pole,
 * UG p.D-7: ours, reconciling D-6's text with D-D's example) is no root, reported at the point of
 * least |f|. */
#define SOLVE_MIN_EVALS 80  /* evaluations the minimizer may use, within SOLVE_MAX (unit 024b) */

/* The points the search for a sign change evaluated, for the minimizer (unit 024b). */
static abn sp_x[SOLVE_SEARCH + 4], sp_f[SOLVE_SEARCH + 4];
static int sp_n;

static void sp_add(const abn *x, const abn *f)
{
    if (sp_n < SOLVE_SEARCH + 4) {
        sp_x[sp_n] = *x;
        sp_f[sp_n] = *f;
        sp_n++;
    }
}

typedef struct {
    abn x, fx, w, fw;       /* the least |f| and f there; its nearest neighbour and f there */
    bool found;             /* f was 0 at x */
    bool sign;              /* f changed sign: x and w now straddle a root */
} min_out;

/* Brent's localmin (R. P. Brent, Algorithms for Minimization without Derivatives, 1973, ch.5):
 * golden section with parabolic steps, on |f| over [lo, hi] from x inside it (f(x) = fx given,
 * |f| below both ends). Stops at its tolerance, its own cap or SOLVE's budget, an unusable point or
 * an arithmetic failure; reports the best point so far. */
static abn_status solve_min(ab_calc *c, abn lo, abn hi, abn x, abn fx, min_out *o)
{
    abn cg, eps, tabs, half, two, v, w, fv, fw, fxa, sfw, d = abn_zero(), e = abn_zero();
    abn m, tol, t2, p, q, r, u, su, fu, t, t3;
    bool bad;
    abn_status st;
    abn_from_text(&cg, "0.3819660112501051517954131656343619");    /* (3 - sqrt 5) / 2 */
    abn_from_text(&eps, "1E-17");                                  /* about sqrt of the precision */
    abn_from_text(&tabs, "1E-30");
    abn_from_text(&half, "0.5");
    two = abn_from_int(2);
    o->found = o->sign = false;
    if (sgn(&lo) * sgn(&hi) < 0 && !abn_is_zero(&x)) {
        /* a bracket around 0: f(0) first, as the root finder does, since a relative tolerance has
           no floor there (ours) */
        abn z = abn_zero(), fz;
        if ((st = ev(c, &z, &fz, &bad)) != ABN_OK) return st;
        if (!bad) {
            if (abn_is_zero(&fz) || sgn(&fz) != sgn(&fx)) {
                o->found = abn_is_zero(&fz);
                o->sign = !o->found;
                o->x = o->found ? z : x;
                o->fx = o->found ? fz : fx;
                o->w = o->found ? x : z;
                o->fw = o->found ? fx : fz;
                return ABN_OK;
            }
            if (cmp_abs(&fz, &fx) < 0) {        /* 0 the better point: the bracket narrows to it */
                if (abn_cmp(&x, &z) < 0) lo = x;
                else hi = x;
                x = z;
                fx = fz;
            } else if (abn_cmp(&x, &z) < 0) {
                hi = z;
            } else {
                lo = z;
            }
        }
    }
    v = w = x;
    fxa = fv = fw = abn_abs(&fx);
    sfw = fx;
    for (int k = 0; k < SOLVE_MIN_EVALS && c->evals < SOLVE_MAX; k++) {
        bool golden = true;
        if (abn_add(&m, &lo, &hi) != ABN_OK || abn_mul(&m, &m, &half) != ABN_OK) break;
        t = abn_abs(&x);
        if (abn_mul(&tol, &t, &eps) != ABN_OK || abn_add(&tol, &tol, &tabs) != ABN_OK ||
            abn_mul(&t2, &tol, &two) != ABN_OK)
            break;
        /* done when |x - m| <= t2 - (hi - lo) / 2 */
        if (abn_sub(&t, &x, &m) != ABN_OK || abn_sub(&t3, &hi, &lo) != ABN_OK || abn_mul(&t3, &t3, &half) != ABN_OK ||
            abn_sub(&t3, &t2, &t3) != ABN_OK)
            break;
        t = abn_abs(&t);
        if (abn_cmp(&t, &t3) <= 0) break;
        if (cmp_abs(&e, &tol) > 0) {            /* a parabola through x, w, v */
            abn a1, a2, xw, xv, pq;
            if (abn_sub(&xw, &x, &w) != ABN_OK || abn_sub(&xv, &x, &v) != ABN_OK ||
                abn_sub(&a1, &fxa, &fv) != ABN_OK || abn_mul(&r, &xw, &a1) != ABN_OK ||
                abn_sub(&a2, &fxa, &fw) != ABN_OK || abn_mul(&q, &xv, &a2) != ABN_OK ||
                abn_mul(&a1, &xv, &q) != ABN_OK || abn_mul(&a2, &xw, &r) != ABN_OK || abn_sub(&p, &a1, &a2) != ABN_OK ||
                abn_sub(&q, &q, &r) != ABN_OK || abn_mul(&q, &q, &two) != ABN_OK)
                break;
            if (sgn(&q) > 0) p = abn_neg(&p);
            else q = abn_neg(&q);
            r = e;
            e = d;
            /* the parabola's step is taken when it is short (|p| < |q r / 2|) and stays inside */
            abn qa, qb;
            if (abn_mul(&pq, &q, &r) != ABN_OK || abn_mul(&pq, &pq, &half) != ABN_OK ||
                abn_sub(&t, &lo, &x) != ABN_OK || abn_mul(&qa, &q, &t) != ABN_OK ||
                abn_sub(&t, &hi, &x) != ABN_OK || abn_mul(&qb, &q, &t) != ABN_OK)
                break;
            if (cmp_abs(&p, &pq) < 0 && abn_cmp(&p, &qa) > 0 && abn_cmp(&p, &qb) < 0 && !abn_is_zero(&q)) {
                abn ua, ub;
                if (abn_div(&d, &p, &q) != ABN_OK || abn_add(&u, &x, &d) != ABN_OK || abn_sub(&ua, &u, &lo) != ABN_OK ||
                    abn_sub(&ub, &hi, &u) != ABN_OK)
                    break;
                if (abn_cmp(&ua, &t2) < 0 || abn_cmp(&ub, &t2) < 0) d = abn_cmp(&x, &m) < 0 ? tol : abn_neg(&tol);
                golden = false;
            }
        }
        if (golden) {                           /* a golden-section step into the larger part */
            if (abn_sub(&e, abn_cmp(&x, &m) < 0 ? &hi : &lo, &x) != ABN_OK || abn_mul(&d, &cg, &e) != ABN_OK) break;
        }
        if (cmp_abs(&d, &tol) >= 0) {           /* a step of at least tol */
            if (abn_add(&u, &x, &d) != ABN_OK) break;
        } else {
            abn nt = abn_neg(&tol);
            if (abn_add(&u, &x, sgn(&d) > 0 ? &tol : &nt) != ABN_OK) break;
        }
        if ((st = ev(c, &u, &su, &bad)) != ABN_OK) return st;
        if (bad) break;
        if (abn_is_zero(&su)) {                 /* a root at the bottom: found */
            o->found = true;
            o->x = u;
            o->fx = su;
            o->w = x;
            o->fw = fx;
            return ABN_OK;
        }
        if (sgn(&su) != sgn(&fx)) {             /* a sign change: the root finder takes it from here */
            o->sign = true;
            o->x = x;
            o->fx = fx;
            o->w = u;
            o->fw = su;
            return ABN_OK;
        }
        fu = abn_abs(&su);
        if (abn_cmp(&fu, &fxa) <= 0) {
            if (abn_cmp(&u, &x) < 0) hi = x;
            else lo = x;
            v = w;
            fv = fw;
            w = x;
            fw = fxa;
            sfw = fx;
            x = u;
            fxa = fu;
            fx = su;
        } else {
            if (abn_cmp(&u, &x) < 0) lo = u;
            else hi = u;
            if (abn_cmp(&fu, &fw) <= 0 || abn_cmp(&w, &x) == 0) {
                v = w;
                fv = fw;
                w = u;
                fw = fu;
                sfw = su;
            } else if (abn_cmp(&fu, &fv) <= 0 || abn_cmp(&v, &x) == 0 || abn_cmp(&v, &w) == 0) {
                v = u;
                fv = fu;
            }
        }
    }
    o->x = x;
    o->fx = fx;
    o->w = w;
    o->fw = sfw;
    return ABN_OK;
}

/* Among the search's points sorted by x, the one with the least |f| when both its neighbours have
   more: the bracket of a minimum of |f| (unit 024b). False when the least is at either end. */
static bool sp_bracket(int *lo, int *mid, int *hi)
{
    int idx[SOLVE_SEARCH + 4], n = 0;
    for (int i = 0; i < sp_n; i++) {            /* sorted by x, a repeated x once */
        int j = n;
        bool dup = false;
        for (int k = 0; k < n; k++)
            if (abn_cmp(&sp_x[idx[k]], &sp_x[i]) == 0) dup = true;
        if (dup) continue;
        while (j > 0 && abn_cmp(&sp_x[idx[j - 1]], &sp_x[i]) > 0) {
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = i;
        n++;
    }
    int best = 0;
    for (int k = 1; k < n; k++)
        if (cmp_abs(&sp_f[idx[k]], &sp_f[idx[best]]) < 0) best = k;
    if (best == 0 || best == n - 1) return false;
    if (cmp_abs(&sp_f[idx[best]], &sp_f[idx[best - 1]]) >= 0 || cmp_abs(&sp_f[idx[best]], &sp_f[idx[best + 1]]) >= 0)
        return false;
    *lo = idx[best - 1];
    *mid = idx[best];
    *hi = idx[best + 1];
    return true;
}

static abn_status solve_core(ab_calc *c, abn a, abn b, solve_out *o)
{
    abn fa, fb, fc, cc, d, e, t1, t2, best, fbest, fmax;
    abn two = abn_from_int(2), half, eps, tiny;
    bool bad;
    abn_status st;
    abn_from_text(&half, "0.5");
    abn_from_text(&eps, "1E-33");
    abn_from_text(&tiny, "1E-6000");
    o->found = false;
    sp_n = 0;
    if (abn_cmp(&a, &b) == 0) {
        /* equal guesses are nudged apart (UG p.7-9): by 1E-6 relative, or 1E-6 at 0 (ours) */
        abn n;
        abn_from_text(&n, "1E-6");
        if (abn_is_zero(&a)) b = n;
        else if (abn_mul(&t1, &a, &n) != ABN_OK || abn_add(&b, &a, &t1) != ABN_OK) b = n;
    }
    if ((st = ev(c, &a, &fa, &bad)) != ABN_OK) return st;
    if (bad) {
        o->x = b;
        o->prev = a;
        o->fx = abn_zero();
        if ((st = ev(c, &b, &o->fx, &bad)) != ABN_OK) return st;
        return ABN_OK;
    }
    if ((st = ev(c, &b, &fb, &bad)) != ABN_OK) return st;
    best = a;
    fbest = fa;
    o->prev = b;
    if (bad) goto noroot;
    sp_add(&a, &fa);
    sp_add(&b, &fb);
    if (abn_is_zero(&fa) || abn_is_zero(&fb)) {
        bool az = abn_is_zero(&fa);
        o->x = az ? a : b;
        o->prev = az ? b : a;
        o->fx = abn_zero();
        o->found = true;
        return ABN_OK;
    }
    /* the search for a sign change */
    for (int k = 0; sgn(&fa) == sgn(&fb); k++) {
        abn x, fx, step, lim;
        if (k >= SOLVE_SEARCH) goto nosign;
        if (cmp_abs(&fa, &fb) < 0) {         /* b the better point */
            abn tx = a, tf = fa;
            a = b;
            fa = fb;
            b = tx;
            fb = tf;
        }
        best = b;
        fbest = fb;
        o->prev = a;
        if (abn_sub(&t1, &b, &a) != ABN_OK) goto noroot;
        if (abn_cmp(&fa, &fb) != 0) {
            /* secant: x = b - fb (b - a) / (fb - fa), the step at most 10 |b - a| */
            abn ten = abn_from_int(10);
            if (abn_sub(&t2, &fb, &fa) != ABN_OK || abn_div(&step, &t1, &t2) != ABN_OK ||
                abn_mul(&step, &step, &fb) != ABN_OK)
                goto noroot;
            step = abn_neg(&step);
            if (abn_mul(&lim, &t1, &ten) != ABN_OK) goto noroot;
            lim = abn_abs(&lim);
            if (cmp_abs(&step, &lim) > 0) step = abn_is_negative(&step) ? abn_neg(&lim) : lim;
        } else if (abn_mul(&step, &t1, &two) != ABN_OK) {   /* flat: double outward, past b */
            goto noroot;
        }
        if (abn_add(&x, &b, &step) != ABN_OK) goto noroot;
        if (abn_cmp(&x, &b) == 0 || abn_cmp(&x, &a) == 0) {
            if (abn_add(&x, &b, &t1) != ABN_OK) goto noroot;
            if (abn_cmp(&x, &b) == 0) goto closed;  /* no room left to move: closed in */
        }
        if ((st = ev(c, &x, &fx, &bad)) != ABN_OK) return st;
        if (bad) goto noroot;
        sp_add(&x, &fx);
        a = b;
        fa = fb;
        b = x;
        fb = fx;
        if (abn_is_zero(&fb)) {
            o->x = b;
            o->prev = a;
            o->fx = fb;
            o->found = true;
            return ABN_OK;
        }
        if (cmp_abs(&fb, &fbest) < 0) {
            best = b;
            fbest = fb;
            o->prev = a;
        }
        /* closing in on a minimum of |f| without a sign change */
        {
            abn ma = abn_abs(&a), mb = abn_abs(&b), w, rel;
            abn_from_text(&rel, "1E-30");
            if (abn_sub(&w, &b, &a) != ABN_OK) goto noroot;
            w = abn_abs(&w);
            if (abn_cmp(&ma, &mb) < 0) ma = mb;
            if (abn_mul(&t2, &ma, &rel) != ABN_OK) goto noroot;
            if (abn_cmp(&w, &t2) <= 0) goto closed;
        }
    }
    goto bracket;
closed:
    if (sgn(&fa) != sgn(&fb)) goto bracket;     /* the last step crossed: a bracket, not a closing-in (unit 042:
                                                   the test ran before the loop's own sign test saw it) */
    /* closed in without a sign change (unit 042). With |f| not rising (b no worse than a) a root may lie just past b,
       approached from one side: step past it, 2, 4, ... 256 times the secant's step (b - a where f is flat at its
       last digit; at least a unit of b's last digit), until f changes sign (then Brent's bracket) or is 0; each a
       counted evaluation within SOLVE_MAX. Otherwise, or if none crosses, no sign change: 024b's minimum path, as
       before. */
    if (cmp_abs(&fb, &fa) <= 0) {
        abn s, u, x, fx, k = abn_from_int(2), mag = abn_abs(&b);
        if (abn_sub(&t1, &b, &a) != ABN_OK) goto nosign;
        if (abn_cmp(&fa, &fb) == 0) {
            s = t1;                             /* f flat at its last digit: on from a past b */
        } else {
            if (abn_sub(&t2, &fb, &fa) != ABN_OK || abn_div(&s, &t1, &t2) != ABN_OK || abn_mul(&s, &s, &fb) != ABN_OK)
                goto nosign;
            s = abn_neg(&s);                    /* the secant's step from b to the projected root */
        }
        if (abn_is_zero(&s)) goto nosign;
        abn_from_text(&u, abn_is_zero(&mag) ? "1E-6176" : "1E-33");
        if (!abn_is_zero(&mag) && abn_mul(&u, &u, &mag) != ABN_OK) goto nosign;   /* a unit of b's last digit */
        for (int i = 0; i < 8 && c->evals < SOLVE_MAX; i++) {
            abn d;
            if (abn_mul(&d, &s, &k) != ABN_OK) break;
            if (cmp_abs(&d, &u) < 0) d = abn_is_negative(&s) ? abn_neg(&u) : u;
            if (abn_add(&x, &b, &d) != ABN_OK || abn_cmp(&x, &b) == 0) break;
            if ((st = ev(c, &x, &fx, &bad)) != ABN_OK) return st;
            if (bad) break;
            sp_add(&x, &fx);
            if (abn_is_zero(&fx)) {
                o->x = x;
                o->prev = b;
                o->fx = fx;
                o->found = true;
                return ABN_OK;
            }
            if (sgn(&fx) != sgn(&fb)) {         /* crossed: [b, x] brackets the root */
                a = b;
                fa = fb;
                b = x;
                fb = fx;
                goto bracket;
            }
            k = abn_from_int(2 << (i + 1));     /* the next: 4, 8, ... 256 */
        }
    }
    goto nosign;
bracket:
    /* Brent's method on the bracket [a, b] */
    fmax = cmp_abs(&fa, &fb) > 0 ? abn_abs(&fa) : abn_abs(&fb);
    if (sgn(&a) * sgn(&b) < 0) {
        /* a bracket around 0: f(0) first, so no later bracket straddles 0, where a relative
           tolerance has no floor (ours) */
        abn z = abn_zero(), fz;
        if ((st = ev(c, &z, &fz, &bad)) != ABN_OK) return st;
        if (!bad) {
            if (abn_is_zero(&fz)) {
                o->x = z;
                o->prev = cmp_abs(&fa, &fb) <= 0 ? a : b;
                o->fx = fz;
                o->found = true;
                return ABN_OK;
            }
            if (sgn(&fz) == sgn(&fa)) {
                a = z;
                fa = fz;
            } else {
                b = z;
                fb = fz;
            }
        }
    }
    cc = a;
    fc = fa;
    if (abn_sub(&d, &b, &a) != ABN_OK) goto noroot;
    e = d;
    while (c->evals < SOLVE_MAX) {
        abn tol, xm, p, q, r, sv, m1, m2;
        if (sgn(&fb) == sgn(&fc)) {
            cc = a;
            fc = fa;
            if (abn_sub(&d, &b, &a) != ABN_OK) goto noroot;
            e = d;
        }
        if (cmp_abs(&fc, &fb) < 0) {
            a = b;
            b = cc;
            cc = a;
            fa = fb;
            fb = fc;
            fc = fa;
        }
        t1 = abn_abs(&b);
        if (abn_mul(&tol, &t1, &eps) != ABN_OK || abn_mul(&tol, &tol, &two) != ABN_OK ||
            abn_add(&tol, &tol, &tiny) != ABN_OK)
            goto noroot;
        if (abn_sub(&xm, &cc, &b) != ABN_OK || abn_mul(&xm, &xm, &half) != ABN_OK) goto noroot;
        if (cmp_abs(&xm, &tol) <= 0 || abn_is_zero(&fb)) break;
        t1 = abn_abs(&e);
        if (abn_cmp(&t1, &tol) >= 0 && cmp_abs(&fa, &fb) > 0) {
            /* inverse quadratic interpolation, or the secant when a = c */
            abn one = abn_from_int(1);
            if (abn_div(&sv, &fb, &fa) != ABN_OK) goto bisect;
            if (abn_cmp(&a, &cc) == 0) {
                if (abn_mul(&p, &xm, &two) != ABN_OK || abn_mul(&p, &p, &sv) != ABN_OK ||
                    abn_sub(&q, &one, &sv) != ABN_OK)
                    goto bisect;
            } else {
                abn u, w;
                if (abn_div(&q, &fa, &fc) != ABN_OK || abn_div(&r, &fb, &fc) != ABN_OK) goto bisect;
                /* p = s (2 xm q (q - r) - (b - a)(r - 1)); q = (q - 1)(r - 1)(s - 1) */
                if (abn_sub(&u, &q, &r) != ABN_OK || abn_mul(&u, &u, &q) != ABN_OK ||
                    abn_mul(&u, &u, &xm) != ABN_OK || abn_mul(&u, &u, &two) != ABN_OK ||
                    abn_sub(&w, &b, &a) != ABN_OK || abn_sub(&t2, &r, &one) != ABN_OK ||
                    abn_mul(&w, &w, &t2) != ABN_OK || abn_sub(&u, &u, &w) != ABN_OK ||
                    abn_mul(&p, &u, &sv) != ABN_OK)
                    goto bisect;
                if (abn_sub(&u, &q, &one) != ABN_OK || abn_sub(&w, &r, &one) != ABN_OK ||
                    abn_mul(&u, &u, &w) != ABN_OK || abn_sub(&w, &sv, &one) != ABN_OK ||
                    abn_mul(&q, &u, &w) != ABN_OK)
                    goto bisect;
            }
            if (sgn(&p) > 0) q = abn_neg(&q);
            p = abn_abs(&p);
            /* accept if 2p < min(3 xm q - |tol q|, |e q|) */
            {
                abn three = abn_from_int(3), tq;
                if (abn_mul(&m1, &xm, &q) != ABN_OK || abn_mul(&m1, &m1, &three) != ABN_OK ||
                    abn_mul(&tq, &tol, &q) != ABN_OK)
                    goto bisect;
                tq = abn_abs(&tq);
                if (abn_sub(&m1, &m1, &tq) != ABN_OK || abn_mul(&m2, &e, &q) != ABN_OK) goto bisect;
                m2 = abn_abs(&m2);
                if (abn_cmp(&m1, &m2) > 0) m1 = m2;
                if (abn_mul(&t2, &p, &two) != ABN_OK) goto bisect;
                if (abn_cmp(&t2, &m1) < 0) {
                    e = d;
                    if (abn_div(&d, &p, &q) != ABN_OK) goto bisect;
                } else {
                    d = xm;
                    e = d;
                }
            }
        } else {
        bisect:     /* also an interpolation whose arithmetic left the range (f at the largest
                       number, unit 026): a bisection step instead */
            d = xm;
            e = d;
        }
        a = b;
        fa = fb;
        if (cmp_abs(&d, &tol) > 0) {
            if (abn_add(&b, &b, &d) != ABN_OK) goto noroot;
        } else {
            abn st1 = sgn(&xm) < 0 ? abn_neg(&tol) : tol;
            if (abn_add(&b, &b, &st1) != ABN_OK) goto noroot;
        }
        if ((st = ev(c, &b, &fb, &bad)) != ABN_OK) return st;
        if (bad) {
            /* f failed at the new b: report from the last defined point, the old b (now a), so the
               NO ROOT report's X is a point where f is defined and Z is f there (33s p.7-6, D-12;
               unit 024 rule 2) */
            b = a;
            fb = fa;
            goto noroot_bracket;
        }
    }
    /* bisection until the ends are adjacent */
    while (!abn_is_zero(&fb) && c->evals < SOLVE_MAX) {
        abn mid, fm;
        if (abn_sub(&t1, &cc, &b) != ABN_OK || abn_mul(&t1, &t1, &half) != ABN_OK ||
            abn_add(&mid, &b, &t1) != ABN_OK)
            goto noroot;
        if (abn_cmp(&mid, &b) == 0 || abn_cmp(&mid, &cc) == 0) break;
        if ((st = ev(c, &mid, &fm, &bad)) != ABN_OK) return st;
        if (bad) goto noroot_bracket;
        if (sgn(&fm) == sgn(&fb)) {
            b = mid;
            fb = fm;
        } else {
            cc = mid;
            fc = fm;
        }
        if (abn_is_zero(&fm)) {
            b = mid;
            fb = fm;
        }
    }
    if (!abn_is_zero(&fb) && c->evals >= SOLVE_MAX) goto noroot_bracket;
    {
        /* the end with the smaller |f| (ties: the lower) is the root */
        int k = abn_is_zero(&fb) ? -1 : cmp_abs(&fb, &fc);
        bool take_b = k < 0 || (k == 0 && abn_cmp(&b, &cc) < 0);
        o->x = take_b ? b : cc;
        o->fx = take_b ? fb : fc;
        o->prev = take_b ? cc : b;
        t1 = abn_abs(&o->fx);
        o->found = abn_cmp(&t1, &fmax) <= 0;    /* else a pole */
        return ABN_OK;
    }
nosign:
    /* No sign change (unit 024b): when the search's points bracket a minimum of |f|, Brent's
       minimizer finds it, and the report is that minimum (33s p.D-8, D-10: the search ends near a
       local minimum, X and Y close by, Z = f there). A zero at the bottom is a root; a sign change
       found on the way goes to the root finder. With no interior minimum, the report is as before. */
    {
        int il, im, ih;
        min_out mo;
        if (sp_bracket(&il, &im, &ih)) {
            if ((st = solve_min(c, sp_x[il], sp_x[ih], sp_x[im], sp_f[im], &mo)) != ABN_OK) return st;
            if (mo.found) {
                o->x = mo.x;
                o->prev = mo.w;
                o->fx = mo.fx;
                o->found = true;
                return ABN_OK;
            }
            if (mo.sign) {
                a = mo.x;
                fa = mo.fx;
                b = mo.w;
                fb = mo.fw;
                goto bracket;
            }
            o->x = mo.x;
            o->fx = mo.fx;
            o->prev = abn_cmp(&mo.w, &mo.x) != 0 ? mo.w
                      : cmp_abs(&sp_f[il], &sp_f[ih]) <= 0 ? sp_x[il] : sp_x[ih];   /* never moved: the nearer end */
            return ABN_OK;
        }
    }
    goto noroot;
noroot_bracket:
    best = cmp_abs(&fb, &fc) <= 0 ? b : cc;
    fbest = cmp_abs(&fb, &fc) <= 0 ? fb : fc;
    o->prev = cmp_abs(&fb, &fc) <= 0 ? cc : b;
noroot:
    o->x = best;
    o->fx = fbest;
    return ABN_OK;
}

static abn_status fn_first(ab_calc *c);

/* A SOLVE or integral begins (unit 015): what it will restore or report is kept in c->susp, the
 * return level it holds is taken (UG p.15-11), and its first evaluation follows. */
static void susp_begin(ab_calc *c, int kind, int var, int code, ab_val var0)
{
    c->susp.kind = kind;
    c->susp.var = var;
    c->susp.code = code;
    c->susp.x = c->x;
    c->susp.y = c->y;
    c->susp.z = c->z;
    c->susp.t = c->t;
    c->susp.lastx = c->lastx;
    c->susp.lift = c->lift;
    c->susp.var0 = var0;
    c->susp.pc = c->pc;
    c->susp.rbase = c->rbase;
    c->susp.rdepth = c->rdepth;
    c->susp.from_prog = c->running > 0;
    c->active = kind;
    c->solve_var = var;
    c->solve_phase = 1;
    c->evals = 0;
    c->ev_cached = false;
    c->ev_y = c->y;
    c->ev_z = c->z;
    c->ev_t = c->t;
    c->rstack[c->rdepth++] = -1;               /* the level SOLVE / integrate holds */
}

/* Everything as it was before the SOLVE or integral: an error, or C at a prompt of its first
 * evaluation. */
static void susp_abort(ab_calc *c)
{
    slot sl;
    slot_of_id(c, c->susp.var, &sl);
    if (sl.val) *sl.val = c->susp.var0;
    c->x = c->susp.x;
    c->y = c->susp.y;
    c->z = c->susp.z;
    c->t = c->susp.t;
    c->lastx = c->susp.lastx;
    c->lift = c->susp.lift;
    c->rdepth = c->susp.rdepth;
    c->rbase = c->susp.rbase;
    c->pc = c->susp.pc;
    c->active = 0;
    c->solve_phase = 0;
    c->susp.kind = 0;
    c->ev_cached = false;
    c->peq_line = -1;
    c->peq_ready = false;
}

/* SOLVE var (UG p.7-2, 7-8, 15-1): the guesses are the variable and X; the root goes to the
 * variable and X, the previous estimate to Y, f at the root to Z; T keeps its value and lift is
 * enabled (ours: the guide does not say; SOLVE is in neither of p.B-5's lists); LAST X is kept
 * (not in p.B-6's list). No root: ABN_NO_ROOT with the same report at the point of least |f| (UG
 * p.7-8); a program skips the next line instead (p.15-6). The search uses one of the 20 return
 * levels (p.15-11). Any other error leaves everything as it was. */
static abn_status solve_finish(ab_calc *c)
{
    slot sl;
    solve_out o;
    abn_status st = solve_core(c, c->susp.var0.re, c->susp.x.re, &o);
    slot_of_id(c, c->susp.var, &sl);
    if (st != ABN_OK) return st;
    *sl.val = ab_real(o.x);
    c->x = ab_real(o.x);
    c->y = ab_real(o.prev);
    c->z = ab_real(o.fx);
    c->t = c->susp.t;
    c->lastx = c->susp.lastx;
    c->lift = true;
    return o.found ? ABN_OK : ABN_NO_ROOT;
}

static abn_status solve_op(ab_calc *c, int code)
{
    slot sl;
    abn_status st;
    if (c->active) {
        err_note(c, AB_SOLVE, code, NULL);      /* SOLVE(SOLVE) or ∫(SOLVE) (p.F-1, F-4) */
        return ABN_NESTED;
    }
    if (c->fn_eqn < 0 && (c->fn < 0 || c->labelpos[c->fn] < 0)) return ABN_SELECT_FN;
    if ((st = resolve(c, code, false, &sl)) != ABN_OK) return st;
    if (!sl.val) return ABN_DOMAIN;             /* not an unnamed or statistics register (p.14-23) */
    if (sl.val->kind != AB_REAL || c->x.kind != AB_REAL) return ABN_DOMAIN;   /* BAD GUESS (p.F-1) */
    if (c->rdepth >= AB_RTN_DEPTH) return ABN_XEQ_OVERFLOW;
    susp_begin(c, AB_SOLVE, slot_id(c, &sl), code, *sl.val);
    return fn_first(c);
}

/* Gauss-Kronrod 7-15 on [-1, 1] (A. S. Kronrod, Nodes and Weights of Quadrature Formulas, 1965;
 * the values solved here from exactness to degree 22 with mpmath at 80 digits): the positive nodes,
 * their Kronrod weights, and the Gauss weights of the 7-point rule (every other node, and 0). */
static const char *const GK_X[8] = {"0.9914553711208126392068546975263285166",
    "0.9491079123427585245261896840478512624", "0.8648644233597690727897127886409262012",
    "0.7415311855993944398638647732807884071", "0.5860872354676911302941448382587295984",
    "0.4058451513773971669066064120769614633", "0.2077849550078984676006894037732449135", "0"};
static const char *const GK_WK[8] = {"0.02293532201052922496373200805896959199",
    "0.06309209262997855329070066318920428667", "0.1047900103222501838398763225415180174",
    "0.1406532597155259187451895905102379204", "0.1690047266392679028265834265985502841",
    "0.1903505780647854099132564024210136828", "0.2044329400752988924141619992346490847",
    "0.2094821410847278280129991748917142637"};
static const char *const GK_WG[4] = {"0.1294849661688696932706114326790820183",
    "0.2797053914892766679014677714237795825", "0.3818300505051189449503697754889751339",
    "0.4179591836734693877551020408163265306"};

#define INTEG_PARTS 100     /* subintervals one integral may use: 1,500 evaluations (ours) */

static abn ia[INTEG_PARTS], ib[INTEG_PARTS], ires[INTEG_PARTS], ierr[INTEG_PARTS], iabs[INTEG_PARTS];

/* One 15-point rule on [a, b]: the Kronrod result, its error estimate (QUADPACK's QK15: R.
 * Piessens et al., QUADPACK, 1983: resasc min(1, (200 |K - G| / resasc)^1.5)) and the integral
 * of |f| (for the display-format uncertainty). */
static abn_status gk15(ab_calc *c, const abn *a, const abn *b, abn *res, abn *err, abn *rabs)
{
    abn cen, h, fv[15], two = abn_from_int(2), half, rk = abn_zero(), rg = abn_zero(), ra = abn_zero(),
        rasc = abn_zero(), t, x, w, kh, k200 = abn_from_int(200);
    abn_status st;
    abn_from_text(&half, "0.5");
    if ((st = abn_add(&cen, a, b)) != ABN_OK || (st = abn_mul(&cen, &cen, &half)) != ABN_OK ||
        (st = abn_sub(&h, b, a)) != ABN_OK || (st = abn_mul(&h, &h, &half)) != ABN_OK)
        return st;
    for (int i = 0; i < 8; i++) {
        abn xi, wk;
        abn_from_text(&xi, GK_X[i]);
        abn_from_text(&wk, GK_WK[i]);
        if ((st = abn_mul(&t, &h, &xi)) != ABN_OK) return st;
        for (int side = 0; side < (i < 7 ? 2 : 1); side++) {
            abn f, fa;
            if ((st = side ? abn_sub(&x, &cen, &t) : abn_add(&x, &cen, &t)) != ABN_OK) return st;
            /* an equation's integrand (it can't store): the engine's path; a program's correctly rounded, since what it
               stores a person can read (unit 044 rule 5, abacus #4893, #4897) */
            bool was = fn_set_fast(ev_graph || c->fn_eqn >= 0);
            st = eval_fn(c, &x, &f);
            fn_set_fast(was);
            if (st != ABN_OK) return st;
            fv[i * 2 + side] = f;
            if ((st = abn_mul(&w, &wk, &f)) != ABN_OK || (st = abn_add(&rk, &rk, &w)) != ABN_OK) return st;
            fa = abn_abs(&f);
            if ((st = abn_mul(&w, &wk, &fa)) != ABN_OK || (st = abn_add(&ra, &ra, &w)) != ABN_OK) return st;
            if (i % 2 == 1) {
                abn wg;
                abn_from_text(&wg, GK_WG[i / 2]);
                if ((st = abn_mul(&w, &wg, &f)) != ABN_OK || (st = abn_add(&rg, &rg, &w)) != ABN_OK) return st;
            }
        }
    }
    /* resasc: the integral of |f - mean| with the Kronrod weights */
    if ((st = abn_mul(&kh, &rk, &half)) != ABN_OK) return st;
    for (int i = 0; i < 8; i++) {
        abn wk, dv;
        abn_from_text(&wk, GK_WK[i]);
        for (int side = 0; side < (i < 7 ? 2 : 1); side++) {
            if ((st = abn_sub(&dv, &fv[i * 2 + side], &kh)) != ABN_OK) return st;
            dv = abn_abs(&dv);
            if ((st = abn_mul(&w, &wk, &dv)) != ABN_OK || (st = abn_add(&rasc, &rasc, &w)) != ABN_OK) return st;
        }
    }
    {
        abn ah = abn_abs(&h);
        if ((st = abn_mul(res, &rk, &h)) != ABN_OK || (st = abn_sub(&t, &rk, &rg)) != ABN_OK ||
            (st = abn_mul(err, &t, &h)) != ABN_OK || (st = abn_mul(rabs, &ra, &ah)) != ABN_OK ||
            (st = abn_mul(&rasc, &rasc, &ah)) != ABN_OK)
            return st;
        *err = abn_abs(err);
        if (!abn_is_zero(&rasc) && !abn_is_zero(err)) {
            abn r, one = abn_from_int(1), sr;
            if ((st = abn_mul(&r, err, &k200)) != ABN_OK || (st = abn_div(&r, &r, &rasc)) != ABN_OK) return st;
            if (abn_cmp(&r, &one) < 0) {
                if ((st = abn_sqrt(&sr, &r)) != ABN_OK || (st = abn_mul(&r, &r, &sr)) != ABN_OK) return st;
            } else {
                r = one;
            }
            if ((st = abn_mul(err, &rasc, &r)) != ABN_OK) return st;
        }
        (void)two;
    }
    return ABN_OK;
}

/* integrate var from Y to X (UG p.8-2, 15-8): X the integral, Y its uncertainty, Z the upper and T
 * the lower limit; lift enabled; LAST X and the variable kept (ours: the guide does not say). The
 * accuracy follows the display format at the time (p.8-2, 8-6): f is taken as good to the digits
 * shown, an uncertainty U = 10^-n times the integral of |f| in SCI n or ENG n, 10^-n |b - a| in
 * FIX n, ALL as SCI 11 (the guide pairs "SCI 11 or ALL", p.E-5) (ours, unit 013 rule 2). Global
 * adaptive Gauss-Kronrod 7-15 (QUADPACK's QAG) subdivides the worst piece until its error estimate
 * is below U / 10, or INTEG_PARTS pieces; Y = U + the error estimate, so it covers the error of
 * the rule as well as that of f's digits. The ends are never evaluated (p.8-5). An error in the
 * function stops the integral (the guide's integrator cannot proceed either), nothing changed. */
/* The adaptive rule from lo to hi (above): *tot the integral, *unc its uncertainty U + the error estimate (unit 040
   draws it out of integ_finish for GRAPH's AREA; integ_finish's behaviour unchanged). */
static abn_status integ_core(ab_calc *c, const abn *lo, const abn *hi, abn *tot_out, abn *unc_out)
{
    abn_status st;
    int n = 1, mode = c->disp_mode, digits = c->disp_n;
    abn tot = abn_zero(), err, rab, u = abn_zero(), tenth, t;
    if (mode == AB_ALL) digits = 11;
    abn_from_text(&tenth, "0.1");
    ia[0] = *lo;
    ib[0] = *hi;
    st = gk15(c, &ia[0], &ib[0], &ires[0], &ierr[0], &iabs[0]);
    while (st == ABN_OK) {
        int worst = 0;
        tot = err = rab = abn_zero();
        for (int i = 0; i < n; i++) {
            if ((st = abn_add(&tot, &tot, &ires[i])) != ABN_OK || (st = abn_add(&err, &err, &ierr[i])) != ABN_OK ||
                (st = abn_add(&rab, &rab, &iabs[i])) != ABN_OK)
                break;
            if (abn_cmp(&ierr[i], &ierr[worst]) > 0) worst = i;
        }
        if (st != ABN_OK) break;
        {
            abn p;
            if ((st = abn_pow10(&p, -digits)) != ABN_OK) break;
            if (mode == AB_FIX) {
                abn w;
                if ((st = abn_sub(&w, hi, lo)) != ABN_OK) break;
                w = abn_abs(&w);
                st = abn_mul(&u, &w, &p);
            } else {
                st = abn_mul(&u, &rab, &p);
            }
            if (st != ABN_OK) break;
        }
        if ((st = abn_mul(&t, &u, &tenth)) != ABN_OK) break;
        if (abn_cmp(&err, &t) <= 0 || n >= INTEG_PARTS) break;
        /* split the worst piece in two */
        {
            abn mid, half;
            abn_from_text(&half, "0.5");
            if ((st = abn_add(&mid, &ia[worst], &ib[worst])) != ABN_OK || (st = abn_mul(&mid, &mid, &half)) != ABN_OK)
                break;
            ia[n] = mid;
            ib[n] = ib[worst];
            ib[worst] = mid;
            if ((st = gk15(c, &ia[worst], &ib[worst], &ires[worst], &ierr[worst], &iabs[worst])) != ABN_OK) break;
            if ((st = gk15(c, &ia[n], &ib[n], &ires[n], &ierr[n], &iabs[n])) != ABN_OK) break;
            n++;
        }
    }
    if (st == ABN_OK) st = abn_add(&t, &u, &err);
    if (st != ABN_OK) return st;
    *tot_out = tot;
    *unc_out = t;
    return ABN_OK;
}

static abn_status integ_finish(ab_calc *c)
{
    abn_status st;
    ab_val ox = c->susp.x, oy = c->susp.y;
    abn tot, t;
    if ((st = integ_core(c, &oy.re, &ox.re, &tot, &t)) != ABN_OK) return st;
    {
        slot sl;
        slot_of_id(c, c->susp.var, &sl);
        *sl.val = c->susp.var0;
    }
    c->x = ab_real(tot);
    c->y = ab_real(t);
    c->z = ox;
    c->t = oy;
    c->lastx = c->susp.lastx;
    c->lift = true;
    return ABN_OK;
}

static abn_status integ_op(ab_calc *c, int code)
{
    slot sl;
    abn_status st;
    if (c->active) {
        err_note(c, AB_INTEG, code, NULL);      /* SOLVE(∫FN) or ∫(∫FN) */
        return ABN_NESTED;
    }
    if (c->fn_eqn < 0 && (c->fn < 0 || c->labelpos[c->fn] < 0)) return ABN_SELECT_FN;
    if ((st = resolve(c, code, false, &sl)) != ABN_OK) return st;
    if (!sl.val) return ABN_DOMAIN;
    if (c->x.kind != AB_REAL || c->y.kind != AB_REAL) return ABN_DOMAIN;
    if (c->rdepth >= AB_RTN_DEPTH) return ABN_XEQ_OVERFLOW;
    susp_begin(c, AB_INTEG, slot_id(c, &sl), code, *sl.val);
    return fn_first(c);
}

/* The search after the first evaluation (unit 015): SOLVE's or the integral's own, its later
 * evaluations silent (no prompts: each variable is asked once per SOLVE, UG p.15-2..15-4), then
 * the results; an error restores everything. The held return level is given back. */
static abn_status fn_finish(ab_calc *c)
{
    abn_status st;
    int kind = c->susp.kind;
    c->solve_phase = 2;
    c->rbase = c->susp.rbase;
    c->pc = c->susp.pc;
    c->rdepth = c->susp.rdepth + 1;
    ev_best_set = false;
    st = kind == AB_SOLVE ? solve_finish(c) : integ_finish(c);
    if (st != ABN_OK && st != ABN_NO_ROOT) {
        bool keep = st == ABN_INTERRUPTED && kind == AB_SOLVE && ev_best_set;
        abn best = ev_best_x;
        int var = c->susp.var;
        susp_abort(c);
        if (keep) {
            /* interrupted SOLVE (UG p.7-8): the best estimate in the unknown, the stack as before */
            slot sl;
            slot_of_id(c, var, &sl);
            if (sl.val) *sl.val = ab_real(best);
        }
        return st;
    }
    c->rdepth = c->susp.rdepth;
    c->active = 0;
    c->solve_phase = 0;
    c->susp.kind = 0;
    c->ev_cached = false;
    return st;
}

/* The first evaluation's end: its value kept for the search's first call, which asks for the same
 * point; a math error is kept too (SOLVE reads it as an unusable point, the integral stops on it);
 * any other error ends the SOLVE or integral with everything restored. */
static abn_status fn_first_done(ab_calc *c, abn_status st)
{
    c->rdepth = c->susp.rdepth + 1;
    c->evals = 1;
    c->ev_cf = abn_zero();
    if (st == ABN_OK) st = fn_value(c, &c->x, &c->ev_cf);
    if (st != ABN_OK && (!math_error(st) || ev_nonreal)) {
        susp_abort(c);
        return st;
    }
    c->ev_cst = st;
    c->ev_cached = true;
    return fn_finish(c);
}

/* The first evaluation (unit 015): for the FN= program, a run that may stop at an INPUT or a
 * flag-11 prompt (each variable asked once, UG p.15-2, 15-4); R/S answers it and the SOLVE or
 * integral goes on (prog_resume). A listed equation's prompts came before (eqn_solve_key). The
 * point is the first the search will ask for: SOLVE's first guess, the integral's first node. */
static abn_status fn_first(ab_calc *c)
{
    slot sl;
    abn_status st;
    abn x0 = c->susp.var0.re;
    if (c->fn_eqn >= 0) return fn_finish(c);
    if (c->susp.kind == AB_INTEG) {
        abn a = c->susp.y.re, b = c->susp.x.re, cen, h, xi, half;
        abn_from_text(&half, "0.5");
        abn_from_text(&xi, GK_X[0]);
        if ((st = abn_add(&cen, &a, &b)) != ABN_OK || (st = abn_mul(&cen, &cen, &half)) != ABN_OK ||
            (st = abn_sub(&h, &b, &a)) != ABN_OK || (st = abn_mul(&h, &h, &half)) != ABN_OK ||
            (st = abn_mul(&h, &h, &xi)) != ABN_OK || (st = abn_add(&x0, &cen, &h)) != ABN_OK) {
            susp_abort(c);
            return st;
        }
    }
    slot_of_id(c, c->solve_var, &sl);
    if ((st = store_val(c, &sl, ab_real(x0))) != ABN_OK) {
        susp_abort(c);
        return st;
    }
    c->ev_cx = x0;
    c->x = ab_real(x0);
    c->y = c->ev_y;
    c->z = c->ev_z;
    c->t = c->ev_t;
    c->lift = true;
    c->rbase = c->rdepth;
    c->pc = c->labelpos[c->fn];
    st = prog_run(c, c->budget);
    if (st == ABN_OK && c->stop == AB_STOP_INPUT) return ABN_OK;   /* waiting at a prompt */
    return fn_first_done(c, st);
}

/* FN= label (UG p.15-1, 15-8): chooses the function; (I)/(J) name a label (p.14-22). Refused while
 * a SOLVE or integral evaluates (SOLVE ACTIVE, p.F-4). A label not (yet) keyed is accepted: SOLVE
 * then reports SELECT FN (p.F-4). */
static abn_status fn_op(ab_calc *c, int arg)
{
    int t;
    abn_status st;
    if (c->active) {
        err_note(c, AB_FN, arg, NULL);          /* SOLVE ACTIVE or ∫FN ACTIVE */
        return ABN_NESTED;
    }
    if (arg == AB_TGT_IND_I || arg == AB_TGT_IND_J) {
        if ((st = ind_target(c, arg, &t)) != ABN_OK) return st;
        arg = t / 1000;                         /* the label (I) names */
    }
    if (arg < 0 || arg >= 26) return ABN_DOMAIN;
    c->fn = arg;
    return ABN_OK;
}

/* R/S, or SST (limit 1), resuming a stopped program: at an INPUT prompt X is stored in the
 * variable first and the run goes on after the INPUT line, lift enabled (UG p.13-13, p.B-5); a
 * reply the variable cannot hold (a complex number for an unnamed variable) is an error and the
 * prompt stays. A cancelled prompt is asked again (p.13-15). */
static abn_status prog_resume(ab_calc *c, long limit)
{
    if (c->stop == AB_STOP_INPUT && c->pc >= 0 && c->peq_line == c->pc) {
        /* an equation line's flag-11 prompt: the next one, or the line runs again and evaluates */
        c->var[c->stop_var] = c->x;
        c->lift = true;
        if (++c->eqn_next < c->eqn_nvars) {
            eqn_prompt(c);
            return ABN_OK;
        }
        c->peq_ready = true;
        c->stop = AB_STOP_STOP;
    } else if (c->stop == AB_STOP_INPUT && c->pc >= 0) {
        slot sl;
        abn_status st;
        slot_of_id(c, c->stop_var, &sl);
        if ((st = store_val(c, &sl, c->x)) != ABN_OK) return st;
        c->lift = true;
        c->pc++;
        c->stop = AB_STOP_STOP;
        if (limit == 1) return ABN_OK;      /* SST stores and moves on (p.G-9) */
    }
    abn_status st = prog_run(c, limit);
    if (c->susp.kind && c->solve_phase == 1 && !(st == ABN_OK && c->stop != AB_STOP_DONE)) {
        /* a SOLVE's or integral's first evaluation ended after its prompts (unit 015): the search
           follows; keyed by a program line, the program goes on after it (a failed SOLVE skips
           the next line, p.15-6) */
        bool from_prog = c->susp.from_prog;
        int line = c->susp.pc, kind = c->susp.kind;
        st = fn_first_done(c, st);
        if (!from_prog) return st;
        if (kind == AB_SOLVE && st == ABN_NO_ROOT) {
            c->pc = line + 2;
        } else if (st != ABN_OK) {
            c->pc = line;
            c->stop = AB_STOP_ERROR;
            c->stop_line = line;
            return st;
        } else {
            c->pc = line + 1;
        }
        return prog_run(c, c->budget);
    }
    return st;
}

/* The C key outside program entry (UG p.13-15, p.B-5): a number being typed is cleared to 0; at
 * an INPUT prompt, the prompt is cancelled with the variable's value in X; at a VIEW or PSE
 * display, the display goes; otherwise it acts as clear X. */
static abn_status cancel_key(ab_calc *c)
{
    if (c->entry.active) {
        entry_cancel(c);
        return ABN_OK;
    }
    if (c->stop == AB_STOP_INPUT && c->susp.kind && c->solve_phase == 1) {
        /* a prompt of a SOLVE's or integral's first evaluation: it is abandoned, everything as
           before; keyed by a program line, R/S runs that line again (unit 015) */
        susp_abort(c);
        c->stop = AB_STOP_CANCELLED;
        return ABN_OK;
    }
    if (c->stop == AB_STOP_INPUT) {
        slot sl;
        slot_of_id(c, c->stop_var, &sl);
        c->x = load_val(&sl);
        c->stop = AB_STOP_CANCELLED;
        c->eqn_pending = 0;                     /* an equation's evaluation is abandoned */
        c->peq_line = -1;                       /* an equation line prompts again (p.13-15) */
        c->peq_ready = false;
        return ABN_OK;
    }
    if (c->stop == AB_STOP_VIEW || c->stop == AB_STOP_PSE || c->stop == AB_STOP_MSG) {
        c->stop = AB_STOP_STOP;
        return ABN_OK;
    }
    c->x = ab_real(abn_zero());
    c->lift = false;
    return ABN_OK;
}

/* the equation editor, below (unit 014) */
static void ed_backspace(ab_calc *c);
static int cp_prev(const char *s, int i);
static int cp_next(const char *s, int i);
static bool eqn_type(ab_calc *c, ab_op op, int arg);

/* A key in program entry (UG p.13-6, 13-7): keys that build a number build a number line, which
 * any other key ends; the nonprogrammable keys act (p.13-24): moving the pointer, deleting the line
 * shown (backspace, p.13-20), clearing all programs, leaving program entry, and FDISP is refused;
 * every other key is recorded as a line. */
static abn_status program_key(ab_calc *c, ab_op op, int arg)
{
    abn_status st;
    if (c->eqn_typing) {
        /* an equation line being typed (UG p.13-7): ENTER records it (C and EQN too, as in the
           list, p.6-4); the editor's keys act as in Equation mode; other keys do nothing (ours) */
        switch (op) {
        case AB_ENTER:
        case AB_CANCEL:
        case AB_EQN:
            c->eqn_typing = false; c->cas_sub = c->cas_def = c->cas_pop = 0;
            return c->ebuf[0] ? prog_insert_eqn(c, c->ebuf) : ABN_OK;
        case AB_KBKSP:
            ed_backspace(c);
            return ABN_OK;
        case AB_KLEFT:
        case AB_KRIGHT:
            c->ecur = op == AB_KLEFT ? (c->ecur > 0 ? cp_prev(c->ebuf, c->ecur) : 0) : cp_next(c->ebuf, c->ecur);
            return ABN_OK;
        default:
            eqn_type(c, op, arg);
            return ABN_OK;
        }
    }
    bool building = op <= AB_KPOINT || op == AB_KE || (op >= AB_KA && op <= AB_KF) || op == AB_KI ||
                    op == AB_KTHETA || op == AB_KCOMMA || op == AB_SUF ||
                    ((op == AB_KCHS || op == AB_KBKSP) && c->entry.active) ||
                    (op == AB_KVEC && (!c->entry.active || c->vlen));
    if (building) {
        if (op == AB_KI || op == AB_KTHETA) return complex_key(c, op);
        if (op == AB_KVEC || op == AB_KCOMMA) return vector_key(c, op);
        if (op == AB_SUF) return suffix(c, arg);
        return entry_key(c, op);
    }
    if (c->entry.active) {
        ab_val v = c->plit;
        if (c->m35 && !c->cpart && !c->vlen && no_mantissa(&c->entry)) {
            c->entry.back = c->entry.len;       /* as in do_key (unit 017 rule 3) */
            return ABN_SYNTAX;
        }
        c->entry.back = 0;
        c->entry.active = false;
        c->cpart = 0;
        c->vlen = 0;
        if ((st = prog_insert_lit(c, v)) != ABN_OK) return st;
    }
    switch (op) {
    case AB_PRGM:
        c->prgm = false;
        return ABN_OK;
    case AB_EQN:                                /* starts an equation line (UG p.13-7) */
        c->eqn_typing = true;
        c->eqn_edit = -1;
        c->ebuf[0] = '\0';
        c->ecur = 0;
        return ABN_OK;
    case AB_KVEC:
        return vector_key(c, op);
    case AB_GTOPOS:
        return prog_goto_pos(c, arg);
    case AB_SST:                                /* the list is circular (UG p.13-21) */
        c->pc = c->pc + 1 >= c->prog_len ? AB_PC_TOP : c->pc + 1;
        return ABN_OK;
    case AB_BST:
        c->pc = c->pc < 0 ? c->prog_len - 1 : c->pc - 1;
        return ABN_OK;
    case AB_KBKSP:
        if (c->pc >= 0) prog_delete(c, c->pc);
        return ABN_OK;
    case AB_CLPGM:
        prog_clear(c);
        return ABN_OK;
    case AB_MODEL:
        /* on a LBL line, the 33s/35s keys restamp that label (unit 025 rule 5); not a line */
        if (c->pc >= 0 && c->pc < c->prog_len && c->prog[c->pc].op == AB_LBL) {
            if (arg != 33 && arg != 35 && arg != AB_MODEL_STU) return ABN_DOMAIN;   /* or STU (unit 029) */
            c->lmode[c->prog[c->pc].arg] = (uint8_t)arg;
            return ABN_OK;
        }
        return ABN_NONPROGRAMMABLE;         /* elsewhere, a device menu setting (unit 017) */
    case AB_ENTRYM:                         /* STU's entry and the line's keys: not lines (unit 029) */
    case AB_ANS:
    case AB_HUP:
    case AB_HDOWN:
    case AB_TABLE: case AB_TVAR: case AB_TSTART: case AB_TSTEP: case AB_TUP: case AB_TDOWN:   /* 032 */
    case AB_GRAPH: case AB_XMIN: case AB_XMAX: case AB_YMIN: case AB_YMAX: case AB_GAUTO: case AB_GLEFT:
    case AB_GRIGHT:                                                                      /* 033 */
    case AB_GZOOM: case AB_GMOVE: case AB_GTRACE: case AB_GMARK: case AB_GFACT: case AB_GFCN:    /* 040 */
    case AB_CASD: case AB_CASSIMP: case AB_CASEXP: case AB_CASCOL: case AB_CASSUB:              /* CAS 004 */
    case AB_CASSOLVE: case AB_CASINT: case AB_CASFACT: case AB_CASDEF:                          /* 048, 056 */
    case AB_CASQUOT: case AB_CASGCD: case AB_CASCANCL:                                          /* 057 */
    case AB_CASSUM:                                                                             /* 070 */
    case AB_CASCPLX:                        /* 076: a setting, as ENTRYM */
        return ABN_NONPROGRAMMABLE;
    case AB_FDISP:
    case AB_CLALL:                          /* in program entry the menu offers CLPGM in its place
                                               (UG p.1-5) */
    case AB_FRACFMT:                        /* a device menu setting, as FDISP (unit 019) */
    case AB_CLEQN:                          /* as CLPGM, a clearing that is not a line */
        return ABN_NONPROGRAMMABLE;
    case AB_CLVARX:
        if (arg < 0 || arg > AB_IND_MAX) return ABN_DOMAIN;
        return prog_insert(c, op, arg);
    case AB_LIT:
    case AB_EQLINE:                         /* internal: lines, never keys (unit 018 found it) */
        return ABN_DOMAIN;
    case AB_LBL:
        if (arg < 0 || arg >= 26) return ABN_DOMAIN;
        if (c->labelpos[arg] >= 0) return ABN_DUP_LABEL;
        {
            abn_status st2 = prog_insert(c, AB_LBL, arg);
            if (st2 == ABN_OK)                  /* stamped with the mode it was entered in (unit 025
                                                   rule 3); STU's run as 35s (unit 029 rule 9) */
                c->lmode[arg] = c->stu ? AB_MODEL_STU : c->m35 ? 35 : 33;
            return st2;
        }
    case AB_XEQ:
    case AB_GTO:
        if (arg == AB_TGT_IND_I || arg == AB_TGT_IND_J) return prog_insert(c, op, arg);
        if (arg < 0 || arg / 1000 >= 26) return ABN_DOMAIN;
        if (arg % 1000 == 0) arg += 1;      /* XEQ A is XEQ A001 (UG p.13-10) */
        return prog_insert(c, op, arg);
    case AB_CANCEL:                         /* C leaves program entry (UG p.13-7) */
        c->prgm = false;
        return ABN_OK;
    case AB_KEQ: case AB_KPAREN: case AB_KLEFT: case AB_KRIGHT: case AB_KSPACE: case AB_KREG: case AB_EQTOP:
    case AB_KINF:                           /* (070) */
        return ABN_OK;                      /* Equation mode's keys: no line, as they do nothing
                                               outside it (ours, unit 018; an older image's such
                                               line still loads) */
    default:
        return prog_insert(c, op, arg);
    }
}

/* ---- equations (unit 014; UG ch.6) ---- */

/* 2 or 3 when entry i is that built-in linear solver, else 0. */
static int eqn_builtin(const ab_calc *c, int i)
{
    if (i < 0 || c->eqn[i][0] != '\x01') return 0;
    return c->eqn[i][1] == '2' ? 2 : 3;
}

/* Inserts text as a new equation after the one shown and shows it (UG p.6-4); never between the
 * two built-ins: after them (p.6-8). */
static abn_status eqn_insert(ab_calc *c, const char *text)
{
    int at = c->eqn_cur + 1;
    char t[AB_EQN_LEN];
    if (c->eqn_count - 2 >= AB_EQN_MAX) return ABN_MEMORY_FULL;
    abn_status st = stored_text(c, text, t, typed_model(c));
    if (st != ABN_OK) return st;
    if (eqn_builtin(c, at - 1) == 2) at++;
    memmove(c->eqn[at + 1], c->eqn[at], (size_t)(c->eqn_count - at) * AB_EQN_LEN);
    memmove(&c->eqn_model[at + 1], &c->eqn_model[at], (size_t)(c->eqn_count - at));
    memmove(&c->eqn_note[at + 1], &c->eqn_note[at], (size_t)(c->eqn_count - at));     /* unit 057: labels move */
    strcpy(c->eqn[at], t);
    c->eqn_model[at] = typed_model(c);
    c->eqn_note[at] = 0;
    c->eqn_count++;
    c->eqn_cur = at;
    return ABN_OK;
}

/* Deletes equation i; the one before it shows (UG p.6-9). */
static void eqn_delete(ab_calc *c, int i)
{
    memmove(c->eqn[i], c->eqn[i + 1], (size_t)(c->eqn_count - i - 1) * AB_EQN_LEN);
    memmove(&c->eqn_model[i], &c->eqn_model[i + 1], (size_t)(c->eqn_count - i - 1));
    memmove(&c->eqn_note[i], &c->eqn_note[i + 1], (size_t)(c->eqn_count - i - 1));    /* unit 057 */
    c->eqn_count--;
    c->eqn_cur = i - 1;
    while (c->eqn_cur >= 0 && eqn_builtin(c, c->eqn_cur) && !ab_mode35(c)) c->eqn_cur--;   /* 33s: past them (041 A4) */
}

/* Unit 070: the equation shown is a sum's condition (labeled WHERE), casim's text, not an equation: an op on it says
   A CONDITION (abacus #8545), never SYNTAX; editing it is allowed, and the edited text loses the label (057) */
static bool eqn_where(const ab_calc *c)
{
    return c->eqn_cur >= 0 && c->eqn_note[c->eqn_cur] == AB_NOTE_WHERE;
}

/* ENTER or C while typing stores the text: a new equation after the one shown, or the edited one
 * replaced; an empty text deletes the entry (UG p.6-4, 6-8). */
static abn_status cas_subst_done(ab_calc *c, bool run);
static abn_status cas_def_bound(ab_calc *c, bool run);
static abn_status cas_poly_done(ab_calc *c, bool run);

static abn_status eqn_store_typed(ab_calc *c)
{
    char t[AB_EQN_LEN];
    if (!c->eqn_typing) return ABN_OK;
    if (c->cas_sub) return cas_subst_done(c, true);     /* SUBST's replacement: substituted, not stored (CAS 004) */
    if (c->cas_def) return cas_def_bound(c, true);      /* ∫AB's bound (056): held, or the integral run */
    if (c->cas_pop) return cas_poly_done(c, true);      /* QUOT's or GCD's g (057) */
    int model = c->eqn_edit >= 0 && c->eqn_model[c->eqn_edit] ? c->eqn_model[c->eqn_edit] : typed_model(c);
    bool same = c->eqn_edit >= 0 && strcmp(c->ebuf, c->eqn[c->eqn_edit]) == 0;
    abn_status sst = c->ebuf[0] && !same ? stored_text(c, c->ebuf, t, model) : ABN_OK;  /* (an edit unchanged
                                                   stores as it was: an older image's over-deep equation can be looked
                                                   at and left, unit 047 fresh eyes) */
    if (sst != ABN_OK)
        return sst == ABN_TOO_DEEP ? sst : ABN_MEMORY_FULL;   /* STU's x's would not fit, or nested past the bound
                                                   (unit 047): refused, the text unchanged and still typed (unit 031
                                                   answer I) */
    c->eqn_typing = false; c->cas_sub = c->cas_def = c->cas_pop = 0;
    int edit = c->eqn_edit;
    c->eqn_edit = -1;                           /* meaningful only while typing: left set, a delete could leave it past
                                                   the list's end, and the image would not load (unit 041 A, fresh eyes) */
    if (c->ebuf[0] == '\0') {
        if (edit >= 0) eqn_delete(c, edit);
        return ABN_OK;
    }
    if (edit >= 0) {
        if (strcmp(c->ebuf, c->eqn[edit]) != 0) {   /* an edit unchanged keeps its text and stamp (review F4); a
                                                       changed one keeps it too since 041 D (proposed, fresh eyes: a
                                                       33s Cn,r(5:2) edited in 35s mode would no longer parse), as
                                                       its typed names did (typed_model) */
            strcpy(c->eqn[edit], t);
            if (!c->eqn_model[edit]) c->eqn_model[edit] = typed_model(c);
            c->eqn_note[edit] = 0;              /* unit 057: an edited text loses its label (an unchanged one keeps
                                                   it, as it keeps its stamp) */
        }
        c->eqn_cur = edit;
        return ABN_OK;
    }
    return eqn_insert(c, c->ebuf);              /* (the list full: MEMORY_FULL, the text gone, as before) */
}

abn_status ab_eqn_add(ab_calc *c, const char *text)
{
    if (c->prgm) return prog_insert_eqn(c, text);   /* in program entry: a line (p.13-7) */
    if (c->alg) return ABN_RPN_ONLY;                /* Equation mode is RPN's (unit 029 proposal 5) */
    abn_status st = eqn_insert(c, text);
    if (st == ABN_OK) {
        c->eqn_mode = true;
        c->eqn_typing = false; c->cas_sub = c->cas_def = c->cas_pop = 0;
    }
    return st;
}

/* The editor: text inserted at the cursor, a code point at a time for moves. */
static void ed_insert(ab_calc *c, const char *t, int cursor_back)
{
    int n = (int)strlen(t), len = (int)strlen(c->ebuf);
    if (len + n >= AB_EQN_LEN) return;              /* full: the key is ignored (ours) */
    memmove(c->ebuf + c->ecur + n, c->ebuf + c->ecur, (size_t)(len - c->ecur + 1));
    memcpy(c->ebuf + c->ecur, t, (size_t)n);
    c->ecur += n - cursor_back;
}

static void ed_remove(ab_calc *c, int at, int n)
{
    int len = (int)strlen(c->ebuf);
    memmove(c->ebuf + at, c->ebuf + at + n, (size_t)(len - at - n + 1));
    if (c->ecur > at) c->ecur = c->ecur >= at + n ? c->ecur - n : at;
}

static int cp_prev(const char *s, int i)
{
    do i--;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80);
    return i;
}

static int cp_next(const char *s, int i)
{
    if (!s[i]) return i;
    do i++;
    while (s[i] && ((unsigned char)s[i] & 0xC0) == 0x80);
    return i;
}

static bool is_digit_ch(char ch)
{
    return ch >= '0' && ch <= '9';
}

/* The matching ")" of the "(" at byte i, or -1. */
static int match_close(const char *s, int i)
{
    int depth = 0;
    for (int k = i; s[k]; k++) {
        if (s[k] == '(') depth++;
        else if (s[k] == ')' && --depth == 0) return k;
    }
    return -1;
}

/* Backspace removes the element left of the cursor (UG p.6-3, 6-9): a character, or a function's
 * name with its "(" (or a bare "("), together with its ")" (ours: a pair goes as one). */
static void ed_backspace(ab_calc *c)
{
    int i = c->ecur;
    if (i == 0) return;
    if (c->ebuf[i - 1] == '(') {
        int nl = eqn_name_before(c->ebuf, i), close = match_close(c->ebuf, i - 1);
        int start = i - (nl ? nl : 1);
        if (close >= 0) ed_remove(c, close, 1);
        ed_remove(c, start, i - start);
        return;
    }
    ed_remove(c, cp_prev(c->ebuf, i), i - cp_prev(c->ebuf, i));
}

/* CHS (UG p.6-5): while a number is typed, its sign (after E, the exponent's); else a minus. A sign
 * is toggled: a minus directly before the number, where a minus cannot be a subtraction, goes. */
static void ed_chs(ab_calc *c)
{
    char *b = c->ebuf;
    int i = c->ecur, j = i;
    while (j > 0 && (is_digit_ch(b[j - 1]) || b[j - 1] == '.')) j--;
    if (j == i && i > 0 && b[i - 1] == 'E' && i >= 2 && (is_digit_ch(b[i - 2]) || b[i - 2] == '.')) {
        ed_insert(c, "-", 0);                       /* E typed, no exponent digit yet */
        return;
    }
    if (j < i) {
        if (j >= 2 && b[j - 1] == '-' && b[j - 2] == 'E') {
            ed_remove(c, j - 1, 1);
            return;
        }
        /* a minus before the digits where it is a sign is taken away; else one goes in before them
           (after E that is the exponent's sign) */
        if (j >= 1 && b[j - 1] == '-') {
            int k = j - 1;
            bool sign = k == 0 || strchr("(,=^+-", b[k - 1]) || ((unsigned char)b[k - 1] == 0x97 || (unsigned char)b[k - 1] == 0xb7);
            if (sign) {
                ed_remove(c, k, 1);
                return;
            }
        }
        int save = c->ecur;
        c->ecur = j;
        ed_insert(c, "-", 0);
        c->ecur = save + 1;
        return;
    }
    ed_insert(c, "-", 0);
}

/* The text a key types (UG p.6-4 to 6-6, 6-16): infix operators, the function names of p.6-16 with
 * a pair (the cursor inside), the names that take no argument, and the operands. */
static const char *eqn_fname(ab_op op)
{
    switch (op) {
    case AB_LN: return "LN"; case AB_LOG: return "LOG"; case AB_EXP: return "EXP"; case AB_TENX: return "ALOG";
    case AB_SQ: return "SQ"; case AB_SQRT: return "SQRT"; case AB_INV: return "INV"; case AB_IP: return "IP";
    case AB_FP: return "FP"; case AB_RND: return "RND"; case AB_ABS: return "ABS"; case AB_SGN: return "SGN";
    case AB_INTG: return "INTG"; case AB_IDIV: return "IDIV"; case AB_RMDR: return "RMDR";
    case AB_SIN: return "SIN"; case AB_COS: return "COS"; case AB_TAN: return "TAN"; case AB_ASIN: return "ASIN";
    case AB_ACOS: return "ACOS"; case AB_ATAN: return "ATAN"; case AB_SINH: return "SINH"; case AB_COSH: return "COSH";
    case AB_TANH: return "TANH"; case AB_ASINH: return "ASINH"; case AB_ACOSH: return "ACOSH"; case AB_ATANH: return "ATANH";
    case AB_TODEG: return "\xe2\x86\x92" "DEG"; case AB_TORAD: return "\xe2\x86\x92" "RAD";
    case AB_HMSINV: return "HMS\xe2\x86\x92"; case AB_HMS: return "\xe2\x86\x92" "HMS"; case AB_PCTCHG: return "%CHG";
    case AB_XROOT: return "XROOT"; case AB_TOL: return "\xe2\x86\x92L"; case AB_TOGAL: return "\xe2\x86\x92GAL";
    case AB_TOMILE: return "\xe2\x86\x92MILE"; case AB_TOKM: return "\xe2\x86\x92KM"; case AB_NCR: return "nCr";
    case AB_NPR: return "nPr"; case AB_TOKG: return "\xe2\x86\x92KG"; case AB_TOLB: return "\xe2\x86\x92LB";
    case AB_TOC: return "\xe2\x86\x92\xc2\xb0" "C"; case AB_TOF: return "\xe2\x86\x92\xc2\xb0" "F";
    case AB_TOCM: return "\xe2\x86\x92" "CM"; case AB_TOIN: return "\xe2\x86\x92IN"; case AB_SEED: return "SEED";
    case AB_ARG: return "ARG"; case AB_XHAT: return "x\xcc\x82"; case AB_YHAT: return "\xc5\xb7";
    case AB_AND: return "AND"; case AB_XOR: return "XOR"; case AB_OR: return "OR"; case AB_NOT: return "NOT";
    case AB_NAND: return "NAND"; case AB_NOR: return "NOR"; case AB_PCT: return "%";
    default: return NULL;
    }
}

/* Unit 041 D: in 33s typing, the 33s's spellings (33s p.6-15, 6-16): a function with its pair, then a token. */
static const char *eqn_fname33(ab_op op)
{
    switch (op) {
    case AB_NCR: return "Cn,r"; case AB_NPR: return "Pn,r"; case AB_HMSINV: return "\xe2\x86\x92HR";
    case AB_CUBE: return "CB"; case AB_CBRT: return "CBRT";
    default: return NULL;
    }
}

static const char *eqn_token33(ab_op op)
{
    return op == AB_RAND ? "RANDOM" : op == AB_KCOMMA ? ":" : NULL;     /* the colon separates (p.6-5) */
}

static const char *eqn_token(ab_op op)
{
    switch (op) {
    case AB_ADD: return "+"; case AB_SUB: return "-"; case AB_MUL: return "\xc3\x97"; case AB_DIV: return "\xc3\xb7";
    case AB_POW: return "^"; case AB_KEQ: return "="; case AB_KCOMMA: return ","; case AB_FACT: return "!";
    case AB_PI: return "\xcf\x80"; case AB_RAND: return "RAND"; case AB_SX: return "sx"; case AB_SY: return "sy";
    case AB_PSX: return "\xcf\x83x"; case AB_PSY: return "\xcf\x83y"; case AB_XBAR: return "x\xcc\x84";
    case AB_YBAR: return "\xc8\xb3"; case AB_XWBAR: return "x\xcc\x84w"; case AB_CORR: return "r";
    case AB_SLOPE: return "m"; case AB_YINT: return "b"; case AB_NSUM: return "n";
    case AB_SUMX: return "\xce\xa3x"; case AB_SUMY: return "\xce\xa3y"; case AB_SUMX2: return "\xce\xa3x\xc2\xb2";
    case AB_SUMY2: return "\xce\xa3y\xc2\xb2"; case AB_SUMXY: return "\xce\xa3xy";
    case AB_KSPACE: return " ";
    default: return NULL;
    }
}

static abn_status eqn_solve_key(ab_calc *c, ab_op op, int code);
static abn_status eqn_solve_finish(ab_calc *c);
static abn_status lin_key(ab_calc *c, int n);

/* Prompts for the next variable (an INPUT stop: its value in X, lift off; UG p.6-14), or computes
 * the value: to X, lift enabled, LAST X kept (not in p.B-6's list); an assignment's ENTER also
 * stores it (p.6-10). An error changes nothing more. */
static abn_status eqn_step(ab_calc *c)
{
    const char *t = c->eqn[c->eqn_cur];
    int av = eqn_assign_var(t, c->eqn_model[c->eqn_cur] == 33), pos;
    bool rhs = c->eqn_pending == 1 && av >= 0;
    ab_val v;
    abn_status st;
    if (c->eqn_next < c->eqn_nvars) {
        int var = c->eqn_vars[c->eqn_next];
        place_val(c, c->var[var]);
        c->lift = false;
        c->stop = AB_STOP_INPUT;
        c->stop_var = var;
        return ABN_OK;
    }
    if (c->eqn_pending >= 3) return eqn_solve_finish(c);
    c->eqn_pending = 0;
    st = eqn_eval(c, t, c->eqn_model[c->eqn_cur], rhs, false, &v, &pos);
    if (st != ABN_OK) {
        c->eqn_errpos = pos;
        c->stop = AB_STOP_NONE;
        return st;
    }
    if (rhs) c->var[av] = v;
    place_val(c, v);
    c->lift = true;
    c->stop = AB_STOP_DONE;
    return ABN_OK;
}

/* SOLVE or integrate on the equation shown, from the keyboard (UG p.7-2, 8-3): its variables are
 * prompted for first, the unknown (or the variable of integration) apart, in order of appearance;
 * the guesses and limits are the stack as it was when the key was pressed (p.7-11: guesses 10 and
 * 20 find 15 after the V? prompt), and the prompts' pushes give way to the results. SOLVE for a
 * variable the equation does not have is INVALID VAR (ours, p.F-2). On a built-in linear solver,
 * SOLVE prompts for its coefficients, A..F or A..L, and takes no letter; integrate does nothing
 * (p.7-6). Equation mode is left. */
static abn_status eqn_solve_key(ab_calc *c, ab_op op, int code)
{
    const char *t;
    int b, pos, v[27], n;
    ab_val val;
    abn_status st;
    slot sl;
    if (c->eqn_cur < 0) return ABN_OK;             /* EQN LIST TOP: nothing (ours) */
    if (eqn_where(c)) return ABN_CAS_WHERE;        /* a sum's condition, before any prompt (070) */
    if (c->active) return ABN_NESTED;              /* a SOLVE or integral of a program waits (its prompt
                                                      kept): refused before anything is touched (035 review) */
    b = eqn_builtin(c, c->eqn_cur);
    if (b) {
        if (op != AB_SOLVE) return ABN_OK;
        c->eqn_nvars = b * (b + 1);
        for (int i = 0; i < c->eqn_nvars; i++) c->eqn_vars[i] = i;
    } else {
        t = c->eqn[c->eqn_cur];
        (void)val;
        (void)pos;
        /* no syntax check here: SOLVE and integrate meet it at their first evaluation, after the
           prompts (33s p.6-18; unit 024 rule 4) */
        if ((st = resolve(c, code, false, &sl)) != ABN_OK) return st;
        if (!sl.val || sl.val < c->var || sl.val >= c->var + 27) return ABN_DOMAIN;  /* p.14-23; i too (unit 025) */
        n = eqn_vars(t, false, c->eqn_model[c->eqn_cur] == 33, c->eqn_model[c->eqn_cur], v);
        if (op == AB_SOLVE) {
            bool found = false;
            for (int i = 0; i < n; i++) found = found || v[i] == sl.val - c->var;
            if (!found) return ABN_INVALID_VAR;
        }
    }
    c->susp.kind = b ? AB_SOLVE : op;
    c->susp.var = b ? -1 : (int)(sl.val - c->var);
    c->susp.code = code;
    c->susp.x = c->x;
    c->susp.y = c->y;
    c->susp.z = c->z;
    c->susp.t = c->t;
    c->susp.lastx = c->lastx;
    c->susp.lift = c->lift;
    if (!b) c->eqn_nvars = eqn_prompt_vars(c, c->eqn[c->eqn_cur], c->eqn_model[c->eqn_cur], false, c->eqn_vars);
    c->eqn_next = 0;
    c->eqn_pending = b ? 3 + b : op == AB_SOLVE ? 3 : 4;
    c->eqn_mode = false;
    return eqn_step(c);
}

/* After the prompts: the stack as when the key was pressed, then the SOLVE, the integral, or the
 * linear system (unit 015). */
static abn_status eqn_solve_finish(ab_calc *c)
{
    int pend = c->eqn_pending, code = c->susp.code, var = c->susp.var;
    abn_status st;
    /* the unknown is the variable (I) or (i) named when the key was pressed: a prompt's answer for
       I or i does not move it (the guesses and limits are kept the same way, p.7-11; unit 025) */
    if (var >= 0) code = var == 26 ? AB_VAR_SI : AB_VAR_A + var;
    c->eqn_pending = 0;
    c->stop = AB_STOP_NONE;                     /* the last prompt answered: none left (unit 035 rule 1) */
    c->x = c->susp.x;
    c->y = c->susp.y;
    c->z = c->susp.z;
    c->t = c->susp.t;
    c->lastx = c->susp.lastx;
    c->lift = c->susp.lift;
    c->susp.kind = 0;
    if (pend >= 5) {                            /* x= shown, down to y= (z=): 35s p.7-6, 7-7 */
        st = lin_key(c, pend - 3);
        if (st == ABN_OK) {
            c->viewing = true;
            c->view_id = pend - 3 == 2 ? AB_VIEW_LIN2 : AB_VIEW_LIN3;
        }
        return st;
    }
    c->fn_eqn = c->eqn_cur;
    st = pend == 3 ? solve_op(c, code) : integ_op(c, code);
    c->fn_eqn = -1;
    if (st == ABN_OK && pend == 3) {            /* the root VIEWed in its variable (33s, 35s p.7-2) */
        slot sl;
        if (resolve(c, code, false, &sl) == ABN_OK) {
            c->viewing = true;
            c->view_id = slot_id(c, &sl);
        }
    } else if (st == ABN_OK) {                  /* the integral as "∫ =" its value (35s p.8-4) */
        c->viewing = true;
        c->view_id = AB_VIEW_INTEG;
    }
    return st;
}

/* The built-in linear solvers (UG p.7-6, 7-7): the solution in the variables X, Y (Z), and in the
 * X, Y (Z) registers, the rest of the stack as before, lift enabled, LAST X kept (ours: the guide
 * shows only X= and y=); NO SOLUTION / MULT SOLUTION change nothing (p.F-3). */
static abn_status lin_key(ab_calc *c, int n)
{
    abn coef[12], sol[3];
    abn_status st;
    for (int i = 0; i < n * (n + 1); i++) {
        if (c->var[i].kind != AB_REAL) return ABN_DOMAIN;
        coef[i] = c->var[i].re;
    }
    if ((st = lin_solve(n, coef, sol)) == ABN_OVERFLOW) note_overflow(c);  /* written (unit 026) */
    else if (st != ABN_OK) return st;
    c->var['X' - 'A'] = c->x = ab_real(sol[0]);
    c->var['Y' - 'A'] = c->y = ab_real(sol[1]);
    if (n == 3) c->var['Z' - 'A'] = c->z = ab_real(sol[2]);
    c->lift = true;
    return ABN_OK;
}

/* ENTER or XEQ on the equation shown (UG p.6-10 to 6-13): the syntax is checked first (SYNTAX
 * ERROR, p.6-19), then the variables are prompted for in order of appearance (ENTER on an
 * assignment: the right side's only), then the value is computed. Equation mode is left: the result
 * shows and the keys act on it (ours, from p.6-12's keys after the result). */
static abn_status eqn_evaluate(ab_calc *c, bool enter)
{
    const char *t;
    int av, pos;
    bool rhs;
    ab_val v;
    abn_status st;
    if (c->eqn_cur < 0) return ABN_DOMAIN;         /* EQN LIST TOP: nothing to evaluate (ours) */
    if (eqn_builtin(c, c->eqn_cur)) return ABN_OK; /* a built-in: ENTER, XEQ do nothing (p.7-6) */
    if (eqn_where(c)) return ABN_CAS_WHERE;        /* a sum's condition, not shown as a message (070) */
    t = c->eqn[c->eqn_cur];
    av = eqn_assign_var(t, c->eqn_model[c->eqn_cur] == 33);
    rhs = enter && av >= 0;
    (void)v;
    (void)pos;
    (void)st;
    /* the syntax is checked when the value is calculated, after every prompt is answered (33s
       p.6-18, 32SII p.6-20; unit 024 rule 4): eqn_step's evaluation reports it */
    c->eqn_nvars = eqn_vars(t, rhs, c->eqn_model[c->eqn_cur] == 33, c->eqn_model[c->eqn_cur], c->eqn_vars);
    c->eqn_next = 0;
    c->eqn_pending = enter ? 1 : 2;
    c->eqn_mode = false;
    return eqn_step(c);
}

/* A key that types into the equation being edited, starting a new one if none is (UG p.6-4 to 6-6,
 * 6-16; in program entry the text becomes a line, p.13-7). False when the key does not type. */
/* Whether a key types into the editor (eqn_type would take it). */
static bool eqn_typable(ab_op op, int arg)
{
    return op <= AB_KE || op == AB_KCHS || op == AB_KPAREN || (op == AB_RCL && arg >= 0 && arg <= AB_VAR_SI) ||
           (op == AB_KREG && arg >= 0 && arg < 4) || (op >= AB_KA && op <= AB_KF) || op == AB_KI ||
           op == AB_KTHETA || op == AB_KVEC || (op == AB_SUF && (arg == 2 || arg == 8 || arg == 10 || arg == 16)) ||
           eqn_fname(op) || eqn_token(op) || op == AB_CUBE || op == AB_CBRT;  /* (041 D: the 33s's CB, CBRT) */
}

static bool eqn_type(ab_calc *c, ab_op op, int arg)
{
    const char *t;
    if (!eqn_typable(op, arg)) return false;
    if (op == AB_PCT && typed_model(c) == 33) return true;     /* a 33s equation has no plain % (041 D, abacus #4883) */
    if (!c->eqn_typing) {
        c->eqn_typing = true;
        c->eqn_edit = -1;
        c->ebuf[0] = '\0';
        c->ecur = 0;
    }
    if (op <= AB_K9) {
        char d[2] = {(char)('0' + (op - AB_K0)), '\0'};
        ed_insert(c, d, 0);
    } else if (op == AB_KPOINT) {
        ed_insert(c, c->ecur > 0 && is_digit_ch(c->ebuf[c->ecur - 1]) ? "." : "0.", 0);   /* .25 shows 0.25 */
    } else if (op == AB_KE) {
        ed_insert(c, "E", 0);
    } else if (op == AB_KCHS) {
        ed_chs(c);
    } else if (op == AB_KPAREN) {
        ed_insert(c, "()", 1);
    } else if (op == AB_RCL && arg == AB_VAR_SI) {
        ed_insert(c, "i", 0);                   /* the variable i (33s p.13-23; unit 025) */
    } else if (op == AB_RCL && arg >= AB_VAR_IND_I) {
        ed_insert(c, arg == AB_VAR_IND_I ? "\xe2\x92\xa4" : "\xe2\x92\xa5", 0);  /* (I), (J): p.14-23 */
    } else if (op == AB_RCL) {
        char v[2] = {(char)('A' + arg), '\0'};
        ed_insert(c, v, 0);
    } else if (op == AB_KREG) {
        static const char *const REG[4] = {"REGX", "REGY", "REGZ", "REGT"};
        ed_insert(c, REG[arg], 0);
    } else if (op >= AB_KA && op <= AB_KF) {
        char d[2] = {(char)('A' + (op - AB_KA)), '\0'};     /* hex digits (UG p.11-2) */
        ed_insert(c, d, 0);
    } else if (op == AB_KI) {
        ed_insert(c, "i", 0);                       /* x i y (UG p.9-7) */
    } else if (op == AB_KTHETA) {
        ed_insert(c, "\xce\xb8", 0);               /* r theta a */
    } else if (op == AB_KVEC) {
        ed_insert(c, "[]", 1);                      /* a vector's brackets, the cursor inside (p.10-8) */
    } else if (op == AB_SUF) {
        ed_insert(c, arg == 2 ? "b" : arg == 8 ? "o" : arg == 10 ? "d" : "h", 0);   /* p.11-3 */
    } else if ((t = typed_model(c) == 33 ? eqn_fname33(op) : NULL) != NULL || (t = eqn_fname(op)) != NULL) {
        char buf[24];
        strcpy(buf, t);
        strcat(buf, "()");
        ed_insert(c, buf, 1);
    } else if ((t = typed_model(c) == 33 ? eqn_token33(op) : NULL) != NULL) {
        ed_insert(c, t, 0);
    } else if ((t = eqn_token(op)) != NULL) {
        ed_insert(c, t, 0);
    }
    return true;
}

/* A key in Equation mode (UG p.6-3 to 6-9). */
/* CAS 004 (casim's work/004 rules 4-5): Casimir's step op (a casim_op) on the equation shown, read as its stamp:
   the result is a new equation after it, stamped STU (the list's mode), and shown; the original stays. A refusal
   changes nothing: its status is the message (#4797). STU only; not at EQN LIST TOP nor on a built-in. */
#ifdef AB_CASIM
/* Unit 041 D (fresh eyes): a 33s equation in casim's spelling, which is the 35s's: the colon a comma, Cn,r nCr, Pn,r
   nPr, →HR HMS→, RANDOM RAND. CB and CBRT casim doesn't have: 1, with the name in bad (UNSUPPORTED CB). 2: too long. */
static int cas_text33(const char *in, char *out, size_t cap, char *bad, size_t badcap)
{
    static const char *const FROM[] = {"Cn,r", "Pn,r", "\xe2\x86\x92HR", "RANDOM", "CBRT", "CB", ":"};
    static const char *const TO[] = {"nCr", "nPr", "HMS\xe2\x86\x92", "RAND", NULL, NULL, ","};
    size_t o = 0;
    while (*in) {
        size_t k = 0, n = 0;
        for (; k < sizeof FROM / sizeof FROM[0]; k++)
            if (!strncmp(in, FROM[k], n = strlen(FROM[k])) && (FROM[k][0] == ':' || FROM[k][0] == 'R' || in[n] == '('))
                break;
        if (k < sizeof FROM / sizeof FROM[0]) {
            if (!TO[k]) {
                snprintf(bad, badcap, "%s", FROM[k]);
                return 1;
            }
            size_t m = strlen(TO[k]);
            if (o + m >= cap) return 2;
            memcpy(out + o, TO[k], m);
            o += m;
            in += n;
        } else {
            if (o + 1 >= cap) return 2;
            out[o++] = *in++;
        }
    }
    out[o] = '\0';
    return 0;
}

/* Unit 057: an inserted CAS answer, stamped STU as the list's; but one that only the 33s reads (the variable i, from a
   33s equation: STU refuses a lone i) keeps 33, so it still means what it meant (fresh eyes F3) */
static void cas_stamp(ab_calc *c, const char *text)
{
    ab_val v;
    int pos;
    if (eqn_eval(c, text, AB_MODEL_STU, false, true, &v, &pos) == ABN_SYNTAX &&
        eqn_eval(c, text, 33, false, true, &v, &pos) == ABN_OK)
        c->eqn_model[c->eqn_cur] = 33;
}

/* Unit 057 (#5670 rule 5): CAS 010's rows ("q; r", "quotient; X=a; ..."), each inserted as an equation after the one
   shown, in order, with its label; all or none: the free places are counted first, and an insert that fails takes
   back those before it. The first row is shown. Unit 070: CAS 018's sum the same ("answer; X≠1; ..."), each condition
   labeled WHERE (casim's text kept: the list stores a text it can't parse as typed). */
static abn_status cas_rows(ab_calc *c, int op, const char *out)
{
    char rows[14][AB_EQN_LEN];
    int n = 0, at0 = c->eqn_cur, first = -1;
    for (const char *s = out; *s && n < 14; n++) {
        const char *e = strstr(s, "; ");
        size_t len = e ? (size_t)(e - s) : strlen(s);
        if (len >= AB_EQN_LEN) return ABN_CAS_TOO_LONG;
        memcpy(rows[n], s, len);
        rows[n][len] = '\0';
        s = e ? e + 2 : s + len;
    }
    if (c->eqn_count - 2 + n > AB_EQN_MAX) return ABN_MEMORY_FULL;
    for (int i = 0; i < n; i++) {
        abn_status st = eqn_insert(c, rows[i]);
        if (st != ABN_OK) {
            for (int k = i - 1; k >= 0; k--) eqn_delete(c, first + k);
            c->eqn_cur = at0;
            return st;
        }
        if (i == 0) first = c->eqn_cur;
        c->eqn_note[c->eqn_cur] = op == CASIM_PDIV ? (i == 0 ? AB_NOTE_QUOT : AB_NOTE_REM) : i == 0 ? 0
                                  : op == CASIM_SUM ? AB_NOTE_WHERE : AB_NOTE_NOTAT;
        cas_stamp(c, rows[i]);
    }
    c->eqn_cur = first;
    return ABN_OK;
}

/* Unit 056: a letter A-Z in a bound's text, other than inside a function's name (SQRT, LN, π is not a letter): the
   text only picks BOUND NOT NUMERIC's message for casim's BAD_ARG (#5630 rule 3). Unit 057's helpers and this one are
   under AB_CASIM with their callers, so casim's own build of the core (without it) compiles them neither in nor unused
   (abacus #5873) */
static bool cas_has_letter(const char *s)
{
    int v[27];
    return s && eqn_vars(s, false, false, AB_MODEL_STU, v) > 0;
}

#endif

static abn_status cas_step(ab_calc *c, int op, int var, const char *repl, const char *repl2)
{
#ifdef AB_CASIM
    char out[AB_EQN_LEN], t33[AB_EQN_LEN];
    casim_status cs;
    if (!c->stu) return ABN_STU_ONLY;
    if (c->eqn_cur < 0 || eqn_builtin(c, c->eqn_cur)) return ABN_NO_EQN;
    const char *text = c->eqn[c->eqn_cur];
    if (eqn_too_deep(c, text, c->eqn_model[c->eqn_cur]))
        return ABN_TOO_DEEP;                    /* an older image's, quarantined: casim never walks it (unit 047) */
    if (c->eqn_model[c->eqn_cur] == 33) {       /* 041 D: what the 33s refuses is refused; its spelling translated */
        ab_val v;
        int pos;
        if (eqn_eval(c, text, 33, false, true, &v, &pos) == ABN_SYNTAX) return ABN_SYNTAX;
        int r = cas_text33(text, t33, sizeof t33, c->cas_bad, sizeof c->cas_bad);
        if (r == 1) return ABN_CAS_UNSUPPORTED;
        if (r == 2) return ABN_CAS_TOO_LONG;
        text = t33;
    }
    casim_req rq = {(casim_op)op, text, c->eqn_model[c->eqn_cur], var, repl, AB_MODEL_STU,
                    (int)c->angle | (c->cas_cplx ? CASIM_COMPLEX : 0), repl2};  /* repl2: ∫AB's upper bound (056); 076:
                                                   CPLX's bit on every op, as CAS 020 rule 2 allows */
    cs = casim_run(&rq, out, sizeof out, ab_cas_arena, sizeof ab_cas_arena);
    if (cs == CASIM_DECIMAL_FORM) cs = CASIM_OK;    /* CAS 019 rule 5: fractions past the cap or the arena, so casim's
                                                   answer in 4f24e3e's decimal form: an answer as before (its label waits
                                                   on Michael's word, abacus #8809) */
    bool ext = op == CASIM_SOLVE || op == CASIM_INTEGRATE || op == CASIM_FACTOR;     /* unit 048's keys */
    if (ext) {
        if (c->intr_after > 0 && --c->intr_after == 0) c->intr = true;     /* the tests' interrupt */
        if (c->intr) {                          /* a C during the call (the walk can't stop): its result discarded
                                                   on return (abacus #5267) */
            c->intr = false;
            return ABN_INTERRUPTED;
        }
    }
    if ((op == CASIM_PDIV || op == CASIM_CANCEL || op == CASIM_SUM) && cs == CASIM_OK)
        return cas_rows(c, op, out);            /* units 057, 070 */
    if (op == CASIM_SOLVE && cs == CASIM_OK) {  /* a report, not an equation: its view opens on the status row */
        if (!c->cas_cplx && c->eqn_model[c->eqn_cur] != 33 && !strcmp(out, "NO REAL ROOTS"))
            snprintf(out, sizeof out, "NO REAL ROOTS. CAS3 CPLX ON");   /* 076: where to turn it on (stu-discover; a 33s
                                                   equation has no complex numbers, so not there) */
        snprintf(c->cas_rep, sizeof c->cas_rep, "%s", out);
        c->cas_row = 0;
        return ABN_OK;
    }
    switch (cs) {
    case CASIM_PARTIAL:                         /* FACTOR's product, exact (read back) but a factor unproven: inserted,
                                                   MAY FACTOR MORE beside it (048 rule 4) */
    case CASIM_OK: {
        abn_status st = eqn_insert(c, out);
        if (st == ABN_OK && (op == CASIM_INTEGRATE || cs == CASIM_PARTIAL))
            c->eqn_note[c->eqn_cur] = op == CASIM_INTEGRATE ? AB_NOTE_PLUSC : AB_NOTE_MORE;  /* ∫'s +C: beside, never
                                                   in the text (048 rule 3) */
        if (st == ABN_OK) cas_stamp(c, out);
        return st;
    }
    case CASIM_SYNTAX: return ABN_SYNTAX;
    case CASIM_NOT_DIFFERENTIABLE:          /* out names it; subst's only refusal of a name is a draw copied */
        snprintf(c->cas_bad, sizeof c->cas_bad, "%.15s", out);      /* (a name: a few bytes) */
        return op == CASIM_SUBST && (!strcmp(out, "RAND") || !strcmp(out, "SEED")) ? ABN_CAS_COPY : ABN_CAS_UNSUPPORTED;
    case CASIM_TOO_LONG: return ABN_CAS_TOO_LONG;
    case CASIM_NO_ROOM: return ABN_CAS_NO_ROOM;
    case CASIM_NOT_POLYNOMIAL: return ABN_CAS_NOT_POLY;          /* unit 048 rule 6 */
    case CASIM_NOT_NUMERIC: return ABN_CAS_NOT_NUMERIC;
    case CASIM_NOT_INTEGRABLE: return ABN_CAS_NOT_INTEGRABLE;
    case CASIM_BAD_ARG:                     /* ∫ and FACTOR take an expression; the variable and angle are in range */
        if (op == CASIM_SUM) return ABN_CAS_BAD_BOUND;  /* 070: the equation was refused before the call */
        if (op == CASIM_DEFINT)             /* 056 (#5630 rule 3): an equation was refused before the call, so a
                                               bound: one with a letter, else one that isn't real (casim decides the
                                               refusal; the text only picks its message) */
            return cas_has_letter(repl) || cas_has_letter(repl2) ? ABN_CAS_BOUND_LETTER : ABN_CAS_BOUND_REAL;
        return op == CASIM_INTEGRATE || op == CASIM_FACTOR || op == CASIM_PDIV || op == CASIM_PGCD || op == CASIM_CANCEL
                   ? ABN_CAS_EXPR_ONLY : ABN_DOMAIN;    /* 057: CAS 010's take polynomials */
    case CASIM_IMPROPER: return ABN_CAS_IMPROPER;          /* unit 056: CAS 009 */
    case CASIM_UNDEFINED: return ABN_CAS_UNDEFINED;        /* unit 056: CAS 008, any op (INVALID DATA before) */
    case CASIM_UNDECIDED: return ABN_CAS_UNDECIDED;        /* CAS 015 rule 5: limit's walk undecided */
    case CASIM_NO_CLOSED_FORM: return ABN_CAS_NO_CLOSED;   /* unit 070: CAS 018 rule 6 */
    default: return ABN_DOMAIN;             /* INVALID DATA */
    }
#else
    (void)op, (void)var, (void)repl, (void)repl2;
    return c->stu ? ABN_DOMAIN : ABN_STU_ONLY;     /* a core built without Casimir (casim's own build of it) */
#endif
}

/* The CAS menu's keys: D/DX and COLCT in arg's variable, SIMP, EXPND; SUBST in arg's variable opens the editor for
   its replacement (ENTER substitutes, C or an empty one cancels). */
static abn_status cas_key(ab_calc *c, ab_op op, int arg)
{
    if (!c->stu) return ABN_STU_ONLY;
    if (c->eqn_cur < 0 || eqn_builtin(c, c->eqn_cur)) return ABN_NO_EQN;
    if (eqn_where(c)) return ABN_CAS_WHERE;     /* a sum's condition (070): before any editor opens */
    if ((op == AB_CASD || op == AB_CASCOL || op == AB_CASSUB || op == AB_CASSOLVE || op == AB_CASINT ||
         op == AB_CASFACT || op == AB_CASDEF || op == AB_CASQUOT || op == AB_CASGCD || op == AB_CASCANCL ||
         op == AB_CASSUM) &&
        (arg < 0 || arg > 25))
        return ABN_DOMAIN;
#ifdef AB_CASIM
    switch (op) {
    case AB_CASSOLVE: return cas_step(c, CASIM_SOLVE, arg, NULL, NULL);       /* unit 048 */
    case AB_CASINT: return cas_step(c, CASIM_INTEGRATE, arg, NULL, NULL);
    case AB_CASFACT: return cas_step(c, CASIM_FACTOR, arg, NULL, NULL);
    case AB_CASD: return cas_step(c, CASIM_DERIVE, arg, NULL, NULL);
    case AB_CASSIMP: return cas_step(c, CASIM_SIMPLIFY, 0, NULL, NULL);
    case AB_CASEXP: return cas_step(c, CASIM_EXPAND, 0, NULL, NULL);
    case AB_CASCOL: return cas_step(c, CASIM_COLLECT, arg, NULL, NULL);
    case AB_CASCANCL:                           /* unit 057: the equation shown is num÷den */
        if (strchr(c->eqn[c->eqn_cur], '=')) return ABN_CAS_EXPR_ONLY;
        return cas_step(c, CASIM_CANCEL, arg, NULL, NULL);
    default: break;
    }
#endif
    if (op != AB_CASSUB && op != AB_CASDEF && op != AB_CASQUOT && op != AB_CASGCD && op != AB_CASSUM)
        return cas_step(c, 0, 0, NULL, NULL);
    if (op == AB_CASDEF || op == AB_CASSUM) {   /* units 056, 070: the editor opens for the lower bound */
        c->cas_def = (uint8_t)(arg + 1);
        c->cas_def_step = 1;
        c->cas_def_sum = op == AB_CASSUM;
        c->cas_lo[0] = '\0';
    } else if (op == AB_CASQUOT || op == AB_CASGCD) {   /* unit 057: the editor opens for g */
        c->cas_pop = op == AB_CASQUOT ? 1 : 2;
        c->cas_pvar = (uint8_t)arg;
    } else
        c->cas_sub = (uint8_t)(arg + 1);
    c->eqn_typing = true;
    c->eqn_edit = -1;
    c->ebuf[0] = '\0';
    c->ecur = 0;
    return ABN_OK;
}

/* SUBST's replacement, typed: substituted (its implied products written in, read as STU's), or cancelled. */
static abn_status cas_subst_done(ab_calc *c, bool run)
{
    int var = c->cas_sub - 1;
    char t[AB_EQN_LEN];
    if (run && c->ebuf[0] && eqn_too_deep(c, c->ebuf, AB_MODEL_STU))
        return ABN_TOO_DEEP;                    /* nested past the bound (unit 047): still typed */
    if (run && c->ebuf[0] && eqn_write_implied(c, c->ebuf, t, sizeof t) != 1)
        return ABN_SYNTAX;                      /* doesn't parse (or won't fit): still typed, as 031 answer I */
    c->cas_sub = c->cas_def = c->cas_pop = 0;
    c->eqn_typing = false;
    if (!run || !c->ebuf[0]) return ABN_OK;
#ifdef AB_CASIM
    return cas_step(c, CASIM_SUBST, var, t, NULL);
#else
    (void)var;
    return cas_step(c, 0, 0, t, NULL);
#endif
}

/* Unit 057: QUOT's or GCD's g typed (run: ENTER) or dropped (C), as SUBST's replacement: checked against the parser's
   bound, its implied products written in; C, or ENTER on an empty g, cancels and keeps the list. An equation is refused
   before casim is called. */
static abn_status cas_poly_done(ab_calc *c, bool run)
{
    int which = c->cas_pop, var = c->cas_pvar;
    char t[AB_EQN_LEN];
    if (run && c->ebuf[0] && eqn_too_deep(c, c->ebuf, AB_MODEL_STU))
        return ABN_TOO_DEEP;                    /* nested past the bound (unit 047): still typed */
    if (run && c->ebuf[0] && eqn_write_implied(c, c->ebuf, t, sizeof t) != 1)
        return ABN_SYNTAX;                      /* doesn't parse (or won't fit): still typed, as 031 answer I */
    c->cas_pop = 0;
    c->eqn_typing = false;
    if (!run || !c->ebuf[0]) return ABN_OK;
    if (c->eqn_cur >= 0 && strchr(c->eqn[c->eqn_cur], '=')) return ABN_CAS_EXPR_ONLY;
#ifdef AB_CASIM
    return cas_step(c, which == 1 ? CASIM_PDIV : CASIM_PGCD, var, t, NULL);
#else
    (void)which, (void)var;
    return cas_step(c, 0, 0, t, NULL);
#endif
}

/* Unit 056: a bound of ∫AB typed (run: ENTER) or dropped (C). The lower bound is held and the editor opens for the
   upper; the upper runs CAS 009's defint. ENTER on an empty bound, or C, cancels the step and keeps the list. Each
   bound is checked against the parser's bound (047) and has its implied products written in, read as STU's, as
   SUBST's replacement. An equation is refused before casim is called (#5630 rule 3). Unit 070: Σ's bounds the same,
   and the upper runs CAS 018's sum. The parser has no ∞, and casim takes exactly two texts holding one: ∞, the sum to
   infinity, and -∞, its BAD_ARG (BAD BOUND), so those two go to casim as typed. Any other text holding ∞ (∞+1, a
   blank after it) fails the parser here, SYNTAX and still typed; sent, casim would say SYNTAX too, but after the step
   had closed (fresh eyes). */
static abn_status cas_def_bound(ab_calc *c, bool run)
{
    int var = c->cas_def - 1;
    char t[AB_EQN_LEN];
    bool up = c->cas_def_sum && c->cas_def_step == 2;
    bool inf = up && (!strcmp(c->ebuf, "\xe2\x88\x9e") || !strcmp(c->ebuf, "-\xe2\x88\x9e"));
    if (inf) strcpy(t, c->ebuf);
    if (run && c->ebuf[0] && !inf && eqn_too_deep(c, c->ebuf, AB_MODEL_STU))
        return ABN_TOO_DEEP;                    /* nested past the bound (unit 047): still typed */
    if (run && c->ebuf[0] && !inf && eqn_write_implied(c, c->ebuf, t, sizeof t) != 1)
        return ABN_SYNTAX;                      /* doesn't parse (or won't fit): still typed, as 031 answer I */
    if (!run || !c->ebuf[0]) {
        c->cas_def = c->cas_def_step = 0;
        c->cas_lo[0] = '\0';
        c->eqn_typing = false;
        return ABN_OK;
    }
    if (c->cas_def_step == 1) {                 /* the lower bound held; the editor empties for the upper */
        memcpy(c->cas_lo, t, sizeof c->cas_lo);
        c->cas_def_step = 2;
        c->ebuf[0] = '\0';
        c->ecur = 0;
        return ABN_OK;
    }
    c->cas_def = c->cas_def_step = 0;
    c->eqn_typing = false;
    char lo[AB_EQN_LEN];
    memcpy(lo, c->cas_lo, sizeof lo);
    c->cas_lo[0] = '\0';
    if (c->eqn_cur >= 0 && strchr(c->eqn[c->eqn_cur], '=')) return ABN_CAS_EXPR_ONLY;
#ifdef AB_CASIM
    return cas_step(c, c->cas_def_sum ? CASIM_SUM : CASIM_DEFINT, var, lo, t);
#else
    (void)var;
    return cas_step(c, 0, 0, lo, t);
#endif
}

/* Unit 073: a setting acts in Equation mode, as on the 35s ("selecting a setting menu ... will end the equation edit
   status", 35s UG p.6-8): the angle, the display (with the complex format, xiy and rθa), the base, the fraction
   display, the model, the beeper, the flags; and the clearings that don't touch the list (variables, statistics), as
   ALG lets them act. It went silently nowhere before (stu-discover, abacus #8714). Not the entry (RPN or ALG): ALG
   with Equation mode on is no state the image holds (state.c), so it says NOT IN EQN (fresh eyes). */
static bool eqn_setting(ab_op op)
{
    switch (op) {
    case AB_DEG: case AB_RAD: case AB_GRAD: case AB_SETFIX: case AB_SETSCI: case AB_SETENG: case AB_SETALL:
    case AB_DEC: case AB_HEX: case AB_OCT: case AB_BIN: case AB_FDISP: case AB_FRACFMT: case AB_MODEL:
    case AB_XIY: case AB_RTA: case AB_BEEP: case AB_SF: case AB_CF: case AB_CLVARS: case AB_CLSIGMA:
    case AB_CASCPLX:                                /* unit 076: CPLX acts while the list is shown */
        return true;
    default:
        return false;
    }
}

/* a key ab_do does nothing with: a 35s feature in 33s mode, or a 33s one in 35s (unit 041 A, C); XEQ or GTO to a
   line number in 33s mode (33s p.13-4; abacus #4799 rule 3.2; GTO . to one moves the pointer: AB_GTOPOS, p.13-6); the
   35s's 41st constant, e, in 33s mode (041 B) */
static bool key_inert(const ab_calc *c, ab_op op, int arg)
{
    return ab_hidden(c, op) || (!ab_rec35(c) && (op == AB_XEQ || op == AB_GTO) && arg >= 0 && arg % 1000 != 0) ||
           (op == AB_CONST && arg >= ab_consts(c));
}

/* whether ab_do takes the key before ALG's line or Equation mode see it (unit 075's predicates): ENG's view keys, an
   inert key, an open table or graph (their keys alone), SHOW */
static bool key_taken_first(const ab_calc *c, ab_op op, int arg)
{
    return op == AB_ENGL || op == AB_ENGR || key_inert(c, op, arg) || c->tbl_on || c->gr_on || op == AB_SHOW;
}

/* the keys eqn_key's switch acts on itself, never refused there (unit 075: ab_eqn_refuses reads it; a key added to the
   switch goes here too, or the predicate refuses it first, and help_test's side-by-side run fails) */
static bool eqn_handles(ab_op op)
{
    switch (op) {
    case AB_EQN: case AB_CANCEL: case AB_ENTER: case AB_CASD: case AB_CASSIMP: case AB_CASEXP: case AB_CASCOL:
    case AB_CASSUB: case AB_CASSOLVE: case AB_CASINT: case AB_CASFACT: case AB_CASDEF: case AB_CASQUOT: case AB_CASGCD:
    case AB_CASCANCL: case AB_CASSUM: case AB_KINF: case AB_XEQ: case AB_SOLVE: case AB_INTEG: case AB_EQTOP:
    case AB_SST: case AB_BST: case AB_KBKSP: case AB_KLEFT: case AB_KRIGHT:
        return true;
    default:
        return false;
    }
}

/* whether ab_do sends the key to eqn_key: Equation mode's list or editor, where CLEAR ALL, CLR EQN and CLPGM act
   (unit 021) and TABLE's and GRAPH's keys act as ever (032, 033) */
static bool eqn_routes(const ab_calc *c, ab_op op)
{
    return c->eqn_mode && !c->eqn_pending && !c->prgm && !c->alg && op != AB_CLALL && op != AB_CLEQN &&
           op != AB_CLPGM && !(op >= AB_TABLE && op <= AB_TSTEP) && !(op >= AB_GRAPH && op <= AB_GAUTO);
}

bool ab_eqn_refuses(const ab_calc *c, ab_op op, int arg)
{
    if (key_taken_first(c, op, arg) || !eqn_routes(c, op) || eqn_setting(op)) return false;
    if (op == AB_XEQ) return arg != AB_TGT_EQN;     /* XEQ of a label (unit 073) */
    return !eqn_handles(op) && !eqn_typable(op, arg);
}

static abn_status eqn_key(ab_calc *c, ab_op op, int arg)
{
    abn_status st;
    if (ab_eqn_refuses(c, op, arg)) return ABN_NOT_IN_EQN;     /* unit 075: the one test, which HELP's page asks too */
    if (eqn_setting(op)) {                          /* unit 073: the typed text stored first (the edit ends) */
        if (c->cas_sub) cas_subst_done(c, false);   /* a CAS step's text: the step dropped, never run (fresh eyes) */
        else if (c->cas_def) cas_def_bound(c, false);
        else if (c->cas_pop) cas_poly_done(c, false);
        else if (c->eqn_typing && (st = eqn_store_typed(c)) != ABN_OK) return st;
        st = do_key(c, op, arg);
        if (op == AB_MODEL && !ab_mode35(c) && eqn_builtin(c, c->eqn_cur))
            c->eqn_cur = -1;                        /* 33s mode has no built-ins (041 A4): the list's top, as EQN does */
        return st;
    }
    switch (op) {
    case AB_EQN:
    case AB_CANCEL:                                 /* C stores what is typed and leaves (p.6-4) */
        if (c->cas_sub) return cas_subst_done(c, false);    /* SUBST's replacement: dropped, the list kept (CAS 004) */
        if (c->cas_def) return cas_def_bound(c, false);     /* ∫AB's step cancelled (056) */
        if (c->cas_pop) return cas_poly_done(c, false);     /* QUOT's or GCD's cancelled (057) */
        st = eqn_store_typed(c);
        if (st != ABN_OK && c->eqn_typing) return st;   /* refused and still typed (unit 031 answer I) */
        c->eqn_mode = false;
        return st;
    case AB_ENTER:
        if (c->eqn_typing) return eqn_store_typed(c);     /* (SUBST's replacement: substituted, CAS 004) */
        return eqn_evaluate(c, true);
    case AB_CASD: case AB_CASSIMP: case AB_CASEXP: case AB_CASCOL: case AB_CASSUB:
    case AB_CASSOLVE: case AB_CASINT: case AB_CASFACT: case AB_CASDEF:
    case AB_CASQUOT: case AB_CASGCD: case AB_CASCANCL: case AB_CASSUM:
        if (c->eqn_typing) return ABN_OK;           /* the list's keys (CAS 004, 048, 056, 070) */
        return cas_key(c, op, arg);
    case AB_KINF:                               /* unit 070: ∞, only as Σ's upper bound (the bar shows it there) */
        if (c->eqn_typing && c->cas_def && c->cas_def_sum && c->cas_def_step == 2) ed_insert(c, "\xe2\x88\x9e", 0);
        return ABN_OK;
    case AB_XEQ:
        if (arg != AB_TGT_EQN) return ABN_NOT_IN_EQN;  /* (a backstop: ab_eqn_refuses refused it first, 075) */
        if (c->eqn_typing && (st = eqn_store_typed(c)) != ABN_OK) return st;
        return eqn_evaluate(c, false);
    case AB_SOLVE:
    case AB_INTEG:
        if (c->eqn_typing && (st = eqn_store_typed(c)) != ABN_OK) return st;
        return eqn_solve_key(c, op, arg);
    case AB_EQTOP:                                  /* NEW: typing then starts at TOP (unit 020) */
        if (!c->eqn_typing) c->eqn_cur = -1;
        return ABN_OK;
    case AB_SST:                                    /* the list wraps through EQN LIST TOP (p.6-7) */
    case AB_BST:                                    /* in 33s mode without the built-ins (unit 041 A4, 33s p.6-7) */
        if (c->eqn_typing) return ABN_OK;
        do {
            if (op == AB_SST) c->eqn_cur = c->eqn_cur + 1 >= c->eqn_count ? -1 : c->eqn_cur + 1;
            else c->eqn_cur = c->eqn_cur < 0 ? c->eqn_count - 1 : c->eqn_cur - 1;
        } while (c->eqn_cur >= 0 && eqn_builtin(c, c->eqn_cur) && !ab_mode35(c));
        return ABN_OK;
    case AB_KBKSP:
        if (c->eqn_typing) ed_backspace(c);
        else if (c->eqn_cur >= 0 && !eqn_builtin(c, c->eqn_cur)) eqn_delete(c, c->eqn_cur);
        return ABN_OK;                              /* the built-ins cannot be cleared (p.6-8) */
    case AB_KLEFT:
    case AB_KRIGHT:
        if (!c->eqn_typing) {
            /* on an equation shown: editing starts, > at its start, < at its end (p.6-7, 6-8) */
            if (c->eqn_cur < 0 || eqn_builtin(c, c->eqn_cur)) return ABN_OK;   /* nor edited */
            strcpy(c->ebuf, c->eqn[c->eqn_cur]);
            c->eqn_typing = true;
            c->eqn_edit = c->eqn_cur;
            c->ecur = op == AB_KLEFT ? (int)strlen(c->ebuf) : 0;
            return ABN_OK;
        }
        c->ecur = op == AB_KLEFT ? (c->ecur > 0 ? cp_prev(c->ebuf, c->ecur) : 0) : cp_next(c->ebuf, c->ecur);
        return ABN_OK;
    default:
        break;
    }
    /* a key that types: the first one starts a new equation (p.6-4); any other is refused, NOT IN EQN, where it did
       nothing silently (unit 073: the screen says what a key does) */
    if (!eqn_typable(op, arg)) return ABN_NOT_IN_EQN;     /* (a backstop: ab_eqn_refuses refused it first, 075) */
    eqn_type(c, op, arg);
    return ABN_OK;
}

/* ---- STU's ALG: the expression line (unit 029; abacus decision 51, answers 153d606) ---- */

/* Leaving ALG: ANS arrives in X as a keyed number would, the stack lifting unless lift is disabled
 * (rule 6, Michael's ruling); the line is put away. */
static void alg_leave(ab_calc *c)
{
    if (!c->alg) return;
    c->alg = false;
    c->eqn_typing = false; c->cas_sub = c->cas_def = c->cas_pop = 0;
    c->ebuf[0] = '\0';
    c->ecur = 0;
    c->alg_shown = false;
    c->hist_at = -1;
    place_val(c, c->ans);
    c->lift = true;
}

/* A new line: empty, or ANS before an operator typed after a result (rule 3). */
static void alg_new_line(ab_calc *c, bool with_ans)
{
    strcpy(c->ebuf, with_ans ? "ANS" : "");
    c->ecur = (int)strlen(c->ebuf);
    c->alg_shown = false;
    c->hist_at = -1;
}

/* = : the line by the equation engine (ANS a name here only), its variables read without prompts;
 * the result is ANS and the history's newest; the stack untouched. A syntax error puts the cursor
 * where it is; any error leaves the line editable. */
static abn_status alg_evaluate(ab_calc *c)
{
    ab_val v;
    int pos;
    char t[AB_EQN_LEN];
    c->alg_eval = true;
    int w = eqn_write_implied(c, c->ebuf, t, sizeof t);    /* the x written in at = (unit 031 answer K) */
    if (w < 0) {
        c->alg_eval = false;
        return ABN_MEMORY_FULL;                 /* refused: the line unchanged (answer I) */
    }
    if (w > 0 && strcmp(t, c->ebuf) != 0) {
        strcpy(c->ebuf, t);
        c->ecur = (int)strlen(c->ebuf);
    }
    abn_status st = eqn_eval(c, c->ebuf, AB_MODEL_STU, false, false, &v, &pos);
    c->alg_eval = false;
    if (st != ABN_OK) {
        if (st == ABN_SYNTAX) {
            int b = 0;
            for (int ch = 0; c->ebuf[b] && ch < pos; ch++) b = cp_next(c->ebuf, b);
            c->ecur = b;
            c->eqn_errpos = pos;
        }
        c->alg_shown = false;
        return st;
    }
    c->ans = v;
    if (c->hist_n == AB_HIST) {                     /* the oldest goes */
        memmove(c->hist[0], c->hist[1], sizeof c->hist[0] * (AB_HIST - 1));
        memmove(&c->hist_val[0], &c->hist_val[1], sizeof c->hist_val[0] * (AB_HIST - 1));
        c->hist_n--;
    }
    snprintf(c->hist[c->hist_n], sizeof c->hist[0], "%s", c->ebuf);
    c->hist_val[c->hist_n++] = v;
    c->alg_shown = true;
    c->ecur = (int)strlen(c->ebuf);
    c->hist_at = -1;
    return ABN_OK;
}

/* XMIN, XMAX, YMIN, YMAX set to a; YMIN and YMAX turn the fit off (033 rule 2). */
static void graph_set(ab_calc *c, ab_op op, const abn *a)
{
    if (op == AB_XMIN) c->gr_xmin = *a;
    else if (op == AB_XMAX) c->gr_xmax = *a;
    else if (op == AB_YMIN) c->gr_ymin = *a, c->gr_auto = false;
    else c->gr_ymax = *a, c->gr_auto = false;
}

/* What X's place shows on the line, for STO and TABLE's START and STEP: an entry shown, its result
   (029b); with a line typed, the line evaluated first (ours, rule 8); else ANS. */
static abn_status alg_x_shown(ab_calc *c, ab_val *v)
{
    abn_status st;
    if (c->hist_at >= 0) {
        *v = c->hist_val[c->hist_at];
        return ABN_OK;
    }
    if (!c->alg_shown && c->ebuf[0] && (st = alg_evaluate(c)) != ABN_OK) return st;
    *v = c->ans;
    return ABN_OK;
}

/* A key on the expression line. The stack keys, programs and Equation mode are RPN ONLY (rule 7,
 * proposal 5); STO stores ANS, or the line's value when one is typed (rule 8); mode and display
 * settings act as ever. */
bool ab_not_in_eqn(const ab_calc *c, ab_op op)
{
    if (!c->eqn_mode || c->eqn_pending || c->prgm || c->alg) return false;
    switch (op) {
    case AB_STO: case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:
    case AB_RCLADD: case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV:     /* (plain RCL types its letter) */
    case AB_LBL: case AB_INPUT: case AB_ISG: case AB_DSE: case AB_GTO: case AB_FN: case AB_FS: case AB_CLVARX:
    case AB_XVAR: case AB_VIEW:
        return true;
    default:
        return false;
    }
}

bool ab_rpn_only(const ab_calc *c, ab_op op)
{
    if (!c->alg || c->prgm) return false;
    switch (op) {
    case AB_RDN: case AB_XY: case AB_RUP: case AB_CLSTK: case AB_PRGM: case AB_RS: case AB_SST: case AB_BST:
    case AB_EQN: case AB_GTO: case AB_SOLVE: case AB_INTEG: case AB_FN: case AB_SIGMAPLUS: case AB_SIGMAMINUS:
    case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV: case AB_XVAR:
    case AB_XEQ:                                    /* programs are RPN (decision 46) */
        return true;
    default:
        return false;
    }
}

/* the mode and display settings ALG's line lets act as ever (alg_key passes them to do_key) */
static bool alg_setting(ab_op op)
{
    switch (op) {
    case AB_MODEL: case AB_ENTRYM: case AB_DEG: case AB_RAD: case AB_GRAD: case AB_SETFIX: case AB_SETSCI:
    case AB_SETENG: case AB_SETALL: case AB_DEC: case AB_HEX: case AB_OCT: case AB_BIN: case AB_FDISP: case AB_CLALL:
    case AB_CLPGM: case AB_CLVARS: case AB_VIEW: case AB_FRACFMT: case AB_SF: case AB_CF: case AB_BEEP: case AB_XIY:
    case AB_RTA: case AB_CASCPLX:                   /* (076: CPLX, a setting) */
        return true;
    default:
        return false;
    }
}

/* the keys alg_key's switch acts on itself (unit 075: ab_alg_refuses reads it; a key added to the switch goes here too,
   or the predicate refuses it first, and help_test's side-by-side run fails) */
static bool alg_handles(ab_op op)
{
    switch (op) {
    case AB_CANCEL: case AB_HUP: case AB_HDOWN: case AB_ENTER: case AB_KLEFT: case AB_KRIGHT: case AB_KBKSP: case AB_STO:
    case AB_TABLE: case AB_TVAR: case AB_GRAPH: case AB_GAUTO: case AB_XMIN: case AB_XMAX: case AB_YMIN: case AB_YMAX:
    case AB_TSTART: case AB_TSTEP: case AB_ANS: case AB_LASTX:
        return true;
    default:
        return false;
    }
}

bool ab_alg_refuses(const ab_calc *c, ab_op op, int arg)
{
    if (!c->alg || c->prgm || key_taken_first(c, op, arg)) return false;
    if (ab_rpn_only(c, op)) return true;
    return !alg_handles(op) && !alg_setting(op) && (op == AB_KEQ || !eqn_typable(op, arg));
}

static abn_status alg_key(ab_calc *c, ab_op op, int arg)
{
    if (ab_alg_refuses(c, op, arg)) return ABN_RPN_ONLY;   /* unit 075: the one test, which HELP's page asks too */
    switch (op) {
    case AB_CANCEL:                                 /* C clears the line; the history stays */
        alg_new_line(c, false);
        return ABN_OK;
    case AB_HUP:
        if (c->hist_n == 0) return ABN_OK;
        c->hist_at = c->hist_at < 0 ? c->hist_n - 1 : (c->hist_at > 0 ? c->hist_at - 1 : 0);
        return ABN_OK;
    case AB_HDOWN:
        if (c->hist_at < 0) return ABN_OK;
        c->hist_at = c->hist_at + 1 < c->hist_n ? c->hist_at + 1 : -1;
        return ABN_OK;
    case AB_ENTER:
        if (c->hist_at >= 0) {                      /* an entry shown: its text to the line */
            snprintf(c->ebuf, sizeof c->ebuf, "%s", c->hist[c->hist_at]);
            c->ecur = (int)strlen(c->ebuf);
            c->hist_at = -1;
            c->alg_shown = false;
            return ABN_OK;
        }
        if (!c->ebuf[0]) {                          /* an empty line: the most recent history line again,
                                                       nothing with none (029b) */
            if (c->hist_n == 0) return ABN_OK;
            snprintf(c->ebuf, sizeof c->ebuf, "%s", c->hist[c->hist_n - 1]);
            c->ecur = (int)strlen(c->ebuf);
        }
        return alg_evaluate(c);
    case AB_KLEFT:
    case AB_KRIGHT:
        if (c->hist_at >= 0) {                      /* editing a copy of the entry shown */
            snprintf(c->ebuf, sizeof c->ebuf, "%s", c->hist[c->hist_at]);
            c->ecur = op == AB_KLEFT ? (int)strlen(c->ebuf) : 0;
            c->hist_at = -1;
            c->alg_shown = false;
            return ABN_OK;
        }
        c->alg_shown = false;
        c->ecur = op == AB_KLEFT ? (c->ecur > 0 ? cp_prev(c->ebuf, c->ecur) : 0) : cp_next(c->ebuf, c->ecur);
        return ABN_OK;
    case AB_KBKSP:
        c->alg_shown = false;
        c->hist_at = -1;
        ed_backspace(c);
        return ABN_OK;
    case AB_STO: {
        ab_val v;
        abn_status st;
        slot sl;
        if ((st = alg_x_shown(c, &v)) != ABN_OK) return st;
        if ((st = resolve(c, arg, false, &sl)) != ABN_OK) return st;
        return store_val(c, &sl, v);
    }
    case AB_TABLE:                                  /* TABLE from the line (unit 032b): GO and VAR as ever */
    case AB_TVAR:
    case AB_GRAPH:                                  /* GRAPH from the line too (033, as 032b) */
    case AB_GAUTO:
        return do_key(c, op, arg);
    case AB_XMIN: case AB_XMAX: case AB_YMIN: case AB_YMAX: {   /* what X's place shows (as 032b) */
        ab_val v;
        abn_status st;
        if ((st = alg_x_shown(c, &v)) != ABN_OK) return st;
        if (v.kind != AB_REAL) return ABN_DOMAIN;
        graph_set(c, op, &v.re);
        return ABN_OK;
    }
    case AB_TSTART:                                 /* START and STEP take what X's place shows (032b) */
    case AB_TSTEP: {
        ab_val v;
        abn_status st;
        if ((st = alg_x_shown(c, &v)) != ABN_OK) return st;
        if (v.kind != AB_REAL) return ABN_DOMAIN;
        if (op == AB_TSTART) {
            c->tbl_start = v.re;
            c->tbl_start_set = true;
        } else {
            c->tbl_step = v.re;
        }
        return ABN_OK;
    }
    case AB_ANS:
    case AB_LASTX:                                  /* LASTx's key types ANS (proposal 1) */
        if (c->alg_shown || c->hist_at >= 0) alg_new_line(c, false);
        ed_insert(c, "ANS", 0);
        return ABN_OK;
    case AB_ADD: case AB_SUB: case AB_MUL: case AB_DIV: case AB_POW:
        if (c->alg_shown) alg_new_line(c, true);    /* an operator after a result: ANS first */
        break;
    default:
        break;
    }
    /* mode and display settings act as ever; a key that types types into the line (a digit or a
       function after a result starting a new one) */
    if (alg_setting(op)) return do_key(c, op, arg);
    /* a key with no place on the line (= is ENTER's here) has no meaning on it: RPN ONLY, the line
       as it was (rule 7; the reviewer's sweep: →POL, CLΣ, LBL, ISG, RCL+, /c and the rest) */
    if (op == AB_KEQ || !eqn_typable(op, arg)) return ABN_RPN_ONLY;     /* (a backstop: ab_alg_refuses, 075) */
    if (c->alg_shown && op != AB_ADD && op != AB_SUB && op != AB_MUL && op != AB_DIV && op != AB_POW)
        alg_new_line(c, false);
    if (c->hist_at >= 0) c->hist_at = -1;
    eqn_type(c, op, arg);
    return ABN_OK;
}

abn_status ab_do(ab_calc *c, ab_op op)
{
    return ab_do_arg(c, op, 0);
}

/* Operations that take x (and y) as numbers. */
static bool reads_x(ab_op op)
{
    return (op >= AB_ADD && op <= AB_PCTCHG && op != AB_PI && op != AB_DEG && op != AB_RAD &&
            op != AB_GRAD) ||
           (op >= AB_HMS && op <= AB_TOREC && op != AB_RAND) || op == AB_SIGMAPLUS ||
           op == AB_SIGMAMINUS || op == AB_XHAT || op == AB_YHAT || op == AB_SETC ||
           (op >= AB_AND && op <= AB_NOR) || op == AB_ARG || (op >= AB_EXPM1 && op <= AB_LOG2) ||
           op == AB_CUBE || op == AB_CBRT;
}

static bool reads_y(ab_op op)
{
    switch (op) {
    case AB_ADD: case AB_SUB: case AB_MUL: case AB_DIV: case AB_IDIV: case AB_RMDR: case AB_POW:
    case AB_XROOT: case AB_PCT: case AB_PCTCHG: case AB_NCR: case AB_NPR: case AB_TOPOL:
    case AB_TOREC: case AB_SIGMAPLUS: case AB_SIGMAMINUS: case AB_AND: case AB_XOR: case AB_OR:
    case AB_NAND: case AB_NOR:
        return true;
    default:
        return false;
    }
}

/* What takes a vector (UG p.10-1 to 10-5): +, -, x, / (vec_arith sorts out which pairs), ABS. */
static bool takes_vector(ab_op op)
{
    return op == AB_ADD || op == AB_SUB || op == AB_MUL || op == AB_DIV || op == AB_ABS;
}

/* The guide's list of what takes complex numbers (UG p.9-3), in decimal; ABS and ARG give reals. */
static bool takes_complex(ab_op op)
{
    switch (op) {
    case AB_ADD: case AB_SUB: case AB_MUL: case AB_DIV: case AB_POW: case AB_INV: case AB_LN:
    case AB_EXP: case AB_SIN: case AB_COS: case AB_TAN: case AB_ABS: case AB_ARG:
        return true;
    default:
        return false;
    }
}

/* An error message shows until the next key (unit 017 rule 4, Michael's machines): on the 33s
 * that key clears it and acts, except C and backspace, which only clear it (unit 024 rule 1);
 * on the 35s every key only clears it (also 35s p.F-1). Any error a key
 * causes leaves a message, a program's error stop included; ABN_CLAMPED (a base-mode result
 * clamped to the word) is a completed operation, not a message. */
/* Unit 041 C (B4): the exponent a line of X shows (after its E; none, 0) and the place of its last digit: the
   exponent less the digits after the radix ('.': the core's text and the entry; separators skipped). exp_neg: a keyed
   exponent's sign, which the entry keeps apart from its text. */
static void shown_form(const char *s, int len, bool exp_neg, int *e, int *low)
{
    int frac = 0, ex = 0;
    bool point = false, in_exp = false, neg = exp_neg;
    for (int i = 0; i < len && s[i]; i++) {
        char ch = s[i];
        if (ch == 'E') {
            in_exp = true;
        } else if (in_exp) {
            if (ch == '-') neg = true;
            else if (ch >= '0' && ch <= '9' && ex < 100000) ex = ex * 10 + (ch - '0');
        } else if (ch == '.') {
            point = true;
        } else if (ch >= '0' && ch <= '9' && point) {
            frac++;
        }
    }
    *e = neg ? -ex : ex;
    *low = *e - frac;
}

static int floor3(int x)
{
    return x >= 0 ? x / 3 : -((-x + 2) / 3);
}

/* Unit 041 C (B4): ENG→ (op AB_ENGR) and ←ENG (AB_ENGL), abacus #4850 approving soroban's reading of 33s p.1-20 and
   35s p.1-23: the shown exponent moves to the next multiple of three below or above the one shown now (the keyed
   number's, which the key completes; the view's; else the X line's as the core's format draws it at the screen's
   widths, with the device's grouping), keeping the shown digits: the view rounds X at the last place shown, so it
   shows the value the display showed (fresh eyes: FIX 2's 10.00 for 9.999). Display only: X, the stack and lift as any completed entry leaves them. A view
   wider than the screen's widest line is not taken: the last that fits stays. At the RPN keyboard, in decimal, with a
   real X; elsewhere nothing. */
static abn_status eng_key(ab_calc *c, ab_op op)
{
    int ref, low;
    char t[AB_ENTRY_TEXT + 8];
    if (c->base != 10 || c->cpart || c->vlen || c->prgm || c->eqn_mode || c->alg || c->tbl_on || c->gr_on ||
        c->viewing || c->x.kind != AB_REAL)
        return ABN_OK;
    bool keyed = c->entry.active, fraction = false;
    if (keyed) {
        if (c->m35 && no_mantissa(&c->entry)) {     /* the 35s's E with no mantissa: as any key (unit 017 rule 3) */
            c->entry.back = c->entry.len;
            return ABN_SYNTAX;
        }
        int dots = 0;
        for (int i = 0; i < c->entry.len; i++) dots += c->entry.text[i] == '.';
        fraction = dots > 1;                    /* a keyed fraction shows no exponent: read as formatted */
        if (!fraction) shown_form(c->entry.text, c->entry.len, c->entry.exp_neg, &ref, &low);
        c->entry.active = false;
        c->entry.back = 0;
        c->lift = true;
    }
    if (c->eng_on && !keyed) {
        ref = c->eng_e;
        low = c->eng_low;
    } else if (!keyed || fraction) {
        ab_fmt f = {c->disp_mode, c->disp_n, AB_FMT_LINE, '.', (char)c->disp_group};
        int len = ab_format(&c->x.re, &f, t, sizeof t);
        if (len < 0) {
            f.width = AB_FMT_WIDE;
            len = ab_format(&c->x.re, &f, t, sizeof t);
        }
        if (len < 0) return ABN_OK;
        shown_form(t, len, false, &ref, &low);
    }
    int e = op == AB_ENGR ? floor3(ref - 1) * 3 : (floor3(ref) + 1) * 3;
    ab_fmt w = {AB_ENG, 0, AB_FMT_WIDE, '.', 0};
    if (e < -AB_ENG_LIMIT || e > AB_ENG_LIMIT || low < -AB_ENG_LIMIT || low > AB_ENG_LIMIT ||
        ab_format_eng_view(&c->x.re, low, e, &w, t, sizeof t) < 0)
        return ABN_OK;
    c->eng_on = true;
    c->eng_e = (int16_t)e;
    c->eng_low = (int16_t)low;
    return ABN_OK;
}

static bool casrep_key(ab_calc *c, ab_op op, abn_status *st);

abn_status ab_do_arg(ab_calc *c, ab_op op, int arg)
{
    abn_status st;
    ab_val x0 = c->x;
    if (c->msg) {
        /* the 35s: any key only clears the message; the 33s: C and backspace only clear it, any other
           key clears it and acts (33s p.1-23 and its error appendix, 32SII p.1-19, E-1; unit 024
           rule 1, amending 017 rule 4) */
        c->msg = false;
        if (c->m35 || op == AB_CANCEL || op == AB_KBKSP) return ABN_OK;
    }
    if (op == AB_ENGL || op == AB_ENGR) {      /* unit 041 C (B4); a 35s SYNTAX shows its message (fresh eyes) */
        st = eng_key(c, op);
        goto done;
    }
    c->eng_on = false;                          /* the view lasts until the next key */
    if (key_inert(c, op, arg)) return ABN_OK;
    if (c->tbl_on || c->gr_on) {                /* the table or the graph open: its keys alone (units 032,
                                                   033), each with a fresh error record and overflow count */
        c->err_set = false;
        c->ovf = 0;
        st = c->tbl_on ? table_key(c, op) : graph_key(c, op, arg);
        goto done;
    }
    if (op == AB_SHOW) return ABN_OK;           /* unit 030: SHOW changes nothing, in program entry too
                                                   (neutral, appendix B; not a line, 35s p.G-1) */
    if (ab_view_key(c, op)) return ABN_OK;
    c->err_set = false;
    c->ovf = 0;
    if (c->cas_rep[0] && casrep_key(c, op, &st)) goto done;    /* unit 048: SOLVE's report open */
    /* CLEAR ALL, CLR EQN and CLPGM act in Equation mode too (unit 021: never a silent no-op once the
       device has asked CLR ALL?, CLR EQN? or CLR PGMS?; CLPGM's question: abacus #8497) */
    if (c->prgm) st = program_key(c, op, arg);
    else if (c->alg) st = alg_key(c, op, arg);      /* STU's expression line (unit 029) */
    else if (eqn_routes(c, op))
        st = eqn_key(c, op, arg);               /* (TABLE's GO and settings act in Equation mode: 032) */
    else st = do_key(c, op, arg);
done:
    if (st != ABN_OK && st != ABN_CLAMPED && st != ABN_RPN_ONLY && st != ABN_STU_ONLY && st != ABN_NO_EQN &&
        st != ABN_NO_VAR && st != ABN_NOT_IN_EQN) {     /* (the brief notices: not messages) */
        if (st == ABN_OVERFLOW) note_overflow(c);   /* a complex or vector one (unit 026, abacus #4198) */
        c->msg = true;
        err_note(c, op, arg, &x0);
        c->err_status = st;
    }
    return st;
}

/* Unit 048: SOLVE's report. Its rows: the status (before ": "), then each root ("; " between them). */
const char *ab_cas_report(const ab_calc *c)
{
    return c->cas_rep[0] ? c->cas_rep : NULL;
}

/* Unit 079 (Michael's wording, abacus #10707): casim's ", n CPLX" at the end of a status (CAS 028; put_left_out is its
   one producer, so nothing follows it) is shown as "n CPLX ROOTS NOT SHOWN". The count's digits' offset in the status
   of length len, or -1 for none. */
static int cas_cplx_at(const char *r, int len)
{
    int d = len - 5;
    if (d < 3 || strncmp(r + d, " CPLX", 5) != 0) return -1;
    while (d > 0 && r[d - 1] >= '0' && r[d - 1] <= '9') d--;
    return d < len - 5 && d >= 2 && r[d - 2] == ',' && r[d - 1] == ' ' ? d : -1;
}

/* the report's row i as the screen shows it (079): SOLVED keeps the note on its row ("SOLVED, 2 CPLX ROOTS NOT
   SHOWN", 31 cells at most: solve's degree cap of 12 bounds the count at 10); any other status (PARTIAL, DEGREE n LEFT,
   already up to 31 cells) gives the note a row of its own under it, before the roots. cas_rep keeps casim's line */
bool ab_cas_row(const ab_calc *c, int i, char *out, int size)
{
    const char *r = c->cas_rep, *p = strstr(r, ": ");
    if (!r[0] || i < 0) return false;
    int len = p ? (int)(p - r) : (int)strlen(r), d = cas_cplx_at(r, len);
    bool own = d >= 0 && !(d == 8 && strncmp(r, "SOLVED", 6) == 0);     /* the note on a row of its own */
    if (i == 0) {
        if (d >= 0 && !own) snprintf(out, size, "%.*s CPLX ROOTS NOT SHOWN", len - 5, r);
        else snprintf(out, size, "%.*s", own ? d - 2 : len, r);
        return true;
    }
    if (own && i == 1) {
        snprintf(out, size, "%.*s CPLX ROOTS NOT SHOWN", len - 5 - d, r + d);
        return true;
    }
    if (own) i--;
    if (!p) return false;
    p += 2;
    for (int k = 1; k < i; k++) {
        if ((p = strstr(p, "; ")) == NULL) return false;
        p += 2;
    }
    const char *e = strstr(p, "; ");
    snprintf(out, size, "%.*s", e ? (int)(e - p) : (int)strlen(p), p);
    return true;
}

int ab_cas_rows(const ab_calc *c)
{
    char row[AB_EQN_LEN];
    int n = 0;
    while (ab_cas_row(c, n, row, sizeof row)) n++;
    return n;
}

/* A key while the report shows (048 rule 5, abacus #5267): down and up step among the rows, wrapping, as a linear
   solution's view (35s p.7-7); ENTER on a root puts its value in X, read through the core's own evaluation of the
   root's text (STU's parse, the angle mode; its (MULT k) is not part of it), the stack lifted as RCL's, and leaves;
   ENTER on the status row does nothing; C or backspace leaves. Any other key leaves and then acts (false). */
static bool casrep_key(ab_calc *c, ab_op op, abn_status *st)
{
    int n = ab_cas_rows(c);
    *st = ABN_OK;
    if (op == AB_SST || op == AB_BST) {
        c->cas_row = (int8_t)((c->cas_row + (op == AB_SST ? 1 : n - 1)) % n);
        return true;
    }
    if (op == AB_ENTER) {
        char row[AB_EQN_LEN], *eq, *mult;
        ab_val v;
        int pos;
        if (c->cas_row == 0 || !ab_cas_row(c, c->cas_row, row, sizeof row) || (eq = strchr(row, '=')) == NULL)
            return true;
        if ((mult = strstr(eq, " (MULT")) != NULL) *mult = '\0';
        if ((*st = eqn_eval(c, eq + 1, AB_MODEL_STU, false, false, &v, &pos)) != ABN_OK) return true;
        c->cas_rep[0] = '\0';
        place_val(c, v);
        c->lift = true;
        c->eqn_mode = false;                    /* X shown, as an evaluated equation's value is */
        return true;
    }
    c->cas_rep[0] = '\0';
    return op == AB_CANCEL || op == AB_KBKSP;
}

/* Unit 048: the note beside the equation shown, while it is (cleared by ab_do_arg once the list leaves it); a table
   since 056, read for the equation shown. */
const char *ab_cas_note(const ab_calc *c)
{
    static const char *const TEXT[AB_NOTE_LAST + 1] = {NULL, "+C", "MAY FACTOR MORE", "QUOTIENT", "REMAINDER", "NOT AT",
                                                       "WHERE"};      /* (070) */
    if (!c->eqn_mode || c->eqn_typing || c->eqn_cur < 0 || c->eqn_note[c->eqn_cur] > AB_NOTE_LAST) return NULL;
    return TEXT[c->eqn_note[c->eqn_cur]];       /* unit 057: the equation's own label, wherever it now is */
}

/* Units 056, 057: Y's label while a CAS step's text is typed. */
const char *ab_cas_prompt_label(const ab_calc *c, char *buf, int size)
{
    if (c->cas_pop && c->eqn_typing) {
        snprintf(buf, (size_t)size, "%s", c->cas_pop == 1 ? "\xc3\xb7 BY" : "GCD WITH");
        return buf;
    }
    if (!c->cas_def || !c->eqn_typing) return NULL;
    snprintf(buf, (size_t)size, "%s%c %s", c->cas_def_sum ? "\xce\xa3" : "\xe2\x88\xab", 'A' + c->cas_def - 1,
             c->cas_def_step == 2 ? "TO" : "FROM");    /* (070: Σ's, "ΣK FROM", "ΣK TO") */
    return buf;
}

bool ab_view_linear(const ab_calc *c)
{
    return c->viewing && c->view_id <= AB_VIEW_LIN2 && c->view_id >= AB_VIEW_LIN3 - 2 &&
           (c->view_id >= AB_VIEW_LIN2 - 1 || c->view_id <= AB_VIEW_LIN3);
}

bool ab_view_key(ab_calc *c, int op)
{
    if (!c->viewing) return false;
    int id = c->view_id;
    bool lin2 = id == AB_VIEW_LIN2 || id == AB_VIEW_LIN2 - 1, lin3 = id <= AB_VIEW_LIN3 && id >= AB_VIEW_LIN3 - 2;
    if ((lin2 || lin3) && (op == AB_SST || op == AB_BST)) {   /* a linear solution: down and up step among
                                                                 its variables, wrapping (35s p.7-7) */
        int base = lin2 ? AB_VIEW_LIN2 : AB_VIEW_LIN3, n = lin2 ? 2 : 3, j = base - id;
        j = (j + (op == AB_SST ? 1 : n - 1)) % n;
        c->view_id = base - j;
        return true;
    }
    c->viewing = false;
    return op == AB_CANCEL || op == AB_KBKSP;
}

bool ab_mode35(const ab_calc *c)
{
    if (c->running > 0 && c->exec_at >= 0 && c->exec_at < c->prog_len) {
        int l = prog_owner(c, c->exec_at);
        if (l >= 0 && c->lmode[l]) return c->lmode[l] != 33;   /* 35 or STU */
    }
    return c->m35;
}

/* Unit 075: the model a key acts in: in program entry the edited label's stamp, during a run the running label's,
   else the calculator's (33, 35 or AB_MODEL_STU); HELP describes that one (abacus #8740, H52) */
int ab_rec_model(const ab_calc *c)
{
    int pc = c->running <= 0 && c->prgm ? c->pc : c->running > 0 ? c->exec_at : -1;
    if (pc >= 0 && pc < c->prog_len) {
        int l = prog_owner(c, pc);
        if (l >= 0 && c->lmode[l]) return c->lmode[l];
    }
    return c->stu ? AB_MODEL_STU : c->m35 ? 35 : 33;
}

bool ab_rec35(const ab_calc *c)
{
    if (c->running <= 0 && c->prgm && c->pc >= 0 && c->pc < c->prog_len) {
        int l = prog_owner(c, c->pc);           /* the label the new line joins */
        if (l >= 0 && c->lmode[l]) return c->lmode[l] != 33;
    }
    return ab_mode35(c);
}

bool ab_hidden(const ab_calc *c, int op)
{
    if (ab_rec35(c)) return op == AB_CUBE || op == AB_CBRT;     /* 33s mode's features the 35s lacks (041 C) */
    switch (op) {
    case AB_KI: case AB_KTHETA: case AB_KCOMMA:     /* typing an equation: the parser refuses (or, i, the variable) */
        return !c->eqn_typing;
    case AB_ARG: case AB_XIY: case AB_RTA:          /* A1, A2: one-value complex numbers (33s p.9-1 to 9-3, 1-19) */
    case AB_KVEC:                                   /* A3: vectors */
    case AB_AND: case AB_OR: case AB_XOR: case AB_NOT: case AB_NAND: case AB_NOR: case AB_SUF:     /* A5 (p.10-1) */
    case AB_CLSTK: case AB_CLVARX:                  /* A6 (p.1-6) */
    case AB_TOKM: case AB_TOMILE:                   /* A7 (p.4-13) */
    case AB_KREG:                                   /* A11: REGX..REGT, typed (p.6-15) */
    case AB_TABLE: case AB_GRAPH:                   /* A14: their menus don't open */
        return true;
    default:
        return false;
    }
}

int ab_consts(const ab_calc *c)
{
    return ab_rec35(c) ? ab_const_count() : AB_CONST_33;
}

bool ab_builtin_shown(const ab_calc *c)
{
    return c->eqn_mode && !c->eqn_typing && eqn_builtin(c, c->eqn_cur) != 0;
}

bool ab_label_indirect(const ab_calc *c, int label)
{
    if (label < 0 || label >= 26 || c->labelpos[label] < 0) return false;
    int end = prog_label_end(c, c->labelpos[label]);
    for (int i = c->labelpos[label] + 1; i < end; i++) {
        int op = c->prog[i].op, arg = c->prog[i].arg;
        bool takes_var = (op >= AB_STO && op <= AB_VIEW) || op == AB_INPUT || op == AB_ISG || op == AB_DSE ||
                         op == AB_SOLVE || op == AB_INTEG;
        if (takes_var && (arg == AB_VAR_IND_I || arg == AB_VAR_IND_J)) return true;
        if ((op == AB_XEQ || op == AB_GTO || op == AB_FN) && (arg == AB_TGT_IND_I || arg == AB_TGT_IND_J)) return true;
        if (op == AB_EQLINE && (strstr(c->peqn[arg], "\xe2\x92\xa4") || strstr(c->peqn[arg], "\xe2\x92\xa5")))
            return true;
    }
    return false;
}

void ab_interrupt(ab_calc *c)
{
    if (c->stop == AB_STOP_SLICE) c->stop = AB_STOP_STOP;  /* between slices: stopped at once */
    else if (c->running) c->intr = true;                  /* during a call: read between lines */
}

abn_status ab_continue(ab_calc *c)
{
    abn_status st;
    if (c->stop != AB_STOP_SLICE) return ABN_OK;
    c->stop = AB_STOP_NONE;
    continuing = true;
    c->err_set = false;
    st = prog_run(c, c->budget - c->run_lines);
    if (st != ABN_OK && st != ABN_CLAMPED) {                /* as ab_do_arg (units 017, 018) */
        c->msg = true;
        err_note(c, AB_RS, 0, NULL);
        c->err_status = st;
    }
    return st;
}

void ab_prog_relabel(ab_calc *c)
{
    prog_labels(c);
}

/* CLEAR ALL (UG p.B-4's middle column, p.1-29, G-5): the modes, the format, the flags, the
 * fraction settings and the seed unchanged; the equation list (the built-ins stay), program memory,
 * the stack (LAST X too: a stored number, ours), the variables, the statistics registers (ours:
 * p.1-29 "all numbers stored"; the table has no row for them) cleared, the indirect variables not
 * defined, FN= null, both pointers at the top, stack lift enabled; whatever was being typed,
 * prompted or run is let go. */
void ab_clear_all(ab_calc *c)
{
    ab_val zero = ab_real(abn_zero());
    c->x = c->y = c->z = c->t = c->lastx = zero;
    c->lift = true;
    for (int i = 0; i < 27; i++) c->var[i] = zero;  /* A..Z and i (unit 025) */
    for (int i = 0; i < AB_STAT_REGS; i++) c->stat[i] = abn_zero();
    for (int i = 0; i <= c->ind_top; i++) c->ind[i] = abn_zero();
    c->ind_top = -1;
    prog_clear(c);
    c->eqn_count = 2;
    memset(c->eqn_note, 0, sizeof c->eqn_note);     /* unit 057 */
    strcpy(c->eqn[0], EQN_LIN2);
    strcpy(c->eqn[1], EQN_LIN3);
    c->eqn_model[0] = c->eqn_model[1] = 35;
    c->eqn_cur = -1;
    c->fn = -1;
    c->fn_eqn = -1;
    memset(&c->entry, 0, sizeof c->entry);
    memset(&c->entry1, 0, sizeof c->entry1);
    memset(c->ventry, 0, sizeof c->ventry);
    c->cpart = c->vlen = 0;
    c->prgm = false;
    c->plit = zero;
    c->eqn_mode = c->eqn_typing = false; c->cas_sub = c->cas_def = c->cas_pop = 0;
    c->eqn_edit = -1;
    c->ebuf[0] = '\0';
    c->ecur = 0;
    c->eqn_pending = c->eqn_nvars = c->eqn_next = 0;
    c->stop = AB_STOP_NONE;
    c->stop_var = -1;
    c->active = c->solve_phase = 0;
    c->susp.kind = 0;
    c->peq_line = -1;
    c->peq_ready = false;
    c->ev_cached = false;
    c->run_lines = 0;
    /* STU's ALG is a mode, kept; its line, ANS and history are data, cleared (unit 029) */
    c->eqn_typing = c->alg;
    c->alg_shown = false;
    c->ans = zero;
    c->hist_n = 0;
    c->hist_at = -1;
}

/* MEMORY CLEAR (UG p.B-3, B-4's right column): everything at its default, DEG, DEC, FIX 4, /c 4095,
 * xiy, fraction display off, flags clear, seed zero; the core's run limit (budget) at its default
 * too, and the device's slice kept (abacus #3971: a calloc'd host had budget 0, so every run
 * stopped at once).
 * Contrast, the radix mark and the thousands separator are the device's (the core shows numbers in
 * its own form). A new calculator (ab_init) shows the same FIX 4 (unit 067). */
static void memory_clear(ab_calc *c, bool keep_hist)
{
    long slice = c->slice;
    init_fields(c, keep_hist);
    c->disp_mode = AB_FIX;
    c->disp_n = 4;
    c->slice = slice;
}

void ab_memory_clear(ab_calc *c)
{
    memory_clear(c, false);
}

void ab_memory_clear_state(ab_calc *c)
{
    memory_clear(c, true);
}

abn_status ab_eqn_op(ab_calc *c, ab_op op, const ab_val *y, const ab_val *x, ab_val *out)
{
    ab_val sx = c->x, sy = c->y, sz = c->z, st_ = c->t, sl = c->lastx;
    bool lift = c->lift;
    abn_status st;
    if (y) c->y = *y;
    if (x) c->x = *x;
    st = do_key(c, op, 0);
    if (st != ABN_OK && st != ABN_CLAMPED) err_note(c, op, 0, x);   /* inside an equation */
    *out = c->x;
    c->x = sx;
    c->y = sy;
    c->z = sz;
    c->t = st_;
    c->lastx = sl;
    c->lift = lift;
    return st;
}

static abn_status do_key(ab_calc *c, ab_op op, int arg)
{
    if (op == AB_KI || op == AB_KTHETA) return complex_key(c, op);
    if (op == AB_KVEC || op == AB_KCOMMA) return vector_key(c, op);
    if (op <= AB_KE || (op >= AB_KA && op <= AB_KF)) return entry_key(c, op);
    if (op == AB_SUF) return suffix(c, arg);
    if ((op == AB_KCHS || op == AB_KBKSP) && c->entry.active) return entry_key(c, op);
    if (op == AB_CANCEL) return cancel_key(c);     /* before any completion: C clears an entry */

    /* Every other key completes the number being entered (UG p.1-17). A completed typed
     * number enables lift before the operation acts, so a recall after `1 ENTER 2` keeps
     * the 2 (work unit 001, amendment 2, ruling 1; the guide does not show this case). */
    if (c->entry.active) {
        if (c->m35 && !c->cpart && !c->vlen && no_mantissa(&c->entry)) {
            /* the 35s (unit 017 rule 3): SYNTAX ERROR, nothing done, the text kept for editing
               with the insertion point at its start */
            c->entry.back = c->entry.len;
            return ABN_SYNTAX;
        }
        c->entry.active = false;
        c->entry.back = 0;
        c->lift = true;
    }
    c->cpart = 0;
    c->vlen = 0;

    /* Any other function given a complex number (UG p.9-3, p.F) or a vector (UG ch.10, unit 010
       rule 2) is INVALID DATA, nothing changed. */
    if (((reads_x(op) && is_cx(&c->x)) || (reads_y(op) && is_cx(&c->y))) &&
        !(takes_complex(op) && c->base == 10))
        return ABN_DOMAIN;
    if (((reads_x(op) && is_vec(&c->x)) || (reads_y(op) && is_vec(&c->y))) &&
        !(takes_vector(op) && c->base == 10))
        return ABN_DOMAIN;

    /* In HEX the keys of these functions type digits, and the guide lists them as unavailable
     * (UG p.11-4): an error here, changing nothing. */
    if (c->base == 16 && (op == AB_SQRT || op == AB_EXP || op == AB_LN || op == AB_POW ||
                          op == AB_INV || op == AB_SIGMAPLUS))
        return ABN_DOMAIN;

    switch (op) {
    case AB_CMPLX:
        return cmplx_pair(c, arg);
    case AB_KBKSP:  /* with no entry in progress backspace acts as clear X (UG p.1-17) */
    case AB_CLX:
        /* Clear X puts 0 in X and disables lift, so the next number replaces it
         * (UG p.2-7, p.B-5). */
        c->x = ab_real(abn_zero());
        c->lift = false;
        return ABN_OK;
    case AB_CLSTK:
        /* Clear the stack: all four registers to 0, lift disabled (UG p.2-7, p.B-5). */
        c->x = c->y = c->z = c->t = ab_real(abn_zero());
        c->lift = false;
        return ABN_OK;
    case AB_ENTER:
        if (c->stop == AB_STOP_VIEW) {
            /* at a program's VIEW, ENTER copies the value shown to X (UG p.13-15) */
            slot sl;
            slot_of_id(c, c->stop_var, &sl);
            place_val(c, load_val(&sl));
            c->stop = AB_STOP_STOP;
            break;
        }
        /* ENTER lifts the stack, copying X into Y, and disables lift so the next number
         * writes over the copy left in X (UG p.2-6, p.B-5). */
        stack_lift(c);
        c->lift = false;
        return ABN_OK;
    case AB_RDN: {
        /* Roll down: Y to X, Z to Y, T to Z, X to T (UG p.2-3). */
        ab_val x = c->x;
        c->x = c->y;
        c->y = c->z;
        c->z = c->t;
        c->t = x;
        break;
    }
    case AB_RUP: {
        /* Roll up: X to Y, Y to Z, Z to T, T to X (UG p.2-4). */
        ab_val t = c->t;
        c->t = c->z;
        c->z = c->y;
        c->y = c->x;
        c->x = t;
        break;
    }
    case AB_XY: {
        /* Exchange X and Y; Z and T are untouched (UG p.2-4). */
        ab_val x = c->x;
        c->x = c->y;
        c->y = x;
        break;
    }
    case AB_LASTX:
        /* LAST X brings back the number X held before the last numeric operation; it
         * arrives like any new number, and the LAST X register keeps it (UG p.2-8, p.2-11). */
        place_val(c, c->lastx);
        break;
    case AB_KCHS:
        /* Change sign of a completed X (UG p.1-15); of both parts of a complex one (UG p.9-3).
           The guide lists no sign change for a vector: INVALID DATA (unit 010 rule 2). */
        if (is_vec(&c->x)) return ABN_DOMAIN;
        c->x.re = abn_neg(&c->x.re);
        c->x.im = abn_neg(&c->x.im);
        break;
    case AB_ARG: {
        /* The angle of x i y in the angle mode (UG p.9-3); a real x is x i 0. Saves x in LAST X as
           ABS does (UG p.B-6). */
        abn r, th;
        abn_status st = fn_topol(&r, &th, &c->x.re, &c->x.im, angle_of(c));
        if (st != ABN_OK && st != ABN_OVERFLOW) return st;     /* an r beyond the range: theta stands */
        c->lastx = c->x;
        c->x = ab_real(th);
        break;
    }
    case AB_XIY:
    case AB_RTA:
        /* the complex display form; neutral for lift (UG p.9-1, p.B-5) */
        c->cpolar = op == AB_RTA;
        return ABN_OK;
    case AB_ADD:
    case AB_SUB:
    case AB_MUL:
    case AB_DIV:
        if (c->base != 10) return base_arith(c, op);
        return two_number_op(c, op);
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
    case AB_EXPM1: case AB_LN1P: case AB_TWOX: case AB_LOG2:
    case AB_CUBE: case AB_CBRT:                 /* unit 041 C */
        return one_number_fn(c, op);
    case AB_TOPOL:
    case AB_TOREC: {
        /* Two numbers in, two out: x and y in X and Y become r and theta, or back (ours, unit
           004). LAST X keeps the old x, as for the other conversions (UG p.B-6). */
        abn a, b;
        abn_status st = op == AB_TOPOL ? fn_topol(&a, &b, &c->x.re, &c->y.re, angle_of(c))
                                       : fn_torec(&a, &b, &c->x.re, &c->y.re, angle_of(c));
        if (st == ABN_OVERFLOW) note_overflow(c);  /* a part at the largest number (unit 026) */
        else if (st != ABN_OK) return st;
        c->lastx = c->x;
        c->x = ab_real(a);
        c->y = ab_real(b);
        break;
    }
    case AB_RAND:
        /* RAND arrives like a recalled number (UG p.4-15). */
        place_in_x(c, fn_rand(&c->rng));
        break;
    case AB_SEED:
        /* SEED takes x as the seed and leaves the stack as it is (UG p.4-15; ours: X stays). */
        fn_seed(&c->rng, &c->x.re);
        break;
    case AB_CONST: {
        abn v;
        abn_status st = ab_const_value(&v, arg);
        if (st != ABN_OK) return st;
        place_in_x(c, v);   /* a constant arrives like a recalled number (UG p.4-8) */
        break;
    }
    case AB_STO: case AB_RCL:
    case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:
    case AB_RCLADD: case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV:
    case AB_XVAR: case AB_VIEW:
        return storage_op(c, op, arg);
    case AB_SIGMAPLUS:
    case AB_SIGMAMINUS: {
        /* Sigma+ / Sigma- add or remove the pair (x in X, y in Y; UG p.12-2): X becomes n, Y keeps
           y, x goes to LAST X (UG p.12-3, p.B-6) and lift is disabled (UG p.B-5). All six
           registers change or none does (ours, unit 006 rule 2). */
        bool over = false;
        abn_status st = st_accumulate(c->stat, &c->x.re, &c->y.re, op == AB_SIGMAMINUS, &over);
        if (st != ABN_OK) return st;
        if (over) note_overflow(c);             /* a register at the largest (unit 026, abacus #4198) */
        c->lastx = c->x;
        c->x = ab_real(c->stat[ST_N]);
        c->lift = false;
        return ABN_OK;
    }
    case AB_CLSIGMA:
        /* Clear the six statistics registers; neutral for lift (UG p.12-2, p.B-5). */
        for (int i = 0; i < AB_STAT_REGS; i++) c->stat[i] = abn_zero();
        return ABN_OK;
    case AB_XHAT:
    case AB_YHAT: {
        /* x-hat of the y in X, y-hat of the x in X (UG p.12-8); the input goes to LAST X
           (UG p.B-6). */
        abn r;
        abn_status st = op == AB_XHAT ? st_xhat(&r, c->stat, &c->x.re) : st_yhat(&r, c->stat, &c->x.re);
        /* an exact value rounded once: the engine's sign is the result's */
        if ((st = settle(c, st, &r, abn_overflow_negative())) != ABN_OK) return st;
        c->lastx = c->x;
        c->x = ab_real(r);
        break;
    }
    case AB_XBAR: case AB_YBAR: case AB_XWBAR: case AB_SX: case AB_SY: case AB_PSX: case AB_PSY:
    case AB_CORR: case AB_SLOPE: case AB_YINT: {
        /* A statistic arrives like a recalled number, one value in X (unit 006 rule 1: the guide
           is silent on lift); LAST X is untouched (not in UG p.B-6). STAT ERROR cases change
           nothing. */
        abn r;
        abn_status st;
        switch (op) {
        case AB_XBAR: st = st_mean(&r, c->stat, false); break;
        case AB_YBAR: st = st_mean(&r, c->stat, true); break;
        case AB_XWBAR: st = st_wmean(&r, c->stat); break;
        case AB_SX: st = st_sdev(&r, c->stat, false, false); break;
        case AB_SY: st = st_sdev(&r, c->stat, true, false); break;
        case AB_PSX: st = st_sdev(&r, c->stat, false, true); break;
        case AB_PSY: st = st_sdev(&r, c->stat, true, true); break;
        case AB_CORR: st = st_corr(&r, c->stat); break;
        case AB_SLOPE: st = st_slope(&r, c->stat); break;
        default: st = st_yint(&r, c->stat); break;
        }
        if ((st = settle(c, st, &r, abn_overflow_negative())) != ABN_OK) return st;    /* as x-hat */
        place_in_x(c, r);
        break;
    }
    case AB_NSUM: case AB_SUMX: case AB_SUMY: case AB_SUMX2: case AB_SUMY2: case AB_SUMXY:
        /* The summation registers, recalled (UG p.12-11). */
        place_in_x(c, c->stat[op - AB_NSUM]);
        break;
    case AB_DEC:
    case AB_HEX:
    case AB_OCT:
    case AB_BIN:
        /* the base mode changes only the display; neutral for lift (UG p.11-2, p.B-5) */
        c->base = op == AB_DEC ? 10 : op == AB_HEX ? 16 : op == AB_OCT ? 8 : 2;
        return ABN_OK;
    case AB_AND: case AB_XOR: case AB_OR: case AB_NOT: case AB_NAND: case AB_NOR:
        return logic(c, op);
    case AB_FDISP:
        /* Fraction-display mode on or off; neutral for lift (UG p.5-1, p.B-5) */
        c->fdisp = !c->fdisp;
        return ABN_OK;
    case AB_SF:
    case AB_CF:
        /* Flags 7 (Fraction display), 8 and 9 (its format) (UG p.14-10); others come with
           programming. Not in UG p.B-5's neutral list, so like any other they enable lift. */
        if (arg == 7) c->fdisp = op == AB_SF;
        else if (arg == 8) c->flag8 = op == AB_SF;
        else if (arg == 9) c->flag9 = op == AB_SF;
        else if (arg >= 0 && arg <= 11) {
            /* flags 0-4 the user's, 5, 6, 10, 11 system flags (UG p.14-9 to 14-12) kept for the
               units that give them meaning (unit 011) */
            if (op == AB_SF) c->flags = (uint16_t)(c->flags | (1u << arg));
            else c->flags = (uint16_t)(c->flags & ~(1u << arg));
        } else {
            return ABN_DOMAIN;
        }
        break;
    /* ---- keystroke programs from the keyboard (unit 011) ---- */
    case AB_PRGM:
        c->prgm = true;                         /* neutral (UG p.B-5) */
        return ABN_OK;
    case AB_XEQ: {
        /* XEQ label (nnn): runs from that line (UG p.13-10) with an empty return stack */
        int t, i;
        abn_status st = ind_target(c, arg, &t);
        if (st != ABN_OK) return st;
        i = prog_target(c, t);
        if (i < 0) return ABN_NONEXISTENT;
        c->rdepth = 0;
        c->stop = AB_STOP_NONE;
        c->pc = i;
        c->lift = true;
        return prog_run(c, c->budget);
    }
    case AB_GTO: {
        int t, i;                               /* the pointer only (UG p.13-22) */
        abn_status st = ind_target(c, arg, &t);
        if (st != ABN_OK) return st;
        i = prog_target(c, t);
        if (i < 0) return ABN_NONEXISTENT;
        c->pc = i;
        c->rdepth = 0;
        c->stop = AB_STOP_NONE;
        break;
    }
    case AB_GTOPOS:
        return prog_goto_pos(c, arg);
    case AB_RS:
        if (c->eqn_pending) {
            /* at an equation's prompt: X is stored and the next prompt or the value follows; lift
               is enabled so the value stays on the stack (UG p.6-14) */
            c->var[c->stop_var] = c->x;
            c->lift = true;
            c->eqn_next++;
            return eqn_step(c);
        }
        return prog_resume(c, c->budget);       /* neutral itself (UG p.B-5) */
    case AB_RTN:
        c->pc = AB_PC_TOP;                      /* from the keyboard (ours: the guide does not say) */
        c->rdepth = 0;
        break;
    case AB_SST:
        return prog_resume(c, 1);               /* executes the line (UG p.13-11) */
    case AB_BST:
        c->pc = c->pc < 0 ? c->prog_len - 1 : c->pc - 1;
        return ABN_OK;
    case AB_LBL:
    case AB_PSE:
        return ABN_OK;                          /* only in programs (UG p.G-10, G-11) */
    case AB_BEEP:
        /* one tone for the device, the stack and lift untouched (ours, unit 027 rule 6; neutral
           as PSE, UG p.B-5): from the keys and in programs alike */
        c->beeps++;
        return ABN_OK;
    case AB_CLPGM:
        prog_clear(c);
        return ABN_OK;
    case AB_TEST:
    case AB_FS: {
        bool t;
        abn_status st = prog_test(c, op, arg, &t);
        if (st != ABN_OK) return st;
        c->yes = t;                             /* YES / NO (UG p.14-7), shown as a VIEW is: until the next key,
                                                   C or backspace only clearing it (abacus #8497) */
        c->viewing = true;
        c->view_id = AB_VIEW_TEST;
        break;
    }
    case AB_ISG:
    case AB_DSE: {
        bool skip;
        abn_status st = loop_op(c, op, arg, &skip);
        if (st != ABN_OK) return st;
        break;
    }
    case AB_INPUT:
        return ABN_OK;                          /* only in programs (UG p.G-9) */
    case AB_CANCEL:
        return cancel_key(c);
    case AB_CASD: case AB_CASSIMP: case AB_CASEXP: case AB_CASCOL: case AB_CASSUB:
    case AB_CASSOLVE: case AB_CASINT: case AB_CASFACT: case AB_CASDEF:
    case AB_CASQUOT: case AB_CASGCD: case AB_CASCANCL: case AB_CASSUM:
        return c->stu ? ABN_NO_EQN : ABN_STU_ONLY;     /* CAS 004, 048, 056, 070: from Equation mode's list only */
    case AB_EQN:
        c->eqn_mode = true;                     /* neutral (UG p.B-5) */
        if (!ab_mode35(c) && eqn_builtin(c, c->eqn_cur)) c->eqn_cur = -1;     /* 33s mode: no built-ins (041 A4) */
        return ABN_OK;
    case AB_KEQ:
    case AB_KPAREN:
    case AB_KLEFT:
    case AB_KRIGHT:
    case AB_KINF:                               /* (070) */
        return ABN_OK;                          /* only in Equation mode */
    case AB_FN: {
        abn_status st = fn_op(c, arg);          /* FN= enables lift (not in p.B-5's lists) */
        if (st != ABN_OK) return st;
        break;
    }
    case AB_SOLVE:
        return solve_op(c, arg);
    case AB_INTEG:
        return integ_op(c, arg);
    case AB_LIT:
    case AB_EQLINE:
        return ABN_DOMAIN;
    case AB_SETC: {
        /* /c from |IP(x)| (UG p.5-5): 1 recalls the maximum denominator into X; 0 or anything
           above 4095 resets it to 4095; otherwise it is set. Setting turns Fraction display on.
           LAST X is untouched. After setting, X keeps the number and lift is disabled, so the
           next number replaces it (ours, unit 007: the guide's examples show Y = 0 there). */
        abn t, big = abn_from_int(4095), one = abn_from_int(1);
        if (abn_trunc(&t, &c->x.re) != ABN_OK) return ABN_DOMAIN;
        t = abn_abs(&t);
        if (abn_cmp(&t, &one) == 0) {
            c->x = ab_real(abn_from_int((int32_t)c->frac_c));
            break;
        }
        if (abn_is_zero(&t) || abn_cmp(&t, &big) > 0) {
            c->frac_c = 4095;
        } else {
            int v;
            bool ng;
            char dg[ABN_DIGITS + 1];
            int ld;
            abn_digits(&t, &ng, dg, &ld);
            v = 0;
            for (int i = 0; i <= ld; i++) v = v * 10 + (i < (int)strlen(dg) ? dg[i] - '0' : 0);
            c->frac_c = (uint32_t)v;
        }
        c->fdisp = true;
        c->lift = false;
        return ABN_OK;
    }
    case AB_RCLC:
        /* /c recalled like any number (ours, unit 007: the guide recalls with 1 /c) */
        place_in_x(c, abn_from_int((int32_t)c->frac_c));
        break;
    case AB_MODEL:
        /* the 33s or the 35s behaviour where Michael's machines differ (unit 017 rule 1);
           neutral for lift, as the other mode settings (UG p.B-5) */
        if (arg != 33 && arg != 35 && arg != AB_MODEL_STU) return ABN_DOMAIN;
        if (arg != AB_MODEL_STU) alg_leave(c);      /* ALG is STU's: 33s or 35s returns to RPN (rule 2) */
        c->stu = arg == AB_MODEL_STU;               /* STU behaves as 35s (decision 51, rule 1) */
        c->m35 = arg != 33;
        return ABN_OK;
    case AB_CASCPLX:                            /* unit 076: SOLVE's complex roots on or off (CAS3's CPLX); neutral
                                                   for lift as the other settings; STU's, as every CAS key (fresh eyes) */
        if (!c->stu) return ABN_STU_ONLY;
        c->cas_cplx = !c->cas_cplx;
        return ABN_OK;
    case AB_ENTRYM:
        /* RPN or ALG (unit 029 rule 2: ALG in STU only); neutral for lift as the other settings.
           RPN to ALG sets ANS to X (ours, rule 6); ALG to RPN puts ANS in X (Michael's ruling) */
        if (arg == 1) {
            if (!c->stu) return ABN_DOMAIN;
            if (c->alg) return ABN_OK;
            c->ans = c->x;
            c->alg = true;
            c->eqn_typing = true;
            c->eqn_edit = -1;
            alg_new_line(c, false);
            return ABN_OK;
        }
        if (arg != 0) return ABN_DOMAIN;
        alg_leave(c);
        return ABN_OK;
    case AB_ANS:
    case AB_HUP:
    case AB_HDOWN:
    case AB_TUP:
    case AB_TDOWN:
    case AB_GLEFT:
    case AB_GRIGHT:
    case AB_GZOOM: case AB_GMOVE: case AB_GTRACE: case AB_GMARK: case AB_GFACT: case AB_GFCN:    /* 040 */
        return ABN_OK;                              /* the line's, the table's, the graph's own keys: nothing */
    case AB_GRAPH:
        return graph_go(c);
    case AB_GAUTO:                                  /* Y fitted again (033 rule 2) */
        c->gr_auto = true;
        c->lift = true;
        return ABN_OK;
    case AB_XMIN: case AB_XMAX: case AB_YMIN: case AB_YMAX:
        if (c->x.kind != AB_REAL) return ABN_DOMAIN;
        graph_set(c, op, &c->x.re);
        c->lift = true;
        return ABN_OK;
    case AB_TABLE:
        return table_go(c);
    case AB_TVAR:                                   /* the variable: the start follows it again (answer 3) */
        if (arg < AB_VAR_A || arg > AB_VAR_A + 25) return ABN_DOMAIN;
        c->tbl_var = arg - AB_VAR_A;
        c->tbl_start_set = false;
        c->lift = true;                             /* as STO (answer: START and STEP take X as STO does) */
        return ABN_OK;
    case AB_TSTART:
    case AB_TSTEP:
        if (c->x.kind != AB_REAL) return ABN_DOMAIN;
        if (op == AB_TSTART) {
            c->tbl_start = c->x.re;
            c->tbl_start_set = true;
        } else {
            c->tbl_step = c->x.re;
        }
        c->lift = true;
        return ABN_OK;
    case AB_CLVARS:
        /* Clear the direct variables A..Z (UG p.3-5); neutral for stack lift (UG p.B-5). The
           unnamed indirect variables have their own clearing, CLVARx. */
        for (int i = 0; i < 27; i++) c->var[i] = ab_real(abn_zero());   /* and i (unit 025) */
        return ABN_OK;
    case AB_CLVARX:
        /* Clear the indirect variables whose address is greater than arg (UG p.1-5); the defined
           range then ends at the last nonzero one, as a store of 0 leaves it (UG p.14-24). Not in
           p.B-5's neutral list, so it enables lift. */
        if (arg < 0 || arg > AB_IND_MAX) return ABN_DOMAIN;
        for (int i = arg + 1; i <= c->ind_top; i++) c->ind[i] = abn_zero();
        if (c->ind_top > arg) c->ind_top = arg;
        while (c->ind_top >= 0 && abn_is_zero(&c->ind[c->ind_top])) c->ind_top--;
        break;
    case AB_CLALL:
        /* CLEAR ALL: data cleared, modes and settings kept (UG p.1-29, p.B-3); the device asks
           first (UG p.1-5) */
        ab_clear_all(c);
        return ABN_OK;
    case AB_CLEQN: {
        /* Clear the equations (UG p.1-5: CLEAR's EQN in Equation mode, CLR EQN?), the built-in
           solvers kept as CLEAR ALL keeps them (UG p.6-6); EQN LIST TOP; FN= kept only on a
           built-in. Neutral for lift (ours: it clears no number). */
        int n = 0, fn = -1;
        for (int i = 0; i < c->eqn_count; i++) {
            if (c->eqn[i][0] != '\x01') continue;
            if (i == c->fn_eqn) fn = n;
            if (n != i) {
                strcpy(c->eqn[n], c->eqn[i]);
                c->eqn_model[n] = c->eqn_model[i];
            }
            n++;
        }
        c->eqn_count = n;
        memset(c->eqn_note, 0, sizeof c->eqn_note);     /* unit 057: only the built-ins are left */
        c->fn_eqn = fn;
        c->eqn_cur = -1;
        c->eqn_typing = false; c->cas_sub = c->cas_def = c->cas_pop = 0;
        c->eqn_edit = -1;
        c->ebuf[0] = '\0';
        c->ecur = 0;
        c->eqn_pending = c->eqn_nvars = c->eqn_next = 0;
        return ABN_OK;
    }
    case AB_FRACFMT:
        /* The fraction format as its two flags at once (UG p.5-6; unit 019 rule 2): precise
           clears 8; factors sets 8 and clears 9; fixed sets both. A display setting, so neutral
           for lift as FDISP and the formats are (UG p.B-5), where SF and CF enable it. */
        if (arg < 0 || arg > 2) return ABN_DOMAIN;
        c->flag8 = arg > 0;
        if (arg > 0) c->flag9 = arg == 2;
        return ABN_OK;
    case AB_SETFIX:
    case AB_SETSCI:
    case AB_SETENG:
    case AB_SETALL:
        /* Display settings change no number and leave stack lift alone (UG p.1-21, p.B-5). */
        if (arg < 0 || arg > AB_FMT_N_MAX) return ABN_DOMAIN;
        c->disp_mode = op == AB_SETFIX ? AB_FIX : op == AB_SETSCI ? AB_SCI : op == AB_SETENG ? AB_ENG : AB_ALL;
        c->fdisp = false;   /* choosing a decimal format ends Fraction-display mode (UG p.5-2) */
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

/* ---- program memory queries (unit 011) ---- */

void ab_prog_addr(const ab_calc *c, int i, char *buf)
{
    if (i < 0 || i >= c->prog_len) {
        strcpy(buf, "TOP");
        return;
    }
    int l = prog_owner(c, i);
    int n = l < 0 ? i + 1 : i - c->labelpos[l] + 1;
    if (l < 0) {
        buf[0] = (char)('0' + n / 1000 % 10);
        buf[1] = (char)('0' + n / 100 % 10);
    } else {
        buf[0] = (char)('A' + l);
        buf[1] = (char)('0' + n / 100 % 10);
    }
    buf[2] = (char)('0' + n / 10 % 10);
    buf[3] = (char)('0' + n % 10);
    buf[4] = '\0';
}

int ab_prog_find(const ab_calc *c, const char *addr)
{
    if (strlen(addr) != 4) return -1;
    if (addr[0] >= 'A' && addr[0] <= 'Z') {
        int n = (addr[1] - '0') * 100 + (addr[2] - '0') * 10 + (addr[3] - '0');
        return n < 1 ? -1 : prog_target(c, (addr[0] - 'A') * 1000 + n);
    }
    int n = (addr[0] - '0') * 1000 + (addr[1] - '0') * 100 + (addr[2] - '0') * 10 + (addr[3] - '0');
    return n >= 1 && n <= c->prog_len && prog_owner(c, n - 1) < 0 ? n - 1 : -1;
}

bool ab_flag(const ab_calc *c, int n)
{
    if (n == 7) return c->fdisp;
    if (n == 8) return c->flag8;
    if (n == 9) return c->flag9;
    return n >= 0 && n <= 11 && (c->flags >> n) & 1u;
}

void ab_stop_text(const ab_calc *c, char *buf)
{
    static const char *const NAMES[] = {"NONE", "DONE", "STOP", "INPUT", "VIEW", "PSE", "CANCELLED",
                                        "ERROR", "BUDGET", "MSG", "SLICE"};
    static const char *const STATS[] = {"n", "SX", "SY", "SX2", "SY2", "SXY"};
    char v[16] = "";
    int id = c->stop_var;
    bool named = c->stop == AB_STOP_INPUT || c->stop == AB_STOP_VIEW || (c->stop == AB_STOP_PSE && id != -1);
    if (named) {
        if (id >= 0 && id < 26) {
            v[0] = (char)('A' + id);
            v[1] = '\0';
        } else if (id == 26) {
            snprintf(v, sizeof v, "i");          /* the 33s's i (unit 025) */
        } else if (id >= 1000) {
            snprintf(v, sizeof v, "#%d", id - 1000);
        } else if (id <= -27 && id >= -32) {
            snprintf(v, sizeof v, "%s", STATS[-id - 27]);
        }
    }
    if (c->stop == AB_STOP_PSE && id == -2) snprintf(buf, 32, "PSE:MSG");   /* a message shown */
    else if (named) snprintf(buf, 32, "%s:%s", NAMES[c->stop], v);
    else snprintf(buf, 32, "%s", NAMES[c->stop]);
}

const char *ab_eqn_mark(const ab_calc *c, const char *text, int model)
{
    bool typed_stu = model == AB_MODEL_STU;
    if (typed_stu == c->stu || !eqn_has_chain((ab_calc *)c, text, model)) return "";   /* (a check only: unchanged) */
    return typed_stu ? " \xc2\xb7STU" : model == 33 ? " \xc2\xb7" "33" : " \xc2\xb7" "35";
}

/* One number's SHOW text: a minus for a negative, then all ABN_DIGITS significant digits, trailing
   zeros kept; zero is ABN_DIGITS zeros. */
static void show_digits(const abn *a, char *out)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead, n = 0;
    abn_digits(a, &neg, d, &lead);
    if (neg) out[n++] = '-';
    int k = (int)strlen(d);
    for (int i = 0; i < ABN_DIGITS; i++) out[n++] = i < k ? d[i] : '0';
    out[n] = '\0';
}

int ab_show_text(const ab_calc *c, char out[2][AB_SHOW_LEN])
{
    const ab_val *v = &c->x;
    out[0][0] = out[1][0] = '\0';
    /* nothing while a number is typed (the entry line shows it as typed), while a VIEW display or
       Equation mode shows something else, and in a base mode (the integer is shown whole): answers
       6, 5 and 8 to #4291 */
    if (c->entry.active || c->cpart || c->vlen || c->viewing || c->base != 10) return 0;
    if (c->eqn_typing && !c->alg) return 0;     /* an equation being typed, in program entry too (answer 5) */
    if (c->prgm) {                              /* the number on the line shown (35s p.G-13); else nothing */
        if (c->pc < 0 || c->pc >= c->prog_len || c->prog[c->pc].op != AB_LIT) return 0;
        int i = c->prog[c->pc].arg;
        if (i < 0 || i >= AB_PROG_LITS) return 0;
        v = &c->lit[i];
    } else if (c->eqn_mode) {
        return 0;
    } else if (c->alg) {                        /* STU's ALG: what X's place shows (as 029b's STO) */
        v = c->hist_at >= 0 && c->hist_at < c->hist_n ? &c->hist_val[c->hist_at] : &c->ans;
    }
    if (v->kind == AB_REAL) {
        show_digits(&v->re, out[0]);
        return 1;
    }
    if (v->kind == AB_COMPLEX) {                /* the real part, then the imaginary (answer 3); in the
                                                   polar form r and the angle, what X's place shows (030b) */
        abn a = v->re, b = v->im;
        if (c->cpolar) {
            fn_angle m = c->angle == AB_ANGLE_RAD ? FN_RAD : c->angle == AB_ANGLE_GRAD ? FN_GRAD : FN_DEG;
            if (fn_topol(&a, &b, &v->re, &v->im, m) != ABN_OK) return 0;
        }
        show_digits(&a, out[0]);
        show_digits(&b, out[1]);
        return 2;
    }
    return 0;                                   /* a vector: nothing for now (answer 3) */
}

static abn_status table_row(ab_calc *c, int32_t k, abn *x, ab_val *y, bool pure);

abn_status ab_table_row(ab_calc *c, int32_t k, abn *x, ab_val *y)
{
    return table_row(c, k, x, y, true);
}

/* A row; pure (the screen's, the harness's): everything an evaluation may touch put back; not pure
   (ENTER's): as XEQ, an overflow sets flag 6 (unit 026), the variable still restored (rule 3). */
static abn_status table_row(ab_calc *c, int32_t k, abn *x, ab_val *y, bool pure)
{
    abn kk = abn_from_int(k), p;
    int pos, v = c->tbl_v, e = c->tbl_eq;
    abn_status st;
    memset(y, 0, sizeof *y);
    *x = c->tbl_start;
    if (v < 0 || v >= 26 || e < 0 || e >= c->eqn_count) return ABN_DOMAIN;
    /* what an evaluation may touch, put back after a pure row, so a row (the screen draws them)
       changes nothing (rule 3) */
    ab_val keep = c->var[v], ex = c->err_x;
    uint16_t flags = c->flags;
    uint64_t rng = c->rng;
    int eo = c->err_op, ea = c->err_arg, eac = c->err_active, ovf = c->ovf;
    bool es = c->err_set, em = c->err_m35;
    abn_status est = c->err_status;
    /* start + k x step, each rounded once (not repeated addition: the drift mutant, 032); an overflow
       gives the largest, flag 6 set, an underflow 0 (unit 026: review F3) */
    st = settle(c, abn_mul(&p, &kk, &c->tbl_step), &p, abn_overflow_negative());
    if (st == ABN_OK) st = settle(c, abn_add(x, &c->tbl_start, &p), x, abn_overflow_negative());
    /* the row is the equation's value there, as XEQ gives it (rule 6), the variable set for it alone */
    if (st == ABN_OK) {
        c->var[v] = ab_real(*x);
        st = eqn_eval(c, c->eqn[e], c->eqn_model[e], false, false, y, &pos);
    }
    c->var[v] = keep;
    if (!pure) return st;
    c->flags = flags;
    c->rng = rng;
    c->err_op = eo, c->err_arg = ea, c->err_active = eac, c->ovf = ovf, c->err_set = es, c->err_m35 = em;
    c->err_status = est, c->err_x = ex;
    return st;
}

/* A key while the table is open (unit 032 answer 5): up and down a row; ENTER takes the row's value as a
   keyed number would arrive and leaves (an erring row: its error, the table kept); C leaves; nothing
   else. */
static abn_status table_key(ab_calc *c, ab_op op)
{
    abn x;
    ab_val y;
    abn_status st;
    switch (op) {
    case AB_TUP:
        if (c->tbl_k > INT32_MIN + 3) c->tbl_k--;
        return ABN_OK;
    case AB_TDOWN:
        if (c->tbl_k < INT32_MAX) c->tbl_k++;
        return ABN_OK;
    case AB_ENTER:
        if ((st = table_row(c, c->tbl_k, &x, &y, false)) != ABN_OK) return st;
        c->tbl_on = false;
        if (c->alg) {                           /* on STU's line: into ANS and the line, the stack untouched
                                                   (unit 032b) */
            c->ans = y;
            if (y.kind == AB_REAL) num_text(&y.re, c->ebuf, sizeof c->ebuf);
            else strcpy(c->ebuf, "ANS");
            c->ecur = (int)strlen(c->ebuf);
            c->alg_shown = true;
            c->hist_at = -1;
            return ABN_OK;
        }
        place_val(c, y);
        c->lift = true;
        return ABN_OK;
    case AB_CANCEL:
        c->tbl_on = false;
        return ABN_OK;
    default:
        return ABN_OK;
    }
}

/* A number as the line reads it back exactly (unit 032b): every significant digit, plainly when its leading
   digit is from 10^-6 to 10^33, else d.ddd E n; zero "0". */
static void num_text(const abn *a, char *out, int size)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead, n = 0, k;
    abn_digits(a, &neg, d, &lead);
    k = (int)strlen(d);
    if (k == 0) {
        snprintf(out, (size_t)size, "0");
        return;
    }
    char t[96];
    if (neg) t[n++] = '-';
    if (lead >= 0 && lead < ABN_DIGITS) {
        for (int i = 0; i <= lead; i++) t[n++] = i < k ? d[i] : '0';
        if (k > lead + 1) {
            t[n++] = '.';
            for (int i = lead + 1; i < k; i++) t[n++] = d[i];
        }
    } else if (lead < 0 && lead >= -6) {
        t[n++] = '0';
        t[n++] = '.';
        for (int i = 0; i < -lead - 1; i++) t[n++] = '0';
        for (int i = 0; i < k; i++) t[n++] = d[i];
    } else {
        t[n++] = d[0];
        if (k > 1) {
            t[n++] = '.';
            for (int i = 1; i < k; i++) t[n++] = d[i];
        }
        n += snprintf(t + n, sizeof t - (size_t)n, "E%d", lead);
    }
    t[n] = '\0';
    snprintf(out, (size_t)size, "%s", t);
}

/* GO (unit 032): the equation shown, or last shown, against its variable from its start. */
static abn_status table_go(ab_calc *c)
{
    int e = c->eqn_cur, vars[27];
    abn_status st;
    if (!c->stu) return ABN_STU_ONLY;           /* an STU feature (decision 52; answer 1) */
    if (c->eqn_mode && c->eqn_typing && (st = eqn_store_typed(c)) != ABN_OK) return st;
    e = c->eqn_cur;
    if (e < 0 || e >= c->eqn_count || c->eqn[e][0] == '\x01') return ABN_NO_EQN;   /* answer 2 */
    if (c->eqn_note[e] == AB_NOTE_WHERE) return ABN_CAS_WHERE;                  /* a sum's condition (070) */
    int v = c->tbl_var;
    if (v < 0) {                                /* the equation's first variable (answer 3) */
        int n = eqn_vars(c->eqn[e], false, false, c->eqn_model[e], vars);
        int i = 0;
        while (i < n && vars[i] >= 26) i++;
        if (i == n) return ABN_NO_VAR;
        v = vars[i];
    }
    if (!c->tbl_start_set) {                    /* the variable's value when GO is pressed */
        if (c->var[v].kind != AB_REAL) return ABN_DOMAIN;
        c->tbl_start = c->var[v].re;
    }
    c->tbl_v = v;
    c->tbl_eq = e;
    c->tbl_k = 0;
    c->tbl_on = true;
    return ABN_OK;
}

/* ---- GRAPH (unit 033): the model ---- */

/* The equation e at x for variable v: rule 1's form (V=expr with V not in expr: expr; any other L=R:
   L-R; an expression: its value), evaluated with the variable set for it alone; pure: everything an
   evaluation may touch put back (rule 7, as TABLE's rule 3). */
static abn_status eval_at(ab_calc *c, int e, int v, const abn *x, ab_val *y, bool pure)
{
    int pos, vars[27];
    const char *t = c->eqn[e];
    int av = eqn_assign_var(t, false);
    bool rhs = false;
    if (av >= 0) {                              /* V=expr: is V in expr? */
        int n = eqn_vars(t, true, false, c->eqn_model[e], vars);
        rhs = true;
        for (int i = 0; i < n; i++) rhs = rhs && vars[i] != av;
    }
    ab_val keep = c->var[v], ex = c->err_x;
    uint16_t flags = c->flags;
    uint64_t rng = c->rng;
    int eo = c->err_op, ea = c->err_arg, eac = c->err_active, ovf = c->ovf;
    bool es = c->err_set, em = c->err_m35;
    abn_status est = c->err_status;
    memset(y, 0, sizeof *y);
    c->var[v] = ab_real(*x);
    abn_status st = eqn_eval(c, t, c->eqn_model[e], rhs, false, y, &pos);
    c->var[v] = keep;
    if (!pure) return st;
    c->flags = flags;
    c->rng = rng;
    c->err_op = eo, c->err_arg = ea, c->err_active = eac, c->ovf = ovf, c->err_set = es, c->err_m35 = em;
    c->err_status = est, c->err_x = ex;
    return st;
}

/* Column col's x: d = XMAX - XMIN, t = col x d, u = t / 399, x = XMIN + u, each rounded once (answer (a)). */
static abn_status graph_x(ab_calc *c, int col, abn *x)
{
    abn d, t, u, k = abn_from_int(col), n = abn_from_int(AB_GRAPH_W - 1);
    uint16_t flags = c->flags;
    int ovf = c->ovf;
    abn_status st = settle(c, abn_sub(&d, &c->gr_xmax, &c->gr_xmin), &d, abn_overflow_negative());
    if (st == ABN_OK) st = settle(c, abn_mul(&t, &k, &d), &t, abn_overflow_negative());
    if (st == ABN_OK) st = settle(c, abn_div(&u, &t, &n), &u, abn_overflow_negative());
    if (st == ABN_OK) st = settle(c, abn_add(x, &c->gr_xmin, &u), x, abn_overflow_negative());
    c->flags = flags;                           /* (an x is part of drawing: nothing set) */
    c->ovf = ovf;
    return st;
}

unsigned long ab_graph_sample_calls;

/* fast: a pixel's sample, the engine's path for unit 044's functions (rule 5, abacus #4893); a value read (the trace's
   readout, F(X), ENTER, a zoom's centre) is correctly rounded. */
static abn_status graph_sample(ab_calc *c, int col, abn *x, ab_val *y, bool pure, bool fast)
{
    ab_graph_sample_calls++;
    abn_status st;
    *x = abn_zero();
    memset(y, 0, sizeof *y);
    if (c->gr_eq < 0 || c->gr_eq >= c->eqn_count || c->gr_v < 0 || c->gr_v >= 26 || col < 0 || col >= AB_GRAPH_W)
        return ABN_DOMAIN;
    if ((st = graph_x(c, col, x)) != ABN_OK) return st;
    bool was = fn_set_fast(fast);
    st = eval_at(c, c->gr_eq, c->gr_v, x, y, pure);
    fn_set_fast(was);
    if (st == ABN_OK && y->kind != AB_REAL) st = ABN_DOMAIN;    /* (a complex or vector value: not plotted) */
    return st;
}

abn_status ab_graph_sample(ab_calc *c, int col, abn *x, ab_val *y)
{
    return graph_sample(c, col, x, y, true, true);
}

static void clamp_big(abn_status st, abn *r, bool neg)
{
    if (st == ABN_OVERFLOW) {
        abn m = abn_largest();
        *r = neg ? abn_neg(&m) : m;
    } else if (st == ABN_UNDERFLOW) {
        *r = abn_zero();
    }
}

/* One sample into the fit's range: errors and overflows left out (answer 2). */
static void fit_take(const abn *y, bool ok, abn *lo, abn *hi, bool *any)
{
    abn big = abn_largest(), a = abn_abs(y);
    if (!ok || abn_cmp(&a, &big) == 0) return;
    if (!*any || abn_cmp(y, lo) < 0) *lo = *y;
    if (!*any || abn_cmp(y, hi) > 0) *hi = *y;
    *any = true;
}

static void fit_finish(abn *lo, abn *hi, bool any);

void ab_graph_fit(const abn *y, const bool *ok, int n, abn *lo, abn *hi)
{
    bool any = false;
    for (int i = 0; i < n; i++) fit_take(&y[i], ok[i], lo, hi, &any);
    fit_finish(lo, hi, any);
}

/* The fit from the samples' range (rule 2): none, -10..10; a constant, plus and minus 1 (or 5%); else 5% of the
   span each way. */
static void fit_finish(abn *lo, abn *hi, bool any)
{
    abn m, span, p05;
    if (!any) {                                 /* no finite sample: -10..10 (answer 2) */
        *lo = abn_from_int(-10);
        *hi = abn_from_int(10);
        return;
    }
    if (abn_cmp(lo, hi) == 0) {                 /* a constant: +-1, or 5% of it where 1 is lost to rounding
                                                   (review F6: 1E40) */
        abn one = abn_from_int(1), c0 = *lo, m, p05, mag = abn_abs(&c0);
        clamp_big(abn_sub(lo, &c0, &one), lo, true);
        clamp_big(abn_add(hi, &c0, &one), hi, false);
        if (abn_cmp(lo, hi) == 0 || abn_cmp(lo, &c0) == 0 || abn_cmp(hi, &c0) == 0) {
            abn_from_text(&p05, "0.05");
            clamp_big(abn_mul(&m, &mag, &p05), &m, false);
            clamp_big(abn_sub(lo, &c0, &m), lo, true);
            clamp_big(abn_add(hi, &c0, &m), hi, false);
        }
        return;
    }
    abn_from_text(&p05, "0.05");
    clamp_big(abn_sub(&span, hi, lo), &span, false);    /* (huge finite values: the window clamps to the
                                                           largest, as an overflow does, unit 026) */
    clamp_big(abn_mul(&m, &span, &p05), &m, false);
    clamp_big(abn_sub(lo, lo, &m), lo, true);
    clamp_big(abn_add(hi, hi, &m), hi, false);
}

void ab_graph_window(ab_calc *c, abn *ymin, abn *ymax)
{
    bool any = false;                           /* (the range as the samples come: no arrays, unit 040, which
                                                   brought this onto the device and its 6.8 KB of statics) */
    if (!c->gr_auto) {
        *ymin = c->gr_ymin;
        *ymax = c->gr_ymax;
        return;
    }
    if (c->gr_on && c->gr_fit_ok) {             /* the open graph's fit, known (040 review) */
        *ymin = c->gr_fit_lo;
        *ymax = c->gr_fit_hi;
        return;
    }
    for (int i = 0; i < AB_GRAPH_W; i++) {
        abn x;
        ab_val v;
        fit_take(&v.re, graph_sample(c, i, &x, &v, true, true) == ABN_OK, ymin, ymax, &any);
    }
    fit_finish(ymin, ymax, any);
    if (c->gr_on) c->gr_fit_ok = true, c->gr_fit_lo = *ymin, c->gr_fit_hi = *ymax;
}

void ab_graph_fit_known(ab_calc *c, const abn *lo, const abn *hi)
{
    if (!c->gr_on || !c->gr_auto) return;
    c->gr_fit_ok = true;
    c->gr_fit_lo = *lo;
    c->gr_fit_hi = *hi;
}

/* p rounded to an integer, half even, as an int clamped to lo..hi (an integer's digits read back). */
static int round_clamped(const abn *p, int lo, int hi)
{
    abn f, fr, half, one = abn_from_int(1), r;
    if (abn_floor(&f, p) != ABN_OK) return abn_is_negative(p) ? lo : hi;
    abn_sub(&fr, p, &f);
    abn_from_text(&half, "0.5");
    int c = abn_cmp(&fr, &half);
    r = f;
    if (c > 0) abn_add(&r, &f, &one);
    else if (c == 0) {                          /* a tie: to the even integer */
        abn two = abn_from_int(2), q, qf;
        abn_div(&q, &f, &two);
        abn_floor(&qf, &q);
        if (abn_cmp(&q, &qf) != 0) abn_add(&r, &f, &one);
    }
    abn lo_n = abn_from_int(lo), hi_n = abn_from_int(hi);
    if (abn_cmp(&r, &lo_n) <= 0) return lo;
    if (abn_cmp(&r, &hi_n) >= 0) return hi;
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead, v = 0;
    abn_digits(&r, &neg, d, &lead);
    for (int i = 0; i <= lead; i++) v = v * 10 + (i < (int)strlen(d) ? d[i] - '0' : 0);
    return neg ? -v : v;
}

int ab_graph_row(const abn *y, const abn *ymin, const abn *ymax)
{
    abn a, b, q, p, n = abn_from_int(AB_GRAPH_H - 1);
    abn_status st = abn_sub(&a, ymax, y);       /* a = YMAX - y; b = YMAX - YMIN; q = a / b; p = q x 184 */
    if (st == ABN_OVERFLOW) return abn_overflow_negative() ? -1 : AB_GRAPH_H;  /* y far above: -1 (review F2) */
    if (st == ABN_UNDERFLOW) a = abn_zero();    /* (review F5) */
    st = abn_sub(&b, ymax, ymin);
    if (st == ABN_OVERFLOW) {                   /* a window past the range: its span the largest (review F6) */
        b = abn_largest();
        if (abn_overflow_negative()) b = abn_neg(&b);
    } else if (st == ABN_UNDERFLOW) {
        b = abn_zero();
    }
    if (abn_is_zero(&b)) return -1;             /* (a negative b, YMIN above YMAX: the formula flips the plot) */
    st = abn_div(&q, &a, &b);
    if (st == ABN_OVERFLOW) return abn_is_negative(&a) ? -1 : AB_GRAPH_H;
    if (st == ABN_UNDERFLOW) q = abn_zero();
    st = abn_mul(&p, &q, &n);
    if (st == ABN_OVERFLOW) return abn_is_negative(&q) ? -1 : AB_GRAPH_H;
    if (st == ABN_UNDERFLOW) p = abn_zero();
    return round_clamped(&p, -1, AB_GRAPH_H);
}

bool ab_graph_join(bool prev_ok, int prev_row, int row, int *lo, int *hi)
{
    int a = row, b = row;
    if (prev_ok && !((prev_row == -1 && row == AB_GRAPH_H) || (prev_row == AB_GRAPH_H && row == -1))) {
        if (prev_row < row) a = prev_row + 1;   /* row and every row strictly between (answer 1) */
        else if (prev_row > row) b = prev_row - 1;
    }
    if (a < 0) a = 0;
    if (b > AB_GRAPH_H - 1) b = AB_GRAPH_H - 1;
    *lo = a;
    *hi = b;
    return a <= b;
}

bool ab_graph_column(ab_calc *c, int col, const abn *ymin, const abn *ymax, int *lo, int *hi)
{
    abn x;
    ab_val y, py;
    *lo = *hi = 0;
    if (graph_sample(c, col, &x, &y, true, true) != ABN_OK) return false;
    int r = ab_graph_row(&y.re, ymin, ymax), pr = 0;
    bool pok = col > 0 && graph_sample(c, col - 1, &x, &py, true, true) == ABN_OK;
    if (pok) pr = ab_graph_row(&py.re, ymin, ymax);
    return ab_graph_join(pok, pr, r, lo, hi);
}

void ab_graph_axes(const ab_calc *c, const abn *ymin, const abn *ymax, int *xrow, int *ycol)
{
    abn zero = abn_zero(), a, b, q, p, n = abn_from_int(AB_GRAPH_W - 1);
    int r = ab_graph_row(&zero, ymin, ymax);
    *xrow = r >= 0 && r <= AB_GRAPH_H - 1 ? r : -1;
    *ycol = -1;                                 /* a = 0 - XMIN; b = XMAX - XMIN; q = a / b; p = q x 399 */
    if (abn_sub(&a, &zero, &c->gr_xmin) != ABN_OK || abn_sub(&b, &c->gr_xmax, &c->gr_xmin) != ABN_OK ||
        abn_is_zero(&b) || abn_div(&q, &a, &b) != ABN_OK || abn_mul(&p, &q, &n) != ABN_OK)
        return;
    int k = round_clamped(&p, -1, AB_GRAPH_W);
    if (k >= 0 && k <= AB_GRAPH_W - 1) *ycol = k;
}

int ab_graph_form(const ab_calc *c, int e)
{
    int vars[27];
    if (e < 0 || e >= c->eqn_count) return 0;
    const char *t = c->eqn[e];
    int av = eqn_assign_var(t, false);
    if (av >= 0) {
        int n = eqn_vars(t, true, false, c->eqn_model[e], vars);
        bool in = false;
        for (int i = 0; i < n; i++) in = in || vars[i] == av;
        if (!in) return av + 1;
    }
    return strchr(t, '=') ? -1 : 0;
}

/* ---- unit 040: the plot screen's cursor and zooms (50g guide p.12-47 to 12-49; the formulas pinned in
   work/040-vectors.txt's header) ---- */

/* A row's y: d = YMAX - YMIN, t = row d, u = t / 184, y = YMAX - u, each rounded once (an overflow the largest, as
   graph_x; nothing set). */
static abn_status graph_rowy(ab_calc *c, int row, const abn *ymin, const abn *ymax, abn *y)
{
    abn d, t, u, k = abn_from_int(row), n = abn_from_int(AB_GRAPH_H - 1);
    uint16_t flags = c->flags;
    int ovf = c->ovf;
    abn_status st = settle(c, abn_sub(&d, ymax, ymin), &d, abn_overflow_negative());
    if (st == ABN_OK) st = settle(c, abn_mul(&t, &k, &d), &t, abn_overflow_negative());
    if (st == ABN_OK) st = settle(c, abn_div(&u, &t, &n), &u, abn_overflow_negative());
    if (st == ABN_OK) st = settle(c, abn_sub(y, ymax, &u), y, abn_overflow_negative());
    c->flags = flags;
    c->ovf = ovf;
    return st;
}

/* The row the cursor shows: TRACE's on the curve (its row, -1 when off the window or erring), else its own. */
static int graph_vrow(ab_calc *c, const abn *ymin, const abn *ymax)
{
    abn x;
    ab_val v;
    if (!c->gr_trace) return c->gr_row;
    if (graph_sample(c, c->gr_cur, &x, &v, true, true) != ABN_OK) return -1;     /* the plot's pixel */
    return ab_graph_row(&v.re, ymin, ymax);
}

abn_status ab_graph_x(ab_calc *c, int col, abn *x)
{
    return graph_x(c, col, x);
}

abn_status ab_graph_cursor(ab_calc *c, abn *x, abn *y)
{
    abn lo, hi;
    ab_val v;
    if (c->gr_trace) {
        abn_status st = graph_sample(c, c->gr_cur, x, &v, true, false);
        *y = st == ABN_OK ? v.re : abn_zero();
        return st;
    }
    abn_status st = graph_x(c, c->gr_cur, x);
    if (st != ABN_OK) return st;
    ab_graph_window(c, &lo, &hi);
    return graph_rowy(c, c->gr_row, &lo, &hi, y);
}

/* r = v rounded to an integer, half even (exact). */
static void round_even(abn *r, const abn *v)
{
    abn f, fr, half, one = abn_from_int(1), two = abn_from_int(2), q, qf;
    abn_floor(&f, v);
    abn_sub(&fr, v, &f);
    abn_from_text(&half, "0.5");
    int k = abn_cmp(&fr, &half);
    *r = f;
    if (k > 0) abn_add(r, &f, &one);
    else if (k == 0 && (abn_div(&q, &f, &two), abn_floor(&qf, &q), abn_cmp(&q, &qf) != 0)) abn_add(r, &f, &one);
}

/* An arithmetic step of a zoom, an overflow the largest and an underflow 0 (as the window's own, unit 033). */
#define ZS(expr, r) clamp_big((expr), (r), abn_overflow_negative())

/* The window about the cursor (cx, cy) with spans wx and wy (NULL: that direction kept): t = wx 200, u = t / 399,
   XMIN = cx - u, XMAX = XMIN + wx; h = wy / 2, YMAX = cy + h, YMIN = YMAX - wy. The cursor then at (200, 92). */
static void zoom_about(ab_calc *c, const abn *cx, const abn *cy, const abn *wx, const abn *wy)
{
    abn t, u, h, k200 = abn_from_int(200), n399 = abn_from_int(AB_GRAPH_W - 1), two = abn_from_int(2);
    if (wx) {
        ZS(abn_mul(&t, wx, &k200), &t);
        ZS(abn_div(&u, &t, &n399), &u);
        ZS(abn_sub(&c->gr_xmin, cx, &u), &c->gr_xmin);
        ZS(abn_add(&c->gr_xmax, &c->gr_xmin, wx), &c->gr_xmax);
    }
    if (wy) {
        ZS(abn_div(&h, wy, &two), &h);
        ZS(abn_add(&c->gr_ymax, cy, &h), &c->gr_ymax);
        ZS(abn_sub(&c->gr_ymin, &c->gr_ymax, wy), &c->gr_ymin);
        c->gr_auto = false;
    }
}

static abn_status graph_zoom(ab_calc *c, int kind)
{
    abn ylo, yhi, cx, cy, sx, sy, w, v, t;
    abn n399 = abn_from_int(AB_GRAPH_W - 1), n184 = abn_from_int(AB_GRAPH_H - 1), two = abn_from_int(2);
    abn_status st;
    if (kind < 0 || kind >= AB_Z_N) return ABN_DOMAIN;
    ab_graph_window(c, &ylo, &yhi);             /* the window as drawn (a fitted Y its values) */
    if (kind == AB_Z_LAST) {
        if (!c->gr_has_last) return ABN_DOMAIN;
        abn a = c->gr_xmin, b = c->gr_xmax;
        bool au = c->gr_auto;
        c->gr_xmin = c->gr_lxmin, c->gr_xmax = c->gr_lxmax, c->gr_ymin = c->gr_lymin, c->gr_ymax = c->gr_lymax;
        c->gr_auto = c->gr_last_auto;
        c->gr_lxmin = a, c->gr_lxmax = b, c->gr_lymin = ylo, c->gr_lymax = yhi, c->gr_last_auto = au;
        c->gr_cur = AB_GRAPH_W / 2, c->gr_row = AB_GRAPH_CROW;
        c->gr_res_kind = 0, c->gr_mark = false, c->gr_fit_ok = false;   /* as every zoom (040 review) */
        return ABN_OK;
    }
    if ((st = graph_x(c, c->gr_cur, &cx)) != ABN_OK) return st;
    {   /* the cursor's y: with TRACE the curve's value at its x (the zoom centres the point itself); free, or
           where the curve errs, its row's */
        ab_val fv;
        abn fx;
        if (c->gr_trace && graph_sample(c, c->gr_cur, &fx, &fv, true, false) == ABN_OK) cy = fv.re;
        else if ((st = graph_rowy(c, c->gr_trace ? AB_GRAPH_CROW : c->gr_row, &ylo, &yhi, &cy)) != ABN_OK) return st;
    }
    abn bx0 = abn_zero(), bx1 = abn_zero(), by0 = abn_zero(), by1 = abn_zero();
    if (kind == AB_Z_BOX) {                     /* the mark's and the cursor's x and y, low to high */
        abn mx, my;
        if (!c->gr_mark) return ABN_DOMAIN;
        if ((st = graph_x(c, c->gr_mcol, &mx)) != ABN_OK || (st = graph_rowy(c, c->gr_mrow, &ylo, &yhi, &my)) != ABN_OK)
            return st;
        int kx = abn_cmp(&mx, &cx), ky = abn_cmp(&my, &cy);
        if (kx == 0 || ky == 0) return ABN_DOMAIN;      /* a flat box */
        bx0 = kx < 0 ? mx : cx, bx1 = kx < 0 ? cx : mx, by0 = ky < 0 ? my : cy, by1 = ky < 0 ? cy : my;
    }
    ZS(abn_sub(&sx, &c->gr_xmax, &c->gr_xmin), &sx);
    if (kind == AB_Z_DECI && abn_is_zero(&sx)) return ABN_DOMAIN;  /* (before ZLAST's window: 040 review) */
    /* ZLAST's window: this one as drawn */
    c->gr_lxmin = c->gr_xmin, c->gr_lxmax = c->gr_xmax, c->gr_lymin = ylo, c->gr_lymax = yhi;
    c->gr_last_auto = c->gr_auto;
    c->gr_has_last = true;
    c->gr_ymin = ylo, c->gr_ymax = yhi;         /* (a fitted Y held at its values unless refitted) */
    bool keep_auto = c->gr_auto;
    ZS(abn_sub(&sy, &yhi, &ylo), &sy);
    switch (kind) {
    case AB_Z_IN: case AB_Z_OUT: case AB_Z_HIN: case AB_Z_HOUT: case AB_Z_VIN: case AB_Z_VOUT: case AB_Z_CNTR: {
        bool in = kind == AB_Z_IN || kind == AB_Z_HIN || kind == AB_Z_VIN;
        bool cn = kind == AB_Z_CNTR;
        bool hx = kind != AB_Z_VIN && kind != AB_Z_VOUT, hy = kind != AB_Z_HIN && kind != AB_Z_HOUT;
        if (cn) w = sx, v = sy;
        else if (in) ZS(abn_div(&w, &sx, &c->gr_hf), &w), ZS(abn_div(&v, &sy, &c->gr_vf), &v);
        else ZS(abn_mul(&w, &sx, &c->gr_hf), &w), ZS(abn_mul(&v, &sy, &c->gr_vf), &v);
        zoom_about(c, &cx, &cy, hx ? &w : NULL, hy ? &v : NULL);
        if (!hy) c->gr_auto = keep_auto;        /* x only: a fitted Y refits */
        break;
    }
    case AB_Z_BOX:
        c->gr_xmin = bx0, c->gr_xmax = bx1, c->gr_ymin = by0, c->gr_ymax = by1, c->gr_auto = false;
        c->gr_mark = false;
        break;
    case AB_Z_AUTO:
        c->gr_auto = true;
        break;
    case AB_Z_DFLT:                             /* 033's: X -10..10, Y fitted */
        c->gr_xmin = abn_from_int(-10), c->gr_xmax = abn_from_int(10), c->gr_auto = true;
        break;
    case AB_Z_DECI: {                           /* step = span / 399 to the nearest of 1, 2, 5 x 10^k */
        abn q, m, mant, p10, step, r;
        bool neg;
        char d[ABN_DIGITS + 1], e[24];
        int lead;
        ZS(abn_div(&q, &sx, &n399), &q);        /* (a reversed window: q < 0, the step too, its direction kept) */
        abn aq = abn_abs(&q);
        abn_digits(&aq, &neg, d, &lead);
        snprintf(e, sizeof e, "%sE%d", abn_is_negative(&q) ? "-1" : "1", lead);
        abn_from_text(&p10, e);
        abn_div(&m, &q, &p10);                  /* the mantissa, 1 <= m < 10 (exact: a power of ten) */
        abn b15, b35, b75;
        abn_from_text(&b15, "1.5"), abn_from_text(&b35, "3.5"), abn_from_text(&b75, "7.5");
        mant = abn_from_int(abn_cmp(&m, &b15) < 0 ? 1 : abn_cmp(&m, &b35) < 0 ? 2 : abn_cmp(&m, &b75) < 0 ? 5 : 10);
        ZS(abn_mul(&step, &mant, &p10), &step);
        ZS(abn_div(&t, &c->gr_xmin, &step), &t);
        round_even(&r, &t);
        ZS(abn_mul(&c->gr_xmin, &r, &step), &c->gr_xmin);
        ZS(abn_mul(&t, &n399, &step), &t);
        ZS(abn_add(&c->gr_xmax, &c->gr_xmin, &t), &c->gr_xmax);
        c->gr_auto = keep_auto;
        break;
    }
    case AB_Z_INTG: {                           /* a column a unit and a row a unit about the rounded cursor */
        abn rx, ry, k199 = abn_from_int(199), k92 = abn_from_int(AB_GRAPH_CROW);
        round_even(&rx, &cx);
        round_even(&ry, &cy);
        ZS(abn_sub(&c->gr_xmin, &rx, &k199), &c->gr_xmin);
        ZS(abn_add(&c->gr_xmax, &c->gr_xmin, &n399), &c->gr_xmax);
        ZS(abn_add(&c->gr_ymax, &ry, &k92), &c->gr_ymax);
        ZS(abn_sub(&c->gr_ymin, &c->gr_ymax, &n184), &c->gr_ymin);
        c->gr_auto = false;
        break;
    }
    case AB_Z_SQR: {                            /* the x span 399 (YMAX - YMIN) / 184 about the x centre; y kept */
        abn q, sp, mid, h;
        ZS(abn_div(&q, &sy, &n184), &q);
        ZS(abn_mul(&sp, &q, &n399), &sp);
        ZS(abn_div(&t, &sx, &two), &t);
        ZS(abn_add(&mid, &c->gr_xmin, &t), &mid);
        ZS(abn_div(&h, &sp, &two), &h);
        ZS(abn_sub(&c->gr_xmin, &mid, &h), &c->gr_xmin);
        ZS(abn_add(&c->gr_xmax, &mid, &h), &c->gr_xmax);
        c->gr_auto = false;
        break;
    }
    default: {                                  /* AB_Z_TRIG: -pi..pi, y kept */
        abn pi;
        abn_from_text(&pi, "3.141592653589793238462643383279503");
        c->gr_xmin = abn_neg(&pi), c->gr_xmax = pi;
        c->gr_auto = keep_auto;
        break;
    }
    }
    c->gr_cur = AB_GRAPH_W / 2, c->gr_row = AB_GRAPH_CROW;
    c->gr_res_kind = 0;
    c->gr_mark = false;                         /* (its pixel would mean another point: 040 review) */
    c->gr_fit_ok = false;
    return ABN_OK;
}

/* The cursor by a numpad digit (8 up, 2 down, 4 left, 6 right, the corners diagonal, 5 home); +10: ten pixels.
   With TRACE only across. Stops at the edges. */
static abn_status graph_move(ab_calc *c, int arg)
{
    static const int DX[10] = {0, -1, 0, 1, -1, 0, 1, -1, 0, 1}, DY[10] = {0, 1, 1, 1, 0, 0, 0, -1, -1, -1};
    int d = arg % 10, n = arg >= 10 ? 10 : 1;
    if (arg < 1 || arg > 19 || d == 0) return ABN_DOMAIN;
    c->gr_res_kind = 0;                         /* a move clears the tool's result */
    if (d == 5) {
        c->gr_cur = AB_GRAPH_W / 2, c->gr_row = AB_GRAPH_CROW;
        return ABN_OK;
    }
    int col = c->gr_cur + DX[d] * n, row = c->gr_row + (c->gr_trace ? 0 : DY[d] * n);
    c->gr_cur = col < 0 ? 0 : col > AB_GRAPH_W - 1 ? AB_GRAPH_W - 1 : col;
    c->gr_row = row < 0 ? 0 : row > AB_GRAPH_H - 1 ? AB_GRAPH_H - 1 : row;
    return ABN_OK;
}

/* SOLVE's engine on the curve (mode 1) or its slope (2), the guesses the cursor's column's x and the next column's
   (the previous at the right edge): the root, or NO ROOT FND as SOLVE's. */
/* f's departures from f(x) at x + D and x - D, as magnitudes in a[0], a[1]: ABN_OK when both go the way sg says (up
   for a minimum, sg > 0), ABN_INTERRUPTED (C), else ABN_DOMAIN (040d). */
static abn_status f_depart(ab_calc *c, const abn *x, const abn *fx, const abn *D, int sg, abn a[2])
{
    for (int side = 0; side < 2; side++) {
        abn q;
        abn_status st;
        if ((side ? abn_sub(&q, x, D) : abn_add(&q, x, D)) != ABN_OK) return ABN_DOMAIN;
        if ((st = graph_eval(c, false, &q, &a[side])) != ABN_OK) return st == ABN_INTERRUPTED ? st : ABN_DOMAIN;
        if (abn_sub(&a[side], &a[side], fx) != ABN_OK || sgn(&a[side]) != sg) return ABN_DOMAIN;
        a[side] = abn_abs(&a[side]);
    }
    return ABN_OK;
}

/* Whether x, SOLVE's answer on the slope, is an extremum (unit 040c for an answer not found, 040d for every one):
   *ok, and ABN_OK, or ABN_INTERRUPTED (C). SOLVE's x is a zero of the 5-point slope: near an extremum that slope is
   noise (about 1E-33 |f| / h at worst), it sits up to about 2.4h off a flat or one-sided extremum, and near a pole
   it can have a zero of its own. So:
   - the slope: at the first d of 1E-12, 1E-10, ... 1E-4 times max(1, |x|) where the slopes at x + d and x - d are
     both clear of the noise (|slope| > 1E-30 |f(x)| / h, a thousand times it), they have opposite signs;
   - f brackets it: at the first D of 10d, 100d, ... up to 1E-3 max(1, |x|) where f(x + D) and f(x - D) both depart
     from f(x) the way the slopes say (up from a minimum, down from a maximum); a D where they don't, x off a flat
     extremum, tries the next;
   - and the departure grows with distance: at 10D more than 1.5 times at D, on each side (a smooth extremum: 100
     times; a cusp: 10; ABS(X)^0.5: 3.2). At a pole f(x) is huge and the departure saturates near 1 times: no
     extremum (040d; the slope's own growth passes a pole, as its stencil straddles it).
   A constant's slope is never clear: no extremum. The answer stays x, as close as the noise allows. Each slope and
   f is one of the tool's evaluations: at most 10 slopes and 21 f. (Fresh eyes found no case on which a second
   growth step or a noise limit on f's departures changed the answer, in 1865; they aren't here.) */
static abn_status extr_check(ab_calc *c, const abn *x, bool *ok)
{
    abn ax = abn_abs(x), one = abn_from_int(1), h, fx, lim, e, ten = abn_from_int(10), top, k15;
    const abn *m = abn_cmp(&ax, &one) > 0 ? &ax : &one;
    abn_status st;
    *ok = false;
    abn_from_text(&e, "1E-7"), abn_from_text(&lim, "1E-30"), abn_from_text(&top, "1E-3"), abn_from_text(&k15, "1.5");
    if ((st = graph_eval(c, false, x, &fx)) != ABN_OK) return st == ABN_INTERRUPTED ? st : ABN_OK;
    abn afx = abn_abs(&fx);
    if (abn_mul(&h, m, &e) != ABN_OK || abn_mul(&top, &top, m) != ABN_OK || abn_mul(&lim, &lim, &afx) != ABN_OK ||
        abn_div(&lim, &lim, &h) != ABN_OK)
        return ABN_OK;
    abn_from_text(&e, "1E-12");
    for (int r = 0; r < 5; r++, abn_mul(&e, &e, &ten), abn_mul(&e, &e, &ten)) {
        abn d, p, sp, sm, ap, am, D, a[2], b[2];
        if (abn_mul(&d, m, &e) != ABN_OK || abn_add(&p, x, &d) != ABN_OK) return ABN_OK;
        if ((st = graph_eval(c, true, &p, &sp)) != ABN_OK) return st == ABN_INTERRUPTED ? st : ABN_OK;
        if (abn_sub(&p, x, &d) != ABN_OK) return ABN_OK;
        if ((st = graph_eval(c, true, &p, &sm)) != ABN_OK) return st == ABN_INTERRUPTED ? st : ABN_OK;
        ap = abn_abs(&sp), am = abn_abs(&sm);
        if (abn_cmp(&ap, &lim) <= 0 || abn_cmp(&am, &lim) <= 0) continue;      /* inside the noise: a wider d */
        int sg = sgn(&sp);
        if (sg * sgn(&sm) >= 0) return ABN_OK;                                  /* one sign: none */
        if (abn_mul(&D, &d, &ten) != ABN_OK) return ABN_OK;
        for (; abn_cmp(&D, &top) <= 0; abn_mul(&D, &D, &ten)) {
            abn D10, k;
            if ((st = f_depart(c, x, &fx, &D, sg, a)) == ABN_INTERRUPTED) return st;
            if (st != ABN_OK) continue;                                         /* not bracketed: a wider D */
            if (abn_mul(&D10, &D, &ten) != ABN_OK || (st = f_depart(c, x, &fx, &D10, sg, b)) == ABN_INTERRUPTED)
                return st == ABN_INTERRUPTED ? st : ABN_OK;
            for (int side = 0; side < 2; side++)                                /* grows, or saturates: a pole */
                if (st != ABN_OK || abn_mul(&k, &a[side], &k15) != ABN_OK || abn_cmp(&b[side], &k) <= 0) return ABN_OK;
            *ok = true;
            return ABN_OK;
        }
        return ABN_OK;                                                          /* never bracketed */
    }
    return ABN_OK;
}

static abn_status graph_solve(ab_calc *c, int mode, abn *root)
{
    abn a, b;
    solve_out o;
    abn_status st;
    int evals = c->evals;
    if ((st = graph_x(c, c->gr_cur, &a)) != ABN_OK ||
        (st = graph_x(c, c->gr_cur < AB_GRAPH_W - 1 ? c->gr_cur + 1 : c->gr_cur - 1, &b)) != ABN_OK)
        return st;
    ev_graph = mode;
    ev_best_set = false;
    c->evals = 0;
    st = solve_core(c, a, b, &o);
    if (st == ABN_OK && mode == 2) {    /* EXTR's answer, found or not (040c, 040d) */
        bool ok;
        st = extr_check(c, &o.x, &ok);
        o.found = ok;
    }
    ev_graph = 0;
    c->evals = evals;
    if (st != ABN_OK) return st;
    if (!o.found) return ABN_NO_ROOT;
    *root = o.x;
    return ABN_OK;
}

/* The column nearest x (round((x - XMIN) / (XMAX - XMIN) 399)), or -1 off the window. */
static int graph_col_of(const ab_calc *c, const abn *x)
{
    abn a, b, q, p, n = abn_from_int(AB_GRAPH_W - 1);
    if (abn_sub(&a, x, &c->gr_xmin) != ABN_OK || abn_sub(&b, &c->gr_xmax, &c->gr_xmin) != ABN_OK || abn_is_zero(&b) ||
        abn_div(&q, &a, &b) != ABN_OK || abn_mul(&p, &q, &n) != ABN_OK)
        return -1;
    int k = round_clamped(&p, -1, AB_GRAPH_W);
    return k >= 0 && k <= AB_GRAPH_W - 1 ? k : -1;
}

/* A tool at the cursor (unit 040 rule 4; 50g guide p.12-5, 12-6): its result held for the readout and ENTER. */
static abn_status graph_fcn(ab_calc *c, int kind)
{
    abn x, r = abn_zero(), b = abn_zero(), y;
    ab_val v;
    abn_status st;
    c->gr_res_kind = 0;                         /* a failed tool leaves no result (040 review) */
    if (kind < 0 || kind >= AB_F_N) return ABN_DOMAIN;
    if ((st = graph_x(c, c->gr_cur, &x)) != ABN_OK) return st;
    switch (kind) {
    case AB_F_ROOT: case AB_F_EXTR: {           /* the curve's root, or its slope's (an extremum's x) */
        if ((st = graph_solve(c, kind == AB_F_ROOT ? 1 : 2, &r)) != ABN_OK) {
            c->gr_res_kind = 0;
            return st;
        }
        int k = graph_col_of(c, &r);
        if (k >= 0) c->gr_cur = k;              /* the cursor to the result's column */
        break;
    }
    case AB_F_SLOPE:
        if ((st = graph_slope(c, &x, &r)) != ABN_OK) return st;
        break;
    case AB_F_AREA: {                           /* from the mark's x to the cursor's, by the integral's engine */
        abn mx, unc;
        if (!c->gr_mark) return ABN_DOMAIN;
        if ((st = graph_x(c, c->gr_mcol, &mx)) != ABN_OK) return st;
        int evals = c->evals;
        ev_graph = 1;
        st = integ_core(c, &mx, &x, &r, &unc);
        ev_graph = 0;
        c->evals = evals;
        if (st != ABN_OK) return st;
        c->gr_mark = false;
        break;
    }
    case AB_F_FX:
        if ((st = graph_sample(c, c->gr_cur, &x, &v, true, false)) != ABN_OK) return st;
        r = v.re;
        break;
    default:                                    /* AB_F_TANL: y = m x + b through (x, f(x)) */
        if ((st = graph_slope(c, &x, &r)) != ABN_OK || (st = graph_f(c, &x, &y)) != ABN_OK) return st;
        {
            abn t;
            if ((st = abn_mul(&t, &r, &x)) != ABN_OK || (st = abn_sub(&b, &y, &t)) != ABN_OK) return st;
        }
        break;
    }
    c->gr_res = r;
    c->gr_res_b = b;
    c->gr_res_kind = kind + 1;
    return ABN_OK;
}

/* GO (unit 033): the equation shown or last shown, against TABLE's variable (or the first of what is
   plotted), the trace at column 200. */
static abn_status graph_go(ab_calc *c)
{
    int e, vars[27];
    abn_status st;
    if (!c->stu) return ABN_STU_ONLY;
    if (c->eqn_mode && c->eqn_typing && (st = eqn_store_typed(c)) != ABN_OK) return st;
    e = c->eqn_cur;
    if (e < 0 || e >= c->eqn_count || c->eqn[e][0] == '\x01') return ABN_NO_EQN;
    if (c->eqn_note[e] == AB_NOTE_WHERE) return ABN_CAS_WHERE;                  /* a sum's condition (070) */
    int v = c->tbl_var;
    if (v < 0) {
        const char *t = c->eqn[e];
        int av = eqn_assign_var(t, false), n = eqn_vars(t, av >= 0, false, c->eqn_model[e], vars), i = 0;
        bool rhs_form = av >= 0;
        for (int j = 0; j < n && rhs_form; j++) rhs_form = vars[j] != av;
        if (!rhs_form) n = eqn_vars(t, false, false, c->eqn_model[e], vars);   /* L-R or an expression: its first variable */
        while (i < n && vars[i] >= 26) i++;
        if (i == n) return ABN_NO_VAR;
        v = vars[i];
    }
    c->gr_eq = e;
    c->gr_v = v;
    c->gr_cur = AB_GRAPH_W / 2;
    c->gr_row = AB_GRAPH_CROW;      /* unit 040: the cursor home on the curve, no mark, no tool's result */
    c->gr_trace = true;
    c->gr_mark = false;
    c->gr_res_kind = 0;
    c->gr_fit_ok = false;
    c->gr_on = true;
    return ABN_OK;
}

/* A key while the graph is open: the trace's left and right (stopping at the ends); ENTER takes the traced
   value as a keyed number (in ALG into ANS and the line) and leaves, an erring column giving its error;
   C leaves; nothing else (rule 6, answer 5). */
static abn_status graph_key(ab_calc *c, ab_op op, int arg)
{
    abn x;
    ab_val y;
    abn_status st;
    switch (op) {
    case AB_GZOOM:                              /* unit 040 */
        return graph_zoom(c, arg);
    case AB_GMOVE:
        return graph_move(c, arg);
    case AB_GTRACE:
        c->gr_trace = !c->gr_trace;
        return ABN_OK;
    case AB_GMARK: {                            /* the cursor's column and the row it shows */
        abn lo, hi;
        ab_graph_window(c, &lo, &hi);
        int r = graph_vrow(c, &lo, &hi);
        if (r < 0 || r > AB_GRAPH_H - 1) return ABN_DOMAIN;    /* a traced point off the window */
        c->gr_mark = true, c->gr_mcol = c->gr_cur, c->gr_mrow = r;
        return ABN_OK;
    }
    case AB_GFACT:
        if (arg < 1 || arg > 100) return ABN_DOMAIN;
        c->gr_hf = c->gr_vf = abn_from_int(arg);
        return ABN_OK;
    case AB_GFCN:
        return graph_fcn(c, arg);
    case AB_GLEFT:
        if (c->gr_cur > 0) c->gr_cur--;
        c->gr_res_kind = 0;
        return ABN_OK;
    case AB_GRIGHT:
        if (c->gr_cur < AB_GRAPH_W - 1) c->gr_cur++;
        c->gr_res_kind = 0;
        return ABN_OK;
    case AB_ENTER:
        if (c->gr_res_kind) {                   /* unit 040: a tool's result, as the trace's value */
            y = ab_real(c->gr_res);
            c->gr_res_kind = 0;
            goto place;
        }
        if ((st = graph_sample(c, c->gr_cur, &x, &y, false, false)) != ABN_OK) return st;
    place:
        c->gr_on = false;
        if (c->alg) {
            c->ans = y;
            num_text(&y.re, c->ebuf, sizeof c->ebuf);
            c->ecur = (int)strlen(c->ebuf);
            c->alg_shown = true;
            c->hist_at = -1;
            return ABN_OK;
        }
        place_val(c, y);
        c->lift = true;
        return ABN_OK;
    case AB_CANCEL:
        c->gr_on = false;
        return ABN_OK;
    default:
        return ABN_OK;
    }
}
