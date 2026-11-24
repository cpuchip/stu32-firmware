/* steps_test (unit 095, work/cas-steps-device.md; abacus #14234: the blueprint's census kept as a test): every case of
 * tests/steps-corpus.txt paged as the firmware pages it (CASIM_STEPS_ONE_LINE, the CAS arena's AB_CAS_ARENA bytes, a
 * page's buffer of STEP_CAP bytes, STU, RAD). A case Casimir's op refuses has no steps to page, and its steps call must
 * refuse it the same way; otherwise every page must come, never NO_ROOM or TOO_LONG. It prints the largest arena any
 * case needed (found in 1 KiB steps), the most steps and the longest page, so a later op that pages deeper shows up.
 * Since the Casimir 1.8.0 repin (work/repin-v180.md): CAS 041 answers a solve with other letters (a letter quadratic or
 * pure power) and refuses its steps BAD_ARG by contract (casim 1.8.0's notes). Only that refusal, of only such a solve,
 * is counted apart; any other refusal of an answered op's pages is still bad.
 *
 *   build/steps_test tests/steps-corpus.txt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "casim.h"

#define STEP_CAP 220            /* core/calc.c step_page's out */

/* a letter other than var standing alone in text (not inside a name like SQRT or SIN) */
static int other_letter(const char *t, char var)
{
    for (int i = 0; t[i]; i++) {
        char p = i ? t[i - 1] : 0, n = t[i + 1];
        if (t[i] >= 'A' && t[i] <= 'Z' && t[i] != var && !(p >= 'A' && p <= 'Z') && !(n >= 'A' && n <= 'Z') && n != '(')
            return 1;
    }
    return 0;
}

static unsigned char arena[AB_CAS_ARENA];

static int cells(const char *s)
{
    int n = 0;
    for (; *s; s++) n += ((unsigned char)*s & 0xC0) != 0x80;
    return n;
}

static int answered(casim_status s)
{
    return s == CASIM_OK || s == CASIM_PARTIAL || s == CASIM_DECIMAL_FORM;
}

/* every page within cap bytes of arena: 0, or the first status that isn't a page */
static casim_status pages(const casim_req *rq, size_t cap, int *count, int *maxc)
{
    casim_step_info in = {0, 0, 0, 0};
    char out[STEP_CAP];
    casim_status s = casim_step(rq, 0, CASIM_STEPS_ONE_LINE, &in, out, sizeof out, arena, cap);
    if (!answered(s)) return s;
    *count = in.count;
    for (int k = 0; k < in.count; k++) {
        s = casim_step(rq, k, CASIM_STEPS_ONE_LINE, &in, out, sizeof out, arena, cap);
        if (!answered(s)) return s;
        if (maxc && cells(out) > *maxc) *maxc = cells(out);
    }
    return CASIM_OK;
}

int main(int argc, char **argv)
{
    FILE *in = argc > 1 ? fopen(argv[1], "r") : NULL;
    char line[512], ans[512];
    int cases = 0, paged = 0, refused = 0, lettered = 0, bad = 0, most = 0, longest = 0;
    size_t need_max = 0;
    if (!in) {
        fprintf(stderr, "usage: steps_test CORPUS\n");
        return 2;
    }
    while (fgets(line, sizeof line, in)) {
        char op, var, text[480];
        if (line[0] == '#' || sscanf(line, "%c %c %479s", &op, &var, text) != 3) continue;
        casim_req rq = {op == 'd' ? CASIM_DERIVE : op == 'i' ? CASIM_INTEGRATE : CASIM_SOLVE, text, CASIM_MODEL_STU,
                        var - 'A', NULL, CASIM_MODEL_STU, 1, NULL};
        int count = 0, maxc = 0;
        cases++;
        casim_status run = casim_run(&rq, ans, sizeof ans, arena, sizeof arena);
        casim_status st = pages(&rq, sizeof arena, &count, &maxc);
        if (!answered(run)) {                   /* the op refuses: its steps must refuse the same way */
            refused++;
            if (st != run) bad++, printf("steps_test: %c %c %s: the op says %d, its steps %d\n", op, var, text, run, st);
            continue;
        }
        if (st == CASIM_BAD_ARG && op == 's' && other_letter(text, var)) {
            lettered++;                         /* CAS 041's contract: a letter solve's steps are refused */
            continue;
        }
        if (st != CASIM_OK) {
            bad++;
            printf("steps_test: %c %c %s: %s\n", op, var, text, st == CASIM_NO_ROOM ? "NO_ROOM in the CAS arena"
                   : st == CASIM_TOO_LONG ? "a page past the page's buffer" : "a page refused");
            continue;
        }
        paged++;
        size_t need = sizeof arena;
        for (size_t a = 1024; a < sizeof arena; a += 1024) {
            int c2 = 0;
            if (pages(&rq, a, &c2, NULL) == CASIM_OK) {
                need = a;
                break;
            }
        }
        if (need > need_max) need_max = need;
        if (count > most) most = count;
        if (maxc > longest) longest = maxc;
    }
    fclose(in);
    printf("steps_test: %d cases, %d paged, %d refused as their ops refuse, %d letter solves' steps refused (CAS 041); the "
           "most arena %zu KiB of %d, the most steps %d, the longest page %d cells; %d bad\n", cases, paged, refused,
           lettered, need_max / 1024, AB_CAS_ARENA / 1024, most, longest, bad);
    return bad != 0;
}
