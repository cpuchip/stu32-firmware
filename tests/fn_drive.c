/* fn_drive: applies one unit 003 function per input line, for tests/fn_fuzz.py.
 *
 *   input:  NAME MODE x [y]        (MODE is DEG, RAD or GRAD; y for two-number functions)
 *   output: the result as engine text, or ERR <status>
 * Two-number functions take (y, x) as the calculator does: POW is y^x, XROOT the x-th root
 * of y, IDIV and RMDR y by x, PCT x percent of y, PCTCHG the change from y to x.
 */
#include <stdio.h>
#include <string.h>

#include "fn.h"

typedef abn_status (*f1)(abn *, const abn *);
typedef abn_status (*fa)(abn *, const abn *, fn_angle);
typedef abn_status (*f2)(abn *, const abn *, const abn *);

static const struct { const char *name; f1 one; fa ang; f2 two; } FNS[] = {
    {"EXP", fn_exp, 0, 0}, {"LN", fn_ln, 0, 0}, {"TENX", fn_tenx, 0, 0}, {"LOG", fn_log, 0, 0},
    {"SQ", fn_sq, 0, 0}, {"SQRT", fn_sqrt, 0, 0}, {"INV", fn_inv, 0, 0},
    {"SINH", fn_sinh, 0, 0}, {"COSH", fn_cosh, 0, 0}, {"TANH", fn_tanh, 0, 0},
    {"ASINH", fn_asinh, 0, 0}, {"ACOSH", fn_acosh, 0, 0}, {"ATANH", fn_atanh, 0, 0},
    {"SIN", 0, fn_sin, 0}, {"COS", 0, fn_cos, 0}, {"TAN", 0, fn_tan, 0},
    {"ASIN", 0, fn_asin, 0}, {"ACOS", 0, fn_acos, 0}, {"ATAN", 0, fn_atan, 0},
    {"POW", 0, 0, fn_pow}, {"XROOT", 0, 0, fn_xroot}, {"IDIV", 0, 0, fn_idiv},
    {"RMDR", 0, 0, fn_rmdr}, {"PCT", 0, 0, fn_pct}, {"PCTCHG", 0, 0, fn_pctchg},
};

int main(void)
{
    char line[512], name[16], mode[8], xs[128], ys[128];
    while (fgets(line, sizeof line, stdin)) {
        int got = sscanf(line, "%15s %7s %127s %127s", name, mode, xs, ys);
        fn_angle m = strcmp(mode, "RAD") == 0 ? FN_RAD : (strcmp(mode, "GRAD") == 0 ? FN_GRAD : FN_DEG);
        abn x, y, r;
        abn_status st = ABN_BAD_TEXT;
        size_t i;
        for (i = 0; i < sizeof FNS / sizeof FNS[0]; i++)
            if (strcmp(name, FNS[i].name) == 0) break;
        if (got < 3 || i == sizeof FNS / sizeof FNS[0] || abn_from_text(&x, xs) != ABN_OK) {
            puts("BAD");
            continue;
        }
        if (FNS[i].one) {
            st = FNS[i].one(&r, &x);
        } else if (FNS[i].ang) {
            st = FNS[i].ang(&r, &x, m);
        } else if (got == 4 && abn_from_text(&y, ys) == ABN_OK) {
            st = FNS[i].two(&r, &y, &x);
        }
        if (st != ABN_OK) {
            printf("ERR %d\n", (int)st);
        } else {
            char t[ABN_TEXT_MAX];
            abn_to_text(&r, t, sizeof t);
            puts(t);
        }
    }
    return 0;
}
