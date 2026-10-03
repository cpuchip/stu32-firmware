/* stat: statistics on the six registers (unit 006).
 *
 * Accumulation (ours, unit 006 rule 2): each register changes by one correctly rounded operation,
 * a fused multiply-add for the squares and the product, and a data point goes in or out whole.
 * Results (ours, rule 3): every statistic is its textbook formula evaluated exactly on the
 * register values as stored, then rounded once; the square roots are the exact ratio's root,
 * correctly rounded (unit 010b). Formulas from the work unit; the guide's chapter 12 defines the quantities.
 */
#include "arena.h"
#include "stat.h"
#include "xnum.h"

abn_status st_accumulate(abn r[6], const abn *x, const abn *y, bool remove)
{
    abn t[6], one = abn_from_int(1), nx = abn_neg(x);
    abn_status st;
    const abn *sx = remove ? &nx : x;               /* -x for removal: -x*x and -x*y */
    if ((st = remove ? abn_sub(&t[ST_N], &r[ST_N], &one) : abn_add(&t[ST_N], &r[ST_N], &one)) != ABN_OK) return st;
    if ((st = remove ? abn_sub(&t[ST_SX], &r[ST_SX], x) : abn_add(&t[ST_SX], &r[ST_SX], x)) != ABN_OK) return st;
    if ((st = remove ? abn_sub(&t[ST_SY], &r[ST_SY], y) : abn_add(&t[ST_SY], &r[ST_SY], y)) != ABN_OK) return st;
    if ((st = abn_fma(&t[ST_SX2], sx, x, &r[ST_SX2])) != ABN_OK) return st;
    {
        abn ny = abn_neg(y);
        if ((st = abn_fma(&t[ST_SY2], remove ? &ny : y, y, &r[ST_SY2])) != ABN_OK) return st;
    }
    if ((st = abn_fma(&t[ST_SXY], sx, y, &r[ST_SXY])) != ABN_OK) return st;
    for (int i = 0; i < 6; i++) r[i] = t[i];
    return ABN_OK;
}

/* The registers as exact numbers, taken from the wide pool (core/arena.h) by each statistic. */
typedef struct {
    xnum *N, *X, *Y, *X2, *Y2, *XY;
} regs;

static abn_status load(regs *g, const abn r[6])
{
    if (!ar_xn(6, &g->N, &g->X, &g->Y, &g->X2, &g->Y2, &g->XY)) return ABN_RANGE;
    if (!xn_from_abn(g->N, &r[ST_N]) || !xn_from_abn(g->X, &r[ST_SX]) || !xn_from_abn(g->Y, &r[ST_SY]) ||
        !xn_from_abn(g->X2, &r[ST_SX2]) || !xn_from_abn(g->Y2, &r[ST_SY2]) || !xn_from_abn(g->XY, &r[ST_SXY]))
        return ABN_RANGE;
    return ABN_OK;
}

/* n*a - b*c, exactly. */
static bool ndiff(xnum *out, const xnum *n, const xnum *a, const xnum *b, const xnum *c)
{
    int m = ar_mark();
    xnum *p, *q;
    bool ok = ar_xn(2, &p, &q) && xn_mul(p, n, a) && xn_mul(q, b, c) && xn_add(out, p, q, true);
    ar_release(m);
    return ok;
}

/* sqrt(num / den) for num / den >= 0: the exact ratio's root, correctly rounded (unit 010b; it
 * was the radicand rounded once, then a correctly rounded root). */
static abn_status sqrt_ratio(abn *out, const xnum *num, const xnum *den)
{
    if (xn_sign(den) == 0) return ABN_DIV_ZERO;
    if (xn_sign(num) * xn_sign(den) < 0) return ABN_DOMAIN;     /* a negative variance */
    return xn_sqrt_ratio_to_abn(out, num, den);
}

static abn_status mean_(abn *out, const abn r[6], bool of_y)
{
    regs g;
    abn_status st = load(&g, r);
    if (st != ABN_OK) return st;
    if (xn_sign(g.N) == 0) return ABN_DIV_ZERO;     /* no data */
    return xn_div_to_abn(out, of_y ? g.Y : g.X, g.N);
}

/* The mean of x weighted by y: sum xy / sum y. */
static abn_status wmean_(abn *out, const abn r[6])
{
    regs g;
    abn_status st = load(&g, r);
    if (st != ABN_OK) return st;
    if (xn_sign(g.Y) == 0) return ABN_DIV_ZERO;
    return xn_div_to_abn(out, g.XY, g.Y);
}

/* Sample: (n sum x^2 - (sum x)^2) / (n (n - 1)); population: the same over n^2. */
static abn_status sdev_(abn *out, const abn r[6], bool of_y, bool population)
{
    regs g;
    xnum *D, *den, *nm1, *one;
    abn_status st = load(&g, r);
    if (st != ABN_OK) return st;
    if (!ar_xn(4, &D, &den, &nm1, &one)) return ABN_RANGE;
    if (!ndiff(D, g.N, of_y ? g.Y2 : g.X2, of_y ? g.Y : g.X, of_y ? g.Y : g.X)) return ABN_RANGE;
    if (population) {
        if (xn_sign(g.N) == 0) return ABN_DIV_ZERO;
        if (!xn_mul(den, g.N, g.N)) return ABN_RANGE;
    } else {
        xn_set_int(one, 1);
        if (!xn_add(nm1, g.N, one, true)) return ABN_RANGE;
        if (xn_sign(nm1) <= 0) return ABN_DIV_ZERO;   /* n <= 1 */
        if (!xn_mul(den, g.N, nm1)) return ABN_RANGE;
    }
    return sqrt_ratio(out, D, den);
}

/* m = (n sum xy - sum x sum y) / (n sum x^2 - (sum x)^2). */
static abn_status slope_(abn *out, const abn r[6])
{
    regs g;
    xnum *C, *D;
    abn_status st = load(&g, r);
    if (st != ABN_OK) return st;
    if (!ar_xn(2, &C, &D)) return ABN_RANGE;
    if (!ndiff(C, g.N, g.XY, g.X, g.Y) || !ndiff(D, g.N, g.X2, g.X, g.X)) return ABN_RANGE;
    if (xn_sign(D) == 0) return ABN_DIV_ZERO;     /* all x equal */
    return xn_div_to_abn(out, C, D);
}

/* b = (sum y sum x^2 - sum x sum xy) / (n sum x^2 - (sum x)^2). */
static bool intercept_num(xnum *B, const regs *g)
{
    int m = ar_mark();
    xnum *p, *q;
    bool ok = ar_xn(2, &p, &q) && xn_mul(p, g->Y, g->X2) && xn_mul(q, g->X, g->XY) && xn_add(B, p, q, true);
    ar_release(m);
    return ok;
}

static abn_status yint_(abn *out, const abn r[6])
{
    regs g;
    xnum *B, *D;
    abn_status st = load(&g, r);
    if (st != ABN_OK) return st;
    if (!ar_xn(2, &B, &D)) return ABN_RANGE;
    if (!intercept_num(B, &g) || !ndiff(D, g.N, g.X2, g.X, g.X)) return ABN_RANGE;
    if (xn_sign(D) == 0) return ABN_DIV_ZERO;
    return xn_div_to_abn(out, B, D);
}

/* r = C / sqrt(Dx Dy), taken as sign(C) sqrt(C^2 / (Dx Dy)): the exact ratio's root, correctly
 * rounded. */
static abn_status corr_(abn *out, const abn r[6])
{
    regs g;
    xnum *C, *Dx, *Dy, *C2, *DD;
    abn v;
    abn_status st = load(&g, r);
    if (st != ABN_OK) return st;
    if (!ar_xn(5, &C, &Dx, &Dy, &C2, &DD)) return ABN_RANGE;
    if (!ndiff(C, g.N, g.XY, g.X, g.Y) || !ndiff(Dx, g.N, g.X2, g.X, g.X) || !ndiff(Dy, g.N, g.Y2, g.Y, g.Y))
        return ABN_RANGE;
    if (xn_sign(Dx) == 0 || xn_sign(Dy) == 0) return ABN_DIV_ZERO;   /* all x or all y equal */
    if (xn_sign(Dx) < 0 || xn_sign(Dy) < 0) return ABN_DOMAIN;
    if (!xn_mul(C2, C, C) || !xn_mul(DD, Dx, Dy)) return ABN_RANGE;
    if ((st = sqrt_ratio(&v, C2, DD)) != ABN_OK) return st;
    *out = xn_sign(C) < 0 ? abn_neg(&v) : v;
    return ABN_OK;
}

/* y-hat = m x + b = (x C + B) / D, one exact quotient. */
static abn_status yhat_(abn *out, const abn r[6], const abn *x)
{
    regs g;
    xnum *C, *D, *B, *xv, *p, *num;
    abn_status st = load(&g, r);
    if (st != ABN_OK) return st;
    if (!ar_xn(6, &C, &D, &B, &xv, &p, &num)) return ABN_RANGE;
    if (!ndiff(C, g.N, g.XY, g.X, g.Y) || !ndiff(D, g.N, g.X2, g.X, g.X) || !intercept_num(B, &g)) return ABN_RANGE;
    if (xn_sign(D) == 0) return ABN_DIV_ZERO;
    if (!xn_from_abn(xv, x) || !xn_mul(p, xv, C) || !xn_add(num, p, B, false)) return ABN_RANGE;
    return xn_div_to_abn(out, num, D);
}

/* x-hat = (y - b) / m = (y D - B) / C, one exact quotient; m = 0 is an error. */
static abn_status xhat_(abn *out, const abn r[6], const abn *y)
{
    regs g;
    xnum *C, *D, *B, *yv, *p, *num;
    abn_status st = load(&g, r);
    if (st != ABN_OK) return st;
    if (!ar_xn(6, &C, &D, &B, &yv, &p, &num)) return ABN_RANGE;
    if (!ndiff(C, g.N, g.XY, g.X, g.Y) || !ndiff(D, g.N, g.X2, g.X, g.X) || !intercept_num(B, &g)) return ABN_RANGE;
    if (xn_sign(D) == 0 || xn_sign(C) == 0) return ABN_DIV_ZERO;
    if (!xn_from_abn(yv, y) || !xn_mul(p, yv, D) || !xn_add(num, p, B, true)) return ABN_RANGE;
    return xn_div_to_abn(out, num, C);
}

/* The public statistics: each releases its pool numbers on every return. */
#define POOLED(call)                    \
    do {                                \
        int m_ = ar_mark();             \
        abn_status st_ = (call);        \
        ar_release(m_);                 \
        return st_;                     \
    } while (0)

abn_status st_mean(abn *out, const abn r[6], bool of_y) { POOLED(mean_(out, r, of_y)); }
abn_status st_wmean(abn *out, const abn r[6]) { POOLED(wmean_(out, r)); }
abn_status st_sdev(abn *out, const abn r[6], bool of_y, bool population) { POOLED(sdev_(out, r, of_y, population)); }
abn_status st_slope(abn *out, const abn r[6]) { POOLED(slope_(out, r)); }
abn_status st_yint(abn *out, const abn r[6]) { POOLED(yint_(out, r)); }
abn_status st_corr(abn *out, const abn r[6]) { POOLED(corr_(out, r)); }
abn_status st_yhat(abn *out, const abn r[6], const abn *x) { POOLED(yhat_(out, r, x)); }
abn_status st_xhat(abn *out, const abn r[6], const abn *y) { POOLED(xhat_(out, r, y)); }
