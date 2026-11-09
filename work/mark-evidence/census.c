/* THROWAWAY (decision 95's mark, measured first): each line "OPNAME mode x [y]" run once through the core in STU, X (and
   Y) set exactly, the engine's sticky rounded flag cleared first; prints "status rounded result". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "opnames.h"
extern bool abn_rounded;
static ab_val real(abn v) { ab_val r; memset(&r, 0, sizeof r); r.kind = AB_REAL; r.re = v; r.im = r.p3 = abn_zero(); return r; }
int main(int argc, char **argv)
{
    static ab_calc c;
    char line[256];
    FILE *in = fopen(argv[1], "r");
    while (fgets(line, sizeof line, in)) {
        char op[32], mode[8], xs[64], ys[64] = "";
        int k = sscanf(line, "%31s %7s %63s %63s", op, mode, xs, ys);
        if (k < 3) continue;
        int o = -1;
        for (int i = 0; i < (int)(sizeof AB_OPNAMES / sizeof AB_OPNAMES[0]); i++)
            if (AB_OPNAMES[i] && !strcmp(AB_OPNAMES[i], op)) o = i;
        if (o < 0) { printf("NOOP\n"); continue; }
        ab_init(&c);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_do(&c, !strcmp(mode, "RAD") ? AB_RAD : AB_DEG);
        abn x, y;
        if (abn_from_text(&x, xs) != ABN_OK) { printf("BADX\n"); continue; }
        c.x = real(x);
        if (k == 4 && abn_from_text(&y, ys) == ABN_OK) c.y = real(y);
        c.lift = true;
        abn_rounded = false;
        abn_status st = ab_do(&c, (ab_op)o);
        char out[64];
        abn_to_text(&c.x.re, out, sizeof out);
        printf("%d %d %s\n", (int)st, abn_rounded ? 1 : 0, out);
    }
    return 0;
}
