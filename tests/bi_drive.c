/* bi_drive: bigint_impl.h's operations, on both its types (bigint, 760 limbs; sbig, 72), one per input line, for
 * tests/bi_fuzz.py, which checks each against Python's exact integers (unit 045: the limb arithmetic sped up, its
 * results unchanged).
 *
 *   input:  T OP a [b]     T is W (bigint) or S (sbig); a and b non-negative decimal integers ('0' is zero)
 *   output: the result in decimal; OVF where the operation reports no room (false); for DIVU "q r", for DIV
 *           "q inexact", for SUBU NEG when a < v, for CMP -1, 0 or 1, for NDIG the digit count.
 *
 *   OPs: MUL ADD SUB (a >= b) MULU ADDU SUBU (b a uint32) MULP DIVP (b a digit count) DIVU (b a nonzero uint32)
 *        DIV (b nonzero) CMP NDIG
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "bigint.h"
#include "sbig.h"

static char line[16000], ta[8000], tb[8000];

#define DRIVE(T, P)                                                                                                  \
    static void P##print(const T *a)                                                                               \
    {                                                                                                                \
        if (a->n == 0) {                                                                                             \
            fputs("0", stdout);                                                                                      \
            return;                                                                                                  \
        }                                                                                                            \
        printf("%u", (unsigned)a->l[a->n - 1]);                                                                      \
        for (int i = a->n - 2; i >= 0; i--) printf("%09u", (unsigned)a->l[i]);                                       \
    }                                                                                                                \
    static void P##run(const char *op, const char *sa, const char *sb)                                              \
    {                                                                                                                \
        static T a, b, r;                                                                                            \
        bool inexact;                                                                                                \
        if (!P##from_digits(&a, sa, 0) || (*sb && isdig(sb) && !P##from_digits(&b, sb, 0))) {                       \
            puts("BAD");                                                                                             \
            return;                                                                                                  \
        }                                                                                                            \
        uint32_t u = (uint32_t)strtoul(sb, NULL, 10);                                                                \
        if (!strcmp(op, "MUL")) {                                                                                    \
            if (!P##mul(&r, &a, &b)) { puts("OVF"); return; }                                                        \
            P##print(&r);                                                                                            \
        } else if (!strcmp(op, "ADD")) {                                                                             \
            if (!P##add(&r, &a, &b)) { puts("OVF"); return; }                                                        \
            P##print(&r);                                                                                            \
        } else if (!strcmp(op, "SUB")) {                                                                             \
            P##sub(&r, &a, &b);                                                                                      \
            P##print(&r);                                                                                            \
        } else if (!strcmp(op, "MULU")) {                                                                            \
            if (!P##mul_u32(&a, u)) { puts("OVF"); return; }                                                         \
            P##print(&a);                                                                                            \
        } else if (!strcmp(op, "ADDU")) {                                                                            \
            if (!P##add_u32(&a, u)) { puts("OVF"); return; }                                                         \
            P##print(&a);                                                                                            \
        } else if (!strcmp(op, "SUBU")) {                                                                            \
            if (!P##sub_u32(&a, u)) { puts("NEG"); return; }                                                         \
            P##print(&a);                                                                                            \
        } else if (!strcmp(op, "MULP")) {                                                                            \
            if (!P##mul_pow10(&a, (int)u)) { puts("OVF"); return; }                                                  \
            P##print(&a);                                                                                            \
        } else if (!strcmp(op, "DIVP")) {                                                                            \
            P##div_pow10(&a, (int)u);                                                                                \
            P##print(&a);                                                                                            \
        } else if (!strcmp(op, "DIVU")) {                                                                            \
            uint32_t rem = P##divmod_u32(&a, u);                                                                     \
            P##print(&a);                                                                                            \
            printf(" %u", (unsigned)rem);                                                                            \
        } else if (!strcmp(op, "DIV")) {                                                                             \
            if (!P##div(&r, &a, &b, &inexact)) { puts("OVF"); return; }                                              \
            P##print(&r);                                                                                            \
            printf(" %d", (int)inexact);                                                                             \
        } else if (!strcmp(op, "CMP")) {                                                                             \
            printf("%d", P##cmp(&a, &b));                                                                            \
        } else if (!strcmp(op, "NDIG")) {                                                                            \
            printf("%d", P##ndigits(&a));                                                                            \
        } else {                                                                                                     \
            fputs("BADOP", stdout);                                                                                  \
        }                                                                                                            \
        putchar('\n');                                                                                               \
    }

static bool isdig(const char *s)
{
    for (; *s; s++)
        if (*s < '0' || *s > '9') return false;
    return true;
}

DRIVE(bigint, bi_)
DRIVE(sbig, sb_)

int main(void)
{
    while (fgets(line, sizeof line, stdin)) {
        char t[4], op[8];
        ta[0] = tb[0] = '\0';
        if (sscanf(line, "%3s %7s %7999s %7999s", t, op, ta, tb) < 3) continue;
        if (t[0] == 'W') bi_run(op, ta, tb);
        else sb_run(op, ta, tb);
        fflush(stdout);
    }
    return 0;
}
