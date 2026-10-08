/* eqn_stack: the stack each equation's guarded parse takes, by a painted thread stack, beside the units it reached
 * (unit 047's census check: a recursion the guard does not count shows as stack beyond base + units x the bytes a unit
 * costs; tests/eqn_stack_fuzz.py asserts the bound). Host only.
 *
 *   input:  MODEL TEXT          (as tests/eqn_depth.c)
 *   output: "used units": the bytes the parse touched, the units the unbounded measure gives
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "eqn.h"

#define STK (1 << 20)
static unsigned char *stk;
static ab_calc c;
static char text[1024];
static int model;

static void *run(void *x)
{
    (void)x;
    ab_val v;
    int pos;
    eqn_eval(&c, text, model, false, true, &v, &pos);     /* the guarded parse, as a store's check and every use */
    return NULL;
}

int main(void)
{
    static char line[1100];
    stk = aligned_alloc(4096, STK);
    ab_init(&c);
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *sp = strchr(line, ' ');
        if (!sp) continue;
        *sp = '\0';
        model = !strcmp(line, "STU") ? AB_MODEL_STU : !strcmp(line, "33") ? 33 : 35;
        snprintf(text, sizeof text, "%s", sp + 1);
        memset(stk, 0xA5, STK);
        pthread_attr_t at;
        pthread_attr_init(&at);
        pthread_attr_setstack(&at, stk, STK);
        pthread_t t;
        pthread_create(&t, &at, run, NULL);
        pthread_join(t, NULL);
        size_t i = 0;
        while (i < STK && stk[i] == 0xA5) i++;
        printf("%zu %d\n", STK - i, eqn_depth_peak(&c, text, model));
    }
    return 0;
}
