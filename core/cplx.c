/* cplx: complex arithmetic (unit 009; see cplx.h). */
#include "cplx.h"
#include <string.h>

#include "arena.h"
#include "hp.h"
#include "xnum.h"

abn_status cx_add(cplx *out, const cplx *a, const cplx *b)
{
    cplx r;
    abn_status st;
    if ((st = abn_add(&r.re, &a->re, &b->re)) != ABN_OK) return st;
    if ((st = abn_add(&r.im, &a->im, &b->im)) != ABN_OK) return st;
    *out = r;
    return ABN_OK;
}

abn_status cx_sub(cplx *out, const cplx *a, const cplx *b)
{
    cplx r;
    abn_status st;
    if ((st = abn_sub(&r.re, &a->re, &b->re)) != ABN_OK) return st;
    if ((st = abn_sub(&r.im, &a->im, &b->im)) != ABN_OK) return st;
    *out = r;
    return ABN_OK;
}

cplx cx_neg(const cplx *a)
{
    cplx r;
    r.re = abn_neg(&a->re);
    r.im = abn_neg(&a->im);
    return r;
}

/* p*q +/- r*s exactly. */
static bool dot2(xnum *out, const xnum *p, const xnum *q, const xnum *r, const xnum *s, bool subtract)
{
    int m = ar_mark();
    xnum *t1, *t2;
    bool ok = ar_xn(2, &t1, &t2) && xn_mul(t1, p, q) && xn_mul(t2, r, s) && xn_add(out, t1, t2, subtract);
    ar_release(m);
    return ok;
}

/* The four parts of two complex numbers as exact numbers, and room for the results, taken from the
 * wide pool (core/arena.h). */
typedef struct {
    xnum *A, *B, *C, *D, *N1, *N2, *DEN;
} quad;

static abn_status load4(quad *q, const cplx *a, const cplx *b)
{
    if (!ar_xn(7, &q->A, &q->B, &q->C, &q->D, &q->N1, &q->N2, &q->DEN)) return ABN_RANGE;
    if (!xn_from_abn(q->A, &a->re) || !xn_from_abn(q->B, &a->im) || !xn_from_abn(q->C, &b->re) ||
        !xn_from_abn(q->D, &b->im))
        return ABN_RANGE;
    return ABN_OK;
}

/* A part rounded from its exact value: one below the range is 0 (unit 026: an underflow gives 0; the
 * part is exact, so it is truly that small); the other statuses stand. */
static abn_status part(abn *out, abn_status st)
{
    if (st == ABN_UNDERFLOW) {
        *out = abn_zero();
        return ABN_OK;
    }
    return st;
}

/* (a + bi)(c + di) = (ac - bd) + (ad + bc)i, each part exact, then rounded once. */
static abn_status mul_(cplx *out, const cplx *a, const cplx *b)
{
    quad q;
    cplx r;
    abn_status st;
    if ((st = load4(&q, a, b)) != ABN_OK) return st;
    xn_set_int(q.DEN, 1);
    if (!dot2(q.N1, q.A, q.C, q.B, q.D, true) || !dot2(q.N2, q.A, q.D, q.B, q.C, false)) return ABN_RANGE;
    if ((st = part(&r.re, xn_div_to_abn(&r.re, q.N1, q.DEN))) != ABN_OK) return st;
    if ((st = part(&r.im, xn_div_to_abn(&r.im, q.N2, q.DEN))) != ABN_OK) return st;
    *out = r;
    return ABN_OK;
}

/* (a + bi)/(c + di) = ((ac + bd) + (bc - ad)i) / (c^2 + d^2), each part exact, rounded once. */
static abn_status div_(cplx *out, const cplx *a, const cplx *b)
{
    quad q;
    cplx r;
    abn_status st;
    if ((st = load4(&q, a, b)) != ABN_OK) return st;
    if (!dot2(q.DEN, q.C, q.C, q.D, q.D, false)) return ABN_RANGE;
    if (xn_sign(q.DEN) == 0) return ABN_DIV_ZERO;
    if (!dot2(q.N1, q.A, q.C, q.B, q.D, false) || !dot2(q.N2, q.B, q.C, q.A, q.D, true)) return ABN_RANGE;
    if ((st = part(&r.re, xn_div_to_abn(&r.re, q.N1, q.DEN))) != ABN_OK) return st;
    if ((st = part(&r.im, xn_div_to_abn(&r.im, q.N2, q.DEN))) != ABN_OK) return st;
    *out = r;
    return ABN_OK;
}

/* |a + bi| = sqrt(a^2 + b^2): the root of the exact sum of squares, correctly rounded (unit 010b;
 * it was the sum rounded once, then a correctly rounded root; unit 034: hp_hypot, shared with ->POL). */
static abn_status abs_(abn *out, const cplx *a)
{
    return hp_hypot(out, &a->re, &a->im);       /* unit 034: parts far apart too (they were a range error) */
}

abn_status cx_mul(cplx *out, const cplx *a, const cplx *b)
{
    int m = ar_mark();
    abn_status st = mul_(out, a, b);
    ar_release(m);
    return st;
}

abn_status cx_div(cplx *out, const cplx *a, const cplx *b)
{
    int m = ar_mark();
    abn_status st = div_(out, a, b);
    ar_release(m);
    return st;
}

abn_status cx_inv(cplx *out, const cplx *a)
{
    cplx one;
    one.re = abn_from_int(1);
    one.im = abn_zero();
    return cx_div(out, &one, a);
}

abn_status cx_abs(abn *out, const cplx *a)
{
    int m = ar_mark();
    abn_status st = abs_(out, a);
    ar_release(m);
    return st;
}

/* ---- functions (UG p.9-3: e^z, ln z, sin z, cos z, tan z, z1^z2) ---- */

static abn K(const char *text)
{
    abn v;
    abn_from_text(&v, text);
    return v;
}

/* The exponent of a's leading digit (0 for zero). */
static int lead_of(const abn *a)
{
    bool neg;
    char d[ABN_DIGITS + 1];
    int lead;
    abn_digits(a, &neg, d, &lead);
    return d[0] ? lead : 0;
}

/* a * b rounded once; a product below the range is 0 (a part negligible beside the other). */
static abn_status mulp(abn *out, const abn *a, const abn *b)
{
    abn_status st = abn_mul(out, a, b);
    if (st == ABN_UNDERFLOW && lead_of(a) + lead_of(b) < 0) {
        *out = abn_zero();
        return ABN_OK;
    }
    return st;
}

/* The functions are formed to HP_WORK digits (core/hp.c) and each part rounded once, so a part
 * is correctly rounded but for the rarest ties; past the stored pi (a radian argument beyond about
 * 10^180) the engine's own functions stand in. */
#define HP_WORK 50

/* Rounds both parts once; a part below the range is 0 (unit 026, as for a real result: an underflow
 * gives 0; each part is rounded from its value to HP_WORK digits, so it is truly that small). A part
 * beyond the range is an overflow, which a complex result reports as an error (abacus #4198). */
static abn_status round_parts(cplx *out, const hp *re, const hp *im)
{
    cplx r;
    abn_status s1 = part(&r.re, hp_round(&r.re, re)), s2 = part(&r.im, hp_round(&r.im, im));
    if (s1 != ABN_OK) return s1;
    if (s2 != ABN_OK) return s2;
    *out = r;
    return ABN_OK;
}

static hp ha, hb, e1, e2, e3, e4, e5, e6, pr, pi_;

/* e^(a + bi) with the engine's functions (huge b). */
static abn_status exp_lib(cplx *out, const cplx *z)
{
    abn ea, c, s;
    cplx r;
    abn_status st;
    if ((st = abn_exp(&ea, &z->re)) != ABN_OK) return st;
    if ((st = abn_cos(&c, &z->im)) != ABN_OK || (st = abn_sin(&s, &z->im)) != ABN_OK) return st;
    if ((st = mulp(&r.re, &ea, &c)) != ABN_OK || (st = mulp(&r.im, &ea, &s)) != ABN_OK) return st;
    *out = r;
    return ABN_OK;
}

/* e^(a + bi) = e^a (cos b + i sin b). */
abn_status cx_exp(cplx *out, const cplx *z)
{
    if (!hp_from_abn(&ha, &z->re) || !hp_from_abn(&hb, &z->im)) return ABN_RANGE;
    if (!hp_exp(&e1, &ha, HP_WORK)) {                       /* |a| >= 10^7: beyond the range */
        if (!abn_is_negative(&z->re)) return ABN_OVERFLOW;
        out->re = out->im = abn_zero();                     /* e^a below it: 0 (unit 026) */
        return ABN_OK;
    }
    if (!hp_sincos(&e2, &e3, &hb, HP_WORK)) return exp_lib(out, z);
    if (!hp_mul(&pr, &e1, &e3, HP_WORK) || !hp_mul(&pi_, &e1, &e2, HP_WORK)) return ABN_RANGE;
    return round_parts(out, &pr, &pi_);
}

/* s = a^2 + b^2 and t = s - 1 into (hs, ht), to 80 digits: each square exact (at most 68
 * digits), and the sum exact unless one square lies wholly below the other's 80th digit, which
 * then cannot matter to ln |z| beside arg z (a part is 34 digits, so a != 1 means |a - 1| >= 1e-33).
 * t near 0 is formed from the exact squares, so a z close to the unit circle keeps its small real
 * part. */
static hp hs, ht;
static abn_status sum_squares(const cplx *z)
{
    static hp one;
    hp_set_int(&one, 1);
    if (!hp_from_abn(&ha, &z->re) || !hp_from_abn(&hb, &z->im) || !hp_mul(&e1, &ha, &ha, 80) ||
        !hp_mul(&e2, &hb, &hb, 80) || !hp_add(&hs, &e1, &e2, false, 80) || !hp_add(&ht, &hs, &one, true, 80))
        return ABN_RANGE;
    return ABN_OK;
}

/* ln z = ln |z| + i arg z, arg in (-pi, pi] (principal; unit 009 rule 5): ln |z| is half the log of
 * a^2 + b^2. */
abn_status cx_ln(cplx *out, const cplx *z)
{
    abn_status st;
    if (abn_is_zero(&z->re) && abn_is_zero(&z->im)) return ABN_DOMAIN;
    if (!hp_atan2(&pi_, &z->im, &z->re, HP_WORK)) return ABN_RANGE;
    if ((st = sum_squares(z)) != ABN_OK) return st;
    if (!hp_ln(&e3, &hs, &ht, HP_WORK + 2)) return ABN_RANGE;
    hp_set_int(&e4, 5);
    e4.e = -1;
    if (!hp_mul(&pr, &e3, &e4, HP_WORK + 2)) return ABN_RANGE;
    return round_parts(out, &pr, &pi_);
}

/* sin a, cos a, sinh b, cosh b to HP_WORK digits; false past the stored pi. */
static abn_status parts4(const cplx *z)
{
    if (!hp_from_abn(&ha, &z->re) || !hp_from_abn(&hb, &z->im)) return ABN_RANGE;
    if (!hp_sinhcosh(&e3, &e4, &hb, HP_WORK + 2)) return ABN_RANGE;   /* |b| >= 10^7 */
    if (!hp_sincos(&e1, &e2, &ha, HP_WORK + 2)) return ABN_INEXACT;    /* the engine stands in */
    return ABN_OK;
}

/* The engine's sin a, cos a, sinh b, cosh b (huge a). */
static abn_status parts4_lib(abn *sa, abn *ca, abn *sh, abn *ch, const cplx *z)
{
    abn_status st;
    if ((st = abn_sin(sa, &z->re)) != ABN_OK || (st = abn_cos(ca, &z->re)) != ABN_OK) return st;
    if ((st = abn_sinh(sh, &z->im)) != ABN_OK || (st = abn_cosh(ch, &z->im)) != ABN_OK) return st;
    return ABN_OK;
}

/* sin(a + bi) = sin a cosh b + i cos a sinh b; cos(a + bi) = cos a cosh b - i sin a sinh b. The
 * argument is in radians whatever the angle mode (the guide's sin(2 i 3), UG p.9-4). */
static abn_status sin_or_cos(cplx *out, const cplx *z, bool want_cos)
{
    abn_status st = parts4(z);
    if (st == ABN_INEXACT) {
        abn sa, ca, sh, ch;
        cplx r;
        if ((st = parts4_lib(&sa, &ca, &sh, &ch, z)) != ABN_OK) return st;
        if (want_cos) {
            if ((st = mulp(&r.re, &ca, &ch)) != ABN_OK || (st = mulp(&r.im, &sa, &sh)) != ABN_OK) return st;
            r.im = abn_neg(&r.im);
        } else if ((st = mulp(&r.re, &sa, &ch)) != ABN_OK || (st = mulp(&r.im, &ca, &sh)) != ABN_OK) {
            return st;
        }
        *out = r;
        return ABN_OK;
    }
    if (st != ABN_OK) return st;
    if (!hp_mul(&pr, want_cos ? &e2 : &e1, &e4, HP_WORK + 2) ||
        !hp_mul(&pi_, want_cos ? &e1 : &e2, &e3, HP_WORK + 2))
        return ABN_RANGE;
    if (want_cos) hp_neg(&pi_);
    return round_parts(out, &pr, &pi_);
}

abn_status cx_sin(cplx *out, const cplx *z)
{
    return sin_or_cos(out, z, false);
}

abn_status cx_cos(cplx *out, const cplx *z)
{
    return sin_or_cos(out, z, true);
}

/* tan z = (sin a cos a + i sinh b cosh b) / (cos^2 a + sinh^2 b): every term of the same sign, so
 * nothing cancels, near a pole or far from the axis. Past |b| = 10^5 it is i sgn(b) to far more
 * than 34 digits (the real part is below the range). */
abn_status cx_tan(cplx *out, const cplx *z)
{
    abn big = abn_from_int(100000), ab = abn_abs(&z->im);
    abn_status st;
    if (abn_cmp(&ab, &big) > 0) {
        abn one = abn_from_int(1);
        out->re = abn_zero();
        out->im = abn_is_negative(&z->im) ? abn_neg(&one) : one;
        return ABN_OK;
    }
    st = parts4(z);
    if (st == ABN_INEXACT) {
        /* huge a: the same formula on the engine's values */
        abn sa, ca, sh, ch, n1, n2, d1, d2, den;
        cplx r;
        if ((st = parts4_lib(&sa, &ca, &sh, &ch, z)) != ABN_OK) return st;
        if ((st = abn_mul(&n1, &sa, &ca)) != ABN_OK || (st = abn_mul(&n2, &sh, &ch)) != ABN_OK ||
            (st = abn_mul(&d1, &ca, &ca)) != ABN_OK || (st = abn_mul(&d2, &sh, &sh)) != ABN_OK ||
            (st = abn_add(&den, &d1, &d2)) != ABN_OK || (st = abn_div(&r.re, &n1, &den)) != ABN_OK ||
            (st = abn_div(&r.im, &n2, &den)) != ABN_OK)
            return st;
        *out = r;
        return ABN_OK;
    }
    if (st != ABN_OK) return st;
    int w = HP_WORK + 4;
    if (!hp_mul(&e5, &e1, &e2, w) || !hp_mul(&e6, &e3, &e4, w)) return ABN_RANGE;     /* numerators */
    if (!hp_mul(&e1, &e2, &e2, w) || !hp_mul(&e4, &e3, &e3, w) || !hp_add(&e2, &e1, &e4, false, w))
        return ABN_RANGE;                                                           /* denominator */
    if (!hp_div(&pr, &e5, &e2, w) || !hp_div(&pi_, &e6, &e2, w)) return ABN_DIV_ZERO;
    return round_parts(out, &pr, &pi_);
}

/* (p + qi)(r + si) into (u + vi) to 60 digits (exact when the true parts have at most 60). */
static bool xcmul(hp *u, hp *v, const hp *p, const hp *q, const hp *r, const hp *s)
{
    static hp t1, t2;
    return hp_mul(&t1, p, r, 120) && hp_mul(&t2, q, s, 120) && hp_add(u, &t1, &t2, true, 60) &&
           hp_mul(&t1, p, s, 120) && hp_mul(&t2, q, r, 120) && hp_add(v, &t1, &t2, false, 60);
}

/* z^n for an integer n, |n| < 2^31, by repeated squaring to 60 digits (exact whenever the true
 * power has at most 60 digits in each part), rounded once at the end; for n < 0 the reciprocal
 * of the 60-digit power, to 60 digits, rounded once. */
static abn_status cx_powi(cplx *out, const cplx *z, long n)
{
    static hp pr2, pi2, br, bi, tr, ti, den, t1, t2;
    unsigned long m = n < 0 ? (unsigned long)-n : (unsigned long)n;
    if (!hp_from_abn(&br, &z->re) || !hp_from_abn(&bi, &z->im)) return ABN_RANGE;
    hp_set_int(&pr2, 1);
    hp_set_int(&pi2, 0);
    while (m) {
        if (m & 1) {
            if (!xcmul(&tr, &ti, &pr2, &pi2, &br, &bi)) return ABN_RANGE;
            pr2 = tr;
            pi2 = ti;
        }
        m >>= 1;
        if (m) {
            if (!xcmul(&tr, &ti, &br, &bi, &br, &bi)) return ABN_RANGE;
            br = tr;
            bi = ti;
        }
    }
    if (n < 0) {
        /* 1/(p + qi) = (p - qi) / (p^2 + q^2) */
        if (!hp_mul(&t1, &pr2, &pr2, 120) || !hp_mul(&t2, &pi2, &pi2, 120) || !hp_add(&den, &t1, &t2, false, 120))
            return ABN_RANGE;
        if (hp_is_zero(&den)) return ABN_DIV_ZERO;
        hp_neg(&pi2);
        if (!hp_div(&tr, &pr2, &den, 60) || !hp_div(&ti, &pi2, &den, 60)) return ABN_RANGE;
        pr2 = tr;
        pi2 = ti;
    }
    return round_parts(out, &pr2, &pi2);
}

/* ln z1 to p digits into (e5, e6). */
static abn_status ln_hp(const cplx *z, int p)
{
    abn_status st;
    if (!hp_atan2(&e6, &z->im, &z->re, p)) return ABN_RANGE;
    if ((st = sum_squares(z)) != ABN_OK) return st;
    if (!hp_ln(&e3, &hs, &ht, p)) return ABN_RANGE;
    hp_set_int(&e4, 5);
    e4.e = -1;
    if (!hp_mul(&e5, &e3, &e4, p)) return ABN_RANGE;
    return ABN_OK;
}

static long lead_hp2(const hp *a, const hp *b)
{
    long la = hp_is_zero(a) ? -100000 : hp_lead(a), lb = hp_is_zero(b) ? -100000 : hp_lead(b);
    return la > lb ? la : lb;
}

/* z1^z2 (UG p.9-3). 0^z2 is 0 for Re z2 > 0 and an error otherwise; an integer power (a real
 * z2 with |z2| < 2^31) is formed by repeated multiplication; any other is e^w, w = z2 ln z1 with
 * the principal ln (unit 009 rule 5). e^w is as accurate as w is absolutely, so ln z1 and w are
 * carried to 45 digits past the units of w; beyond HP_PMAX digits (|w| past about 10^175) the
 * angle of the result cannot be placed and the power is a range error (ours). */
abn_status cx_pow(cplx *out, const cplx *z1, const cplx *z2)
{
    static hp c, d, wr, wi, t1, t2, E, sn, cs;
    abn_status st;
    if (abn_is_zero(&z1->re) && abn_is_zero(&z1->im)) {
        abn zero = abn_zero();
        if (abn_cmp(&z2->re, &zero) <= 0) return ABN_DOMAIN;
        out->re = out->im = abn_zero();
        return ABN_OK;
    }
    if (abn_is_zero(&z2->im) && abn_is_integer(&z2->re)) {
        abn lim = K("2147483647"), a = abn_abs(&z2->re);
        if (abn_cmp(&a, &lim) <= 0) {
            char dg[ABN_DIGITS + 1];
            bool neg;
            int lead;
            long n = 0;
            abn_digits(&z2->re, &neg, dg, &lead);
            for (int i = 0; dg[0] && i <= lead; i++) n = n * 10 + (i < (int)strlen(dg) ? dg[i] - '0' : 0);
            return cx_powi(out, z1, neg ? -n : n);
        }
    }
    if (!hp_from_abn(&c, &z2->re) || !hp_from_abn(&d, &z2->im)) return ABN_RANGE;
    if ((st = ln_hp(z1, HP_WORK)) != ABN_OK) return st;
    long L = lead_hp2(&c, &d) + lead_hp2(&e5, &e6) + 1;
    int p = 45 + (L > 0 ? (int)L : 0);
    if (p > HP_PMAX) return ABN_RANGE;
    if (p > HP_WORK && (st = ln_hp(z1, p)) != ABN_OK) return st;
    /* w = (c + di)(lr + li i) */
    if (!hp_mul(&t1, &c, &e5, p + 5) || !hp_mul(&t2, &d, &e6, p + 5) || !hp_add(&wr, &t1, &t2, true, p))
        return ABN_RANGE;
    if (!hp_mul(&t1, &c, &e6, p + 5) || !hp_mul(&t2, &d, &e5, p + 5) || !hp_add(&wi, &t1, &t2, false, p))
        return ABN_RANGE;
    if (!hp_exp(&E, &wr, HP_WORK)) {                        /* |Re w| >= 10^7: beyond the range */
        if (!wr.neg) return ABN_OVERFLOW;
        out->re = out->im = abn_zero();                     /* below it: 0 (unit 026) */
        return ABN_OK;
    }
    if (!hp_sincos(&sn, &cs, &wi, HP_WORK)) return ABN_RANGE;
    if (!hp_mul(&pr, &E, &cs, HP_WORK) || !hp_mul(&pi_, &E, &sn, HP_WORK)) return ABN_RANGE;
    return round_parts(out, &pr, &pi_);
}
