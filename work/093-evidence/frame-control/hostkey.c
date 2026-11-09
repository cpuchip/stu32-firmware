/* throwaway: the ROWBENCH key loop (1 +, 100 pairs) on the host, no page, many reps */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "calc.h"
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e6+t.tv_nsec/1e3;}
int main(void){
    ab_calc *c = calloc(1, sizeof *c);
    double best = 1e18;
    for (int r = 0; r < 200; r++) {
        ab_init(c); ab_do_arg(c, AB_MODEL, AB_MODEL_STU);
        double t0 = now();
        for (int i = 0; i < 100; i++) ab_do(c, AB_K1), ab_do(c, AB_ADD);
        double t1 = now();
        if ((t1 - t0) / 200 < best) best = (t1 - t0) / 200;
    }
    printf("best per ab_do %.3f us\n", best);
    return 0;
}
