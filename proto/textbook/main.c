#define _POSIX_C_SOURCE 199309L
/* The prototype's frames and numbers (abacus #8391). THROWAWAY. Writes build/proto/ PNGs (400 x 240, one bit) and
 * x3 previews, and prints the fit table, the node counts and the host render times. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tb.h"

/* ---- PNG, one bit, stored deflate (as tests/screen_test.c writes them) ---- */

static uint32_t crc_tab[256];
static uint32_t crc32_(const uint8_t *p, size_t n)
{
    if (!crc_tab[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_tab[i] = c;
        }
    uint32_t c = 0xFFFFFFFFu;
    while (n--) c = crc_tab[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v; }
static void chunk(FILE *o, const char *type, const uint8_t *d, size_t n)
{
    uint8_t *b = malloc(4 + n), l[4], c[4];
    memcpy(b, type, 4);
    if (n) memcpy(b + 4, d, n);
    be32(l, (uint32_t)n), be32(c, crc32_(b, 4 + n));
    fwrite(l, 1, 4, o), fwrite(b, 1, 4 + n, o), fwrite(c, 1, 4, o);
    free(b);
}
/* The frame, magnified m times (1: the panel's own pixels). */
static void png(const char *path, const ls027_frame *f, int m)
{
    int W = LS027_W * m, H = LS027_H * m, row = 1 + (W + 7) / 8;
    size_t raw = (size_t)row * H, nblk = (raw + 65534) / 65535;
    uint8_t *z = malloc(2 + raw + nblk * 5 + 4), *r = malloc(raw), hdr[13] = {0};
    for (int y = 0; y < H; y++) {
        uint8_t *q = r + (size_t)y * row;
        memset(q, 0, row);
        for (int x = 0; x < W; x++) {
            int X = x / m, Y = y / m, dark = f->px[Y][X >> 3] >> (X & 7) & 1;
            if (!dark) q[1 + x / 8] |= (uint8_t)(0x80 >> (x & 7));
        }
    }
    size_t k = 0;
    z[k++] = 0x78, z[k++] = 0x01;
    for (size_t off = 0; off < raw; off += 65535) {
        size_t n = raw - off < 65535 ? raw - off : 65535;
        z[k++] = off + n == raw, z[k++] = (uint8_t)n, z[k++] = (uint8_t)(n >> 8), z[k++] = (uint8_t)~n, z[k++] = (uint8_t)(~n >> 8);
        memcpy(z + k, r + off, n), k += n;
    }
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) a = (a + r[i]) % 65521, b = (b + a) % 65521;
    be32(z + k, b << 16 | a), k += 4;
    be32(hdr, (uint32_t)W), be32(hdr + 4, (uint32_t)H), hdr[8] = 1;
    FILE *o = fopen(path, "wb");
    if (!o) { perror(path); exit(1); }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, o);
    chunk(o, "IHDR", hdr, 13), chunk(o, "IDAT", z, k), chunk(o, "IEND", NULL, 0);
    fclose(o);
    free(z), free(r);
}
static void out(const char *name, const ls027_frame *f)
{
    char p[256];
    snprintf(p, sizeof p, "build/proto/%s.png", name), png(p, f, 1);
    snprintf(p, sizeof p, "build/proto/x3/%s.png", name), png(p, f, 3);
}

/* ---- the samples (abacus #8391), each as casim prints it and in the fraction form ---- */

typedef struct { const char *id, *input, *dec, *frac; } sample;
static const sample S[] = {
    {"xlnx", "\xe2\x88\xab X\xc3\x97LN(X) dX", "-0.25\xc3\x97X^2+0.5\xc3\x97X^2\xc3\x97LN(X)", "-X^2\xc3\xb7" "4+X^2\xc3\x97LN(X)\xc3\xb7" "2"},
    {"atan", "\xe2\x88\xab ATAN(X) dX", "-0.5\xc3\x97LN(X^2+1)+X\xc3\x97" "ATAN(X)", "-LN(X^2+1)\xc3\xb7" "2+X\xc3\x97" "ATAN(X)"},
    {"cycle", "\xe2\x88\xab EXP(X)\xc3\x97SIN(X) dX", "0.5\xc3\x97" "EXP(X)\xc3\x97SIN(X)-0.5\xc3\x97" "EXP(X)\xc3\x97" "COS(X)",
     "EXP(X)\xc3\x97SIN(X)\xc3\xb7" "2-EXP(X)\xc3\x97" "COS(X)\xc3\xb7" "2"},
    {"atandeg", "\xe2\x88\xab ATAN(X) dX  DEG", "-90\xc3\x97LN(X^2+1)\xc3\xb7\xcf\x80+X\xc3\x97" "ATAN(X)", NULL},
    {"pow43", "\xe2\x88\xab X^(1\xc3\xb7" "3) dX", "3\xc3\x97X^(4\xc3\xb7" "3)\xc3\xb7" "4", NULL},
    {"lnabs", "\xe2\x88\xab (X^2-3)\xc3\xb7((X-2)\xc3\x97(X-1)\xc3\x97(X+3)) dX",
     "0.2\xc3\x97LN(ABS(X-2))+0.5\xc3\x97LN(ABS(X-1))+0.3\xc3\x97LN(ABS(X+3))",
     "LN(ABS(X-2))\xc3\xb7" "5+LN(ABS(X-1))\xc3\xb7" "2+3\xc3\x97LN(ABS(X+3))\xc3\xb7" "10"},
    {"partfrac", "PARTFRAC (sample)",
     "7\xc3\xb7(9\xc3\x97(3\xc3\x97X-2)^2)+55\xc3\xb7(9\xc3\x97(3\xc3\x97X-2)^3)+73\xc3\xb7(9\xc3\x97(3\xc3\x97X-2)^4)", NULL},
    {"sum", "\xce\xa3 X^(2\xc3\x97K), K=0..N",
     "-1\xc3\xb7(X^2-1)+(X^2)^(N+1)\xc3\xb7(X^2-1); X\xe2\x89\xa0-1; X\xe2\x89\xa0" "1", NULL},
    {"long", "\xce\xa3 (sample, long)",
     "N\xc3\x97X^(N+1)\xc3\xb7(X-1)-X\xc3\xb7(X-1)+X^(N+1)\xc3\xb7(X-1)+0.5\xc3\x97X^(N+2)\xc3\xb7(X-0.5\xc3\x97X^2-0.5)-0.5\xc3\x97X^2\xc3\xb7(X-0.5\xc3\x97X^2-0.5); X\xe2\x89\xa0" "1",
     "N\xc3\x97X^(N+1)\xc3\xb7(X-1)-X\xc3\xb7(X-1)+X^(N+1)\xc3\xb7(X-1)+X^(N+2)\xc3\xb7(2\xc3\x97X-X^2-1)-X^2\xc3\xb7(2\xc3\x97X-X^2-1); X\xe2\x89\xa0" "1"},
    {"taylor189", "TAYLOR SQRT(X+1) at \xcf\x80\xc3\xb7" "3, order 5",
     "SQRT(\xcf\x80\xc3\xb7" "3+1)+0.5\xc3\x97(X-\xcf\x80\xc3\xb7" "3)\xc3\xb7SQRT(\xcf\x80\xc3\xb7" "3+1)-0.125\xc3\x97(X-\xcf\x80\xc3\xb7" "3)^2\xc3\xb7(\xcf\x80\xc3\xb7" "3+1)^1.5+0.0625\xc3\x97(X-\xcf\x80\xc3\xb7" "3)^3\xc3\xb7(\xcf\x80\xc3\xb7" "3+1)^2.5-0.0390625\xc3\x97(X-\xcf\x80\xc3\xb7" "3)^4\xc3\xb7(\xcf\x80\xc3\xb7" "3+1)^3.5+0.02734375\xc3\x97(X-\xcf\x80\xc3\xb7" "3)^5\xc3\xb7(\xcf\x80\xc3\xb7" "3+1)^4.5",
     "SQRT(\xcf\x80\xc3\xb7" "3+1)+(X-\xcf\x80\xc3\xb7" "3)\xc3\xb7(2\xc3\x97SQRT(\xcf\x80\xc3\xb7" "3+1))-(X-\xcf\x80\xc3\xb7" "3)^2\xc3\xb7(8\xc3\x97(\xcf\x80\xc3\xb7" "3+1)^(3\xc3\xb7" "2))+(X-\xcf\x80\xc3\xb7" "3)^3\xc3\xb7(16\xc3\x97(\xcf\x80\xc3\xb7" "3+1)^(5\xc3\xb7" "2))-5\xc3\x97(X-\xcf\x80\xc3\xb7" "3)^4\xc3\xb7(128\xc3\x97(\xcf\x80\xc3\xb7" "3+1)^(7\xc3\xb7" "2))+7\xc3\x97(X-\xcf\x80\xc3\xb7" "3)^5\xc3\xb7(256\xc3\x97(\xcf\x80\xc3\xb7" "3+1)^(9\xc3\xb7" "2))"},
};
#define NS ((int)(sizeof S / sizeof S[0]))

#define AREA_X 6        /* the selection mark's room on the left */
#define AREA_R 397
#define MAXW (AREA_R - AREA_X)
#define TOP 23
#define BOT 203

static tb_style style(int s, int implicit) { tb_style st = {(uint8_t)s, (uint8_t)implicit, 1}; return st; }

/* ---- the worksheet's furniture ---- */

static void hline_dots(ls027_frame *f, int x0, int x1, int y)
{
    if (y < 0 || y >= LS027_H) return;
    for (int x = x0; x <= x1; x += 3) f->px[y][x >> 3] |= (uint8_t)(1u << (x & 7));
}

static void fillr(ls027_frame *f, int x, int y, int w, int h, int dark)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) {
            if (i < 0 || j < 0 || i >= LS027_W || j >= LS027_H) continue;
            if (dark) f->px[j][i >> 3] |= (uint8_t)(1u << (i & 7));
            else f->px[j][i >> 3] &= (uint8_t)~(1u << (i & 7));
        }
}

static void band(ls027_frame *f, const char *mode, const char *angle)
{
    fillr(f, 0, 0, LS027_W, TOP - 1, 0);
    tb_text(f, angle, 72, 2, 2);
    tb_text(f, mode, 164, 2, 2);
    tb_text(f, "STU", 330, 2, 2);
    fillr(f, 370, 3, 24, 12, 1), fillr(f, 372, 5, 20, 8, 0), fillr(f, 394, 6, 2, 6, 1), fillr(f, 373, 6, 14, 6, 1);
    fillr(f, 0, 19, LS027_W, 2, 1);
}

static void soft(ls027_frame *f, const char *const lab[6], int on)
{
    fillr(f, 0, 204, LS027_W, LS027_H - 204, 0);
    for (int k = 0; k < 6; k++) {
        int x = k * 66 + 2, y = 206;
        if (!lab[k] || !*lab[k]) continue;
        fillr(f, x, y, 64, 1, 1), fillr(f, x, y + 29, 64, 1, 1), fillr(f, x, y, 1, 30, 1), fillr(f, x + 63, y, 1, 30, 1);
        if (k == on) fillr(f, x, y, 64, 30, 1);
        int w = tb_text_w(lab[k], 2);
        ls027_frame tmp;
        if (k == on) {          /* inverted: draw, then flip within the box */
            memset(&tmp, 0, sizeof tmp);
            tb_text(&tmp, lab[k], x + (64 - w) / 2, y + 8, 2);
            for (int j = y; j < y + 30; j++)
                for (int i = x; i < x + 64; i++)
                    if (tmp.px[j][i >> 3] >> (i & 7) & 1) f->px[j][i >> 3] &= (uint8_t)~(1u << (i & 7));
        } else tb_text(f, lab[k], x + (64 - w) / 2, y + 8, 2);
    }
}

/* An entry: its input linear and small at the left, its answer laid out and right-aligned, any conditions under it.
 * Drawn upward from bottom; returns the new bottom. ans_s 0: the answer as linear text at in_s (no layout). */
typedef struct { const char *input, *answer; } entry;

static int how_wide_warned;

/* Lays the answer out at scale s: whole, else wrapped at + and -, else shrunk a scale and wrapped. Returns the
 * scale used; *lines the line count (1 unwrapped). */
static int fit(tb_tree *t, const char *ans, int s, int implicit, int maxw, int *lines)
{
    for (int sc = s; sc >= 1; sc--) {
        tb_style st = style(sc, implicit);
        if (tb_parse(t, ans, &st) < 0) { fprintf(stderr, "parse failed (%d): %s\n", t->err, ans); exit(1); }
        if (tb_layout(t, t->root, sc) <= maxw) { *lines = 1; return sc; }
        int n = tb_wrap(t, maxw, sc);
        if (n) { *lines = n; return sc; }
    }
    how_wide_warned++;
    *lines = 0;
    return 1;
}

static int draw_entry(ls027_frame *f, int bottom, const entry *e, int in_s, int ans_s, int cond_s, int implicit, int sel)
{
    static tb_tree t;
    int lines, sc = fit(&t, e->answer, ans_s, implicit, MAXW, &lines);
    (void)sc;
    int ch = 0, cw = tb_cond_layout(&t, cond_s, &ch);
    int ah = t.n[t.root].h, aw = t.n[t.root].w;
    int h = 7 * in_s + 3 + ah + (t.ncond ? 3 + ch : 0) + 4;
    int top = bottom - h;
    if (top < TOP) {            /* an older entry that doesn't fit whole isn't drawn; the newest, too tall, shows its top */
        if (bottom < BOT) return TOP - 1;
        top = TOP;
    }
    tb_text(f, e->input, AREA_X, top + 2, in_s);
    tb_draw(&t, t.root, f, AREA_R - aw, top + 2 + 7 * in_s + 3);
    if (t.ncond) tb_cond_draw(&t, f, AREA_R - cw, top + 2 + 7 * in_s + 3 + ah + 3, cond_s);
    if (sel) fillr(f, 0, top + 1, 3, h - 2, 1);
    hline_dots(f, 0, LS027_W - 1, top);
    return top - 1;
}

static void worksheet(const char *name, const entry *e, int n, int in_s, int ans_s, int cond_s, int implicit, int sel,
                      const char *const lab[6])
{
    static ls027_frame f;
    memset(&f, 0, sizeof f);
    int bottom = BOT;
    for (int i = n - 1; i >= 0 && bottom > TOP; i--) bottom = draw_entry(&f, bottom, &e[i], in_s, ans_s, cond_s, implicit, i == sel);
    band(&f, "ALG", "RAD");
    soft(&f, lab, -1);
    out(name, &f);
}

/* ---- one sample alone: both forms at one scale ---- */

static void sample_frame(const sample *x, int s, int implicit, const char *suffix)
{
    static ls027_frame f;
    static tb_tree t;
    char name[64];
    memset(&f, 0, sizeof f);
    int y = TOP + 2;
    tb_text(&f, x->input, AREA_X, y, 1);
    y += 10;
    for (int form = 0; form < 2; form++) {
        const char *a = form ? x->frac : x->dec;
        if (!a) continue;
        int lines, sc = fit(&t, a, s, implicit, MAXW, &lines);
        int ch = 0, cw = tb_cond_layout(&t, s >= 2 ? 2 : 1, &ch);
        char tag[48];
        snprintf(tag, sizeof tag, "%s: scale %d%s, %d line%s", form ? "fractions" : "as casim prints", sc,
                 sc < s ? " (shrunk)" : "", lines, lines == 1 ? "" : "s");
        tb_text(&f, tag, AREA_X, y, 1);
        y += 10;
        tb_draw(&t, t.root, &f, AREA_R - t.n[t.root].w, y);
        y += t.n[t.root].h + 3;
        if (t.ncond) tb_cond_draw(&t, &f, AREA_R - cw, y, s >= 2 ? 2 : 1), y += ch + 3;
        hline_dots(&f, 0, LS027_W - 1, y), y += 4;
    }
    band(&f, "ALG", "RAD");
    static const char *const lab[6] = {"\xe2\x96\xb2", "\xe2\x96\xbc", "COPY", "EDIT", "STO", "1D"};
    soft(&f, lab, -1);
    snprintf(name, sizeof name, "s%d-%s%s", s, x->id, suffix);
    out(name, &f);
}

/* ---- the long one's three ways ---- */

static void overflow_frames(void)
{
    static ls027_frame f;
    static tb_tree t;
    static const char *const lab[6] = {"\xe2\x97\x80", "\xe2\x96\xb6", "COPY", "EDIT", "STO", "1D"};
    const sample *L = &S[8];
    for (int s = 2; s <= 3; s++) {
        /* wrap at the top-level + and - */
        memset(&f, 0, sizeof f);
        tb_style st = style(s, 1);
        tb_parse(&t, L->dec, &st);
        int n = tb_wrap(&t, MAXW, s), ch, cw = tb_cond_layout(&t, 2, &ch);
        tb_text(&f, L->input, AREA_X, TOP + 2, 1);
        int y = TOP + 12;
        if (n) {
            tb_draw(&t, t.root, &f, AREA_R - t.n[t.root].w, y);
            y += t.n[t.root].h + 3;
            tb_cond_draw(&t, &f, AREA_R - cw, y, 2);
        } else tb_text(&f, "(a term is wider than the screen)", AREA_X, y, 1);
        band(&f, "ALG", "RAD"), soft(&f, lab, -1);
        char nm[48];
        snprintf(nm, sizeof nm, "long-wrap-s%d", s), out(nm, &f);
        /* scroll: one line at full size, a window on it */
        for (int pos = 0; pos < 2; pos++) {
            memset(&f, 0, sizeof f);
            tb_parse(&t, L->dec, &st);
            int w = tb_layout(&t, t.root, s), h = t.n[t.root].h;
            int off = pos == 0 ? 0 : w - MAXW;  /* the start, then the end */
            int top = TOP + 40;
            tb_text(&f, L->input, AREA_X, TOP + 2, 1);
            tb_draw(&t, t.root, &f, AREA_X - off, top);
            fillr(&f, 0, top - 2, AREA_X, h + 4, 0), fillr(&f, AREA_R + 1, top - 2, LS027_W - AREA_R - 1, h + 4, 0);
            if (off > 0) tb_text(&f, "\xe2\x97\x80", 0, top + h / 2 - 3, 1);
            if (off < w - MAXW) tb_text(&f, "\xe2\x96\xb6", AREA_R - 3, top + h / 2 - 3, 1);
            char pc[32];
            snprintf(pc, sizeof pc, "%d of %d px", MAXW, w);
            tb_text(&f, pc, AREA_X, top + h + 6, 1);
            band(&f, "ALG", "RAD"), soft(&f, lab, -1);
            snprintf(nm, sizeof nm, "long-scroll-s%d-%s", s, pos ? "end" : "start"), out(nm, &f);
        }
    }
    /* shrink: the whole answer on one line at the scale that fits, if any */
    memset(&f, 0, sizeof f);
    tb_text(&f, L->input, AREA_X, TOP + 2, 1);
    int y = TOP + 12;
    for (int s = 2; s >= 1; s--) {
        tb_style st = style(s, 1);
        tb_parse(&t, L->dec, &st);
        int w = tb_layout(&t, t.root, s);
        char tag[64];
        snprintf(tag, sizeof tag, "one line at scale %d: %d px of %d", s, w, MAXW);
        tb_text(&f, tag, AREA_X, y, 1), y += 10;
        tb_draw(&t, t.root, &f, w <= MAXW ? AREA_R - w : AREA_X, y);
        y += t.n[t.root].h + 8;
    }
    band(&f, "ALG", "RAD"), soft(&f, lab, -1);
    out("long-shrink", &f);
}

/* ---- the numbers ---- */

static int depth(const tb_tree *t, int n)
{
    if (n < 0) return 0;
    int a = depth(t, t->n[n].a), b = t->n[n].kind == TB_ROWS ? 0 : depth(t, t->n[n].b);
    return 1 + (a > b ? a : b);
}

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void)
{
    static tb_tree t;
    static ls027_frame f;
    printf("tb_node %zu B, tb_tree %zu B (TB_MAX_NODES %d)\n\n", sizeof(tb_node), sizeof(tb_tree), TB_MAX_NODES);
    printf("%-10s %-5s %5s | %5s %5s %3s %4s | %5s %5s %3s %4s | %5s %3s | %8s\n", "sample", "form", "bytes", "w@2", "fit2", "ln2",
           "h@2", "w@3", "fit3", "ln3", "h@3", "nodes", "dep", "us/draw");
    for (int i = 0; i < NS; i++)
        for (int form = 0; form < 2; form++) {
            const char *a = form ? S[i].frac : S[i].dec;
            if (!a) continue;
            int w[4] = {0}, ln[4] = {0}, hh[4] = {0}, nodes = 0, dep = 0;
            for (int s = 2; s <= 3; s++) {
                tb_style st = style(s, 1);
                tb_parse(&t, a, &st);
                w[s] = tb_layout(&t, t.root, s);
                if (s == 2) dep = depth(&t, t.root);
                int lines;
                fit(&t, a, s, 1, MAXW, &lines);
                ln[s] = lines;
                int ch;
                tb_cond_layout(&t, 2, &ch);
                hh[s] = t.n[t.root].h + (t.ncond ? 3 + ch : 0);
                if (t.count > nodes) nodes = t.count;
            }
            /* the host's time for parse + fit + draw at scale 2, the best of 5 runs of 2000 */
            double best = 1e9;
            for (int r = 0; r < 5; r++) {
                double t0 = now();
                for (int k = 0; k < 2000; k++) {
                    int lines;
                    fit(&t, a, 2, 1, MAXW, &lines);
                    tb_draw(&t, t.root, &f, AREA_R - t.n[t.root].w, 40);
                }
                double d = (now() - t0) / 2000;
                if (d < best) best = d;
            }
            printf("%-10s %-5s %5zu | %5d %5s %3d %4d | %5d %5s %3d %4d | %5d %3d | %8.2f\n", S[i].id, form ? "frac" : "dec", strlen(a),
                   w[2], w[2] <= MAXW ? "yes" : "no", ln[2], hh[2], w[3], w[3] <= MAXW ? "yes" : "no", ln[3], hh[3], nodes, dep, best * 1e6);
        }
    printf("\n(width available %d px; ln: lines after wrapping at the top-level + and -, 0 when a term alone is too wide;"
           " h: the answer's height wrapped, with its conditions, of %d px for entries;"
           " nodes: the pool's high-water mark across scale 2 and 3 including the wrap's new nodes)\n", MAXW, BOT - TOP);

    for (int i = 0; i < NS; i++) {
        sample_frame(&S[i], 2, 1, "");
        sample_frame(&S[i], 3, 1, "");
    }
    sample_frame(&S[0], 2, 0, "-times");
    sample_frame(&S[2], 2, 0, "-times");
    overflow_frames();

    static const char *const nav[6] = {"COPY", "EDIT", "STO", "1D", "DEL", "INFO"};
    entry w1[] = {{S[0].input, S[0].frac}, {S[4].input, S[4].dec}, {S[7].input, S[7].dec}};
    worksheet("ws-a-main2-in1", w1, 3, 1, 2, 1, 1, 1, nav);
    worksheet("ws-b-main2-in2", w1, 3, 2, 2, 2, 1, 1, nav);
    worksheet("ws-c-main3-in2", w1, 3, 2, 3, 2, 1, 1, nav);
    worksheet("ws-b-main2-in2-times", w1, 3, 2, 2, 2, 0, 1, nav);
    entry w2[] = {{S[1].input, S[1].frac}, {S[5].input, S[5].frac}, {S[6].input, S[6].dec}};
    worksheet("ws-d-main2-in2", w2, 3, 2, 2, 2, 1, 2, nav);
    worksheet("ws-e-main3-in2", w2, 3, 2, 3, 2, 1, 2, nav);
    entry w3[] = {{S[9].input, S[9].frac}};
    worksheet("ws-f-taylor-main2", w3, 1, 2, 2, 2, 1, 0, nav);
    worksheet("ws-g-taylor-main3", w3, 1, 2, 3, 2, 1, 0, nav);
    entry w4[] = {{S[9].input, S[9].dec}};
    worksheet("ws-h-taylor-dec-main2", w4, 1, 2, 2, 2, 1, 0, nav);

    /* a full worksheet frame's host time: three entries at scale 2 */
    double best = 1e9;
    for (int r = 0; r < 5; r++) {
        double t0 = now();
        for (int k = 0; k < 500; k++) {
            memset(&f, 0, sizeof f);
            int bottom = BOT;
            for (int i = 2; i >= 0; i--) bottom = draw_entry(&f, bottom, &w1[i], 2, 2, 2, 1, 0);
        }
        double d = (now() - t0) / 500;
        if (d < best) best = d;
    }
    printf("\nworksheet ws-b (three entries, parse + fit + draw, frame cleared): %.2f us on the host\n", best * 1e6);
    if (how_wide_warned) printf("note: %d answers had a term too wide even at scale 1\n", how_wide_warned);
    return 0;
}
