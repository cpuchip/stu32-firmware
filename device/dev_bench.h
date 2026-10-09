/* dev_bench: the speed benches both boards' consoles run (unit 053; header only, ESP-IDF), compiled with AB_BENCH:
 * on for the stand-in, off in Rev A's default image (decision 68: no bench in the product) and on in its head-less
 * build. Each runs on a calculator of its own in PSRAM, so the user's state is untouched, and each is noinline: on the
 * calc task, inlined into calc_task, their locals sat under every deep path (unit 052). */
#ifndef DEV_BENCH_H
#define DEV_BENCH_H
#ifdef AB_BENCH

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "calc.h"
#include "eqn.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "fn.h"
#include "xnum.h"

#ifdef AB_PROF
static void prof_start(void);                   /* the stand-in's profiler (device/standin/prof.sh) */
static void prof_report(const char *what, int64_t us);
#endif

/* PROGBENCH (unit 045, abacus #4956): a math-heavy program loop timed, so that "programs slow with 044" is a number:
   LBL B, RCL X, the function, DSE N, GTO B with N = 100 and X = 1.234, in RAD: 100 calls of SIN, then of LN. */
static __attribute__((unused)) ab_val pb_real(abn v)
{
    ab_val r;
    memset(&r, 0, sizeof r);
    r.kind = AB_REAL;
    r.re = v;
    return r;
}

static __attribute__((noinline, unused)) void progbench(void)
{
    static const struct { const char *name; ab_op op; } F[] = {{"SIN", AB_SIN}, {"LN", AB_LN}};
    ab_calc *c2 = heap_caps_calloc(1, sizeof *c2, MALLOC_CAP_SPIRAM);
    if (!c2) {
        printf("progbench: no memory\n");
        return;
    }
    for (int f = 0; f < 2; f++) {
        abn x;
        abn_from_text(&x, "1.234");
        ab_init(c2);
        ab_do(c2, AB_RAD);
        ab_do(c2, AB_PRGM);
        ab_do_arg(c2, AB_LBL, 1);
        ab_do_arg(c2, AB_RCL, 'X' - 'A');
        ab_do(c2, F[f].op);
        ab_do_arg(c2, AB_DSE, 'N' - 'A');
        ab_do_arg(c2, AB_GTO, 1 * 1000);
        ab_do(c2, AB_RTN);
        ab_do(c2, AB_PRGM);
        c2->var['N' - 'A'] = pb_real(abn_from_int(100));
        c2->var['X' - 'A'] = pb_real(x);
        int64_t t0 = esp_timer_get_time();
        abn_status st = ab_do_arg(c2, AB_XEQ, 1 * 1000);
        int64_t t1 = esp_timer_get_time();
        char r[64];
        abn_to_text(&c2->x.re, r, sizeof r);
        printf("progbench: 100 x %-3s in a program %lld ms (st %d, lines %ld, X %s)\n", F[f].name,
               (long long)(t1 - t0) / 1000, (int)st, c2->run_lines, r);
    }
    heap_caps_free(c2);
    printf("progbench: done\n");
}

/* OPBENCH (after unit 050, abacus #5307): the stages of X^3-2×X-5's evaluation, each timed alone with esp_timer over
   200 runs, at X = 1.234 and at a 34-digit X: yˣ (X^3), a multiplication (2×X), a subtraction, a literal read,
   one operation through ab_eqn_op (the key's path inside an equation), and the whole evaluation. */
static __attribute__((noinline, unused)) void opbench(void)
{
    ab_calc *c2 = heap_caps_calloc(1, sizeof *c2, MALLOC_CAP_SPIRAM);
    if (!c2) {
        printf("opbench: no memory\n");
        return;
    }
    ab_init(c2);
    for (int d = 0; d < 2; d++) {
        abn x, two, three, five, r, t;
        abn_from_text(&x, d ? "2.094551481542326591482386540579303" : "1.234");
        abn_from_text(&two, "2");
        abn_from_text(&three, "3");
        abn_from_text(&five, "5");
        ab_val vx = pb_real(x), vy = pb_real(two), out, v;
        int pos;
        c2->var['X' - 'A'] = vx;
        int64_t t0, us[7];
#define OPB(k, stmt) do { t0 = esp_timer_get_time(); for (int i = 0; i < 200; i++) { stmt; } \
                          us[k] = (esp_timer_get_time() - t0) * 10 / 200; } while (0)
        OPB(0, fn_pow(&r, &x, &three));
        OPB(1, abn_mul(&r, &two, &x));
        OPB(2, abn_sub(&t, &r, &five));
        OPB(3, abn_from_text(&t, "5"));
        OPB(4, ab_eqn_op(c2, AB_MUL, &vy, &vx, &out));
        OPB(5, abn_mul(&t, &x, &x));
        OPB(6, eqn_eval(c2, "X^3-2\xc3\x97X-5", AB_MODEL_STU, false, false, &v, &pos));
        {   /* inside the exact power (pow_xnum's stages) and an evaluation's fixed cost */
            int64_t u[6];
            int am = ar_mark();
            xnum *xa, *xb, *xr, *xu;
            OPB(0, { int m2 = ar_mark(); ar_xn(4, &xa, &xb, &xr, &xu); ar_release(m2); });
            u[0] = us[0];
            ar_xn(4, &xa, &xb, &xr, &xu);
            OPB(0, xn_from_abn(xb, &x));
            u[1] = us[0];
            xn_from_abn(xb, &x);
            xn_set_int(xu, 1);
            OPB(0, xn_mul(xr, xb, xb));
            u[2] = us[0];
            xn_mul(xr, xb, xb);
            xn_mul(xa, xr, xb);
            OPB(0, xn_div_to_abn(&t, xa, xu));
            u[3] = us[0];
            ar_release(am);
            OPB(0, eqn_eval(c2, "X", AB_MODEL_STU, false, false, &v, &pos));
            u[4] = us[0];
            OPB(0, eqn_eval(c2, "5", AB_MODEL_STU, false, false, &v, &pos));
            u[5] = us[0];
            printf("opbench: X %s: inside X^3: 4 xnums from the arena %lld.%lld, X to an xnum %lld.%lld, X×X exact "
                   "%lld.%lld, the cube rounded %lld.%lld; evaluating \"X\" %lld.%lld, \"5\" %lld.%lld us\n",
                   d ? "34 digits" : "1.234", u[0] / 10, u[0] % 10, u[1] / 10, u[1] % 10, u[2] / 10, u[2] % 10,
                   u[3] / 10, u[3] % 10, u[4] / 10, u[4] % 10, u[5] / 10, u[5] % 10);
            OPB(0, fn_pow(&r, &x, &three));     /* (us[0] again: X^3, as printed below) */
        }
#undef OPB
        printf("opbench: X %s: X^3 %lld.%lld, 2×X %lld.%lld, a subtraction %lld.%lld, a literal %lld.%lld, "
               "ab_eqn_op(×) %lld.%lld, X×X %lld.%lld, the evaluation %lld.%lld us\n", d ? "34 digits" : "1.234",
               us[0] / 10, us[0] % 10, us[1] / 10, us[1] % 10, us[2] / 10, us[2] % 10, us[3] / 10, us[3] % 10,
               us[4] / 10, us[4] % 10, us[5] / 10, us[5] % 10, us[6] / 10, us[6] % 10);
    }
    heap_caps_free(c2);
    printf("opbench: done\n");
}

/* EQNBENCH (unit 049): equation evaluation timed, each evaluation a parse: SOLVE of X^3-2×X-5 (guesses 0 and 3) and
   the integral of SIN(X)^2+X×EXP(-X) from 0 to 1 in RAD, each the best of five; the time a run and an evaluation. */
static __attribute__((noinline, unused)) void eqnbench(void)
{
    static const char *const EQ[2] = {"X^3-2\xc3\x97X-5", "SIN(X)^2+X\xc3\x97" "EXP(-X)"};
    ab_calc *c2 = heap_caps_calloc(1, sizeof *c2, MALLOC_CAP_SPIRAM);
    if (!c2) {
        printf("eqnbench: no memory\n");
        return;
    }
    for (int k = 0; k < 2; k++) {
        int64_t best = -1;
        abn_status st = ABN_OK;
        for (int r = 0; r < 5; r++) {
            ab_init(c2);
            ab_do(c2, AB_RAD);
            ab_eqn_add(c2, EQ[k]);
            ab_do(c2, AB_CANCEL);                   /* the stack */
            if (k == 0) {
                ab_do(c2, AB_K0);
                ab_do_arg(c2, AB_STO, 'X' - 'A');
                ab_do(c2, AB_K3);
            } else {
                ab_do(c2, AB_K0);
                ab_do(c2, AB_ENTER);
                ab_do(c2, AB_K1);
            }
            ab_do(c2, AB_EQN);
#ifdef AB_PROF
            if (r == 0) prof_start();
#endif
            int64_t t0 = esp_timer_get_time();
            st = ab_do_arg(c2, k == 0 ? AB_SOLVE : AB_INTEG, 'X' - 'A');
            int64_t us = esp_timer_get_time() - t0;
#ifdef AB_PROF
            if (r == 0) prof_report(k ? "integral" : "SOLVE", us);
#endif
            if (best < 0 || us < best) best = us;
        }
        char x[64];
        abn_to_text(&c2->x.re, x, sizeof x);
        printf("eqnbench: %s %s: %lld us (st %d, evals %ld, %lld us an evaluation, X %s)\n", k ? "integral" : "SOLVE",
               EQ[k], (long long)best, (int)st, c2->evals, c2->evals ? (long long)best / c2->evals : 0LL, x);
        for (int d = 0; d < 2; d++) {   /* the equation alone, outside SOLVE and the integral: 200 parses (the check),
                                           200 evaluations, at X = 1.234 and at a 34-digit X */
            abn xv;
            ab_val v;
            int pos;
            abn_from_text(&xv, d ? "0.7071067811865475244008443621048490" : "1.234");
            c2->var['X' - 'A'] = pb_real(xv);
            int64_t t0 = esp_timer_get_time();
            for (int r = 0; r < 200; r++) eqn_eval(c2, EQ[k], AB_MODEL_STU, false, true, &v, &pos);
            int64_t t1 = esp_timer_get_time();
            for (int r = 0; r < 200; r++) eqn_eval(c2, EQ[k], AB_MODEL_STU, false, false, &v, &pos);
            int64_t t2 = esp_timer_get_time();
            printf("eqnbench: %s alone, X %s: a parse %lld us, an evaluation %lld us\n", EQ[k], d ? "34 digits" : "1.234",
                   (long long)(t1 - t0) / 200, (long long)(t2 - t1) / 200);
        }
    }
    heap_caps_free(c2);
    printf("eqnbench: done\n");
}


#endif
#endif
