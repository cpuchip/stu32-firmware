/* mark_drive (unit 096, work/mark-exact-rounded.md): each line "OP MODE x [y]" through the core in STU, as the keys do
 * (X and Y set exactly, then ab_do of the op), printed as "status bit result": bit R when the value is marked rounded,
 * E exact; result the engine's text of X's real part. tests/mark_table.py judges these against mpmath's table. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "opnames.h"

static ab_val real(abn v)
{
    ab_val r;
    memset(&r, 0, sizeof r);
    r.kind = AB_REAL;
    r.re = v;
    r.im = r.p3 = abn_zero();
    return r;
}

int main(void)
{
    static ab_calc c;
    char line[256];
    while (fgets(line, sizeof line, stdin)) {
        char op[32], mode[8], xs[80], ys[80] = "";
        int k = sscanf(line, "%31s %7s %79s %79s", op, mode, xs, ys), o = -1;
        if (k < 3) continue;
        for (int i = 0; i < AB_OPNAMES_N; i++)
            if (AB_OPNAMES[i] && !strcmp(AB_OPNAMES[i], op)) o = i;
        abn x, y;
        if (o < 0 || abn_from_text(&x, xs) != ABN_OK || (k == 4 && abn_from_text(&y, ys) != ABN_OK)) {
            printf("BAD\n");
            continue;
        }
        ab_init(&c);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_do(&c, !strcmp(mode, "RAD") ? AB_RAD : !strcmp(mode, "GRAD") ? AB_GRAD : AB_DEG);
        c.x = real(x);
        if (k == 4) c.y = real(y);
        c.lift = true;
        abn_status st = ab_do(&c, (ab_op)o);
        char out[64];
        abn_to_text(&c.x.re, out, sizeof out);
        printf("%d %c %s\n", (int)st, c.x.rounded ? 'R' : 'E', out);
    }
    return 0;
}
