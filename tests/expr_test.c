/* expr_test: unit 082's oracle for expressions as values (work/082-expressions-as-values.md sections 6, 7, 8 and 9).
 *
 *   build/expr_test [--write-golden]
 *
 * 1. The goldens: every tests/golden-*.img loads and saves again as version 20, its bytes pinned by size and body CRC
 *    as the core at b029c45 (before any 082 code) saved them; the two version-20 goldens equal their files (memcmp).
 * 2. A round trip: an expression or a name in every place a value may hold one (the stack, LASTx, the letters, the
 *    message's X, SOLVE's saved stack), saved (version 21), loaded, saved again: the same bytes, every text the same.
 *    tests/golden-v21-082.img is that image (--write-golden writes it), loaded and saved again byte for byte.
 * 3. The reader: a section after the last one this build knows is skipped by its size in a version-21 image and
 *    refused in a version-20 one; an expression in a version-20 image and a kind past AB_NAME (4C) are refused; an
 *    expression in ANS (RPN to ALG sets ANS to X) loads.
 * 4. The store: all 48 places (X, Y, Z, T, LASTx, 27 letters, 16 history values) holding distinct expressions, the
 *    49th is ABN_EXPR_FULL with every text unchanged; freeing a place lets the next succeed in its slot. A fuzz of new,
 *    copy and clear over the 48 places checks every live text against a model after each step, and that the store is
 *    full exactly when 48 distinct expressions are live.
 * 5. The op sweep: every op (args 0 and the letter B), in STU's RPN, with an expression or a name on X, on Y, on both,
 *    or in B or I, args 0, 1, B and (I). An op either gives NOT A NUMBER with the stack and letters unchanged, or
 *    leaves nothing computed from the expression read as a number (judged against the same op run with 0 and 7 in
 *    its place); and whatever it left saves an image that loads.
 * 6, 7. The classes the sweep can't reach from RPN's keys, and →X and →EQN. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "state.h"

static int checks, failures;

static void expect(bool ok, const char *what)
{
    checks++;
    if (!ok) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static uint8_t *file(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *p = malloc((size_t)len);
    *n = fread(p, 1, (size_t)len, f);
    fclose(f);
    return p;
}

static uint32_t body_crc(const uint8_t *p, size_t n) { return ab_crc32(p, n - 4); }

static void put32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

/* the image's length and checksum written again after an edit */
static void reseal(uint8_t *p, size_t n)
{
    put32(p + 8, (uint32_t)n);
    put32(p + n - 4, ab_crc32(p, n - 4));
}

static ab_calc ca, cb, base;
static uint8_t img1[300000], img2[300000];

/* ---- 1. the goldens ---- */

static void goldens(void)
{
    static const struct { const char *path; size_t len; uint32_t crc; bool same; } G[] = {
        /* measured 2026-10-26 with the core at b029c45 (scratch resave program): load, save again */
        {"tests/golden-v1.img", 1264, 0xF0E2C760u, false},
        {"tests/golden-v10-041.img", 814, 0xBCC1631Fu, false},
        {"tests/golden-v12-041d.img", 838, 0xEDEC7390u, false},
        {"tests/golden-v12-041d-regx.img", 743, 0x6A64DB27u, false},
        {"tests/golden-v20-082.img", 1062, 0x455A56E3u, true},
        {"tests/golden-v20-082-full.img", 14231, 0x97018F1Au, true},
    };
    char what[160];
    for (size_t g = 0; g < sizeof G / sizeof G[0]; g++) {
        size_t n;
        uint8_t *p = file(G[g].path, &n);
        snprintf(what, sizeof what, "%s: found", G[g].path);
        expect(p != NULL, what);
        if (!p) continue;
        ab_init(&ca);
        snprintf(what, sizeof what, "%s: loads", G[g].path);
        expect(ab_state_load(&ca, p, n) == ABN_OK, what);
        size_t m = ab_state_save(&ca, img1, sizeof img1);
        snprintf(what, sizeof what, "%s: saves again as version 20, %zu B, body %08X (b029c45: %zu B, %08X)", G[g].path,
                 m, body_crc(img1, m), G[g].len, G[g].crc);
        expect(img1[4] == 20 && img1[5] == 0 && m == G[g].len && body_crc(img1, m) == G[g].crc, what);
        if (G[g].same) {
            snprintf(what, sizeof what, "%s: saved again, byte for byte the file", G[g].path);
            expect(m == n && !memcmp(img1, p, n), what);
        }
        free(p);
    }
}

/* ---- 2. the round trip ---- */

static void put_expr(ab_calc *c, ab_val *place, const char *text)
{
    ab_val v;
    abn_status st = ab_expr_new(c, text, &v);
    expect(st == ABN_OK, "a store with room takes an expression");
    if (st == ABN_OK) *place = v;
}

static ab_val name_val(int k)
{
    ab_val v;
    memset(&v, 0, sizeof v);
    v.kind = AB_NAME;
    v.len = k;
    return v;
}

static ab_val real_int(int k)
{
    ab_val v;
    memset(&v, 0, sizeof v);
    v.re = abn_from_int(k);
    v.im = v.p3 = abn_zero();
    return v;
}

static bool same_val(const ab_calc *c1, const ab_val *a, const ab_calc *c2, const ab_val *b)
{
    if (a->kind != b->kind) return false;
    if (ab_is_sym(a)) return !strcmp(ab_expr_text(c1, a), ab_expr_text(c2, b));
    return a->len == b->len && !memcmp(&a->re, &b->re, sizeof a->re) && !memcmp(&a->im, &b->im, sizeof a->im) &&
           !memcmp(&a->p3, &b->p3, sizeof a->p3);
}

/* every place the round trip fills, in one list */
static int places(ab_calc *c, ab_val **out)
{
    int n = 0;
    out[n++] = &c->x, out[n++] = &c->y, out[n++] = &c->z, out[n++] = &c->t, out[n++] = &c->lastx;
    for (int i = 0; i < 27; i++) out[n++] = &c->var[i];
    out[n++] = &c->err_x;
    out[n++] = &c->susp.x, out[n++] = &c->susp.y, out[n++] = &c->susp.z, out[n++] = &c->susp.t;
    out[n++] = &c->susp.lastx, out[n++] = &c->susp.var0, out[n++] = &c->ev_y, out[n++] = &c->ev_z, out[n++] = &c->ev_t;
    return n;
}

static void build_v21(ab_calc *c)
{
    ab_init(c);
    ab_do_arg(c, AB_MODEL, AB_MODEL_STU);
    /* X an expression; Y the same value (ENTER's copy: one slot); Z a name; T a number; LASTx an expression */
    put_expr(c, &c->x, "3\xc3\x97X^2-4");
    c->y = c->x;
    c->z = name_val(1);
    c->t = real_int(4);
    put_expr(c, &c->lastx, "SIN(A)+B");
    put_expr(c, &c->var[0], "(A+B)\xc3\x97(A-B)");
    c->var[1] = name_val(26);                   /* B holds i (the name) */
    c->var[2] = c->var[0];                      /* C the same expression as A */
    put_expr(c, &c->var[25], "\xe2\x88\x9a(X)");
    for (int i = 3; i < 10; i++) {
        char t[32];
        snprintf(t, sizeof t, "X^%d+%d", i, i);
        put_expr(c, &c->var[i], t);
    }
    c->var[26] = name_val(0);                   /* i holds A */
    /* SOLVE's saved stack: the places hold values while it runs; set here as an image may carry them */
    c->susp.y = c->y, c->susp.z = c->z, c->susp.var0 = name_val(25);
    c->ev_t = c->lastx;
    /* a message showing with X an expression: NOT A NUMBER from SIN */
    expect(ab_do(c, AB_SIN) == ABN_NOT_NUMBER, "082: SIN of an expression is NOT A NUMBER");
    expect(c->msg && c->err_status == ABN_NOT_NUMBER, "082: NOT A NUMBER is the message showing");
}

static void round_trip(bool write_golden)
{
    ab_val *pa[64], *pb[64];
    char what[160];
    build_v21(&ca);
    size_t m1 = ab_state_save(&ca, img1, sizeof img1);
    expect(m1 > 0 && m1 <= sizeof img1 && img1[4] == 21, "082: an image with an expression is version 21");
    ab_init(&cb);
    expect(ab_state_load(&cb, img1, m1) == ABN_OK, "082: the version-21 image loads");
    size_t m2 = ab_state_save(&cb, img2, sizeof img2);
    expect(m1 == m2 && !memcmp(img1, img2, m1), "082: saved, loaded, saved again: byte for byte");
    int n = places(&ca, pa);
    places(&cb, pb);
    for (int i = 0; i < n; i++) {
        snprintf(what, sizeof what, "082: place %d the same value after the round trip (%s)", i,
                 ab_is_sym(pa[i]) ? ab_expr_text(&ca, pa[i]) : "a number");
        expect(same_val(&ca, pa[i], &cb, pb[i]), what);
    }
    expect(cb.x.kind == AB_EXPR && cb.y.kind == AB_EXPR && cb.x.len == cb.y.len,
           "082: one text in two places loads into one slot");
    expect(cb.msg && cb.err_status == ABN_NOT_NUMBER, "082: the message showing loads");
    if (write_golden) {
        FILE *f = fopen("tests/golden-v21-082.img", "wb");
        fwrite(img1, 1, m1, f);
        fclose(f);
        printf("expr_test: wrote tests/golden-v21-082.img, %zu B, body %08X\n", m1, body_crc(img1, m1));
    }
    size_t gn;
    uint8_t *g = file("tests/golden-v21-082.img", &gn);
    expect(g != NULL, "tests/golden-v21-082.img found");
    if (g) {
        ab_init(&cb);
        expect(ab_state_load(&cb, g, gn) == ABN_OK, "golden-v21-082: loads");
        m2 = ab_state_save(&cb, img2, sizeof img2);
        expect(m2 == gn && !memcmp(img2, g, gn), "golden-v21-082: saved again, byte for byte the file");
        expect(cb.x.kind == AB_EXPR && !strcmp(ab_expr_text(&cb, &cb.x), "3\xc3\x97X^2-4") && cb.z.kind == AB_NAME &&
                   !strcmp(ab_expr_text(&cb, &cb.z), "B") && !strcmp(ab_expr_text(&cb, &cb.var[1]), "i"),
               "golden-v21-082: X 3×X^2-4, Z the name B, B the name i");
        snprintf(what, sizeof what, "golden-v21-082: the image this build writes (%zu B, %08X; file %zu B, %08X)", m1,
                 body_crc(img1, m1), gn, body_crc(g, gn));
        expect(m1 == gn && !memcmp(img1, g, gn), what);
        free(g);
    }
}

/* ---- 3. the reader ---- */

/* the offset of section tag's payload, -1 if absent */
static long section(const uint8_t *p, size_t n, unsigned tag, uint32_t *size)
{
    size_t at = 12;
    while (at + 6 <= n - 4) {
        unsigned t = p[at] | p[at + 1] << 8;
        uint32_t s = p[at + 2] | p[at + 3] << 8 | p[at + 4] << 16 | (uint32_t)p[at + 5] << 24;
        if (t == tag) {
            *size = s;
            return (long)(at + 6);
        }
        at += 6 + s;
    }
    return -1;
}

static void reader(void)
{
    static uint8_t e[300000];
    build_v21(&ca);
    size_t m = ab_state_save(&ca, img1, sizeof img1);
    /* an unknown section (tag 40, after T_CASCPLX = 21), five bytes, before the checksum */
    memcpy(e, img1, m - 4);
    size_t k = m - 4;
    e[k++] = 40, e[k++] = 0;
    put32(e + k, 5), k += 4;
    memcpy(e + k, "\x01\x02\x03\x04\x05", 5), k += 5;
    k += 4;
    reseal(e, k);
    ab_init(&cb);
    expect(ab_state_load(&cb, e, k) == ABN_OK, "082: an unknown section in a version-21 image is skipped");
    size_t m2 = ab_state_save(&cb, img2, sizeof img2);
    expect(m2 == m && !memcmp(img1, img2, m), "082: ... and the rest loaded as without it");
    {   /* on an image with no expression (golden-v20-082): refused as version 20, skipped as 21 */
        size_t gn;
        uint8_t *g = file("tests/golden-v20-082.img", &gn);
        static uint8_t u[4096];
        size_t un = gn - 4;
        memcpy(u, g, un);
        u[un++] = 40, u[un++] = 0;
        put32(u + un, 2), un += 4;
        u[un++] = 9, u[un++] = 9;
        un += 4;
        reseal(u, un);
        ab_init(&cb);
        expect(ab_state_load(&cb, u, un) != ABN_OK, "082: an unknown section in a version-20 image is refused");
        u[4] = 21;
        reseal(u, un);
        expect(ab_state_load(&cb, u, un) == ABN_OK, "082: ... and the same image marked 21 loads, the section skipped");
        free(g);
    }
    /* an unknown section out of order (tag 3 after the last) is refused in 21 too */
    e[4] = 21;
    e[m - 4] = 3;
    reseal(e, k);
    expect(ab_state_load(&cb, e, k) != ABN_OK, "082: a section out of order is refused");

    /* an expression in a version-20 image */
    memcpy(e, img1, m);
    e[4] = 20;
    reseal(e, m);
    expect(ab_state_load(&cb, e, m) != ABN_OK, "082: an expression in a version-20 image is refused");
    /* X's kind past AB_NAME (4C, until Michael's card) */
    uint32_t size;
    long st = section(img1, m, 2, &size);   /* T_STACK: X first */
    expect(st > 0 && img1[st] == AB_EXPR, "082: T_STACK found, X an expression");
    memcpy(e, img1, m);
    if (st > 0) {
        e[st] = AB_NAME + 1;
        reseal(e, m);
        expect(ab_state_load(&cb, e, m) != ABN_OK, "082: a kind past AB_NAME is refused (4C)");
        /* a name past i */
        memcpy(e, img1, m);
        long z = st;
        for (int i = 0; i < 2; i++) z += 3 + (img1[z] == AB_EXPR ? img1[z + 1] : 0);   /* past X and Y */
        expect(img1[z] == AB_NAME && img1[z + 1] == 1, "082: Z the name B in the image");
        e[z + 1] = 27;
        reseal(e, m);
        expect(ab_state_load(&cb, e, m) != ABN_OK, "082: a name past i is refused");
        memcpy(e, img1, m);
        e[z] = AB_NAME + 1;                     /* kind 5, three bytes as a name's: shaped to parse, refused for its kind */
        reseal(e, m);
        expect(ab_state_load(&cb, e, m) != ABN_OK, "082: a kind past AB_NAME is refused, shaped as a name (4C)");
        /* an expression's text with a control byte */
        memcpy(e, img1, m);
        e[st + 3] = 0x07;
        reseal(e, m);
        expect(ab_state_load(&cb, e, m) != ABN_OK, "082: an expression's text with a control byte is refused");
    }
    /* MEMORY FULL or NOT A NUMBER showing with no expression in the image: version 21 for the status alone */
    for (int k2 = 0; k2 < 2; k2++) {
        ab_init(&ca);
        ca.msg = true;
        ca.err_status = k2 ? ABN_NOT_NUMBER : ABN_EXPR_FULL;
        size_t m3 = ab_state_save(&ca, e, sizeof e);
        ab_init(&cb);
        expect(e[4] == 21 && ab_state_load(&cb, e, m3) == ABN_OK && cb.msg && cb.err_status == ca.err_status,
               k2 ? "082: NOT A NUMBER showing, nothing else new: version 21, and it loads showing"
                  : "082: MEMORY FULL showing, nothing else new: version 21, and it loads showing");
    }
    /* ANS holding an expression (RPN to ALG sets ANS to X: the op sweep found it), saved and loaded whole */
    build_v21(&ca);
    put_expr(&ca, &ca.ans, "A+1");
    m = ab_state_save(&ca, e, sizeof e);
    ab_init(&cb);
    expect(ab_state_load(&cb, e, m) == ABN_OK && cb.ans.kind == AB_EXPR && !strcmp(ab_expr_text(&cb, &cb.ans), "A+1"),
           "082: an expression in ANS saves and loads (RPN to ALG puts X's there)");
}

/* ---- 4. the store ---- */

#define NP AB_EXPR_SLOTS

static int store_places(ab_calc *c, ab_val **out)
{
    int n = 0;
    out[n++] = &c->x, out[n++] = &c->y, out[n++] = &c->z, out[n++] = &c->t, out[n++] = &c->lastx;
    for (int i = 0; i < 27; i++) out[n++] = &c->var[i];
    for (int i = 0; i < AB_HIST; i++) out[n++] = &c->hist_val[i];
    return n;
}

static int live_slots(const ab_calc *c, ab_val **p, int n)
{
    bool used[AB_EXPR_SLOTS] = {false};
    int k = 0;
    (void)c;
    for (int i = 0; i < n; i++)
        if (p[i]->kind == AB_EXPR && !used[p[i]->len]) used[p[i]->len] = true, k++;
    return k;
}

static void store(void)
{
    ab_val *p[80];
    char model[80][AB_EQN_LEN], what[160];
    ab_init(&ca);
    int n = store_places(&ca, p);
    expect(n == NP, "082: 48 places");
    for (int i = 0; i < n; i++) {
        snprintf(model[i], sizeof model[i], "E%02d+X", i);
        put_expr(&ca, p[i], model[i]);
    }
    ab_val v;
    expect(ab_expr_new(&ca, "ONE MORE", &v) == ABN_EXPR_FULL, "082: the 49th expression is ABN_EXPR_FULL");
    bool same = true;
    for (int i = 0; i < n; i++) same &= p[i]->kind == AB_EXPR && !strcmp(ab_expr_text(&ca, p[i]), model[i]);
    expect(same, "082: a full store refused, every text unchanged");
    int freed = p[7]->len;
    memset(p[7], 0, sizeof *p[7]);              /* (a number in the 8th place) */
    expect(ab_expr_new(&ca, "ONE MORE", &v) == ABN_OK && v.len == freed, "082: one place freed, the next takes its slot");
    *p[7] = v;
    snprintf(model[7], sizeof model[7], "ONE MORE");
    same = true;
    for (int i = 0; i < n; i++) same &= !strcmp(ab_expr_text(&ca, p[i]), model[i]);
    expect(same, "082: ... every other text unchanged");

    /* the fuzz, over every place expr_mark marks that can hold one (abacus #11230: ANS, the message's X, SOLVE's and
       the evaluation's copies too, so a place the mark left out loses its text to the next new one and goes red) */
    ab_init(&ca);
    n = places(&ca, p);
    p[n++] = &ca.ans;
    for (int i = 0; i < AB_HIST; i++) p[n++] = &ca.hist_val[i];
    bool has[80] = {false};
    unsigned seed = 82;
    int bad = 0, fulls = 0, steps = 200000;
    for (int s = 0; s < steps && bad < 5; s++) {
        seed = seed * 1103515245u + 12345u;
        int a = (int)(seed >> 8) % n, b = (int)(seed >> 16) % n, what_op = (int)(seed >> 24) % 10;
        if (s % 997 < n) a = s % 997, what_op = 0;    /* a burst of news over every place: the store fills */
        if (what_op < 5) {                      /* a new expression into place a */
            char t[AB_EQN_LEN];
            snprintf(t, sizeof t, "X+%d", s);
            abn_status st = ab_expr_new(&ca, t, &v);
            int live = live_slots(&ca, p, n);
            if (st == ABN_EXPR_FULL) {
                fulls++;
                if (live != NP) bad++, printf("FAIL: step %d: full with %d live\n", s, live);
            } else {
                if (live == NP) bad++, printf("FAIL: step %d: a 49th live expression\n", s);
                *p[a] = v;
                snprintf(model[a], sizeof model[a], "%s", t);
                has[a] = true;
            }
        } else if (what_op < 8) {               /* a copy, a to b */
            *p[b] = *p[a];
            has[b] = has[a];
            memcpy(model[b], model[a], sizeof model[b]);
        } else {                                /* place a cleared */
            memset(p[a], 0, sizeof *p[a]);
            has[a] = false;
        }
        for (int i = 0; i < n; i++)
            if (has[i] != (p[i]->kind == AB_EXPR) || (has[i] && strcmp(ab_expr_text(&ca, p[i]), model[i]))) {
                bad++;
                printf("FAIL: step %d: place %d lost its text\n", s, i);
                break;
            }
    }
    snprintf(what, sizeof what, "082: the store fuzz, %d steps over %d places (%d refused full), every text kept", steps, n,
             fulls);
    printf("expr_test: %s\n", what);
    expect(bad == 0 && fulls > 0, what);
}

/* ---- 5. the op sweep ---- */

static bool same_state(const ab_calc *a, const ab_calc *b)
{
    const ab_val *va[] = {&a->x, &a->y, &a->z, &a->t, &a->lastx}, *vb[] = {&b->x, &b->y, &b->z, &b->t, &b->lastx};
    for (int i = 0; i < 5; i++)
        if (!same_val(a, va[i], b, vb[i])) return false;
    for (int i = 0; i < 27; i++)
        if (!same_val(a, &a->var[i], b, &b->var[i])) return false;
    return true;
}

static int val_places(ab_calc *c, ab_val **out)
{
    int n = 0;
    out[n++] = &c->x, out[n++] = &c->y, out[n++] = &c->z, out[n++] = &c->t, out[n++] = &c->lastx;
    for (int i = 0; i < 27; i++) out[n++] = &c->var[i];
    return n;
}

static ab_calc cz, cn, bz, bn;
static int sweep_verbose;

/* the base calculator with v in the setup's places (X; Y; X and Y; the letter B) */
static void sweep_base(ab_calc *c, int s, const ab_val *v)
{
    ab_init(c);
    ab_do_arg(c, AB_MODEL, AB_MODEL_STU);
    /* a function for SOLVE and the integral (LBL A, RTN: f(x) = x), so they reach what they read */
    ab_do(c, AB_PRGM);
    ab_do_arg(c, AB_LBL, 0);
    ab_do(c, AB_RTN);
    ab_do(c, AB_PRGM);
    ab_do_arg(c, AB_FN, 0);
    c->var[8] = real_int(2);                    /* I: (I) names indirect register 2 (STU, the 35s model) */
    c->var[1] = real_int(3);
    c->x = real_int(2);
    c->y = real_int(5);
    if (s == 0 || s == 2) c->x = *v;
    if (s == 1 || s == 2) c->y = *v;
    if (s == 3) c->var[1] = *v;
    if (s == 4) c->var[8] = *v;
}

/* Each op runs three times from the same state but for the setup's places: holding the expression (or name), 0, and
   7. An op given an expression either returns NOT A NUMBER with the stack and letters unchanged, or what it leaves
   must not depend on reading the expression as a number: where the expression's run leaves a number equal to the
   0-run's at a place where the 7-run's differs, the number was computed from the expression read as 0 (the fault
   section 8 exists for: is_cx and is_vec let an expression through as zeroed parts). The status and the YES/NO
   answer are compared the same way. */
static void sweep(void)
{
    static const char *const SETUP[] = {"X", "Y", "X and Y", "B", "I"};
    static const int ARGS[] = {0, 1, AB_VAR_A + 1, AB_VAR_IND_I};
    int unloadable = 0;
    int odd = 0, refused = 0, runs = 0, passed = 0;
    for (int sym = 0; sym < 2; sym++)
        for (int s = 0; s < 5; s++) {
            ab_val e, z0 = real_int(0), n7 = real_int(7);
            e = name_val(0);
            sweep_base(&base, s, &e);
            if (!sym) {                         /* an expression: made in base's own store */
                put_expr(&base, &e, "A+1");
                if (s == 0 || s == 2) base.x = e;
                if (s == 1 || s == 2) base.y = e;
                if (s == 3) base.var[1] = e;
                if (s == 4) base.var[8] = e;
            }
            sweep_base(&bz, s, &z0);
            sweep_base(&bn, s, &n7);
            for (int op = 0; op <= AB_OP_LAST; op++)
                for (int ai = 0; ai < 4; ai++) {
                    int arg = ARGS[ai];
                    memcpy(&ca, &base, sizeof ca);
                    abn_status st = ab_do_arg(&ca, (ab_op)op, arg);
                    runs++;
                    {   /* whatever the op left, its image loads (an expression somewhere the reader refuses is a lost state) */
                        size_t m = ab_state_save(&ca, img1, sizeof img1);
                        ab_init(&cb);
                        if (m == 0 || m > sizeof img1 || ab_state_load(&cb, img1, m) != ABN_OK) {
                            if (unloadable++ < 5)
                                printf("FAIL: op %d arg %d (%s on %s): its image doesn't load\n", op, arg,
                                       sym ? "a name" : "an expression", SETUP[s]);
                        }
                    }
                    if (st == ABN_NOT_NUMBER) {
                        refused++;
                        if (!same_state(&ca, &base)) {
                            odd++;
                            printf("FAIL: op %d arg %d (%s on %s): NOT A NUMBER, but the stack or a letter changed\n",
                                   op, arg, sym ? "a name" : "an expression", SETUP[s]);
                        }
                        continue;
                    }
                    passed++;
                    memcpy(&cz, &bz, sizeof cz);
                    memcpy(&cn, &bn, sizeof cn);
                    abn_status sz = ab_do_arg(&cz, (ab_op)op, arg), sn = ab_do_arg(&cn, (ab_op)op, arg);
                    ab_val *pa[40], *pz[40], *pn[40];
                    int np = val_places(&ca, pa);
                    val_places(&cz, pz);
                    val_places(&cn, pn);
                    int leak = -1;
                    for (int i = 0; i < np && leak < 0; i++)
                        if (!ab_is_sym(pa[i]) && same_val(&ca, pa[i], &cz, pz[i]) && !same_val(&cz, pz[i], &cn, pn[i]))
                            leak = i;
                    bool by_status = st == sz && sz != sn, by_yes = ca.yes == cz.yes && cz.yes != cn.yes;
                    /* an error only the expression gives (neither number does) is NOT A NUMBER's to give */
                    bool wrong = st != ABN_OK && st != ABN_CLAMPED && st != sz && st != sn;
                    if (leak >= 0 || by_status || by_yes || wrong) {
                        odd++;
                        printf("FAIL: op %d arg %d (%s on %s): status %d (0: %d, 7: %d); place %d read as 0%s%s%s\n", op,
                               arg, sym ? "a name" : "an expression", SETUP[s], (int)st, (int)sz, (int)sn, leak,
                               by_status ? ", the status as 0's" : "", by_yes ? ", YES/NO as 0's" : "",
                               wrong ? ", an error only the expression gives, not NOT A NUMBER" : "");
                    } else if (sweep_verbose) {
                        printf("op %d arg %d (%s on %s): passes, status %d\n", op, arg, sym ? "a name" : "an expression",
                               SETUP[s], (int)st);
                    }
                }
        }
    char what[200];
    snprintf(what, sizeof what, "082: the op sweep, %d runs: %d NOT A NUMBER, %d through without a number read, %d read one",
             runs, refused, passed - odd, odd);
    printf("expr_test: %s\n", what);
    expect(odd == 0, what);
    snprintf(what, sizeof what, "082: the op sweep: every state an op left saves an image that loads (%d don't)", unloadable);
    expect(unloadable == 0, what);
}

/* ---- 6. the classes the sweep can't reach from RPN's keys alone ---- */

static void classes(void)
{
    ab_val e;
    /* an equation reading a letter, (I), or the expression line's ANS holding an expression */
    for (int k = 0; k < 3; k++) {
        ab_init(&ca);
        ab_do_arg(&ca, AB_MODEL, AB_MODEL_STU);
        put_expr(&ca, &e, "A+1");
        ca.var[1] = e;
        ca.var[8] = real_int(-2);               /* (I): B (the 35s model, STU's: -1 to -26 are A to Z) */
        expect(ab_eqn_add(&ca, k == 1 ? "\xe2\x92\xa4+1" : k == 2 ? "B" : "B+1") == ABN_OK, "082: the equation added");
                                                /* (⒤: (I); B alone: no arithmetic behind the letter's read) */
        expect(ca.eqn_mode, "082: the equation added shown in Equation mode");
        abn_status st = ab_do_arg(&ca, AB_XEQ, AB_TGT_EQN);
        while (st == ABN_OK && ca.stop == AB_STOP_INPUT) st = ab_do(&ca, AB_RS);
        expect(st == ABN_NOT_NUMBER, k == 1   ? "082: an equation reading (I), B holding an expression: NOT A NUMBER"
                                     : k == 2 ? "082: the equation B, B holding an expression: NOT A NUMBER"
                                              : "082: an equation reading B, holding an expression: NOT A NUMBER");
    }
    /* SOLVE: the guess in X, or the unknown, an expression; the integral: a limit in X or Y */
    for (int k = 0; k < 4; k++) {
        static const char *const W[] = {"SOLVE, the guess in X", "SOLVE, the unknown B", "the integral, X a limit",
                                        "the integral, Y a limit"};
        char what[120];
        ab_init(&ca);
        ab_do_arg(&ca, AB_MODEL, AB_MODEL_STU);
        ab_do(&ca, AB_PRGM);
        ab_do_arg(&ca, AB_LBL, 0);
        ab_do(&ca, AB_RTN);
        ab_do(&ca, AB_PRGM);
        ab_do_arg(&ca, AB_FN, 0);
        ca.x = real_int(1), ca.y = real_int(2), ca.var[1] = real_int(3);
        put_expr(&ca, &e, "A+1");
        if (k == 0 || k == 2) ca.x = e;
        if (k == 1) ca.var[1] = e;
        if (k == 3) ca.y = e;
        abn_status st = ab_do_arg(&ca, k < 2 ? AB_SOLVE : AB_INTEG, AB_VAR_A + 1);
        snprintf(what, sizeof what, "082: %s: NOT A NUMBER (%d)", W[k], (int)st);
        expect(st == ABN_NOT_NUMBER, what);
    }
    {   /* the 33s's (i), i holding an expression (a value made in STU, the mode then changed) */
        ab_init(&ca);
        ab_do_arg(&ca, AB_MODEL, 33);
        put_expr(&ca, &e, "A+1");
        ca.var[26] = e;
        expect(ab_do_arg(&ca, AB_RCL, AB_VAR_IND_I) == ABN_NOT_NUMBER, "082: 33s mode: RCL (i), i an expression: NOT A NUMBER");
    }
    {   /* the 35s's built-in 2x2 solver, A an expression */
        ab_init(&ca);
        ab_do_arg(&ca, AB_MODEL, 35);
        put_expr(&ca, &e, "A+1");
        ca.var[0] = e;
        ab_do(&ca, AB_EQN);
        ab_do(&ca, AB_SST);
        abn_status st = ab_do_arg(&ca, AB_SOLVE, -1);
        int n = 0;
        while (st == ABN_OK && ca.stop == AB_STOP_INPUT && n++ < 10) st = ab_do(&ca, AB_RS);
        char what[120];
        snprintf(what, sizeof what, "082: the 2x2 solver, A an expression: NOT A NUMBER (%d after %d prompts)", (int)st, n);
        expect(st == ABN_NOT_NUMBER, what);
    }
    {   /* SOLVE of a program that returns an expression (LBL A, RCL C, RTN with C holding one): f is no number */
        ab_init(&ca);
        ab_do_arg(&ca, AB_MODEL, AB_MODEL_STU);
        ab_do(&ca, AB_PRGM);
        ab_do_arg(&ca, AB_LBL, 0);
        ab_do_arg(&ca, AB_RCL, AB_VAR_A + 2);
        ab_do(&ca, AB_RTN);
        ab_do(&ca, AB_PRGM);
        ab_do_arg(&ca, AB_FN, 0);
        put_expr(&ca, &e, "A+1");
        ca.var[2] = e;
        ca.x = real_int(1), ca.var[1] = real_int(3);
        abn_status st = ab_do_arg(&ca, AB_SOLVE, AB_VAR_A + 1);
        while (st == ABN_OK && ca.stop == AB_STOP_SLICE) st = ab_continue(&ca);
        char what[120];
        snprintf(what, sizeof what, "082: SOLVE of a function returning an expression: NOT A NUMBER (%d)", (int)st);
        expect(st == ABN_NOT_NUMBER, what);
    }
}

/* ---- 7. →X and →EQN (AB_EQDROP, AB_EQSTORE) ---- */

static void drop_store(void)
{
    char what[160];
    ab_init(&ca);
    ab_do_arg(&ca, AB_MODEL, AB_MODEL_STU);
    ab_do(&ca, AB_EQN);
    ab_do(&ca, AB_EQN);                         /* (Equation mode off: the keys outside it) */
    expect(ab_do(&ca, AB_EQDROP) == ABN_NO_EQN && ab_do(&ca, AB_EQSTORE) == ABN_NO_EQN,
           "082: →X and →EQN outside Equation mode: the list's keys only (NO EQN notice)");
    for (int i = 1; i <= 4; i++) ca.x = real_int(i), ab_do(&ca, AB_ENTER);
    ca.x = real_int(4), ca.lift = true;
    expect(ab_eqn_add(&ca, "3\xc3\x97X^2-4") == ABN_OK, "082: an equation typed");
    int n0 = ca.eqn_count, cur = ca.eqn_cur;
    memcpy(&cb, &ca, sizeof cb);
    expect(ab_do(&ca, AB_EQDROP) == ABN_OK && ca.x.kind == AB_EXPR && !strcmp(ab_expr_text(&ca, &ca.x), "3\xc3\x97X^2-4") &&
               same_val(&ca, &ca.y, &cb, &cb.x) && same_val(&ca, &ca.z, &cb, &cb.y) && same_val(&ca, &ca.t, &cb, &cb.z) &&
               ca.eqn_mode && ca.eqn_count == n0 && ca.eqn_cur == cur,
           "082: →X: the shown equation's text in X, the stack lifted, Equation mode and the list as they were");
    expect(ab_do(&ca, AB_EQDROP) == ABN_OK && ca.x.kind == AB_EXPR && ca.y.kind == AB_EXPR && ca.x.len != ca.y.len,
           "082: →X again: a second expression (its own slot)");
    size_t m = ab_state_save(&ca, img1, sizeof img1);
    ab_init(&cb);
    expect(img1[4] == 21 && ab_state_load(&cb, img1, m) == ABN_OK && !strcmp(ab_expr_text(&cb, &cb.y), "3\xc3\x97X^2-4"),
           "082: after →X the image is version 21 and loads with the expressions");
    /* →EQN: X's expression after the shown entry, shown; X kept */
    expect(ab_do(&ca, AB_EQSTORE) == ABN_OK && ca.eqn_count == n0 + 1 && ca.eqn_cur == cur + 1 &&
               !strcmp(ca.eqn[ca.eqn_cur], "3\xc3\x97X^2-4") && ca.eqn_model[ca.eqn_cur] == AB_MODEL_STU && ca.x.kind == AB_EXPR,
           "082: →EQN: X's expression inserted after the shown entry, now shown, stamped STU; X kept");
    ca.x = name_val(2);
    expect(ab_do(&ca, AB_EQSTORE) == ABN_OK && !strcmp(ca.eqn[ca.eqn_cur], "C"), "082: →EQN of the name C: the entry C");
    ca.x = real_int(5);
    memcpy(&cb, &ca, sizeof cb);
    expect(ab_do(&ca, AB_EQSTORE) == ABN_NOT_NUMBER && ca.eqn_count == cb.eqn_count && ca.eqn_cur == cb.eqn_cur,
           "082: →EQN with a number in X: NOT A NUMBER (abacus #11174), the list unchanged");
    /* nothing at TOP or on a built-in; a 35-stamped entry is another mode's text */
    ca.msg = false;
    ca.eqn_cur = -1;
    memcpy(&cb, &ca, sizeof cb);
    expect(ab_do(&ca, AB_EQDROP) == ABN_OK && same_state(&ca, &cb), "082: →X at EQN LIST TOP: nothing");
    ca.eqn_cur = 0;
    while (ca.eqn_cur < ca.eqn_count && ca.eqn[ca.eqn_cur][0] != '\x01') ca.eqn_cur++;   /* (the built-ins follow what was typed at TOP) */
    expect(ca.eqn_cur < ca.eqn_count && ab_do(&ca, AB_EQDROP) == ABN_OK && same_state(&ca, &cb),
           "082: →X on a built-in solver's entry: nothing");
    ca.eqn_cur = cur;
    ca.eqn_model[cur] = 35;
    expect(ab_do(&ca, AB_EQDROP) == ABN_DOMAIN && same_state(&ca, &cb), "082: →X on a 35-stamped entry: INVALID DATA");
    ca.eqn_model[cur] = AB_MODEL_STU;
    /* a full store: MEMORY FULL, nothing changed (the slot asked for before the lift) */
    {
        ab_val *p[64];
        int np = store_places(&ca, p);
        for (int i = 0; i < np; i++) {
            char t[32];
            snprintf(t, sizeof t, "E%02d", i);
            put_expr(&ca, p[i], t);
        }
        ca.msg = false;
        memcpy(&cb, &ca, sizeof cb);
        abn_status st = ab_do(&ca, AB_EQDROP);
        snprintf(what, sizeof what, "082: →X with all 48 places distinct expressions: MEMORY FULL, nothing changed (%d)", (int)st);
        expect(st == ABN_EXPR_FULL && same_state(&ca, &cb) && !strcmp(ab_expr_text(&ca, &ca.t), "E03"), what);
    }
    /* 35s mode: STU only */
    ca.msg = false;                             /* (MEMORY FULL showing: the next key would only clear it) */
    ab_do_arg(&ca, AB_MODEL, 35);
    expect(ab_do(&ca, AB_EQDROP) == ABN_STU_ONLY && ab_do(&ca, AB_EQSTORE) == ABN_STU_ONLY, "082: →X and →EQN: STU only");
}

/* ---- 8. random op sequences with expressions and names in random places (section 8's fuzz) ---- */

/* every value place an op can reach */
static int all_places(ab_calc *c, ab_val **out)
{
    int n = places(c, out);
    out[n++] = &c->ans;
    return n;
}

/* which slots a value names, over every place an expression can be (the test's own list, not expr_mark's) */
static void live_marks(ab_calc *c, bool live[AB_EXPR_SLOTS])
{
    ab_val *p[80];
    int n = all_places(c, p);
    for (int i = 0; i < AB_HIST; i++) p[n++] = &c->hist_val[i];
    memset(live, 0, AB_EXPR_SLOTS);
    for (int i = 0; i < n; i++)
        if (p[i]->kind == AB_EXPR && p[i]->len >= 0 && p[i]->len < AB_EXPR_SLOTS) live[p[i]->len] = true;
}

static unsigned fz = 0x82u;
static unsigned fzr(unsigned n)
{
    fz = fz * 1103515245u + 12345u;
    return (fz >> 8) % n;
}

static void op_fuzz(void)
{
    static const char *const TEXTS[] = {"A+1", "3\xc3\x97X^2-4", "SIN(X)\xc3\xb7X", "(A+B)^2", "\xe2\x88\x9a(X)-2"};
    int seqs = 400, len = 25, steps = 0, refused = 0, leaks = 0, unloadable = 0, changed = 0, changed_text = 0;
    for (int q = 0; q < seqs; q++) {
        ab_init(&ca);
        ab_do_arg(&ca, AB_MODEL, AB_MODEL_STU);
        ca.var[1] = real_int(3), ca.x = real_int(2), ca.y = real_int(5);
        ab_eqn_add(&ca, "X^2-1");               /* (an STU equation for →X; Equation mode on half the time) */
        if (fzr(2)) ab_do(&ca, AB_EQN);
        ab_val *p[64];
        int n = all_places(&ca, p);
        for (int k = 0; k < 4; k++) {           /* expressions and names in random places, ANS and the copies included */
            ab_val v;
            int at = (int)fzr((unsigned)n);
            if (!fzr(3) || ab_expr_new(&ca, TEXTS[fzr(5)], &v) != ABN_OK) v = name_val((int)fzr(27));
            *p[at] = v;
        }
        for (int i = 0; i < len; i++) {
            int op = (int)fzr(AB_OP_LAST + 1), arg;
            if (!fzr(6)) op = fzr(3) ? AB_EQDROP : AB_EQN;   /* (new expressions: a slot taken while others are live) */
            switch (fzr(4)) {
            case 0: arg = 0; break;
            case 1: arg = 1; break;
            case 2: arg = AB_VAR_A + (int)fzr(26); break;
            default: arg = AB_VAR_IND_I; break;
            }
            /* the same state with every expression or name a 0, and a 7 */
            memcpy(&cz, &ca, sizeof cz);
            memcpy(&cn, &ca, sizeof cn);
            ab_val *pz[64], *pn[64], *pa[64];
            all_places(&cz, pz);
            all_places(&cn, pn);
            n = all_places(&ca, pa);
            bool any = false;
            for (int k = 0; k < n; k++)
                if (ab_is_sym(pa[k])) *pz[k] = real_int(0), *pn[k] = real_int(7), any = true;
            memcpy(&base, &ca, sizeof base);
            bool live0[AB_EXPR_SLOTS];
            live_marks(&base, live0);
            ca.budget = cz.budget = cn.budget = 2000;
            abn_status st = ab_do_arg(&ca, (ab_op)op, arg);
            if (ca.stop == AB_STOP_SLICE) ab_continue(&ca);
            steps++;
            if (st == ABN_NOT_NUMBER) {
                refused++;
                if (!same_state(&ca, &base) && changed++ < 3)
                    printf("FAIL: fuzz %d step %d: op %d arg %d: NOT A NUMBER, but the stack or a letter changed\n", q, i, op, arg);
            } else if (any) {
                abn_status sz = ab_do_arg(&cz, (ab_op)op, arg), sn = ab_do_arg(&cn, (ab_op)op, arg);
                if (cz.stop == AB_STOP_SLICE) ab_continue(&cz);
                if (cn.stop == AB_STOP_SLICE) ab_continue(&cn);
                ab_val *qa[40], *qz[40], *qn[40];
                int nv = val_places(&ca, qa);
                val_places(&cz, qz);
                val_places(&cn, qn);
                int leak = -1;
                for (int k = 0; k < nv && leak < 0; k++)
                    if (!ab_is_sym(qa[k]) && same_val(&ca, qa[k], &cz, qz[k]) && !same_val(&cz, qz[k], &cn, qn[k]) &&
                        !same_val(&base, k < 5 ? (const ab_val *[]){&base.x, &base.y, &base.z, &base.t, &base.lastx}[k]
                                               : &base.var[k - 5], &ca, qa[k]))
                        leak = k;               /* (a number new at that place, not one it held before) */
                bool wrong = st != ABN_OK && st != ABN_CLAMPED && st != sz && st != sn;
                bool by_status = st == sz && sz != sn;
                if (leak >= 0 || wrong || by_status) {
                    if (leaks++ < 5)
                        printf("FAIL: fuzz %d step %d: op %d arg %d: status %d (0: %d, 7: %d), place %d%s%s\n", q, i, op, arg,
                               (int)st, (int)sz, (int)sn, leak, wrong ? ", an error only the expression gives" : "",
                               by_status ? ", the status as 0's" : "");
                }
            }
            {   /* an expression never changes: a slot live before the op and after it keeps its text (abacus #11230) */
                bool live1[AB_EXPR_SLOTS];
                live_marks(&ca, live1);
                for (int k = 0; k < AB_EXPR_SLOTS; k++)
                    if (live0[k] && live1[k] && strcmp(base.exprs[k], ca.exprs[k]) && changed_text++ < 3)
                        printf("FAIL: fuzz %d step %d: op %d arg %d: slot %d's text changed under a live value\n", q, i, op,
                               arg, k);
            }
            size_t m = ab_state_save(&ca, img1, sizeof img1);
            ab_init(&cb);
            if (m == 0 || m > sizeof img1 || ab_state_load(&cb, img1, m) != ABN_OK) {
                if (unloadable++ < 3) printf("FAIL: fuzz %d step %d: op %d arg %d: its image doesn't load\n", q, i, op, arg);
            }
        }
    }
    char what[200];
    snprintf(what, sizeof what, "082: the op fuzz, %d sequences, %d steps: %d NOT A NUMBER, %d read a number, %d changed on a "
             "refusal, %d live texts changed, %d images that don't load", seqs, steps, refused, leaks, changed, changed_text,
             unloadable);
    printf("expr_test: %s\n", what);
    expect(leaks == 0 && changed == 0 && changed_text == 0 && unloadable == 0, what);
}

int main(int argc, char **argv)
{
    bool write_golden = argc > 1 && !strcmp(argv[1], "--write-golden");
    sweep_verbose = argc > 1 && !strcmp(argv[1], "-v");
    goldens();
    round_trip(write_golden);
    reader();
    store();
    sweep();
    classes();
    drop_store();
    op_fuzz();
    printf("expr_test: %d/%d checks\n", checks - failures, checks);
    return failures != 0;
}
