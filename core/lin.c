/* lin: the built-in linear solvers (see lin.h). Determinants by cofactor expansion on exact
 * numbers (core/xnum.h) taken from the arena; n is at most 3, so a minor needs at most three
 * levels; the coefficients stay engine numbers until a product needs them. Arena peak: 14 (the
 * singular 3 x 3's rank, measured). */
#include "lin.h"
#include "arena.h"

/* The k x k minor (k = 1..3) of the matrix m (w columns, kept as engine numbers and made exact
 * only where used, to spare the arena) on the given rows and columns, exactly. Arena use: 2
 * numbers for k = 1, 4 for k = 2, 9 for k = 3. */
static bool minor(xnum *r, const abn *m, int w, const int *rows, const int *cols, int k)
{
    int mk = ar_mark();
    bool ok;
    xnum *p, *q;
    if (k == 1) {
        if (!ar_xn(2, &p, &q)) return false;
        xn_set_int(q, 0);
        ok = xn_from_abn(p, &m[rows[0] * w + cols[0]]) && xn_add(r, p, q, false);
    } else if (k == 2) {
        xnum *a, *b;
        if (!ar_xn(4, &p, &q, &a, &b)) return false;
        ok = xn_from_abn(a, &m[rows[0] * w + cols[0]]) && xn_from_abn(b, &m[rows[1] * w + cols[1]]) &&
             xn_mul(p, a, b) && xn_from_abn(a, &m[rows[0] * w + cols[1]]) &&
             xn_from_abn(b, &m[rows[1] * w + cols[0]]) && xn_mul(q, a, b) && xn_add(r, p, q, true);
    } else {
        xnum *sub, *term, *acc, *tmp, *e;
        if (!ar_xn(5, &sub, &term, &acc, &tmp, &e)) return false;
        xn_set_int(acc, 0);
        ok = true;
        for (int j = 0; j < 3 && ok; j++) {
            int sc[2], n = 0;
            for (int t = 0; t < 3; t++)
                if (t != j) sc[n++] = cols[t];
            ok = minor(sub, m, w, rows + 1, sc, 2) && xn_from_abn(e, &m[rows[0] * w + cols[j]]) &&
                 xn_mul(term, e, sub) && xn_add(tmp, acc, term, j == 1);
            if (ok) {
                xnum *t = acc;          /* acc and tmp trade places: xn_add may not alias */
                acc = tmp;
                tmp = t;
            }
        }
        if (ok) {
            xn_set_int(tmp, 0);
            ok = xn_add(r, acc, tmp, false);
        }
    }
    ar_release(mk);
    return ok;
}

/* The members of the bit set `set` below `bits`, into out; their count. */
static int members(int set, int bits, int *out)
{
    int n = 0;
    for (int i = 0; i < bits; i++)
        if (set >> i & 1) out[n++] = i;
    return n;
}

/* Whether some k x k minor of the first n rows and `cols` columns is nonzero (1), none is (0), or
 * a value would not fit (-1). */
static int some_minor(const abn *m, int w, int n, int cols, int k)
{
    int mk = ar_mark(), found = 0;
    xnum *v;
    if (!ar_xn(1, &v)) return -1;
    for (int rm = 0; rm < (1 << n) && found == 0; rm++) {
        int rs[4], cs[4];
        if (members(rm, n, rs) != k) continue;
        for (int cm = 0; cm < (1 << cols) && found == 0; cm++) {
            if (members(cm, cols, cs) != k) continue;
            if (!minor(v, m, w, rs, cs, k)) found = -1;
            else if (xn_sign(v) != 0) found = 1;
        }
    }
    ar_release(mk);
    return found;
}

/* The rank of the n x cols part of m (cols = n: the matrix; n + 1: the augmented one). */
static int rank(const abn *m, int w, int n, int cols)
{
    for (int k = n < cols ? n : cols; k > 0; k--) {
        int f = some_minor(m, w, n, cols, k);
        if (f != 0) return f < 0 ? -1 : k;
    }
    return 0;
}

abn_status lin_solve(int n, const abn *coef, abn *out)
{
    int mk = ar_mark(), w = n + 1, all[3] = {0, 1, 2};
    xnum *det, *num;
    abn res[3];
    abn_status st = ABN_OK;
    bool over = false;
    if (n < 2 || n > 3) return ABN_DOMAIN;
    if (!ar_xn(2, &det, &num) || !minor(det, coef, w, all, all, n)) {
        ar_release(mk);
        return ABN_RANGE;
    }
    if (xn_sign(det) == 0) {
        int ra = rank(coef, w, n, n), rb = rank(coef, w, n, n + 1);
        ar_release(mk);
        if (ra < 0 || rb < 0) return ABN_RANGE;
        return rb > ra ? ABN_NO_SOLUTION : ABN_MULT_SOLUTION;
    }
    /* Cramer: column i replaced by the constants */
    for (int i = 0; i < n && st == ABN_OK; i++) {
        int cols[3];
        for (int j = 0; j < n; j++) cols[j] = j == i ? n : j;
        if (!minor(num, coef, w, all, cols, n)) st = ABN_RANGE;
        else st = xn_div_to_abn(&res[i], num, det);
        /* a solution beyond the range: the largest number with its sign; below it 0 (unit 026) */
        if (st == ABN_UNDERFLOW) {
            res[i] = abn_zero();
            st = ABN_OK;
        } else if (st == ABN_OVERFLOW) {
            res[i] = abn_largest();
            if ((xn_sign(num) < 0) != (xn_sign(det) < 0)) res[i] = abn_neg(&res[i]);
            over = true;
            st = ABN_OK;
        }
    }
    ar_release(mk);
    if (st != ABN_OK) return st;
    for (int i = 0; i < n; i++) out[i] = res[i];
    return over ? ABN_OVERFLOW : ABN_OK;
}
