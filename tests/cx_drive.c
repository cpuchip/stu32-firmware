/* cx_drive: applies one unit 009 complex function per input line, for tests/cx_fuzz.py.
 *
 *   input:  NAME a b [c d]     (z1 = a + bi, z2 = c + di for the two-number functions)
 *   output: the result's parts as engine text "re im" (ABS: the one real), or ERR <status>
 * NAME: ADD SUB MUL DIV POW (z1^z2), INV EXP LN SIN COS TAN ABS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"

#include "cplx.h"

int main(void)
{
    char line[512], name[16], t[4][128];
    while (fgets(line, sizeof line, stdin)) {
        int got = sscanf(line, "%15s %127s %127s %127s %127s", name, t[0], t[1], t[2], t[3]);
        abn v[4];
        int need = (!strcmp(name, "ADD") || !strcmp(name, "SUB") || !strcmp(name, "MUL") ||
                    !strcmp(name, "DIV") || !strcmp(name, "POW")) ? 4 : 2;
        bool ok = got == need + 1;
        for (int i = 0; ok && i < need; i++) ok = abn_from_text(&v[i], t[i]) == ABN_OK;
        if (!ok) {
            puts("BAD");
            continue;
        }
        cplx a = {v[0], v[1]}, b = {v[2 % need], v[3 % need]}, r;
        abn m;
        abn_status st;
        if (!strcmp(name, "ADD")) st = cx_add(&r, &a, &b);
        else if (!strcmp(name, "SUB")) st = cx_sub(&r, &a, &b);
        else if (!strcmp(name, "MUL")) st = cx_mul(&r, &a, &b);
        else if (!strcmp(name, "DIV")) st = cx_div(&r, &a, &b);
        else if (!strcmp(name, "POW")) st = cx_pow(&r, &a, &b);
        else if (!strcmp(name, "INV")) st = cx_inv(&r, &a);
        else if (!strcmp(name, "EXP")) st = cx_exp(&r, &a);
        else if (!strcmp(name, "LN")) st = cx_ln(&r, &a);
        else if (!strcmp(name, "SIN")) st = cx_sin(&r, &a);
        else if (!strcmp(name, "COS")) st = cx_cos(&r, &a);
        else if (!strcmp(name, "TAN")) st = cx_tan(&r, &a);
        else if (!strcmp(name, "ABS")) {
            st = cx_abs(&m, &a);
            r.re = m;
            r.im = abn_zero();
        } else {
            puts("BAD");
            continue;
        }
        if (st != ABN_OK) {
            printf("ERR %d\n", (int)st);
            continue;
        }
        char x[ABN_TEXT_MAX], y[ABN_TEXT_MAX];
        abn_to_text(&r.re, x, sizeof x);
        abn_to_text(&r.im, y, sizeof y);
        printf("%s %s\n", x, y);
    }
    if (getenv("AB_ARENA_PEAK")) fprintf(stderr, "arena peak %d\n", ar_peak());   /* unit 010b measurement */
    return 0;
}
