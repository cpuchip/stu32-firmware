/* help_test (unit 075): HELP's text against the keymap. Every action a key or a menu item reaches has an entry (on
   all three layers, every menu, the paged menus' apps and constants), every entry is reached by some action (no stale
   text), every line fits a page's 31 cells in each mode with the legend it is shown for, the MODE entries and every
   op the 33s hides vary by mode. --list prints what has no entry (kind, op, argument, legend, where).
   The sweep (abacus's 075 review, items 1-3): the real app, every state its keys reach, HELP then every key; which entry
   each page shows (help_lines wrapped at link time), so an entry no state reaches, a line no page shows (a menu's
   second) and a mode's text no state in that mode shows all fail. Its refusal note (abacus #8753) is checked against
   the key pressed for real, and the core's predicates against ab_do for every op. */
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "apps.h"
#include "calc.h"
#include "help.h"
#include "keymap.h"
#include "keys.h"
#include "power.h"
#include "screen.h"

static int checks, bad;
static bool list_mode;
static bool used[1024];

static void expect(bool ok, const char *what)
{
    checks++;
    if (!ok) {
        bad++;
        if (!list_mode) printf("help_test: %s\n", what);
    }
}

static bool varies(const help_entry *e)
{
    for (const char *s = e->text; *s;) {
        if (!strncmp(s, "33", 2) || !strncmp(s, "35", 2) || !strncmp(s, "STU", 3)) {
            const char *c = strchr(s, ':'), *nl = strchr(s, '\n');
            if (c && (!nl || c < nl) && c - s <= 9 && !memchr(s, ' ', (size_t)(c - s))) return true;
        }
        const char *nl = strchr(s, '\n');
        if (!nl) break;
        s = nl + 1;
    }
    return false;
}

static const int MODELS[3] = {33, 35, AB_MODEL_STU};

static void fits(const help_entry *e, const char *legend, const char *where)
{
    for (int m = 0; m < 3; m++) {
        char out[4][HELP_LINE_BYTES];
        int n = help_lines(e, MODELS[m], legend, out, 4);
        char w[320];
        snprintf(w, sizeof w, "%s: %d lines in mode %d (1-3 wanted)", where, n, MODELS[m]);
        expect(n >= 1 && n <= 3, w);
        snprintf(w, sizeof w, "%s: a menu's entry has %d lines in mode %d: its page shows one, then the items", where,
                 n, MODELS[m]);
        expect(e->kind != HELP_MENU || n == 1, w);
        for (int i = 0; i < n; i++) {
            snprintf(w, sizeof w, "%s: line '%s' reached the %d-byte cap (cut)", where, out[i], HELP_LINE_BYTES - 1);
            expect(strlen(out[i]) < HELP_LINE_BYTES - 4, w);    /* a cut line is never measured whole (fresh eyes) */
            snprintf(w, sizeof w, "%s: line '%s' is %d cells, past %d", where, out[i], screen_cells(out[i]), HELP_LINE_CELLS);
            expect(screen_cells(out[i]) <= HELP_LINE_CELLS, w);
            bool drawn = true;                  /* every character the face draws (font5x7 has no semicolon) */
            for (const unsigned char *q = (const unsigned char *)out[i]; *q;) {
                uint32_t cp = *q;
                int len = cp < 0x80 ? 1 : cp < 0xE0 ? 2 : cp < 0xF0 ? 3 : 4;
                if (len > 1) cp &= 0xFFu >> (len + 1);
                for (int b = 1; b < len && q[b]; b++) cp = cp << 6 | (q[b] & 0x3F);
                if (cp >= 0x300 && cp < 0x370) cp = 0;          /* a combining mark: drawn over its letter */
                if (cp && cp != ' ' && !screen_glyph(cp)) drawn = false;
                q += len;
            }
            snprintf(w, sizeof w, "%s: line '%s' has a character the font lacks", where, out[i]);
            expect(drawn, w);
        }
    }
}

static void need(const km_action *act, const char *where)
{
    if (act->kind == KM_NONE || act->kind == KM_SHIFT) return;     /* no page: a soft key's [soft], a shift arms */
    const help_entry *e = help_for_action(act);
    char w[320];
    if (!e) {
        if (list_mode)
            printf("MISSING kind=%d op=%d arg=%d legend=%s where=%s\n", act->kind, act->op, act->arg,
                   act->legend ? act->legend : "", where);
        snprintf(w, sizeof w, "%s: no entry (kind %d, op %d, arg %d, %s)", where, act->kind, act->op, act->arg,
                 act->legend ? act->legend : "");
        expect(false, w);
        return;
    }
    used[e - help_table] = true;
    fits(e, act->legend, where);
}

static void need_kind(int kind, const char *legend, const char *where)
{
    const help_entry *e = help_find(kind, 0, -1, NULL);
    char w[320];
    snprintf(w, sizeof w, "%s: no [%s] entry", where, kind == HELP_APP ? "app" : "const");
    if (!e && list_mode) printf("MISSING %s where=%s\n", kind == HELP_APP ? "[app]" : "[const]", where);
    expect(e != NULL, w);
    if (e) {
        used[e - help_table] = true;
        fits(e, legend, where);
    }
}

/* ---- the sweep ---- */

static const help_entry *rec_e;                 /* the entry help_page asked for, and how */
static int rec_model, rec_n;
static char rec_legend[64];
int __real_help_lines(const help_entry *e, int model, const char *legend, char out[][HELP_LINE_BYTES], int max);
int __wrap_help_lines(const help_entry *e, int model, const char *legend, char out[][HELP_LINE_BYTES], int max);
int __wrap_help_lines(const help_entry *e, int model, const char *legend, char out[][HELP_LINE_BYTES], int max)
{
    rec_e = e, rec_model = model, rec_n++;
    snprintf(rec_legend, sizeof rec_legend, "%s", legend ? legend : "");
    return __real_help_lines(e, model, legend, out, max);
}

enum { R_PLAIN, R_EQN, R_TYPING, R_ALG, R_ALGRES, R_GRAPH, R_TABLE, R_SUM, R_PROG, R_N };
static const char *const RNAME[R_N] = {"plain", "eqn list", "eqn typed", "ALG", "ALG result", "graph", "table", "sum bound",
                                       "program"};
#define MAXP 10
#define MAXS 3000
typedef struct { int8_t root, n; int model, stamp; uint8_t k[MAXP]; } spath;   /* stamp: R_PROG's label's mode */
typedef struct { int model, dev, menu, page, cat, gpage, step, base; bool gr, tbl, alg, shown, eqn, typing, prgm; } ssig;
static spath st[MAXS];
static ssig sg[MAXS];
static int ns;
static ab_calc sc;
static app_state sa;
static app_graph sgb;
static uint32_t snow;
static bool reached[1024][3], full[1024][3], gave[1024][3];
static int k_blue, k_gold, k_9, k_c, k_a, k_0, k_1, k_7;

static void d_load(void *ctx, const uint8_t *s, uint32_t n) { (void)ctx, (void)s, (void)n; }
static void d_start(void *ctx) { (void)ctx; }
static bool d_key(void *ctx, int key, bool down, uint32_t ms) { (void)ctx, (void)key, (void)down, (void)ms; return false; }
static uint32_t d_tick(void *ctx, uint32_t ms) { (void)ctx, (void)ms; return 0; }
static void d_draw(void *ctx, ls027_frame *f) { (void)ctx, (void)f; }
static void d_save(void *ctx, uint8_t *s, uint32_t cap) { (void)ctx, (void)s, (void)cap; }
static void d_stop(void *ctx) { (void)ctx; }
#define DAPP(n) {n, 16, d_load, d_start, d_key, d_tick, d_draw, d_save, d_stop, 0}
static const app_def dapps[7] = {DAPP("D1"), DAPP("D2"), DAPP("D3"), DAPP("D4"), DAPP("D5"), DAPP("D6"), DAPP("D7")};
static const app_def *const dlist[7] = {&dapps[0], &dapps[1], &dapps[2], &dapps[3], &dapps[4], &dapps[5], &dapps[6]};
static const uint8_t dfold[7] = {0, 0, 0, 0, 0, APP_FOLDER_GAMES, APP_FOLDER_GAMES};  /* APPS's GAMES key appears */

static int midx(int model) { return model == 33 ? 0 : model == 35 ? 1 : 2; }
static void stap(int k) { snow += 100; app_key(&sa, k, snow); }
static int key_face(const char *l)
{
    for (int k = 0; k < KB_KEYS; k++)
        if (km_lookup(k, 0)->kind != KM_NONE && !strcmp(km_lookup(k, 0)->legend, l)) return k;
    return -1;
}

static void setup(int root, int model, int stamp)
{
    ab_init(&sc);
    app_init(&sa, &sc, snow);
    app_apps(&sa, dlist, 7);
    app_apps_folders(&sa, dfold);
    app_graph_buffer(&sa, &sgb);
    ab_do_arg(&sc, AB_MODEL, model);
    ab_do(&sc, AB_K5), ab_do_arg(&sc, AB_STO, 1), ab_do(&sc, AB_CLX);          /* B, and a program: MEM's catalogs */
    ab_do(&sc, AB_PRGM), ab_do_arg(&sc, AB_LBL, 25), ab_do(&sc, AB_K1), ab_do(&sc, AB_PRGM);
    if (root == R_PROG) {                       /* program entry on a label stamped for stamp's mode (MODE on its line) */
        ab_do(&sc, AB_PRGM), ab_do_arg(&sc, AB_LBL, 24);
        if (stamp != model) ab_do_arg(&sc, AB_MODEL, stamp);
    }
    if (root == R_TABLE) ab_eqn_add(&sc, "X^2-30"), ab_do(&sc, AB_EQN), ab_do(&sc, AB_TABLE);
    if (root == R_SUM) ab_eqn_add(&sc, "X^2"), ab_do_arg(&sc, AB_CASSUM, 23), ab_do(&sc, AB_K1), ab_do(&sc, AB_ENTER);
    if (root == R_EQN || root == R_TYPING) ab_eqn_add(&sc, "X^2-30");     /* (which turns Equation mode on) */
    if (root == R_TYPING) stap(k_1);
    if (root == R_ALG || root == R_ALGRES) ab_do_arg(&sc, AB_ENTRYM, 1);
    if (root == R_ALGRES) ab_do(&sc, AB_K2), ab_do(&sc, AB_ENTER);     /* a result shown: ANS */
    if (root == R_GRAPH) {                      /* as app_test's graphs: the equation current, blue 7, GO */
        ab_eqn_add(&sc, "X^2-30");
        ab_do(&sc, AB_EQN);
        stap(k_blue), stap(k_7), stap(5);
        const pwr_inputs idle = {false, false, 80, false};
        for (int i = 0; i < 400 && sa.graphing; i++) app_tick(&sa, &idle, snow);
    }
}

static void apply(const spath *p)
{
    setup(p->root, p->model, p->stamp);
    for (int i = 0; i < p->n; i++) stap(p->k[i]);
}

static ssig sig_now(void)
{
    ssig s = {ab_rec_model(&sc), sc.stu ? AB_MODEL_STU : sc.m35 ? 35 : 33, sa.menu, sa.menu_page, sa.cat, sa.gpage,
              sc.cas_def ? sc.cas_def_step : 0, sc.base, sc.gr_on, sc.tbl_on, sc.alg, sc.alg_shown, sc.eqn_mode,
              sc.eqn_typing, sc.prgm};
    return s;
}

static bool sig_eq(const ssig *x, const ssig *y)
{
    return x->model == y->model && x->dev == y->dev && x->menu == y->menu && x->page == y->page && x->cat == y->cat &&
           x->gpage == y->gpage && x->step == y->step && x->base == y->base && x->gr == y->gr && x->tbl == y->tbl &&
           x->alg == y->alg && x->shown == y->shown &&
           x->eqn == y->eqn && x->typing == y->typing && x->prgm == y->prgm;
}

static bool usable(void) { return !app_running(&sa) && sa.prompt_op < 0 && !sa.view && !sa.help && !sa.graphing; }

static void try_state(const spath *q)
{
    apply(q);
    if (!usable() || ns >= MAXS) return;
    ssig s = sig_now();
    for (int i = 0; i < ns; i++)
        if (sig_eq(&sg[i], &s)) return;
    st[ns] = *q, sg[ns] = s, ns++;
}

static void describe(char *w, size_t cap, int i, int k, int sh)
{
    static const char *const SH[3] = {"", "GOLD ", "BLUE "};
    const ssig *s = &sg[i];
    snprintf(w, cap, "[%s, mode %d on %d, base %d, menu %s p%d%s%s%s%s%s%s%s] HELP %skey %d (%s)", RNAME[st[i].root],
             s->model, s->dev, s->base, s->menu >= 0 ? km_menu_at(s->menu)->name : "-", s->page, s->eqn ? " eqn" : "",
             s->typing ? " typing" : "", s->alg ? " ALG" : "", s->shown ? " result" : "", s->gr ? " graph" : "",
             s->tbl ? " table" : "",
             s->prgm ? " prgm" : "", SH[sh], k, km_lookup(k, 0)->legend);
}

/* the page's refusal note: on the title row ("...  RPN ONLY") or a row "Now: RPN ONLY." (*at: 0 the title, else the
   row) */
static const char *page_note(const screen_ui *ui, int *at)
{
    static const abn_status N[2] = {ABN_RPN_ONLY, ABN_NOT_IN_EQN};
    for (int j = 0; j < 2; j++) {
        const char *m = ab_status_message(N[j]);
        char tail[24], row[24];
        snprintf(tail, sizeof tail, "  %s", m);
        snprintf(row, sizeof row, "Now: %s.", m);
        size_t lt = strlen(ui->view[0]), ln = strlen(tail);
        if (lt >= ln && !strcmp(ui->view[0] + lt - ln, tail)) return *at = 0, m;
        for (int r = 1; r < 4; r++)
            if (ui->view[r] && !strcmp(ui->view[r], row)) return *at = r, m;
    }
    return NULL;
}

/* the key pressed for real in the state (a prompted key given the letter A): the refusal notice it shows, if any */
static const char *real_note(const spath *p, int k, int sh)
{
    apply(p);
    if (sh) stap(sh == 1 ? k_gold : k_blue);
    stap(k);
    for (int i = 0; i < 4 && sa.prompt_op >= 0; i++)      /* its argument: digits for a digit, address or time prompt */
        stap(sa.prompt_kind == KM_ARG_DIGITS || sa.prompt_kind == KM_ARG_ADDR || sa.prompt_kind == KM_ARG_TIME ? k_0 : k_a);
    const char *n = sa.notice;
    if (n && (!strcmp(n, ab_status_message(ABN_RPN_ONLY)) || !strcmp(n, ab_status_message(ABN_NOT_IN_EQN)))) return n;
    return NULL;
}

static void sweep_state(int i)
{
    const spath *p = &st[i];
    apply(p);
    for (int sh = 0; sh < 3; sh++)
        for (int k = 0; k < KB_KEYS; k++) {
            const km_action *f = km_lookup(k, 0);
            if (f->kind == KM_NONE || f->kind == KM_SHIFT) continue;
            char w[400], d[200];
            describe(d, sizeof d, i, k, sh);
            stap(k_blue), stap(k_9);
            snprintf(w, sizeof w, "%s: HELP did not arm", d);
            expect(sa.help, w);
            if (!sa.help) {
                apply(p);
                continue;
            }
            if (sh) stap(sh == 1 ? k_gold : k_blue);
            rec_e = NULL, rec_n = 0;
            stap(k);
            screen_ui ui;
            app_ui(&sa, &ui);
            snprintf(w, sizeof w, "%s: no page", d);
            expect(sa.view == APP_VIEW_HELP && ui.view[0], w);
            if (sa.view == APP_VIEW_HELP && ui.view[0]) {
                snprintf(w, sizeof w, "%s: the title '%s' is %d cells, past %d", d, ui.view[0], screen_cells(ui.view[0]),
                         HELP_LINE_CELLS);
                expect(screen_cells(ui.view[0]) <= HELP_LINE_CELLS, w);
                snprintf(w, sizeof w, "%s: no entry ('%s')", d, ui.view[1] ? ui.view[1] : "");
                expect(rec_e != NULL, w);
                if (rec_e) {
                    int e = (int)(rec_e - help_table), m = midx(rec_model);
                    char out[3][HELP_LINE_BYTES];
                    int n = __real_help_lines(rec_e, rec_model, rec_legend, out, 3), shown = 0;
                    while (shown < n && ui.view[1 + shown] && !strcmp(ui.view[1 + shown], out[shown])) shown++;
                    reached[e][m] = true;
                    if (shown == n) full[e][m] = true;
                    /* a page for an op this state's mode hides (a 35s feature in 33s, the 33s's own elsewhere) says
                       so: its entry has lines for that mode (the sweep's form of the hidden-op check: only where a
                       page shows it, so a menu that never opens in that mode needs none) */
                    if (rec_e->kind == HELP_OP && ab_hidden(&sc, rec_e->op)) {
                        char pl[3][HELP_LINE_BYTES];
                        int np = __real_help_lines(rec_e, -1, rec_legend, pl, 3);    /* (no mode: the plain lines) */
                        bool differs = np != n;
                        for (int l = 0; !differs && l < n; l++) differs = strcmp(pl[l], out[l]) != 0;
                        snprintf(w, sizeof w, "%s: mode %d hides it, so its page needs that mode's line", d, rec_model);
                        expect(differs, w);
                    }
                    int at = 0;
                    const char *pn = page_note(&ui, &at);
                    if (pn && at) {                     /* the bottom row only when the title row can't hold it */
                        char tl[96];
                        snprintf(tl, sizeof tl, "%s  %s", ui.view[0], pn);
                        snprintf(w, sizeof w, "%s: the note took a row, but '%s' fits the title", d, tl);
                        expect(screen_cells(tl) > HELP_LINE_CELLS || strlen(tl) >= 48, w);
                    }
                    if (pn && shown == 2 && n == 3 && !gave[e][m]) {
                        gave[e][m] = true;
                        printf("help_test: NOTE gives way: %s: '%s' under '%s'\n", d, out[2], ui.view[0]);
                    }
                    if (sg[i].alg || (sg[i].eqn && !sg[i].prgm)) {      /* the note against the key pressed */
                        const char *rn = real_note(p, k, sh);
                        snprintf(w, sizeof w, "%s: the page says %s, the key pressed shows %s", d, pn ? pn : "nothing",
                                 rn ? rn : "nothing");
                        expect((pn == NULL) == (rn == NULL) && (!pn || !strcmp(pn, rn)), w);
                        apply(p);
                        continue;
                    }
                }
            }
            stap(k_c);                          /* any key closes the page, and does nothing else */
            ssig now = sig_now();
            if (sa.view || !sig_eq(&now, &sg[i])) apply(p);
        }
}

static void sweep(void)
{
    k_blue = k_gold = -1;
    for (int k = 0; k < KB_KEYS; k++) {
        const km_action *f = km_lookup(k, 0);
        if (f->kind == KM_SHIFT) *(f->arg == 1 ? &k_gold : &k_blue) = k;
        if (km_letter_of(k)->kind == KL_LETTER && km_letter_of(k)->value == 0) k_a = k;
    }
    k_9 = key_face("9"), k_c = key_face("C"), k_0 = key_face("0"), k_1 = key_face("1"), k_7 = key_face("7");
    for (int m = 0; m < 3; m++)
        for (int r = 0; r < R_N; r++)
            for (int t = 0; t < 3; t++) {      /* (each stamp for R_PROG; the device's mode for the rest) */
                bool stu = MODELS[m] == AB_MODEL_STU;
                if ((r == R_ALG || r == R_ALGRES || r == R_GRAPH || r == R_TABLE || r == R_SUM) && !stu) continue;
                if (r != R_PROG && t != m) continue;
                spath q = {(int8_t)r, 0, MODELS[m], MODELS[t], {0}};
                apply(&q);                      /* each root is the state it names (a control on the setup) */
                bool is = r == R_PLAIN ? !sc.eqn_mode && !sc.alg && !sc.prgm :
                          r == R_EQN ? sc.eqn_mode && !sc.eqn_typing :
                          r == R_TYPING ? sc.eqn_mode && sc.eqn_typing :
                          r == R_ALG ? sc.alg && !sc.alg_shown :
                          r == R_ALGRES ? sc.alg && sc.alg_shown :
                          r == R_GRAPH ? sc.gr_on && !sa.graphing :
                          r == R_TABLE ? sc.tbl_on :
                          r == R_SUM ? sc.cas_def && sc.cas_def_sum && sc.cas_def_step == 2 :
                          sc.prgm && ab_rec_model(&sc) == MODELS[t];
                char w[96];
                snprintf(w, sizeof w, "the sweep's root %s in mode %d (stamp %d) is not that state", RNAME[r], MODELS[m],
                         MODELS[t]);
                expect(is, w);
                try_state(&q);
            }
    expect(ns >= 1, "the sweep found no state");
    for (int i = 0; i < ns; i++) {              /* every state its keys reach: a root's keys, a menu's soft keys */
        spath p = st[i];
        if (p.n + 2 > MAXP) continue;
        for (int sh = 0; sh < 3; sh++)
            for (int k = 0; k < KB_KEYS; k++) {
                const km_action *f = km_lookup(k, 0);
                if (f->kind == KM_NONE || f->kind == KM_SHIFT) continue;
                if (p.n > 0 && (f->kind != KM_SOFT || sh == 2)) continue;
                spath q = p;
                if (sh) q.k[q.n++] = (uint8_t)(sh == 1 ? k_gold : k_blue);
                q.k[q.n++] = (uint8_t)k;
                try_state(&q);
            }
    }
    expect(ns < MAXS, "the sweep's state table filled");
    for (int i = 0; i < ns; i++) sweep_state(i);
    int nr = 0;
    for (int e = 0; e < help_table_n; e++) {
        char w[200];
        bool any = reached[e][0] || reached[e][1] || reached[e][2];
        nr += any;
        snprintf(w, sizeof w, "entry %d (kind %d, op %d, %.24s): no state's HELP shows it", e, help_table[e].kind,
                 help_table[e].op, help_table[e].text);
        expect(any, w);
        for (int m = 0; any && m < 3; m++) {
            if (reached[e][m]) {
                snprintf(w, sizeof w, "entry %d (%.24s): mode %d never shows all its lines", e, help_table[e].text,
                         MODELS[m]);
                expect(full[e][m], w);
                continue;
            }
            char a[3][HELP_LINE_BYTES], b[3][HELP_LINE_BYTES];   /* a mode no state reaches: its text no other's */
            int na = __real_help_lines(&help_table[e], MODELS[m], NULL, a, 3);
            bool same = false;
            for (int o = 0; o < 3 && !same; o++) {
                if (!reached[e][o]) continue;
                int nb = __real_help_lines(&help_table[e], MODELS[o], NULL, b, 3);
                same = na == nb;
                for (int l = 0; same && l < na; l++) same = !strcmp(a[l], b[l]);
            }
            snprintf(w, sizeof w, "entry %d (%.24s): mode %d's text shows in no state (no state in that mode reaches it)",
                     e, help_table[e].text, MODELS[m]);
            expect(same, w);
        }
    }
    printf("help_test: the sweep: %d states, %d of %d entries shown\n", ns, nr, help_table_n);
}

/* the core's predicates beside ab_do, for every op (abacus #8753 (1)): ALG on an empty line and after a result,
   Equation mode's list and an equation typed, in each mode */
static void refusals(void)
{
    static ab_calc c;
    for (int m = 0; m < 3; m++)
        for (int s = 0; s < 4; s++) {
            if (s < 2 && MODELS[m] != AB_MODEL_STU) continue;
            for (int op = 0; op <= AB_OP_LAST; op++)
                for (int arg = -1; arg <= 1; arg++) {
                    ab_init(&c);
                    ab_do_arg(&c, AB_MODEL, MODELS[m]);
                    if (s < 2) ab_do_arg(&c, AB_ENTRYM, 1);
                    if (s == 1) ab_do(&c, AB_K2), ab_do(&c, AB_ENTER);
                    if (s >= 2) ab_eqn_add(&c, "X^2-30");
                    if (s == 3) ab_do(&c, AB_K1);
                    bool pa = ab_alg_refuses(&c, (ab_op)op, arg), pe = ab_eqn_refuses(&c, (ab_op)op, arg);
                    abn_status got = ab_do_arg(&c, (ab_op)op, arg);
                    char w[160];
                    snprintf(w, sizeof w, "op %d arg %d, mode %d, state %d: the predicates say %d/%d, ab_do %d", op, arg,
                             MODELS[m], s, pa, pe, (int)got);
                    expect(pa == (got == ABN_RPN_ONLY) && pe == (got == ABN_NOT_IN_EQN), w);
                }
        }
}

int main(int argc, char **argv)
{
    list_mode = argc > 1 && !strcmp(argv[1], "--list");
    static const char *const SHIFT[3] = {"", "GOLD ", "BLUE "};
    for (int k = 0; k < KB_KEYS; k++) {
        const km_action *face = km_lookup(k, 0);
        if (face->kind == KM_NONE) continue;    /* no key there */
        for (int sh = 0; sh < 3; sh++) {
            const km_action *act = km_lookup(k, sh);
            char where[64];
            snprintf(where, sizeof where, "key %d %s%s", k, SHIFT[sh], face->legend);
            need(act, where);
        }
    }
    for (int i = 0; km_menu_at(i); i++) {
        const km_menu *mn = km_menu_at(i);
        for (int j = 0; j < KM_SOFT_KEYS; j++) {
            char where[64];
            snprintf(where, sizeof where, "menu %s F%d", mn->name, j + 1);
            if (mn->paged && j == 5) {             /* more: the next page (fresh eyes) */
                const help_entry *m = help_find(HELP_MORE, 0, -1, NULL);
                if (!m && list_mode) printf("MISSING [more] where=%s\n", where);
                expect(m != NULL, "no [more] entry (a paged menu's sixth key)");
                if (m) {
                    used[m - help_table] = true;
                    fits(m, "more", where);
                }
                continue;
            }
            if (mn->paged && j < 5) {
                need_kind(mn->paged == AB_CONST ? HELP_CONST : HELP_APP, mn->paged == AB_CONST ? "R\xe2\x88\x9e" : "MINES",
                          where);
                continue;
            }
            const km_action *it = &mn->item[j];
            if (it->kind == KM_NONE) continue;  /* an empty soft key: [soft] covers it in the app */
            need(it, where);
        }
    }
    /* the keys app_key takes before their own action (fresh eyes): APPS's GAMES key, a graph's digit pad, 33s mode's
       CMPLX prefix on an operator */
    {
        const help_entry *g = help_find(HELP_MENU, 0, -1, "GAMES"), *gp = help_find(HELP_GRAPHPAD, 0, -1, NULL),
                         *cp = help_find(HELP_CMPLXPAIR, 0, -1, NULL);
        expect(g != NULL, "no [menu:GAMES] entry (APPS's GAMES key)");
        expect(gp != NULL, "no [graphpad] entry (a digit while a graph is open)");
        expect(cp != NULL, "no [cmplxpair] entry (an operator under 33s mode's CMPLX prefix)");
        if (g) used[g - help_table] = true, fits(g, "GAMES", "APPS's GAMES key");
        if (gp) used[gp - help_table] = true, fits(gp, "7", "a graph's digit");
        if (cp) used[cp - help_table] = true, fits(cp, "+", "CMPLX's operator");
        for (int ck = HELP_CAT_C; ck <= HELP_CAT_ENTER; ck++) {    /* the keys MEM's catalog takes (075 review) */
            const help_entry *e = help_find(HELP_CATKEY, ck, -1, NULL);
            char w[64];
            snprintf(w, sizeof w, "no [catkey] entry %d (a key MEM's catalog takes)", ck);
            expect(e != NULL, w);
            if (e) used[e - help_table] = true, fits(e, "C", "a catalog's key");
        }
    }
    const help_entry *soft = help_find(HELP_SOFT, 0, -1, NULL);
    expect(soft != NULL, "no [soft] entry (a soft key with nothing under it)");
    if (soft) used[soft - help_table] = true;
    for (int i = 0; help_mode_list[i] >= 0; i++) {
        char w[96];
        const help_entry *e = &help_table[help_mode_list[i]];
        snprintf(w, sizeof w, "MODE entry %d (kind %d, op %d) has no mode variant", help_mode_list[i], e->kind, e->op);
        expect(varies(e), w);
    }
    for (int i = 0; i < help_table_n; i++) {
        char w[160];
        snprintf(w, sizeof w, "entry %d (kind %d, op %d, arg %d, %.20s) is reached by no action (stale)", i,
                 help_table[i].kind, help_table[i].op, help_table[i].arg, help_table[i].text);
        expect(used[i], w);
    }
    refusals();
    sweep();
    printf("help_test: %d/%d checks, %d entries\n", checks - bad, checks, help_table_n);
    return bad != 0;
}
