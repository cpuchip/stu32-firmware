/* tbook_test: unit 080's oracles on the host (work/080-textbook-display.md).
 *   build/tbook_test build/casim/golden/corpus.txt
 * 1. The readback: each answer in casim's golden corpus at CASIM_PIN (every OK, PARTIAL or DECIMAL_FORM answer of
 *    191 B or less, solve's roots each their own), and a synthetic sweep (abacus #10907: each letter A-Z and each
 *    function word after a number, a ')', a letter and an exponent), drawn and written back as STU's parser reads what
 *    is drawn, must evaluate equal to the text at three seeded points, or fail the same way.
 * 2. The census: how the corpus fits the card's rule, and that nothing a real answer holds nests past TB_DEPTH (the
 *    stress nests, casim's own SIN(SIN(...)), X^(X^(...)) and IP(IP(...)), are the only ones drawn as text for depth).
 * 3. The drawn text of some forms, as tutor quotes it. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "eqn.h"
#include "tbook.h"

static ab_calc c;
static tb_tree t;
static int checks, bad;

static void expect(int ok, const char *what)
{
    checks++;
    if (!ok) {
        bad++;
        printf("FAIL: %s\n", what);
    }
}

/* the text's value at seed k: st the status, v the value */
static abn_status value(const char *text, int k, ab_val *v)
{
    for (int i = 0; i < 26; i++) {
        char s[24];
        snprintf(s, sizeof s, "%d.%03d", 1 + (i * 7 + k * 3) % 5, 137 + 211 * i + 97 * k);
        abn_from_text(&c.var[i].re, s);
        c.var[i].kind = AB_REAL;
    }
    int pos = 0;
    return eqn_eval(&c, text, AB_MODEL_STU, false, false, v, &pos);
}

static int close_to(const abn *a, const abn *b)
{
    abn d, m = abn_abs(a), one = abn_from_int(1), tol;
    if (abn_sub(&d, a, b) != ABN_OK) return 0;
    if (abn_cmp(&m, &one) < 0) m = one;
    abn_from_text(&tol, "1E-24");
    abn_mul(&tol, &tol, &m);
    d = abn_abs(&d);
    return abn_cmp(&d, &tol) <= 0;
}

/* whether text and its readback agree at three seeds; 0 when they don't, -1 when the text didn't draw */
static int readback(const char *text, char *rb, int size)
{
    if (tb_parse(&t, text) < 0) return -1;
    if (strstr(text, "RAND") || strstr(text, "SEED")) return 1;     /* a new value at each evaluation */
    t.ncond = 0;                    /* the root alone: eqn_eval reads no conditions */
    if (tb_text(&t, true, rb, size) < 0) return 0;
    char root[256];
    snprintf(root, sizeof root, "%s", text);
    char *semi = strstr(root, "; ");
    if (semi) *semi = '\0';
    for (int k = 0; k < 3; k++) {
        ab_val a, b;
        abn_status sa = value(root, k, &a), sb = value(rb, k, &b);
        if (sa != sb) return 0;
        if (sa != ABN_OK) continue;
        if (a.kind != b.kind || !close_to(&a.re, &b.re)) return 0;
        if (a.kind == AB_COMPLEX && !close_to(&a.im, &b.im)) return 0;
    }
    return 1;
}

static void drawn(const char *text, const char *want)
{
    char out[256], what[400];
    int ok = tb_parse(&t, text) == 0 && tb_text(&t, false, out, sizeof out) >= 0 && !strcmp(out, want);
    snprintf(what, sizeof what, "drawn text of %s is %s (got %s)", text, want, ok ? want : tb_parse(&t, text) ? "a parse failure" : out);
    expect(ok, what);
}

/* whether text was met before (FNV-1a in an open table) */
static int seen(const char *text)
{
    static uint64_t table[1 << 15];
    uint64_t h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) h = (h ^ *p) * 1099511628211ULL;
    h |= 1;
    for (unsigned i = (unsigned)h & ((1u << 15) - 1);; i = (i + 1) & ((1u << 15) - 1)) {
        if (table[i] == h) return 1;
        if (!table[i]) {
            table[i] = h;
            return 0;
        }
    }
}

/* casim's stress nests: a function or a power nested in itself, many deep */
static int nest(const char *text)
{
    return strstr(text, "SIN(SIN(SIN(") || strstr(text, "X^(X^(X^(") || strstr(text, "IP(IP(IP(");
}

static void corpus(const char *path)
{
    FILE *fp = fopen(path, "r");
    char line[70000], rb[512];
    int n = 0, fails = 0, fit[4] = {0}, deep = 0, wide = 0, noparse = 0, full = 0, tall = 0, maxnodes = 0, odd = 0;
    if (!fp) {
        expect(0, "the corpus opens (run scripts/fetch-casim.sh)");
        return;
    }
    while (fgets(line, sizeof line, fp)) {
        char *f[12];
        int k = 0;
        if (line[0] == '#') continue;
        line[strcspn(line, "\n")] = '\0';
        for (char *p = line; k < 12; k++) {
            f[k] = p;
            char *tab = strchr(p, '\t');
            if (!tab) break;
            *tab = '\0', p = tab + 1;
        }
        if (k != 11 || (strcmp(f[10], "OK") && strcmp(f[10], "PARTIAL") && strcmp(f[10], "DECIMAL_FORM"))) continue;
        char *texts[64];
        int nt = 0;
        if (!strcmp(f[0], "solve")) {                   /* each root on its own, X= and (MULT k) off */
            char *p = strstr(f[11], ": ");
            if (!p) continue;
            for (p += 2; p && nt < 64;) {
                char *e = strstr(p, "; "), *eq = strchr(p, '=');
                if (e) *e = '\0';
                char *m = strstr(p, " (MULT");
                if (m) *m = '\0';
                texts[nt++] = eq ? eq + 1 : p;
                p = e ? e + 2 : NULL;
            }
        } else texts[nt++] = f[11];
        for (int i = 0; i < nt; i++) {
            if (strlen(texts[i]) > 191 || seen(texts[i])) continue;    /* each text once: the corpus runs two budgets */
            n++;
            int r = readback(texts[i], rb, sizeof rb);
            if (r < 0) {
                if (t.err == 3 && !nest(texts[i]) && ++odd <= 5) printf("DEEP, not a stress nest: %.80s\n", texts[i]);
                if (t.err == 3) deep++;
                else if (t.err == 2) full++;
                else noparse++;
                continue;
            }
            if (!r && ++fails <= 10) printf("READBACK: %s\n  as %s\n", texts[i], rb);
            tb_parse(&t, texts[i]);
            int s = tb_fit(&t, 391);
            if (t.count > maxnodes) maxnodes = t.count;
            if (!s) {
                if (t.err == 3 && !nest(texts[i]) && ++odd <= 5) printf("DEEP, not a stress nest: %.80s\n", texts[i]);
                if (t.err == 3) deep++;
                else if (t.err == 2) full++;
                else wide++;
                continue;
            }
            int h = 0;
            for (int l = 0; l < tb_lines(&t); l++) h += (l ? 3 * s : 0) + tb_line_h(&t, l);
            if (h > 180) tall++;
            else fit[s == 3 ? 3 : tb_lines(&t) > 1 ? 1 : 2]++;
        }
    }
    fclose(fp);
    printf("tbook corpus: %d answers; whole at 3 %d, at 2 %d, wrapped %d, taller than 180 px %d, a term too wide %d, "
           "past TB_DEPTH %d, the pool full %d, not parsed %d; most nodes %d\n",
           n, fit[3], fit[2], fit[1], tall, wide, deep, full, noparse, maxnodes);
    char what[96];
    snprintf(what, sizeof what, "the corpus reads back: %d of %d differ", fails, n);
    expect(fails == 0, what);
    expect(n > 4000, "the corpus has its answers");
    expect(full == 0, "no answer of 191 B runs out of the pool");
    expect(deep > 0 && odd == 0, "only casim's stress nests are past TB_DEPTH (SIN(SIN(, X^(X^(, IP(IP()");
}

static void sweep(void)
{
    static const char *const CTX[] = {"3", "(1+2)", "B", "X^2"};
    static const char *const FN[] = {"LN", "LOG", "EXP", "ALOG", "SQ", "SQRT", "INV", "IP", "FP", "RND", "ABS", "SGN",
                                     "INTG", "SIN", "COS", "TAN", "ASIN", "ACOS", "ATAN", "SINH", "COSH", "TANH",
                                     "ASINH", "ACOSH", "ATANH"};
    char text[64], rb[128], what[200];
    int n = 0, fails = 0;
    for (int x = 0; x < 4; x++) {
        for (int k = 0; k < 26; k++) {
            snprintf(text, sizeof text, "%s\xc3\x97%c", CTX[x], 'A' + k);
            n++;
            if (readback(text, rb, sizeof rb) != 1 && ++fails <= 10) printf("SWEEP: %s as %s\n", text, rb);
        }
        for (unsigned k = 0; k < sizeof FN / sizeof FN[0]; k++) {
            snprintf(text, sizeof text, "%s\xc3\x97%s(0.5)", CTX[x], FN[k]);
            n++;
            if (readback(text, rb, sizeof rb) != 1 && ++fails <= 10) printf("SWEEP: %s as %s\n", text, rb);
        }
    }
    printf("tbook sweep: %d pairs\n", n);
    snprintf(what, sizeof what, "the sweep reads back: %d of %d differ", fails, n);
    expect(fails == 0, what);
}

int main(int argc, char **argv)
{
    ab_init(&c);
    drawn("3\xc3\x97X^2-4", "3X^2-4");
    drawn("3\xc3\x97" "E", "3\xc2\xb7" "E");                     /* 3E is an exponent being typed */
    drawn("3\xc3\x97" "EXP(X)", "3e^X");
    drawn("X\xc3\x97" "2", "X\xc2\xb7" "2");
    drawn("2\xc3\x97" "3", "2\xc2\xb7" "3");
    drawn("A\xc3\x97SIN(X)", "A SIN(X)");
    drawn("3\xc3\x97SIN(X)", "3SIN(X)");
    drawn("X\xc3\x97Y", "XY");
    drawn("(X+1)\xc3\x97(X-1)", "(X+1)(X-1)");
    drawn("EXP(X)\xc3\x97SIN(X)", "e^X SIN(X)");
    drawn("ABS(X-2)", "|X-2|");
    drawn("SQRT(X+1)", "\xe2\x88\x9a(X+1)");
    drawn("(X+1)\xc3\xb7" "2", "(X+1)\xc3\xb7" "2");
    drawn("3\xc3\x97(X\xc3\xb7" "2)", "3\xc2\xb7X\xc3\xb7" "2");
    drawn("-X^2\xc3\xb7" "4", "-X^2\xc3\xb7" "4");
    drawn("X^(2\xc3\x97N)", "X^(2N)");
    drawn("2^(N+1)-1", "2^(N+1)-1");
    expect(tb_parse(&t, "\x01" "2*2 lin. solve") < 0, "a built-in solver's entry doesn't parse: it's drawn as text");
    expect(tb_parse(&t, "IDIV(A,B)") < 0, "a function of two arguments is drawn as text");
    expect(tb_parse(&t, "\xe2\x88\x9e") == 0 && tb_parse(&t, "-\xe2\x88\x9e") == 0, "\xe2\x88\x9e parses");
    {   /* the pool: 1+1+...+1 at the 191 B an equation holds is a node a byte, and its wrap adds a SUM and a ROWS a line,
           past TB_NODES: it must stop cleanly (err 2) and be drawn as text, never write past the pool */
        static char ones[192];
        for (int i = 0; i < 191; i++) ones[i] = i % 2 ? '+' : '1';
        ones[191] = '\0';
        int p = tb_parse(&t, ones), s = p == 0 ? tb_fit(&t, 391) : 0;
        printf("tbook pool: 1+1+...+1 (191 B): parse %d, fit %d, err %d, nodes %d of %d\n", p, s, t.err, t.count, TB_NODES);
        expect(t.count <= TB_NODES && (s > 0 || t.err == 2), "a text past the pool stops at TB_NODES (err 2), drawn as text");
    }
    sweep();
    corpus(argc > 1 ? argv[1] : "build/casim/golden/corpus.txt");
    printf("tbook_test: %d/%d checks\n", checks - bad, checks);
    return bad ? 1 : 0;
}
