/* eqn_depth: the units the parser's recursion reaches for each equation (unit 047: +8 a group, vector or argument
 * list, +1 a step of STU's power chain; more than 153 is NESTED TOO DEEP), by the parser itself, unbounded (a host's
 * stack). tests/eqn_depth.py is the same rule in Python, for abacus's scan of the hidden sets; tests/eqn_depth_fuzz.py
 * checks the two agree.
 *
 *   input:  MODEL TEXT       MODEL is 33, 35 or STU; TEXT the equation as stored (UTF-8), to the line's end
 *   output: the units reached, then " TOO_DEEP" when past 153 (the guarded parse's answer)
 */
#include <stdio.h>
#include <string.h>

#include "calc.h"
#include "eqn.h"

int main(void)
{
    static ab_calc c;
    static char line[1024];
    ab_init(&c);
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *sp = strchr(line, ' ');
        if (!sp) continue;
        *sp = '\0';
        int model = !strcmp(line, "STU") ? AB_MODEL_STU : !strcmp(line, "33") ? 33 : 35;
        int u = eqn_depth_peak(&c, sp + 1, model);
        printf("%d%s\n", u, eqn_too_deep(&c, sp + 1, model) ? " TOO_DEEP" : "");
    }
    return 0;
}
