/* stat: statistics on the six registers (unit 006).
 *
 * Accumulation (ours, unit 006 rule 2): each register changes by one correctly rounded operation,
 * a fused multiply-add for the squares and the product, and a data point goes in or out whole.
 * Results (ours, rule 3): every statistic is its textbook formula evaluated exactly on the
 * register values as stored, then rounded once; the square roots take the correctly rounded
 * radicand. Formulas from the work unit; the guide's chapter 12 defines the quantities.
 */
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

/* The registers as exact numbers. */
static xnum N, X, Y, X2, Y2, XY;

static abn_status load(const abn r[6])
{
    if (!xn_from_abn(&N, &r[ST_N]) || !xn_from_abn(&X, &r[ST_SX]) || !xn_from_abn(&Y, &r[ST_SY]) ||
        !xn_from_abn(&X2, &r[ST_SX2]) || !xn_from_abn(&Y2, &r[ST_SY2]) || !xn_from_abn(&XY, &r[ST_SXY]))
        return ABN_RANGE;
    return ABN_OK;
}

/* n*a - b*c, exactly. */
static bool ndiff(xnum *out, const xnum *n, const xnum *a, const xnum *b, const xnum *c)
{
    static xnum p, q;
    return xn_mul(&p, n, a) && xn_mul(&q, b, c) && xn_add(out, &p, &q, true);
}

/* sqrt(num / den) for num / den >= 0: the radicand rounded once, then a correctly rounded root. */
static abn_status sqrt_ratio(abn *out, const xnum *num, const xnum *den)
{
    abn q;
    abn_status st;
    if (xn_sign(den) == 0) return ABN_DIV_ZERO;
    if (xn_sign(num) * xn_sign(den) < 0) return ABN_DOMAIN;     /* a negative variance */
    if ((st = xn_div_to_abn(&q, num, den)) != ABN_OK) return st;
    return abn_sqrt(out, &q);
}

abn_status st_mean(abn *out, const abn r[6], bool of_y)
{
    abn_status st = load(r);
    if (st != ABN_OK) return st;
    if (xn_sign(&N) == 0) return ABN_DIV_ZERO;     /* no data */
    return xn_div_to_abn(out, of_y ? &Y : &X, &N);
}

/* The mean of x weighted by y: sum xy / sum y. */
abn_status st_wmean(abn *out, const abn r[6])
{
    abn_status st = load(r);
    if (st != ABN_OK) return st;
    if (xn_sign(&Y) == 0) return ABN_DIV_ZERO;
    return xn_div_to_abn(out, &XY, &Y);
}

/* Sample: (n sum x^2 - (sum x)^2) / (n (n - 1)); population: the same over n^2. */
abn_status st_sdev(abn *out, const abn r[6], bool of_y, bool population)
{
    static xnum D, den, nm1, one;
    abn_status st = load(r);
    if (st != ABN_OK) return st;
    if (!ndiff(&D, &N, of_y ? &Y2 : &X2, of_y ? &Y : &X, of_y ? &Y : &X)) return ABN_RANGE;
    if (population) {
        if (xn_sign(&N) == 0) return ABN_DIV_ZERO;
        if (!xn_mul(&den, &N, &N)) return ABN_RANGE;
    } else {
        xn_set_int(&one, 1);
        if (!xn_add(&nm1, &N, &one, true)) return ABN_RANGE;
        if (xn_sign(&nm1) <= 0) return ABN_DIV_ZERO;   /* n <= 1 */
        if (!xn_mul(&den, &N, &nm1)) return ABN_RANGE;
    }
    return sqrt_ratio(out, &D, &den);
}

/* m = (n sum xy - sum x sum y) / (n sum x^2 - (sum x)^2). */
abn_status st_slope(abn *out, const abn r[6])
{
    static xnum C, D;
    abn_status st = load(r);
    if (st != ABN_OK) return st;
    if (!ndiff(&C, &N, &XY, &X, &Y) || !ndiff(&D, &N, &X2, &X, &X)) return ABN_RANGE;
    if (xn_sign(&D) == 0) return ABN_DIV_ZERO;     /* all x equal */
    return xn_div_to_abn(out, &C, &D);
}

/* b = (sum y sum x^2 - sum x sum xy) / (n sum x^2 - (sum x)^2). */
static bool intercept_num(xnum *B)
{
    static xnum p, q;
    return xn_mul(&p, &Y, &X2) && xn_mul(&q, &X, &XY) && xn_add(B, &p, &q, true);
}

abn_status st_yint(abn *out, const abn r[6])
{
    static xnum B, D;
    abn_status st = load(r);
    if (st != ABN_OK) return st;
    if (!intercept_num(&B) || !ndiff(&D, &N, &X2, &X, &X)) return ABN_RANGE;
    if (xn_sign(&D) == 0) return ABN_DIV_ZERO;
    return xn_div_to_abn(out, &B, &D);
}

/* r = C / sqrt(Dx Dy), taken as sign(C) sqrt(C^2 / (Dx Dy)) so that one rounding precedes the
 * root. */
abn_status st_corr(abn *out, const abn r[6])
{
    static xnum C, Dx, Dy, C2, DD;
    abn v;
    abn_status st = load(r);
    if (st != ABN_OK) return st;
    if (!ndiff(&C, &N, &XY, &X, &Y) || !ndiff(&Dx, &N, &X2, &X, &X) || !ndiff(&Dy, &N, &Y2, &Y, &Y))
        return ABN_RANGE;
    if (xn_sign(&Dx) == 0 || xn_sign(&Dy) == 0) return ABN_DIV_ZERO;   /* all x or all y equal */
    if (xn_sign(&Dx) < 0 || xn_sign(&Dy) < 0) return ABN_DOMAIN;
    if (!xn_mul(&C2, &C, &C) || !xn_mul(&DD, &Dx, &Dy)) return ABN_RANGE;
    if ((st = sqrt_ratio(&v, &C2, &DD)) != ABN_OK) return st;
    *out = xn_sign(&C) < 0 ? abn_neg(&v) : v;
    return ABN_OK;
}

/* y-hat = m x + b = (x C + B) / D, one exact quotient. */
abn_status st_yhat(abn *out, const abn r[6], const abn *x)
{
    static xnum C, D, B, xv, p, num;
    abn_status st = load(r);
    if (st != ABN_OK) return st;
    if (!ndiff(&C, &N, &XY, &X, &Y) || !ndiff(&D, &N, &X2, &X, &X) || !intercept_num(&B)) return ABN_RANGE;
    if (xn_sign(&D) == 0) return ABN_DIV_ZERO;
    if (!xn_from_abn(&xv, x) || !xn_mul(&p, &xv, &C) || !xn_add(&num, &p, &B, false)) return ABN_RANGE;
    return xn_div_to_abn(out, &num, &D);
}

/* x-hat = (y - b) / m = (y D - B) / C, one exact quotient; m = 0 is an error. */
abn_status st_xhat(abn *out, const abn r[6], const abn *y)
{
    static xnum C, D, B, yv, p, num;
    abn_status st = load(r);
    if (st != ABN_OK) return st;
    if (!ndiff(&C, &N, &XY, &X, &Y) || !ndiff(&D, &N, &X2, &X, &X) || !intercept_num(&B)) return ABN_RANGE;
    if (xn_sign(&D) == 0 || xn_sign(&C) == 0) return ABN_DIV_ZERO;
    if (!xn_from_abn(&yv, y) || !xn_mul(&p, &yv, &D) || !xn_add(&num, &p, &B, true)) return ABN_RANGE;
    return xn_div_to_abn(out, &num, &C);
}
