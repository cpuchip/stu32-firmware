/* deep_test (unit 047): the parser's depth guard at the deepest parse found, on a bounded, guarded stack.
 *
 * An equation line is SOLVE's function, run from a program 19 XEQ levels deep (the most: the function takes a level).
 * - At 153 units (17 levels of X^( in STU, 19 of "(" in 35s) the run completes.
 * - An over-deep line, as an image older than 047 could hold it (written into the program directly), stops the run
 *   with NESTED TOO DEEP. Before 047 its parse overflowed the stack (on the S3 the board reset).
 * - Every store refuses 154 units: the equation list in each mode, and program entry.
 * Each case runs on a thread with 40 KB of stack and an unmapped page below it, so an overflow faults. 40 KB is this
 * host's 32 KB: here the deepest case at 153 units needs 32 to 36 KB (x86-64's frames, its decimal library's most of
 * all), where the S3 needs about 28.6 KB of its 32 (measured on the S3, work/047). Before 047 the over-deep case faults
 * at 64 KB.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "calc.h"
#include "eqn.h"

static int checks, bad;
static ab_calc c;

static void expect(int ok, const char *what)
{
    checks++;
    if (!ok) {
        bad++;
        printf("deep_test: FAIL %s\n", what);
    }
}

static void nest(char *out, const char *open, const char *close, int n, const char *core)
{
    out[0] = '\0';
    for (int i = 0; i < n; i++) strcat(out, open);
    strcat(out, core);
    for (int i = 0; i < n; i++) strcat(out, close);
}

/* program A..T, each of A..S XEQ the next, T running SOLVE X on program Z, whose one line is the equation; then XEQ A:
   19 returns pending and SOLVE's call of Z the 20th, AB_RTN_DEPTH (the S3's runs, work/047) */
static abn_status run_deep(int model, const char *eq, const char *quarantine)
{
    ab_init(&c);
    ab_do_arg(&c, AB_MODEL, model);
    ab_do(&c, AB_PRGM);
    for (int i = 0; i < 19; i++) {
        ab_do_arg(&c, AB_LBL, i);
        ab_do_arg(&c, AB_XEQ, (i + 1) * 1000);
        ab_do(&c, AB_RTN);
    }
    ab_do_arg(&c, AB_LBL, 19);
    ab_do_arg(&c, AB_FN, 'Z' - 'A');
    ab_do(&c, AB_K1);
    ab_do_arg(&c, AB_STO, 'X' - 'A');
    ab_do(&c, AB_K2);
    ab_do_arg(&c, AB_SOLVE, 'X' - 'A');
    ab_do(&c, AB_RTN);
    ab_do_arg(&c, AB_LBL, 'Z' - 'A');
    abn_status st = ab_eqn_add(&c, eq);
    ab_do(&c, AB_RTN);
    ab_do(&c, AB_PRGM);
    if (st != ABN_OK) return st;
    if (quarantine)                             /* the line as an older image could hold it */
        for (int k = 0; k < AB_PEQN_MAX; k++)
            if (c.peqn_used[k] && !strcmp(c.peqn[k], eq)) snprintf(c.peqn[k], AB_EQN_LEN, "%s", quarantine);
    ab_do_arg(&c, AB_XEQ, 0);
    return c.err_set ? c.err_status : ABN_OK;
}

static struct { int model; const char *eq, *quarantine; abn_status st; } job;

static void *thread_main(void *x)
{
    (void)x;
    job.st = run_deep(job.model, job.eq, job.quarantine);
    return NULL;
}

/* the thread's stack with an unmapped page below it, so an overflow faults (a plain array would let it run on into
   whatever lies below, as the S3's calc task did, and pass) */
#define STK (40 * 1024)
static abn_status on_stack(int model, const char *eq, const char *quarantine)
{
    static unsigned char *base;
    if (!base) {
        base = mmap(NULL, STK + 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED || mprotect(base, 4096, PROT_NONE)) {
            printf("deep_test: no guarded stack\n");
            exit(2);
        }
    }
    job.model = model, job.eq = eq, job.quarantine = quarantine;
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstack(&at, base + 4096, STK);
    pthread_t t;
    pthread_create(&t, &at, thread_main, NULL);
    pthread_join(t, NULL);
    return job.st;
}

int main(void)
{
    static char e153[256], e154[256], e35[256], deep46[256];
    nest(e153, "X^(", ")", 17, "X-5");             /* 17 x 9 = 153 units in STU */
    nest(e154, "X^(", ")", 18, "X-5");             /* 162 */
    nest(e35, "(", ")", 19, "X-5");                /* 19 x 8 = 152 in any model */
    nest(deep46, "X^(", ")", 46, "X");             /* the case that reset the S3 */
    expect(eqn_depth_peak(&c, e153, AB_MODEL_STU) == 153 && eqn_depth_peak(&c, e154, AB_MODEL_STU) == 162,
           "the shapes measure 153 and 162 units");

    abn_status st = on_stack(AB_MODEL_STU, e153, NULL);
    expect(st != ABN_TOO_DEEP && c.evals > 1, "153 units: SOLVE of the line 19 XEQ levels deep evaluates it, on the bounded stack");
    st = on_stack(35, e35, NULL);
    expect(st != ABN_TOO_DEEP && c.evals > 1, "35s mode, 19 groups: SOLVE 19 XEQ levels deep evaluates it, on the bounded stack");
    st = on_stack(AB_MODEL_STU, e153, e154);
    expect(st == ABN_TOO_DEEP, "an over-deep line from an older image: the run stops with NESTED TOO DEEP");
    st = on_stack(AB_MODEL_STU, e153, deep46);
    expect(st == ABN_TOO_DEEP, "46 levels (the S3's reset): NESTED TOO DEEP on the bounded stack, no crash");

    static const int MODELS[3] = {AB_MODEL_STU, 35, 33};
    for (int m = 0; m < 3; m++) {
        char what[96];
        ab_init(&c);
        ab_do_arg(&c, AB_MODEL, MODELS[m]);
        static char g20[256];
        nest(g20, "(", ")", 20, "X");                /* 160 units in any model */
        int n0 = c.eqn_count;
        st = ab_eqn_add(&c, g20);
        snprintf(what, sizeof what, "model %d: 20 groups refused from the equation list, nothing stored", MODELS[m]);
        expect(st == ABN_TOO_DEEP && c.eqn_count == n0, what);
        static char g19[256];
        nest(g19, "(", ")", 19, "X");
        st = ab_eqn_add(&c, g19);
        snprintf(what, sizeof what, "model %d: 19 groups stored", MODELS[m]);
        expect(st == ABN_OK && c.eqn_count == n0 + 1, what);
        ab_init(&c);
        ab_do_arg(&c, AB_MODEL, MODELS[m]);
        ab_do(&c, AB_PRGM);
        int l0 = c.prog_len;
        st = ab_eqn_add(&c, g20);
        snprintf(what, sizeof what, "model %d: 20 groups refused in program entry, no line added", MODELS[m]);
        expect(st == ABN_TOO_DEEP && c.prog_len == l0, what);
    }
    printf("deep_test: %d/%d checks\n", checks - bad, checks);
    return bad != 0;
}
