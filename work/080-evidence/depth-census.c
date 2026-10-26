/* scratch: depth and node counts of casim answers through the prototype's parser (unit 080 blueprint) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tb.h"
static tb_tree t;
static int fam(int k) { return k == TB_ADD || k == TB_SUB ? 1 : k == TB_MUL ? 2 : 0; }
static int depth(int n)
{
    if (getenv("STRUCT")) {
        if (n < 0) return 0;
        int best = 0, ch[2] = {t.n[n].a, t.n[n].b};
        for (int i = 0; i < 2; i++) {
            if (ch[i] < 0) continue;
            int f = fam(t.n[n].kind), d = depth(ch[i]) + (f && fam(t.n[ch[i]].kind) == f ? 0 : 1);
            if (d > best) best = d;
        }
        return best;
    }
    if (n < 0) return 0;
    int a = depth(t.n[n].a), b = depth(t.n[n].b);
    return 1 + (a > b ? a : b);
}
int main(void)
{
    char line[4096], op[64], txt[4096];
    int hist_d[64] = {0}, maxd = 0, maxn = 0, total = 0, bad = 0;
    char maxd_s[600] = "", maxn_s[600] = "";
    tb_style st = {2, 1, 1};
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\n")] = 0;
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0; snprintf(op, sizeof op, "%s", line); snprintf(txt, sizeof txt, "%s", tab + 1);
        if (tb_parse(&t, txt, &st) != 0) { bad++; if (bad <= 8) fprintf(stderr, "PARSE %s: %s (err %d)\n", op, txt, t.err); continue; }
        int d = depth(t.root);
        for (int i = 0; i < t.ncond; i++) { int c = depth(t.conds[i]); if (c > d) d = c; }
        total++;
        hist_d[d < 63 ? d : 63]++;
        if (d > maxd) { maxd = d; snprintf(maxd_s, sizeof maxd_s, "%s: %.500s", op, txt); }
        if (t.count > maxn) { maxn = t.count; snprintf(maxn_s, sizeof maxn_s, "%s: %.500s", op, txt); }
    }
    printf("parsed %d, failed %d\nmax depth %d: %s\nmax nodes %d: %s\n", total, bad, maxd, maxd_s, maxn, maxn_s);
    int cum = 0;
    for (int i = 0; i < 64; i++) if (hist_d[i]) { cum += hist_d[i]; printf("depth %2d: %5d  (cum %5.1f%%)\n", i, hist_d[i], 100.0 * cum / total); }
    return 0;
}
