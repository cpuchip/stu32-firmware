/* norm: the tests' own reading of a number's text, independent of the engine's parser.
 * A value is reduced to (sign, significant digits, exponent of the leading digit), so
 * "337.5", "337.500" and "3.375E2" are the same value and a parser bug in the engine
 * cannot vouch for itself.
 */
#ifndef TESTS_NORM_H
#define TESTS_NORM_H

#include <ctype.h>
#include <string.h>

#define NDIG 128

typedef struct {
    int zero;
    int neg;
    char dig[NDIG];     /* significant digits, no leading or trailing zeros */
    long exp;           /* exponent of the leading digit */
} norm;

/* Normalizes [-|+]digits[.digits][E|e[-|+]digits]. Returns 0 on success. */
static inline int normalize(const char *s, norm *n)
{
    char raw[NDIG];
    int nraw = 0, seen_point = 0, int_digits = 0, any = 0;
    long e = 0;
    memset(n, 0, sizeof *n);
    if (*s == '-' || *s == '+') {
        n->neg = (*s == '-');
        s++;
    }
    for (; *s && *s != 'E' && *s != 'e'; s++) {
        if (*s == '.') {
            if (seen_point) return -1;
            seen_point = 1;
        } else if (isdigit((unsigned char)*s)) {
            if (nraw >= NDIG - 1) return -1;
            raw[nraw++] = *s;
            if (!seen_point) int_digits++;
            any = 1;
        } else {
            return -1;
        }
    }
    if (!any) return -1;
    if (*s == 'E' || *s == 'e') {
        int eneg = 0, edig = 0;
        s++;
        if (*s == '-' || *s == '+') {
            eneg = (*s == '-');
            s++;
        }
        for (; isdigit((unsigned char)*s); s++, edig++) e = e * 10 + (*s - '0');
        if (!edig || *s) return -1;
        if (eneg) e = -e;
    }
    /* Strip leading zeros, tracking where the point sits. */
    int lead = 0;
    while (lead < nraw && raw[lead] == '0') lead++;
    if (lead == nraw) {
        n->zero = 1;
        n->neg = 0;
        return 0;
    }
    int end = nraw;
    while (end > lead && raw[end - 1] == '0') end--;
    memcpy(n->dig, raw + lead, (size_t)(end - lead));
    n->dig[end - lead] = '\0';
    n->exp = e + (long)(int_digits - lead - 1);
    return 0;
}

/* Rounds a normalized value to `sig` significant digits, half away from zero. */
static inline void round_norm(norm *n, int sig)
{
    int len = (int)strlen(n->dig);
    if (n->zero || len <= sig) return;
    int up = n->dig[sig] >= '5';
    n->dig[sig] = '\0';
    if (up) {
        int i = sig - 1;
        while (i >= 0 && n->dig[i] == '9') n->dig[i--] = '0';
        if (i >= 0) {
            n->dig[i]++;
        } else {
            memmove(n->dig + 1, n->dig, (size_t)sig + 1);
            n->dig[0] = '1';
            n->exp++;
        }
    }
    len = (int)strlen(n->dig);
    while (len > 0 && n->dig[len - 1] == '0') n->dig[--len] = '\0';
}

static inline int norm_equal(const norm *a, const norm *b)
{
    if (a->zero || b->zero) return a->zero && b->zero;
    return a->neg == b->neg && a->exp == b->exp && strcmp(a->dig, b->dig) == 0;
}

/* True when got is within `units` units in the 34th significant digit of want, computed on
 * the digit strings with exact integer arithmetic (no engine, no floating point). A zero
 * want requires an exact zero. */
static inline int norm_within_units(const norm *want, const norm *got, int units)
{
    enum { CAP = 240 };
    if (want->zero) return got->zero;
    if (got->zero) return 0;
    long unit = want->exp - 33;
    long wlow = want->exp - (long)strlen(want->dig) + 1;
    long glow = got->exp - (long)strlen(got->dig) + 1;
    if (got->exp > want->exp + 2 || got->exp < unit - 2) return 0;
    long base = unit;
    if (wlow < base) base = wlow;
    if (glow < base) base = glow;
    long top = (want->exp > got->exp ? want->exp : got->exp) + 2;
    if (top - base + 1 > CAP) return 0;
    int w[CAP] = {0}, g[CAP] = {0}, d[CAP] = {0};   /* little-endian, position - base */
    for (size_t i = 0; i < strlen(want->dig); i++) w[want->exp - (long)i - base] = want->dig[i] - '0';
    for (size_t i = 0; i < strlen(got->dig); i++) g[got->exp - (long)i - base] = got->dig[i] - '0';
    int n = (int)(top - base + 1);
    /* |got - want| as a magnitude: same signs subtract, opposite signs add. */
    if (want->neg == got->neg) {
        int cmp = 0;
        for (int i = n - 1; i >= 0 && !cmp; i--) cmp = (g[i] > w[i]) - (g[i] < w[i]);
        const int *a = cmp >= 0 ? g : w, *b = cmp >= 0 ? w : g;
        int borrow = 0;
        for (int i = 0; i < n; i++) {
            int v = a[i] - b[i] - borrow;
            borrow = v < 0;
            d[i] = v + (borrow ? 10 : 0);
        }
    } else {
        int carry = 0;
        for (int i = 0; i < n; i++) {
            int v = g[i] + w[i] + carry;
            d[i] = v % 10;
            carry = v / 10;
        }
        if (carry) return 0;
    }
    /* Compare d with units * 10^(unit - base). */
    int lim[CAP] = {0};
    long at = unit - base;
    for (int u = units, i = 0; u > 0; u /= 10, i++) lim[at + i] = u % 10;
    for (int i = n - 1; i >= 0; i--)
        if (d[i] != lim[i]) return d[i] < lim[i];
    return 1;
}

#endif
