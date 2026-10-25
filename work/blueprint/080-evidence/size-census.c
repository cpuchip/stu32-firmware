/* scratch: each answer through the card's rule (stu-textbook-size): scale 3 whole, else 2 whole, else wrapped at the
   top-level + and - at 2; then its height (with its conditions) against the answer area */
#include <stdio.h>
#include <string.h>
#include "tb.h"
#define MAXW 391
#define AREA 180
static tb_tree t;
int main(void)
{
    char line[4096];
    int maxn = 0, n3 = 0, n2 = 0, nw = 0, tall = 0, wide = 0, bad = 0, total = 0, maxh = 0;
    char tall_ex[300] = "", wide_ex[300] = "";
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\n")] = 0;
        char *txt = strchr(line, '\t');
        if (!txt) continue;
        txt++;
        int sc, lines = 0, done = 0;
        for (sc = 3; sc >= 2 && !done; sc--) {
            tb_style st = {(uint8_t)sc, 1, 1};
            if (tb_parse(&t, txt, &st) < 0) { bad++; done = -1; break; }
            if (tb_layout(&t, t.root, sc) <= MAXW) { lines = 1; done = sc; break; }
        }
        if (done < 0) continue;
        total++;
        if (!done) {
            tb_style st = {2, 1, 1};
            tb_parse(&t, txt, &st);
            tb_layout(&t, t.root, 2);
            lines = tb_wrap(&t, MAXW, 2);
            if (t.count > maxn) maxn = t.count;
            if (!lines) { wide++; if (!wide_ex[0]) snprintf(wide_ex, sizeof wide_ex, "%.290s", txt); continue; }
        }
        if (t.count > maxn) maxn = t.count;
        int ch = 0;
        tb_cond_layout(&t, 2, &ch);
        int h = t.n[t.root].h + (t.ncond ? 3 + ch : 0);
        if (h > maxh) maxh = h;
        if (h > AREA) { tall++; if (!tall_ex[0]) snprintf(tall_ex, sizeof tall_ex, "%d px: %.280s", h, txt); continue; }
        if (done == 3) n3++; else if (done == 2) n2++; else nw++;
    }
    printf("answers %d (parse failed %d)\n scale 3 whole: %d\n scale 2 whole: %d\n wrapped at 2, within %d px: %d\n"
           " taller than %d px when wrapped: %d (e.g. %s)\n a term alone wider than the line: %d (e.g. %s)\n tallest that fit the rule: %d px\n",
           total, bad, n3, n2, AREA, nw, AREA, tall, tall_ex, wide, wide_ex, maxh);
    printf(" most nodes after the rule (wrap included): %d\n", maxn);
    return 0;
}

