/* THROWAWAY (cas-steps-device's measured cost): casim_step over a corpus, each page timed, its text measured, and the
   smallest arena (1 KiB steps) that pages every step. Reads "op var text" lines. */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "casim.h"
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e6+t.tv_nsec/1e3;}
static unsigned char arena[1<<20];
static int cells(const char *s){int n=0;for(;*s;s++) n+=((unsigned char)*s&0xC0)!=0x80;return n;}
static int pages_ok(casim_req *rq, int flags, size_t cap_arena, int *count)
{
    casim_step_info in; char out[512];
    casim_status s = casim_step(rq, -1, flags, &in, out, sizeof out, arena, cap_arena);
    if (s != CASIM_OK && s != CASIM_PARTIAL && s != CASIM_DECIMAL_FORM) return s;
    *count = in.count;
    for (int k = 0; k < in.count; k++) {
        s = casim_step(rq, k, flags, &in, out, sizeof out, arena, cap_arena);
        if (s != CASIM_OK && s != CASIM_PARTIAL && s != CASIM_DECIMAL_FORM) return s;
    }
    return 0;
}
int main(int argc, char **argv)
{
    FILE *in = fopen(argv[1], "r"); char line[512];
    while (fgets(line, sizeof line, in)) {
        char op, var, text[480];
        if (sscanf(line, "%c %c %479s", &op, &var, text) != 3) continue;
        casim_req rq = {op=='d'?CASIM_DERIVE:op=='i'?CASIM_INTEGRATE:CASIM_SOLVE, text, CASIM_MODEL_STU, var-'A', NULL, 0, 0, NULL};
        for (int flags = 0; flags <= CASIM_STEPS_ONE_LINE; flags++) {
            int count = 0, maxc = 0, maxb = 0, depth = 0;
            casim_step_info inf; char out[512];
            int r = pages_ok(&rq, flags, 16384, &count);
            double tmax = 0, tsum = 0;
            if (!r) for (int k = 0; k < count; k++) {
                double t0 = now();
                casim_step(&rq, k, flags, &inf, out, sizeof out, arena, 16384);
                double dt = now() - t0; tsum += dt; if (dt > tmax) tmax = dt;
                int c = cells(out); if (c > maxc) maxc = c; if ((int)strlen(out) > maxb) maxb = (int)strlen(out);
                if (inf.depth > depth) depth = inf.depth;
            }
            size_t need = 0;            /* the smallest arena, in KiB, that pages them all */
            if (!r) for (size_t a = 1024; a <= 16384; a += 1024) { int cc; if (!pages_ok(&rq, flags, a, &cc)) { need = a; break; } }
            int r64 = r ? pages_ok(&rq, flags, sizeof arena, &count) : 0;
            printf("%c %c %-46s %s steps %2d cells %3d bytes %3d depth %d arena %2zu KiB page max %7.1f us all %8.1f us%s\n",
                   op, var, text, flags ? "1L" : "TL", count, maxc, maxb, depth, need / 1024, tmax, tsum,
                   r ? (r64 ? " REFUSED" : " NO_ROOM at 16 KiB") : "");
        }
    }
    return 0;
}
