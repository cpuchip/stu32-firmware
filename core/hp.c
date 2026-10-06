/* hp: decimal numbers to a chosen precision (see hp.h). */
#include <string.h>

#include "hp.h"

/* pi and ln 10 to 241 digits, truncated (mpmath at 400 and at 1000 digits agree). */
static const char PI_241[] =
    "3141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342"
    "1170679821480865132823066470938446095505822317253594081284811174502841027019385211055596446229"
    "48954930381964428810975665933446128475648233786783165";
static const char LN10_241[] =
    "2302585092994045684017991454684364207601101488628772976033327900967572609677352480235997205089"
    "5982983419677840422862486334095254650828067566662873690987816894829072083255546808437998948262"
    "33198528393505308965377732628846163366222287698219886";

void hp_set_int(hp *r, int v)
{
    sxn_set_int(r, v);
}

bool hp_from_abn(hp *r, const abn *x)
{
    return sxn_from_abn(r, x);
}

bool hp_is_zero(const hp *x)
{
    return sb_is_zero(&x->m);
}

long hp_lead(const hp *x)
{
    return x->e + sb_ndigits(&x->m) - 1;
}

void hp_trim(hp *x, int p)
{
    int n = sb_ndigits(&x->m);
    if (n > p) {
        sb_div_pow10(&x->m, n - p);
        x->e += n - p;
    }
    if (sb_is_zero(&x->m)) {
        x->neg = false;
        x->e = 0;
    }
}

void hp_neg(hp *x)
{
    if (!hp_is_zero(x)) x->neg = !x->neg;
}

bool hp_mul(hp *r, const hp *a, const hp *b, int p)
{
    if (!sxn_mul(r, a, b)) return false;
    hp_trim(r, p);
    return true;
}

bool hp_add(hp *r, const hp *a, const hp *b, bool subtract, int p)
{
    if (!hp_is_zero(a) && !hp_is_zero(b)) {
        /* an operand below the other's last kept digit changes nothing kept */
        long la = hp_lead(a), lb = hp_lead(b);
        if (la - lb > p + 2) {
            *r = *a;
            hp_trim(r, p);
            return true;
        }
        if (lb - la > p + 2) {
            *r = *b;
            if (subtract) hp_neg(r);
            hp_trim(r, p);
            return true;
        }
    }
    if (!sxn_add(r, a, b, subtract)) return false;
    hp_trim(r, p);
    return true;
}

bool hp_div(hp *r, const hp *a, const hp *b, int p)
{
    static sbig num;
    bool inexact;
    if (hp_is_zero(b)) return false;
    if (hp_is_zero(a)) {
        hp_set_int(r, 0);
        return true;
    }
    int k = p + 2 + sb_ndigits(&b->m) - sb_ndigits(&a->m);
    if (k < 0) k = 0;
    num = a->m;
    if (!sb_mul_pow10(&num, k) || !sb_div(&r->m, &num, &b->m, &inexact)) return false;
    r->e = a->e - b->e - k;
    r->neg = a->neg != b->neg;
    hp_trim(r, p);
    return true;
}

/* a / d for a small integer d. */
static bool div_u32(hp *r, const hp *a, uint32_t d, int p)
{
    *r = *a;
    int k = p + 12 - sb_ndigits(&r->m);
    if (k > 0) {
        if (!sb_mul_pow10(&r->m, k)) return false;
        r->e -= k;
    }
    sb_divmod_u32(&r->m, d);
    hp_trim(r, p);
    return true;
}

abn_status hp_round(abn *out, const hp *x)
{
    if (hp_is_zero(x)) {
        *out = abn_zero();
        return ABN_OK;
    }
    return sb_to_abn(out, &x->m, x->neg, x->e, false);
}

/* A stored constant to p digits (at most 241). */
static bool constant(hp *r, const char *digits, int p)
{
    char buf[242];
    int n = p < 241 ? p : 241;
    memcpy(buf, digits, (size_t)n);
    buf[n] = '\0';
    r->neg = false;
    r->e = -(long)(n - 1);
    return sb_from_digits(&r->m, buf, 0);
}

bool hp_pi(hp *r, int p)
{
    return constant(r, PI_241, p);
}

/* The integer nearest q (halves away from zero) into k; its last limb into *low. */
static bool nearest_int(hp *k, const hp *q, uint32_t *low)
{
    *k = *q;
    if (k->e < 0) {
        long drop = -k->e - 1;
        if (drop >= sb_ndigits(&k->m)) {
            hp_set_int(k, 0);
        } else {
            sb_div_pow10(&k->m, (int)drop);
            uint32_t d = sb_divmod_u32(&k->m, 10);
            if (d >= 5 && !sb_add_u32(&k->m, 1)) return false;
        }
        k->e = 0;
    } else if (k->e > 0) {
        if (!sb_mul_pow10(&k->m, (int)k->e)) return false;
        k->e = 0;
    }
    if (sb_is_zero(&k->m)) k->neg = false;
    *low = k->m.n ? k->m.l[0] : 0;
    return true;
}

/* e^x - 1 by its series, for |x| < 1. */
static bool expm1_series(hp *r, const hp *x, int w)
{
    static hp sum, term, t2;
    sum = *x;
    term = *x;
    for (uint32_t n = 2; n < 1000; n++) {
        if (!hp_mul(&t2, &term, x, w) || !div_u32(&term, &t2, n, w)) return false;
        if (hp_is_zero(&term) || hp_lead(&term) < hp_lead(&sum) - w) break;
        if (!hp_add(&t2, &sum, &term, false, w)) return false;
        sum = t2;
    }
    *r = sum;
    return true;
}

bool hp_exp(hp *r, const hp *x, int p)
{
    static hp ln10, q, k, t, y, e, e2, two, one;
    int w = p + 10;
    uint32_t low;
    hp_set_int(&one, 1);
    if (hp_is_zero(x)) {
        hp_set_int(r, 1);
        return true;
    }
    if (hp_lead(x) >= 7) return false;
    if (hp_lead(x) < 0) {
        if (!expm1_series(&e, x, w) || !hp_add(r, &e, &one, false, p)) return false;
        return true;
    }
    /* x = k ln 10 + y with |y| <= ln 10 / 2; e^x = 10^k e^y. k has at most 7 digits. */
    if (!constant(&ln10, LN10_241, w + 8) || !hp_div(&q, x, &ln10, 14) || !nearest_int(&k, &q, &low))
        return false;
    if (!sxn_mul(&t, &k, &ln10) || !hp_add(&y, x, &t, true, w)) return false;
    long kl = (long)(k.m.n > 1 ? k.m.l[1] * 1000000000L : 0) + (long)low;
    if (k.neg) kl = -kl;
    if (hp_is_zero(&y)) {
        hp_set_int(r, 1);
        r->e += kl;
        return true;
    }
    /* e^y = (e^(y / 2^10))^(2^10): the series on y / 1024, then (1 + e)^2 - 1 = e (e + 2) ten times */
    if (!div_u32(&t, &y, 1024, w) || !expm1_series(&e, &t, w)) return false;
    hp_set_int(&two, 2);
    for (int i = 0; i < 10; i++) {
        if (!hp_add(&t, &e, &two, false, w) || !hp_mul(&e2, &e, &t, w)) return false;
        e = e2;
    }
    if (!hp_add(r, &e, &one, false, p)) return false;
    r->e += kl;
    return true;
}

bool hp_expm1(hp *r, const hp *x, int p)
{
    static hp e, one;
    if (hp_is_zero(x)) {
        hp_set_int(r, 0);
        return true;
    }
    if (hp_lead(x) < 0) {
        if (!expm1_series(&e, x, p + 5)) return false;
        *r = e;
        hp_trim(r, p);
        return true;
    }
    hp_set_int(&one, 1);
    return hp_exp(&e, x, p + 5) && hp_add(r, &e, &one, true, p);
}

bool hp_sincos(hp *s, hp *c, const hp *x, int p)
{
    static hp pih, pi, q, k, t, r, r2, term, t2, ss, cc;
    int w = p + 5;
    uint32_t low = 0;
    if (hp_is_zero(x)) {
        hp_set_int(s, 0);
        hp_set_int(c, 1);
        return true;
    }
    long lx = hp_lead(x);
    long need = w + (lx > 0 ? lx : 0) + 5;
    /* x = k pi/2 + r with |r| <= pi/4, pi carried far enough that r keeps w digits; a second pass
       with more digits of pi when r is small (x near a multiple of pi/2) */
    for (int pass = 0; pass < 2; pass++) {
        if (need > 238) return false;
        if (!constant(&pi, PI_241, (int)need) || !div_u32(&pih, &pi, 2, (int)need + 2)) return false;
        if (!hp_div(&q, x, &pih, (int)(lx > 0 ? lx : 0) + 8) || !nearest_int(&k, &q, &low)) return false;
        if (!sxn_mul(&t, &k, &pih) || !sxn_add(&r, x, &t, true)) return false;
        /* with k = 0 nothing was subtracted and r is x, exact */
        if (hp_is_zero(&k) || hp_is_zero(&r) || hp_lead(&r) >= -4 || pass == 1) break;
        need += -hp_lead(&r);
    }
    hp_trim(&r, w);
    unsigned quad = (unsigned)(low % 4);
    if (k.neg) quad = (4 - quad) % 4;
    /* the series for sin r and cos r */
    if (!hp_mul(&r2, &r, &r, w)) return false;
    ss = r;
    term = r;
    for (uint32_t n = 1; n < 500; n++) {
        if (!hp_mul(&t2, &term, &r2, w) || !div_u32(&term, &t2, (2 * n) * (2 * n + 1), w)) return false;
        hp_neg(&term);
        if (hp_is_zero(&term) || hp_lead(&term) < hp_lead(&ss) - w) break;
        if (!hp_add(&t2, &ss, &term, false, w)) return false;
        ss = t2;
    }
    hp_set_int(&cc, 1);
    hp_set_int(&term, 1);
    for (uint32_t n = 1; n < 500; n++) {
        if (!hp_mul(&t2, &term, &r2, w) || !div_u32(&term, &t2, (2 * n - 1) * (2 * n), w)) return false;
        hp_neg(&term);
        if (hp_is_zero(&term) || hp_lead(&term) < -w) break;
        if (!hp_add(&t2, &cc, &term, false, w)) return false;
        cc = t2;
    }
    if (hp_is_zero(&r)) hp_set_int(&ss, 0);
    switch (quad) {
    case 0: *s = ss; *c = cc; break;
    case 1: *s = cc; *c = ss; hp_neg(c); break;
    case 2: *s = ss; *c = cc; hp_neg(s); hp_neg(c); break;
    default: *s = cc; *c = ss; hp_neg(s); break;
    }
    hp_trim(s, p);
    hp_trim(c, p);
    return true;
}

bool hp_sinhcosh(hp *sh, hp *ch, const hp *x, int p)
{
    static hp e, E, t, u, two, one, Ei;
    int w = p + 5;
    hp_set_int(&one, 1);
    hp_set_int(&two, 2);
    if (hp_is_zero(x)) {
        hp_set_int(sh, 0);
        hp_set_int(ch, 1);
        return true;
    }
    if (hp_lead(x) < 0) {
        /* with e = e^x - 1: sinh x = e (e + 2) / (2 (e + 1)), cosh x = 1 + e^2 / (2 (e + 1)) */
        if (!expm1_series(&e, x, w) || !hp_add(&E, &e, &one, false, w) || !hp_mul(&t, &E, &two, w))
            return false;
        if (!hp_add(&u, &e, &two, false, w) || !hp_mul(&Ei, &e, &u, w) || !hp_div(sh, &Ei, &t, p)) return false;
        if (!hp_mul(&u, &e, &e, w) || !hp_div(&Ei, &u, &t, w) || !hp_add(ch, &Ei, &one, false, p)) return false;
        return true;
    }
    if (!hp_exp(&E, x, w) || !hp_div(&Ei, &one, &E, w)) return false;
    if (!hp_add(&t, &E, &Ei, true, w) || !div_u32(sh, &t, 2, p)) return false;
    if (!hp_add(&t, &E, &Ei, false, w) || !div_u32(ch, &t, 2, p)) return false;
    return true;
}

bool hp_ln(hp *r, const hp *s, const hp *t, int p)
{
    static hp y, e, u, v, my, m, one, ln10, L;
    int w = p + 5;
    abn a, y0;
    hp_set_int(&one, 1);
    if (hp_is_zero(t)) {
        hp_set_int(r, 0);
        return true;
    }
    bool near = hp_lead(t) < -1;    /* |s - 1| < 0.1: the form that keeps a small ln small */
    if (near) {
        abn_status st = hp_round(&a, t);
        if (st == ABN_UNDERFLOW) {  /* t below the engine's range: ln(1 + t) = t to far beyond p */
            *r = *t;
            hp_trim(r, p);
            return true;
        }
        if (st != ABN_OK || abn_log1p(&y0, &a) != ABN_OK || !hp_from_abn(&y, &y0)) return false;
    } else {
        /* s = m 10^L with m in [1, 10): ln s = ln m + L ln 10, from the engine as a start */
        long lead = hp_lead(s);
        m = *s;
        m.e -= lead;
        if (hp_round(&a, &m) != ABN_OK || abn_ln(&y0, &a) != ABN_OK || !hp_from_abn(&y, &y0)) return false;
        if (lead != 0) {
            if (!constant(&ln10, LN10_241, 60)) return false;
            hp_set_int(&L, (int)lead);
            if (!hp_mul(&u, &L, &ln10, 60) || !hp_add(&v, &y, &u, false, 60)) return false;
            y = v;
        }
    }
    /* Newton on e^y = s: y += s e^-y - 1, written s (e^-y - 1) + t near s = 1. Each step doubles
       the correct digits, from the engine's 30 or more. */
    for (int digits = 30; digits < w + 5; digits *= 2) {
        my = y;
        hp_neg(&my);
        if (near) {
            if (!hp_expm1(&e, &my, w) || !hp_mul(&u, s, &e, w) || !hp_add(&v, &u, t, false, w)) return false;
        } else {
            if (!hp_exp(&e, &my, w) || !hp_mul(&u, s, &e, w) || !hp_add(&v, &u, &one, true, w)) return false;
        }
        if (!hp_add(&u, &y, &v, false, w)) return false;
        y = u;
    }
    *r = y;
    hp_trim(r, p);
    return true;
}

/* The Stirling series for ln gamma (unit 021): c_k = B_2k / (2k (2k - 1)) to 70 digits, sign and
   the exponent of the leading digit, from mpmath at 150 digits; 31 terms reach 10^-66 at w = 40. */
static const struct { char neg; short lead; const char *d; } STIRLING[] = {
    {0, -2, "8333333333333333333333333333333333333333333333333333333333333333333333"},  /* k = 1 */
    {1, -3, "2777777777777777777777777777777777777777777777777777777777777777777778"},  /* k = 2 */
    {0, -4, "7936507936507936507936507936507936507936507936507936507936507936507937"},  /* k = 3 */
    {1, -4, "5952380952380952380952380952380952380952380952380952380952380952380952"},  /* k = 4 */
    {0, -4, "8417508417508417508417508417508417508417508417508417508417508417508418"},  /* k = 5 */
    {1, -3, "1917526917526917526917526917526917526917526917526917526917526917526918"},  /* k = 6 */
    {0, -3, "6410256410256410256410256410256410256410256410256410256410256410256410"},  /* k = 7 */
    {1, -2, "2955065359477124183006535947712418300653594771241830065359477124183007"},  /* k = 8 */
    {0, -1, "1796443723688305731649384900158893966943502547217717496355267253100070"},  /* k = 9 */
    {1, 0, "1392432216905901116427432216905901116427432216905901116427432216905901"},  /* k = 10 */
    {0, 1, "1340286404416839199447895100069013112491373360938578329882677708764665"},  /* k = 11 */
    {1, 2, "1568482846260020173063651324520889738281042628868715825237564367999151"},  /* k = 12 */
    {0, 3, "2193103333333333333333333333333333333333333333333333333333333333333333"},  /* k = 13 */
    {1, 4, "3610877125372498935717326521924223073648361004682843763303533418475947"},  /* k = 14 */
    {0, 5, "6914722688513130671083952507756734675533340716877980504231894665710016"},  /* k = 15 */
    {1, 7, "1523822153940741619228336495888678051865907653383934218848829854522454"},  /* k = 16 */
    {0, 8, "3829007513914141414141414141414141414141414141414141414141414141414141"},  /* k = 17 */
    {1, 10, "1088226603578439108901514916552510537472943487981081966044372059409653"},  /* k = 18 */
    {0, 11, "3473202837650022522522522522522522522522522522522522522522522522522523"},  /* k = 19 */
    {1, 13, "1236960214226927445425171034927132488108097864195425171034927132488108"},  /* k = 20 */
    {0, 14, "4887880647930793350758151625180229021084705389056738218070362953273576"},  /* k = 21 */
    {1, 16, "2132033396091937389697505898213683855746545331985170205594876980114594"},  /* k = 22 */
    {0, 18, "1021775296525700077565287628053585500394011032308904649330181245074862"},  /* k = 23 */
    {1, 19, "5357547217330020361082770919196920448484904054365881649986781401049236"},  /* k = 24 */
    {0, 21, "3061578263704883415043151051329622758194186765615337043908472479901051"},  /* k = 25 */
    {1, 23, "1899991742639920405029371429306942902947342458996177087187076088296954"},  /* k = 26 */
    {0, 25, "1276337403382883414923495137769782597654163360882990144823974681637707"},  /* k = 27 */
    {1, 26, "9252847176120416307230242348347622779519331243469174503657262277951933"},  /* k = 28 */
    {0, 28, "7218822595185610297836050187301637922489840420259688769947467538903757"},  /* k = 29 */
    {1, 30, "6045183405995856967743148238754547286066144395967196207406301608096013"},  /* k = 30 */
    {0, 32, "5420670471570094545193477814826100013661202185792349726775956284153005"},  /* k = 31 */
};
static const char HALF_LN_2PI[] =       /* ln(2 pi) / 2 = 0.9189... (mpmath at 100 digits) */
    "9189385332046727417803297364056176398613974736377834128171515404827657";

/* d.ddd... times 10^lead */
static bool digits_at(hp *r, const char *d, long lead, bool neg)
{
    r->neg = neg;
    r->e = lead - (long)(strlen(d) - 1);
    return sb_from_digits(&r->m, d, 0);
}

bool hp_lgamma_big(hp *r, const hp *w, int p)
{
    static hp one, t, lw, u, v, sum, inv, inv2, pw, c, term;
    int wp = p + 5;
    hp_set_int(&one, 1);
    /* (w - 1/2) ln w - w + ln(2 pi) / 2 */
    if (!hp_add(&t, w, &one, true, wp + 10) || !hp_ln(&lw, w, &t, wp)) return false;
    hp_set_int(&u, 5);
    u.e = -1;
    if (!hp_add(&v, w, &u, true, wp + 10) || !hp_mul(&u, &v, &lw, wp)) return false;
    if (!hp_add(&v, &u, w, true, wp) || !digits_at(&c, HALF_LN_2PI, -1, false) ||
        !hp_add(&sum, &v, &c, false, wp))
        return false;
    /* + sum of c_k / w^(2k - 1) */
    if (!hp_div(&inv, &one, w, wp) || !hp_mul(&inv2, &inv, &inv, wp)) return false;
    pw = inv;
    for (size_t k = 0; k < sizeof STIRLING / sizeof STIRLING[0]; k++) {
        if (!digits_at(&c, STIRLING[k].d, STIRLING[k].lead, STIRLING[k].neg) || !hp_mul(&term, &c, &pw, wp))
            return false;
        if (hp_lead(&term) < hp_lead(&sum) - wp) break;
        if (!hp_add(&u, &sum, &term, false, wp) || !hp_mul(&v, &pw, &inv2, wp)) return false;
        sum = u;
        pw = v;
    }
    *r = sum;
    hp_trim(r, p);
    return true;
}

bool hp_atan2(hp *r, const abn *b, const abn *a, int p)
{
    static hp th, sn, cs, hb, ha, t1, t2, num, den, corr;
    int w = p + 5;
    abn t0;
    if (abn_is_zero(b) || abn_is_zero(a)) {
        /* on an axis: 0, pi or +-pi/2 */
        if (abn_is_zero(b) && !abn_is_negative(a)) {
            hp_set_int(r, 0);
            return true;
        }
        if (!constant(&th, PI_241, p + 2)) return false;
        if (abn_is_zero(b)) {
            *r = th;
        } else {
            if (!div_u32(r, &th, 2, p)) return false;
            if (abn_is_negative(b)) hp_neg(r);
        }
        hp_trim(r, p);
        return true;
    }
    if (abn_atan2(&t0, b, a) != ABN_OK || !hp_from_abn(&th, &t0) || !hp_from_abn(&hb, b) ||
        !hp_from_abn(&ha, a))
        return false;
    /* theta += atan of tan(theta_true - theta) = (b cos - a sin) / (a cos + b sin); the first-order
       step leaves an error of the cube of the last, so the digits triple each time */
    for (int digits = 30; digits < w + 5; digits *= 3) {
        if (!hp_sincos(&sn, &cs, &th, w)) return false;
        if (!hp_mul(&t1, &hb, &cs, w) || !hp_mul(&t2, &ha, &sn, w) || !hp_add(&num, &t1, &t2, true, w))
            return false;
        if (!hp_mul(&t1, &ha, &cs, w) || !hp_mul(&t2, &hb, &sn, w) || !hp_add(&den, &t1, &t2, false, w))
            return false;
        if (!hp_div(&corr, &num, &den, w) || !hp_add(&t1, &th, &corr, false, w)) return false;
        th = t1;
    }
    *r = th;
    hp_trim(r, p);
    return true;
}
