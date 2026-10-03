/* diff_test: the shipped engine (abn.h, Intel decimal128) against our own reference
 * engine (tests/dref.c), which shares no code with it.
 *
 * First the reference engine must itself pass tests/cases.h (Python decimal results), so
 * it is fit to judge. Then both engines compute the same random operations and must agree
 * digit for digit after normalization (tests/norm.h). The operands include exact ties
 * (half to even), operands far apart (sticky digits) and near-cancellation.
 *
 *   build/diff_test [count] [seed]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abn.h"
#include "cases.h"
#include "dref.h"
#include "norm.h"

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;

static uint64_t rng(void)  /* xorshift64* */
{
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 0x2545F4914F6CDD1Dull;
}

static int below(int n)
{
    return (int)(rng() % (uint64_t)n);
}

/* A random number in text: sign, 1..34 digits, exponent in [-emax, emax]. */
static void random_number(char *s, int emax)
{
    int n = 0, ndig = 1 + below(ABN_DIGITS);
    if (below(2)) s[n++] = '-';
    s[n++] = (char)('1' + below(9));
    for (int i = 1; i < ndig; i++) s[n++] = (char)('0' + below(10));
    sprintf(s + n, "E%d", below(2 * emax + 1) - emax);
}

static int same(const char *x, const char *y)
{
    norm a, b;
    return normalize(x, &a) == 0 && normalize(y, &b) == 0 && norm_equal(&a, &b);
}

int main(int argc, char **argv)
{
    long count = argc > 1 ? atol(argv[1]) : 200000;
    if (argc > 2) rng_state = strtoull(argv[2], NULL, 0) | 1;
    int failures = 0;

    for (size_t i = 0; i < NCASES; i++) {
        dref a, b, r = dref_zero();
        char text[DREF_TEXT_MAX];
        dref_from_text(&a, CASES[i].a);
        dref_from_text(&b, CASES[i].b);
        switch (CASES[i].op) {
        case '+': dref_add(&r, &a, &b); break;
        case '-': dref_sub(&r, &a, &b); break;
        case '*': dref_mul(&r, &a, &b); break;
        case '/': dref_div(&r, &a, &b); break;
        }
        dref_to_text(&r, text, sizeof text);
        if (!same(text, CASES[i].want)) {
            failures++;
            printf("FAIL reference %s %c %s: expected %s, got %s\n", CASES[i].a, CASES[i].op,
                   CASES[i].b, CASES[i].want, text);
        }
    }
    if (failures) {
        printf("diff_test: the reference engine fails its own cases; not fit to judge\n");
        return 1;
    }

    long agreed = 0;
    for (long k = 0; k < count; k++) {
        char sa[64], sb[64];
        int shape = below(4);
        random_number(sa, shape == 1 ? 300 : 40);
        if (shape == 2) {
            /* b is exactly half a unit in a's last place: the sum is a tie. */
            norm na;
            normalize(sa, &na);
            int len = (int)strlen(na.dig);
            sprintf(sb, "%s5E%ld", below(2) ? "-" : "", na.exp - len);
        } else if (shape == 3) {
            /* b agrees with a in its leading digits: near-cancellation. */
            strcpy(sb, sa);
            char *e = strchr(sb, 'E');
            if (e > sb + 2) e[-1] = (char)('0' + below(10));
        } else {
            random_number(sb, shape == 1 ? 300 : 40);
        }
        char op = "+-*/"[below(4)];
        abn xa, xb, xr = abn_zero();
        dref ra, rb, rr = dref_zero();
        abn_status s1 = abn_from_text(&xa, sa), s2 = abn_from_text(&xb, sb);
        dref_status t1 = dref_from_text(&ra, sa), t2 = dref_from_text(&rb, sb);
        if (s1 || s2 || t1 || t2) {
            failures++;
            printf("FAIL parse %s / %s\n", sa, sb);
            continue;
        }
        abn_status sx = ABN_OK;
        dref_status sr = DREF_OK;
        switch (op) {
        case '+': sx = abn_add(&xr, &xa, &xb); sr = dref_add(&rr, &ra, &rb); break;
        case '-': sx = abn_sub(&xr, &xa, &xb); sr = dref_sub(&rr, &ra, &rb); break;
        case '*': sx = abn_mul(&xr, &xa, &xb); sr = dref_mul(&rr, &ra, &rb); break;
        case '/': sx = abn_div(&xr, &xa, &xb); sr = dref_div(&rr, &ra, &rb); break;
        }
        char tx[ABN_TEXT_MAX], tr[DREF_TEXT_MAX];
        abn_to_text(&xr, tx, sizeof tx);
        dref_to_text(&rr, tr, sizeof tr);
        if (sx != ABN_OK || sr != DREF_OK || !same(tx, tr)) {
            if (++failures <= 20)
                printf("FAIL %s %c %s: shipped %s (status %d), reference %s (status %d)\n", sa,
                       op, sb, tx, sx, tr, sr);
            continue;
        }
        agreed++;
    }
    printf("diff_test: reference passes %zu cases; engines agree on %ld/%ld random operations\n",
           NCASES, agreed, count);
    return failures ? 1 : 0;
}
