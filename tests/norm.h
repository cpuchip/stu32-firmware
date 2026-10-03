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

#endif
