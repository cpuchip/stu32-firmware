/* hp_drive: hp_div on given operands, for tests/hp_div_fuzz.py (unit 045's review: the quotient must be the exact one
 * truncated to p digits, as the long division gave it, now that a Newton reciprocal forms it).
 *
 *   input:  DIV p a ea b eb      a 10^ea / b 10^eb to p digits (a and b decimal integers, a sign allowed)
 *   output: "m e n": the quotient's mantissa (signed), exponent and digit count; FAIL when hp_div fails
 */
#include <stdio.h>
#include <string.h>

#include "hp.h"

static void put(const hp *x)
{
    if (x->m.n == 0) {
        puts("0 0 0");
        return;
    }
    printf("%s%u", x->neg ? "-" : "", (unsigned)x->m.l[x->m.n - 1]);
    for (int i = x->m.n - 2; i >= 0; i--) printf("%09u", (unsigned)x->m.l[i]);
    printf(" %ld %d\n", x->e, sb_ndigits(&x->m));
}

static bool get(hp *x, const char *d, long e)
{
    bool neg = d[0] == '-';
    if (!sb_from_digits(&x->m, d + neg, 0)) return false;
    x->e = e;
    x->neg = neg && x->m.n;
    return true;
}

int main(void)
{
    static char op[16], da[4000], db[4000];
    static hp a, b, r;
    long ea, eb;
    int p;
    while (scanf("%15s %d %3999s %ld %3999s %ld", op, &p, da, &ea, db, &eb) == 6) {
        if (strcmp(op, "DIV") || !get(&a, da, ea) || !get(&b, db, eb) || !hp_div(&r, &a, &b, p)) puts("FAIL");
        else put(&r);
        fflush(stdout);
    }
    return 0;
}
