/* help_test (unit 075): HELP's text against the keymap. Every action a key or a menu item reaches has an entry (on
   all three layers, every menu, the paged menus' apps and constants), every entry is reached by some action (no stale
   text), every line fits a page's 31 cells in each mode with the legend it is shown for, the MODE entries and every
   op the 33s hides vary by mode. --list prints what has no entry (kind, op, argument, legend, where). */
#include <stdio.h>
#include <string.h>

#include "calc.h"
#include "help.h"
#include "keymap.h"
#include "keys.h"
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
    static ab_calc cm[3];
    static bool init;
    if (!init) {
        for (int m = 0; m < 3; m++) {
            ab_init(&cm[m]);
            ab_do_arg(&cm[m], AB_MODEL, MODELS[m]);
        }
        init = true;
    }
    for (int m = 0; m < 3; m++)                 /* hidden in a mode (a 35s feature in 33s, the 33s's own elsewhere) */
        if ((act->kind == KM_OP || act->kind == KM_PROMPT) && ab_hidden(&cm[m], act->op)) {
            snprintf(w, sizeof w, "%s: mode %d hides it, so its entry needs a mode line", where, MODELS[m]);
            expect(varies(e), w);
            break;
        }
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
    printf("help_test: %d/%d checks, %d entries\n", checks - bad, checks, help_table_n);
    return bad != 0;
}
