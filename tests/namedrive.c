/* namedrive (unit 058): every public way the parser reads names (eqn_eval, eqn_vars and eqn_has_chain in models 33, 35
 * and STU; eqn_write_implied; eqn_name_before at each position), one line per input line. Two builds of it, before and
 * after a change to how names are matched, must print the same: tests/name_cohort.py gives them the inputs. */
#include <stdio.h>
#include <string.h>
#include "calc.h"
#include "eqn.h"
static ab_calc c;
static ab_val real(abn x) { ab_val r; memset(&r, 0, sizeof r); r.kind = AB_REAL; r.re = x; return r; }
int main(void)
{
    static char line[1024];
    ab_init(&c);
    for (int v = 0; v < 26; v++) c.var[v] = real(abn_from_int(v % 7 + 1));
    static const int M[] = {33, 35, AB_MODEL_STU};
    while (fgets(line, sizeof line, stdin)) {
        size_t L = strlen(line);
        if (L && line[L - 1] == '\n') line[--L] = '\0';
        char *s = line;
        for (int m = 0; m < 3; m++) {
            ab_val v;
            int pos = -1;
            abn_status st = eqn_eval(&c, s, M[m], false, false, &v, &pos);
            char t[ABN_TEXT_MAX] = "-";
            if (st == ABN_OK && v.kind == AB_REAL) abn_to_text(&v.re, t, sizeof t);
            int vars[27], n = eqn_vars(s, false, M[m] == 33, M[m], vars);
            printf("%d %d %s v%d", (int)st, pos, t, n);
            for (int k = 0; k < n; k++) printf(",%d", vars[k]);
            printf(" c%d | ", (int)eqn_has_chain(&c, s, M[m]));
        }
        char w[AB_EQN_LEN + 8];
        int rc = eqn_write_implied(&c, s, w, sizeof w);
        printf("w%d %s nb", rc, rc == 1 ? w : "");
        for (int p = 0; p <= (int)L; p++) printf(",%d", eqn_name_before(s, p));
        printf("\n");
    }
    return 0;
}
