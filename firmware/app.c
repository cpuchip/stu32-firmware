/* app: from a key press to the core (see app.h). */
#include "app.h"

#include <stdio.h>
#include <string.h>

#include "const.h"
#include "keymap.h"
#include "piezo.h"
#include "state.h"

#ifndef ESP_PLATFORM
static uint8_t host_saves[APP_SAVES_IMG_MAX], host_run[APP_SAVE_MAX];    /* a host's own (a device gives PSRAM) */
#endif

void app_init(app_state *a, ab_calc *c, uint32_t now_ms)
{
    memset(a, 0, sizeof *a);
    a->c = c;
    a->prompt_op = -1;
    a->tgt_label = -1;
    a->idle_limit = true;                       /* (a host may turn it off: abacus #4661) */
#ifndef ESP_PLATFORM
    app_save_buffers(a, host_saves, sizeof host_saves, host_run, sizeof host_run);  /* (a device gives its own) */
#endif
    a->confirm_op = -1;
    a->menu = -1;
    a->last_key_ms = now_ms;
    a->tone = -1;
    a->now_ms = now_ms;
    a->beeps = c->beeps;
    a->apps = app_registry;                     /* unit 037: the built-in apps */
    a->napps = app_registry_n;
    pwr_init(&a->pwr, now_ms);
    dev_settings_default(&a->set);
    c->slice = APP_SLICE_LINES;
}

static void prompt_close(app_state *a)
{
    a->prompt_op = -1;
    a->prompt_tens = false;
    a->prompt_n = a->prompt_val = 0;
    a->prompt_text[0] = '\0';
    a->gto_dot = 0;
    a->tgt_label = -1;
}

/* GTO's address form (unit 036): "GTO ._", then "GTO .A___" with the digits so far (three in 35s and STU
   mode, 35s p.14-6; four in 33s mode, 33s p.13-5). */
static void gto_text(app_state *a)
{
    int need = a->c->m35 ? 3 : 4;
    char d[8] = "";
    if (a->gto_dot == 1) {
        snprintf(a->prompt_text, sizeof a->prompt_text, "GTO ._");
        return;
    }
    if (a->prompt_n) snprintf(d, sizeof d, "%0*d", a->prompt_n, a->prompt_val);
    snprintf(a->prompt_text, sizeof a->prompt_text, "GTO .%c%s%s", 'A' + a->gto_label, d, &"____"[4 - (need - a->prompt_n)]);
}

/* 35s and STU GTO / XEQ after the letter (unit 036b): "XEQ A___", then the digits so far */
static void tgt_text(app_state *a)
{
    char d[4] = "";
    if (a->prompt_n) snprintf(d, sizeof d, "%0*d", a->prompt_n, a->prompt_val);
    snprintf(a->prompt_text, sizeof a->prompt_text, "%s %c%s%s", a->prompt_op == AB_XEQ ? "XEQ" : "GTO",
             'A' + a->tgt_label, d, &"___"[a->prompt_n]);
}

/* "STO _", "STO+ _": the op's name as listed (UG Appendix G; list.c's spacing), then the cursor */
static void prompt_open(app_state *a, int op, int kind)
{
    static const char *const ARITH[4] = {"+", "-", "\xc3\x97", "\xc3\xb7"};
    const char *name = op == AB_STO ? "STO" : op == AB_RCL ? "RCL" : op == AB_VIEW ? "VIEW" : op == AB_ISG ? "ISG"
                       : op == AB_DSE ? "DSE" : op == AB_LBL ? "LBL" : op == AB_XEQ ? "XEQ" : op == AB_GTO ? "GTO"
                       : op == AB_SETFIX ? "FIX" : op == AB_SETSCI ? "SCI" : op == AB_SETENG ? "ENG"
                       : op == AB_INPUT ? "INPUT" : op == AB_CLVARX ? "CLVAR" : op == AB_SF ? "SF"
                       : op == AB_CF ? "CF" : op == AB_FS ? "FS?" : op == AB_XVAR ? "x<>" : op == AB_SOLVE ? "SOLVE"
                       : op == AB_INTEG ? "\xe2\x88\xab" "FN d" : op == AB_CASD ? "D/DX" : op == AB_CASCOL ? "COLCT"
                       : op == AB_CASSUB ? "SUBST" : op == AB_FN ? "FN=" : op == AB_CASSOLVE ? "SOLVE"
                       : op == AB_CASINT ? "\xe2\x88\xab" "DX" : op == AB_CASFACT ? "FACTR"
                       : op == AB_CASDEF ? "\xe2\x88\xab" "AB" : op == AB_CASQUOT ? "QUOT" : op == AB_CASGCD ? "GCD"
                       : op == AB_CASCANCL ? "CANCL" : "";
    char buf[16] = "";
    if (op >= AB_STOADD && op <= AB_STODIV) snprintf(buf, sizeof buf, "STO%s", ARITH[op - AB_STOADD]);
    else if (op >= AB_RCLADD && op <= AB_RCLDIV) snprintf(buf, sizeof buf, "RCL%s", ARITH[op - AB_RCLADD]);
    else snprintf(buf, sizeof buf, "%s", name);
    a->prompt_op = op;
    a->prompt_kind = kind;
    if (kind == KM_ARG_TIME) {                  /* SET 1_:__: HH MM as they come (unit 027) */
        char d[5] = "____";
        for (int i = 0, v = a->prompt_val; i < a->prompt_n; i++) d[a->prompt_n - 1 - i] = (char)('0' + v % 10), v /= 10;
        snprintf(a->prompt_text, sizeof a->prompt_text, "SET %c%c:%c%c", d[0], d[1], d[2], d[3]);
        return;
    }
    if (kind == KM_ARG_ADDR) {                  /* CLVAR05_: the digits so far, then the blanks */
        char d[4] = "";
        if (a->prompt_n) snprintf(d, sizeof d, "%0*d", a->prompt_n, a->prompt_val);
        snprintf(a->prompt_text, sizeof a->prompt_text, "%s%s%s", buf, d, &"___"[a->prompt_n]);
    } else
        snprintf(a->prompt_text, sizeof a->prompt_text, a->prompt_tens ? "%s 1_" : "%s _", buf);
}

static void undo_swap(app_state *a, size_t n)
{
    uint8_t *t = a->undo[0];
    a->undo[0] = a->undo[1];
    a->undo[1] = t;
    a->undo_len = n;
}

static void graph_start(app_state *a);    /* unit 033 (below) */
static void graph_step(app_state *a);

/* The state now becomes UNDO's (unit 021): saved into the scratch image, which then takes the
   undo image's place. A state too large for the buffers leaves nothing to undo. */
static void undo_mark(app_state *a)
{
    if (!a->undo[0]) return;
    size_t n = ab_state_save(a->c, a->undo[1], a->undo_cap);
    if (n == 0 || n > a->undo_cap) {
        a->undo_len = 0;
        return;
    }
    undo_swap(a, n);
}

/* UNDO: the state and the undo image change places, so a second UNDO puts it back. */
static void undo(app_state *a)
{
    if (!a->undo_len) return;
    size_t n = ab_state_save(a->c, a->undo[1], a->undo_cap);
    uint16_t beeps = a->c->beeps;               /* a count for the device, not state (unit 027) */
    if (n == 0 || n > a->undo_cap || ab_state_load(a->c, a->undo[0], a->undo_len) != ABN_OK) return;
    a->c->beeps = beeps;
    undo_swap(a, n);
    a->dirty = true;
    a->gr_live = a->graphing = false;           /* a graph UNDO brings back is plotted afresh (review F1) */
    if (a->c->gr_on && a->graph) graph_start(a);
}

/* OVERFLOW, briefly (unit 024 rule 3; unit 026): after the key, or when the run it started ends (33s
   p.13-9: shown "when the program eventually stops"); not over a message (flag 5's stop holds
   OVERFLOW itself, an error has its own). */
static void overflow_notice(app_state *a, uint32_t now_ms)
{
    if (!a->ovf_pending || a->running || a->pause) return;
    a->ovf_pending = false;
    if (a->c->msg) return;
    a->notice = ab_status_message(ABN_OVERFLOW);
    a->notice_ms = now_ms;
}

/* A tone for one of the buzzer's uses, when that use is on and VOL is not 0 (unit 027 rule 3); a
   later one in the same event replaces an earlier (an error's tone over the key's tick). */
static void sound(app_state *a, int use)
{
    if (!(a->set.uses & use) || a->set.vol == 0) return;
    a->tone = a->set.vol;
    a->tone_use = use;
}

/* After the core ran: a BEEP done (PRGM), a new message (ERROR), and the end of a run that took
   APP_DONE_MS or more (DONE), timed from its key to now_ms (unit 027). */
static void after_core(app_state *a, bool msg_before, uint32_t now_ms)
{
    ab_calc *c = a->c;
    if (a->run_timed && !a->running && !a->pause) {
        a->run_timed = false;
        /* only a natural end: not a run the user stopped with C or R/S (abacus #4238) */
        if (!a->run_stopped && (uint32_t)(now_ms - a->run_t0) >= APP_DONE_MS) sound(a, DEV_USE_DONE);
        a->run_stopped = false;
    }
    if (c->beeps != a->beeps) {
        a->beeps = c->beeps;
        sound(a, DEV_USE_PRGM);
    }
    if (c->msg && !msg_before) sound(a, DEV_USE_ERROR);     /* last: an error's tone wins */
}

/* An op from the keys: the state kept for UNDO first (VIEW changes nothing), the busy callback
   when it may take long, and a run that has not ended goes on from app_tick. */
static void do_op(app_state *a, int op, int arg)
{
    ab_calc *c = a->c;
    bool runs = !c->prgm && (op == AB_XEQ || op == AB_RS || op == AB_SST || op == AB_SOLVE || op == AB_INTEG);
    if (runs) {
        a->run_timed = true;                    /* DONE's clock (unit 027) */
        a->run_t0 = a->last_key_ms;
        a->run_stopped = false;
    }
    if (op != AB_VIEW && op != AB_ENGL && op != AB_ENGR) undo_mark(a);    /* ENG's view: no UNDO mark (041 C) */
    /* 040b: a call that may take long (a SOLVE, an integral, a run's first slice, a graph tool) is marked, so that
       the device's key task delivers C into it (app_interrupt); nothing left over before or after */
    bool computing = runs || (op == AB_GFCN && c->gr_on) ||
                     ((op == AB_CASSOLVE || op == AB_CASINT || op == AB_CASFACT) && c->eqn_mode && c->stu);
                                                /* unit 048: casim's walk can't stop; a C during it discards the
                                                   result on return (abacus #5267) */
    if (computing) {
        c->intr = false;
        a->computing = true;
    }
    if ((runs || computing) && a->busy) a->busy(a->ctx, op);
    bool msg = c->msg;
    bool on_lbl = c->prgm && c->pc >= 0 && c->pc < c->prog_len && c->prog[c->pc].op == AB_LBL;
    int was = on_lbl ? c->lmode[c->prog[c->pc].arg] : 0;
    abn_status st = ab_do_arg(c, (ab_op)op, arg);
    if (computing) {
        a->computing = false;
        if (c->stop != AB_STOP_SLICE) c->intr = false;  /* (a C that came as it ended: spent) */
    }
    a->dirty = true;
    if (op == AB_GRAPH && st == ABN_OK && c->gr_on) {      /* unit 033; the first page, the readout on (040) */
        a->gpage = 0;
        a->gxy_off = false;
        graph_start(a);
    }
    if (op == AB_GZOOM && st == ABN_OK && c->gr_on) graph_start(a);    /* unit 040: a zoom redraws */
    if (!c->gr_on) a->graphing = a->gr_live = false;
    if (st == ABN_RPN_ONLY || st == ABN_STU_ONLY || st == ABN_NO_EQN || st == ABN_NO_VAR) {
        /* brief notices, not messages: STU's line (unit 029), TABLE's refusals (unit 032) */
        a->notice = ab_status_message(st);
        a->notice_ms = a->last_key_ms;
    }
    if (st == ABN_CLAMPED || c->ovf > 0) a->ovf_pending = true;
    if (op == AB_MODEL && st == ABN_OK && on_lbl && (was != 33) != (arg != 33) &&
        ab_label_indirect(c, c->prog[c->pc].arg)) {
        /* a label restamped to the other mode (unit 025 rule 5; STU's is the 35s's, unit 029) whose
           lines use an indirect: what it now means */
        a->notice = arg != 33 ? "(i) MEANS (I)" : "(I) MEANS (i)";
        a->notice_ms = a->last_key_ms;
    }
    if (c->stop == AB_STOP_SLICE) {
        a->running = true;
        a->busy_op = op;
    }
    if (c->stop == AB_STOP_PSE) {               /* PSE: going on by itself after the pause */
        a->pause = true;
        a->pause_ms = a->last_key_ms;
    }
    /* a run's length: to now by the device's clock when it ended inside this call (a SOLVE, a short
       program), else to the app_tick that sees it end */
    after_core(a, msg, runs && !a->running && !a->pause && a->clock_ms ? a->clock_ms(a->ctx) : a->last_key_ms);
    overflow_notice(a, a->last_key_ms);
}

static void do_action(app_state *a, const km_action *act);

/* The 33s's CMPLX operation an action names (unit 039: AB_CXP_*), -1 none. */
static int cx_pair_of(const km_action *act)
{
    static const int OPS[AB_CXP_N] = {AB_ADD, AB_SUB, AB_MUL, AB_DIV, AB_POW, AB_KCHS, AB_INV, AB_LN, AB_EXP,
                                      AB_SIN, AB_COS, AB_TAN};
    if (act->kind != KM_OP) return -1;
    for (int k = 0; k < AB_CXP_N; k++)
        if (act->op == OPS[k]) return k;
    return -1;
}

/* ---- unit 041 C (B5, abacus #4850): MEM and its catalogs (33s p.1-24, p.3-3, p.12-20; 35s p.3-4) ---- */

/* Whether variable v holds anything but zero: the VAR catalog shows those (33s p.3-3). */
static bool var_set(const ab_calc *c, int v)
{
    const ab_val *x = &c->var[v];
    return !abn_is_zero(&x->re) || (x->kind != AB_REAL && (!abn_is_zero(&x->im) || !abn_is_zero(&x->p3)));
}

/* A catalog's items in order: the set variables A to Z (1, VAR), or the labels in program order (2, PGM). */
static int cat_list(const ab_calc *c, int kind, int out[27])
{
    int n = 0;
    if (kind == 1) {
        for (int v = 0; v < 26; v++)
            if (var_set(c, v)) out[n++] = v;
        if (!ab_mode35(c) && var_set(c, 26)) out[n++] = 26;    /* the 33s's i (unit 025), after Z (fresh eyes) */
    } else {
        for (int i = 0; i < c->prog_len && n < 26; i++)
            if (c->prog[i].op == AB_LBL && c->labelpos[c->prog[i].arg] == i)
                out[n++] = c->prog[i].arg;
    }
    return n;
}

/* The catalog's lines for the item shown: Variable=Value in the display's format, or LBL L over LN=its bytes. */
static void cat_fill(app_state *a)
{
    const ab_calc *c = a->c;
    memset(a->cat_text, 0, sizeof a->cat_text);
    if (a->cat == 1) {
        const ab_val *v = &c->var[a->cat_at];
        ab_display d;
        memset(&d, 0, sizeof d);
        d.dec.mode = c->disp_mode;
        d.dec.n = c->disp_n;
        d.dec.width = AB_FMT_WIDE - 2;
        d.dec.radix = a->set.radix == ',' ? ',' : '.';
        d.dec.separator = a->set.group;
        d.fdisp = c->fdisp;
        d.c = c->frac_c;
        d.frac_format = !c->flag8 ? 0 : (c->flag9 ? 2 : 1);
        d.base = c->base;
        d.polar = c->cpolar;
        d.angle = (int)c->angle;
        char t[64];
        int ind = 0, n;
        if (v->kind == AB_COMPLEX) n = ab_format_complex(&v->re, &v->im, &d, t, sizeof t);
        else if (v->kind == AB_VECTOR) {
            abn el[3] = {v->re, v->im, v->p3};
            n = ab_format_vector(el, v->len, &d, t, sizeof t);
        } else n = ab_format_display(&v->re, &d, t, sizeof t, &ind);
        if (a->cat_at == 26) snprintf(a->cat_text[1], sizeof a->cat_text[1], "i=%.37s", n < 0 ? "?" : t);
        else snprintf(a->cat_text[1], sizeof a->cat_text[1], "%c=%.37s", 'A' + a->cat_at, n < 0 ? "?" : t);
    } else {
        snprintf(a->cat_text[0], sizeof a->cat_text[0], "LBL %c", 'A' + a->cat_at);
        snprintf(a->cat_text[1], sizeof a->cat_text[1], "LN=%lu", (unsigned long)ab_state_label_bytes(c, a->cat_at));
    }
}

static int menu_named(const char *name);

/* Shows the catalog's item at position i of its list (wrapping), or closes it when the list is empty. */
static void cat_show(app_state *a, int i)
{
    int list[27], n = cat_list(a->c, a->cat, list);
    if (n == 0) {
        a->cat = 0;
        a->menu = -1;
        return;
    }
    a->cat_at = list[((i % n) + n) % n];
    cat_fill(a);
    a->menu = menu_named(a->cat == 1 ? "CAT VAR" : "CAT PGM");
    a->menu_page = 0;
}

/* The shown item's position in its list, -1 if it has gone. */
static int cat_pos(const app_state *a)
{
    int list[27], n = cat_list(a->c, a->cat, list);
    for (int i = 0; i < n; i++)
        if (list[i] == a->cat_at) return i;
    return -1;
}

static void cat_open(app_state *a, int kind)
{
    int list[27];
    if (cat_list(a->c, kind, list) == 0) {      /* nothing to list: a notice, no catalog (ours) */
        a->notice = kind == 1 ? "NO VARIABLES" : "NO PROGRAMS";
        a->notice_ms = a->last_key_ms;
        return;
    }
    a->cat = kind;
    cat_show(a, 0);
}

/* MEM's line: the image's room, the store's slot less the image now (ours: our bytes, not the 33s's 31 KB), and in
   35s mode and STU the indirect variables in use (35s p.3-4). */
static void mem_fill(app_state *a)
{
    app_sysinfo si;
    memset(&si, 0, sizeof si);
    si.soc = -1;
    if (a->sysinfo) a->sysinfo(a->ctx, &si);
    size_t size = ab_state_save(a->c, NULL, 0);
    char room[16];
    if (si.slot_size) snprintf(room, sizeof room, "%lu", (unsigned long)(si.slot_size > size ? si.slot_size - size : 0));
    else snprintf(room, sizeof room, "--");
    if (ab_mode35(a->c)) snprintf(a->mem_text, sizeof a->mem_text, "%s B FREE  %d IND", room, a->c->ind_top + 1);
    else snprintf(a->mem_text, sizeof a->mem_text, "%s B FREE", room);
}

/* The hyperbolic function the 33s's HYP prefix makes of a trig key (unit 041 C, B10; 33s p.4-6), -1 none. */
static int hyp_of(const km_action *act)
{
    static const int TRIG[6] = {AB_SIN, AB_COS, AB_TAN, AB_ASIN, AB_ACOS, AB_ATAN};
    static const int HYP[6] = {AB_SINH, AB_COSH, AB_TANH, AB_ASINH, AB_ACOSH, AB_ATANH};
    if (act->kind != KM_OP) return -1;
    for (int k = 0; k < 6; k++)
        if (act->op == TRIG[k]) return HYP[k];
    return -1;
}

/* The menu named name, -1 if there is none. */
static int menu_named(const char *name)
{
    for (int i = 0; km_menu_at(i); i++)
        if (!strcmp(km_menu_at(i)->name, name)) return i;
    return -1;
}

/* 33s mode's CMPLX prefix (unit 039): the CMPLX menu open in 33s mode, in decimal (the operations are decimal
   only, and in HEX some operator keys type digits), outside Equation mode and an equation's typing (where the
   operator keys type, and the 33s's equations take no complex operation) */
static bool cx_prefix(const app_state *a, int menu)
{
    return menu >= 0 && !a->c->m35 && a->c->base == 10 && !a->c->eqn_mode && !a->c->eqn_typing &&
           menu == menu_named("CMPLX");
}

/* Unit 041 Part A: a menu of the 35s's features, in 33s mode: TABLE and GRAPH (A14), or one whose every item is
   a hidden op (LOGIC, SUFFIX); CMPLX opens where the 33s's prefix works (unit 039: decimal, not Equation mode), its
   items dimmed, and while an equation is typed (its i is live). It doesn't open, and its label is dimmed. */
static bool menu_hidden(const ab_calc *c, const km_menu *mn)
{
    if (mn && !strcmp(mn->name, "CAS")) return !c->stu;   /* CAS 004: STU's only (in 33s and 35s it doesn't exist) */
    if (!mn || ab_rec35(c)) return false;      /* in program entry, the edited label's mode (041 C, #4850) */
    if (!strcmp(mn->name, "TABLE") || !strcmp(mn->name, "GRAPH")) return true;
    if (!strcmp(mn->name, "CMPLX") && c->base == 10 && !c->eqn_mode) return false;   /* where the prefix works */
    int n = 0;
    for (int i = 0; i < KM_SOFT_KEYS; i++) {
        const km_action *it = &mn->item[i];
        if (it->kind == KM_NONE) continue;
        if (it->kind != KM_OP || !ab_hidden(c, it->op)) return false;
        n++;
    }
    return n > 0;
}

/* Whether an item is a 35s feature in 33s mode (unit 041 A): an op or a prompt for one, or such a menu. */
static bool item_hidden(const ab_calc *c, const km_action *it)
{
    if (it->kind == KM_OP || it->kind == KM_PROMPT) return ab_hidden(c, it->op);
    return it->kind == KM_MENU && it->arg >= 0 && menu_hidden(c, km_menu_at(it->arg));
}

/* Equation mode's bar on the soft keys when no menu is open (unit 020): typing's while an equation
   is typed (in program entry too), the list's while Equation mode shows the list; -1 none. */
static int eqn_bar(const app_state *a)
{
    if (a->c->tbl_on) return menu_named("TABLE BAR");                /* a table open (unit 032) */
    if (a->c->gr_on) {                          /* a graph open (unit 033): its page, a shift's bank (040) */
        static const char *const PAGE[3] = {"GRAPH BAR", "GRAPH ZOOM", "GRAPH FCN"};
        char n[24];
        int m = -1;
        if (a->shift) {
            snprintf(n, sizeof n, "%s %s", PAGE[a->gpage % 3], a->shift == 1 ? "GOLD" : "BLUE");
            m = menu_named(n);
        }
        return m >= 0 ? m : menu_named(PAGE[a->gpage % 3]);
    }
    if (ab_view_linear(a->c)) return menu_named("LIN VIEW");         /* x= y= (z=): unit 035 */
    if (ab_cas_report(a->c)) return menu_named("CAS REPORT");       /* SOLVE's exact roots: unit 048 */
    if (a->c->alg && !a->c->prgm) return menu_named("ALG LINE");    /* STU's line (unit 029) */
    if (a->c->eqn_typing) return menu_named("EQN TYPE");
    if (a->c->eqn_mode && !a->c->prgm) {         /* GOLD: the list's bank, the REG names (unit 046) */
        int m = a->shift == 1 ? menu_named("EQN LIST GOLD") : -1;
        return m >= 0 ? m : menu_named("EQN LIST");
    }
    return -1;
}

static void app_key_to(app_state *a, int key, bool down, uint32_t now_ms);   /* unit 037 (below) */
static void app_tick_app(app_state *a, uint32_t now_ms);
static void app_start(app_state *a, const app_def *d);
static void app_exit(app_state *a);

void app_key(app_state *a, int key, uint32_t now_ms)
{
    const km_action *face = km_lookup(key, KM_FACE);
    if (a->soft_off) {                          /* 068: OFF on USB power; C alone turns it on, and only that */
        if (face->kind == KM_OP && face->op == AB_CANCEL) {
            a->soft_off = false;
            a->shift = 0;
            a->render = true;
            a->last_key_ms = a->now_ms = now_ms;
            pwr_activity(&a->pwr, now_ms);
            app_powered_on(a);
        }
        return;
    }
    if (a->app) {                               /* an app runs: the key is its, C ends it (unit 037) */
        a->last_key_ms = now_ms;
        app_key_to(a, key, true, now_ms);
        return;
    }
    a->last_key_ms = a->now_ms = now_ms;
    pwr_activity(&a->pwr, now_ms);
    a->render = true;
    a->notice = NULL;                           /* a key ends a brief notice */
    sound(a, DEV_USE_CLICK);                    /* every key ticks (unit 027) */
    a->key_now = key;
    a->c->disp_group = (uint8_t)a->set.group;  /* X's line as the screen groups it, for ENG's view (041 C) */
    if (a->show) {                              /* a key while SHOW shows: with no release (unit 030 answer
                                                   7) it only ends it; on a keyboard with releases the
                                                   finger rolled on: it ends it and acts (review C1) */
        a->show = false;
        if (!a->key_ups) return;
    }
    if (a->view) {                              /* SYS's view: any key leaves it, and only that */
        a->view = APP_VIEW_NONE;
        a->shift = 0;
        return;
    }

    if (a->running) {                           /* only C and R/S, which interrupt (UG p.13-19) */
        if (face->kind == KM_OP && (face->op == AB_CANCEL || face->op == AB_RS)) app_interrupt(a);
        return;
    }
    if (a->pause) {                             /* PSE's pause: C and R/S stop it (UG p.13-19) */
        if (face->kind == KM_OP && (face->op == AB_CANCEL || face->op == AB_RS)) {
            a->pause = false;
            a->run_timed = false;               /* the user ended it: no DONE (abacus #4238) */
        }
        return;
    }
    {
        /* a keyboard VIEW's display: the core's rule, applied here too for keys that never reach it (a
           menu's, a prompt's); a linear solution's view steps on its bar's up and down and on the shifted
           SST and BST (unit 035's review: they reached the program instead) */
        int vop = a->shift == 0 && face->kind == KM_OP ? face->op : -1;
        if (ab_view_linear(a->c)) {
            const km_action *sh = a->shift ? km_lookup(key, a->shift) : NULL;
            if (face->kind == KM_SOFT && a->menu < 0 && (face->arg == 0 || face->arg == 1))
                vop = face->arg == 0 ? AB_BST : AB_SST;
            else if (sh && sh->kind == KM_OP && (sh->op == AB_SST || sh->op == AB_BST))
                vop = sh->op;
        }
        if (face->kind != KM_SHIFT && ab_view_key(a->c, vop)) {
            a->shift = 0;
            return;
        }
    }
    if (a->c->msg && a->c->m35 && face->kind != KM_SHIFT) {
        /* the 35s: a key with a message showing only clears it (UG p.F-1, unit 017 rule 4), a
           key that would open a menu or a prompt too, so no op a menu confirms is spent on it
           (unit 021's review); not an undo point, so UNDO still goes back before the error */
        a->c->msg = false;
        a->shift = 0;
        a->dirty = true;
        return;
    }
    if (a->confirm_op >= 0) {                   /* CLR ALL? Y N: soft key 1 is yes; any other key
                                                   only cancels (UG p.1-5) */
        int op = a->confirm_op;
        a->confirm_op = -1;
        a->shift = 0;
        if (face->kind == KM_SOFT && face->arg == 0) do_op(a, op, 0);
        return;
    }
    if (a->prompt_op >= 0 && a->prompt_kind == KM_ARG_TIME) {      /* HH MM (CLOCK's SET, unit 027) */
        if (face->kind == KM_OP && face->op >= AB_K0 && face->op <= AB_K9) {
            a->prompt_val = a->prompt_val * 10 + face->op - AB_K0;
            if (++a->prompt_n == 4) {
                int hh = a->prompt_val / 100, mm = a->prompt_val % 100;
                prompt_close(a);
                if (hh < 24 && mm < 60) {       /* else nothing: no such time */
                    int64_t day = APP_CLOCK_KEYS_DATE, w;
                    if (app_wall(a, now_ms, &w)) day = w - w % 86400;
                    app_set_wall(a, day + hh * 3600 + mm * 60, now_ms);
                    if (a->set_wall) a->set_wall(a->ctx, a->wall_s);
                }
            } else {
                prompt_open(a, a->prompt_op, a->prompt_kind);
            }
        } else if (face->kind == KM_OP && face->op == AB_KBKSP && a->prompt_n > 0) {
            a->prompt_n--;
            a->prompt_val /= 10;
            prompt_open(a, a->prompt_op, a->prompt_kind);
        } else if (face->kind == KM_OP && face->op == AB_CANCEL) {
            prompt_close(a);
        }
        return;
    }
    if (a->prompt_op >= 0 && a->prompt_kind == KM_ARG_ADDR) {      /* three digits (CLVARx) */
        if (face->kind == KM_OP && face->op >= AB_K0 && face->op <= AB_K9) {
            a->prompt_val = a->prompt_val * 10 + face->op - AB_K0;
            if (++a->prompt_n == 3) {
                int op = a->prompt_op, v = a->prompt_val;
                prompt_close(a);
                do_op(a, op, v);
            } else {
                prompt_open(a, a->prompt_op, a->prompt_kind);
            }
        } else if (face->kind == KM_OP && face->op == AB_KBKSP && a->prompt_n > 0) {
            a->prompt_n--;
            a->prompt_val /= 10;
            prompt_open(a, a->prompt_op, a->prompt_kind);
        } else if (face->kind == KM_OP && face->op == AB_CANCEL) {
            prompt_close(a);
        }
        return;
    }
    if (a->prompt_op >= 0 && a->prompt_kind == KM_ARG_DIGITS) {    /* a digit setting */
        if (face->kind == KM_OP && face->op >= AB_K0 && face->op <= AB_K9) {
            int op = a->prompt_op, n = (a->prompt_tens ? 10 : 0) + face->op - AB_K0;
            prompt_close(a);
            do_op(a, op, n);
        } else if (face->kind == KM_OP && face->op == AB_KPOINT && !a->prompt_tens) {
            a->prompt_tens = true;              /* 10-19 next */
            prompt_open(a, a->prompt_op, a->prompt_kind);
        } else if (face->kind == KM_OP && face->op == AB_CANCEL) {
            prompt_close(a);
        }
        return;
    }
    if ((a->prompt_op == AB_GTO || a->prompt_op == AB_XEQ) && a->prompt_kind == KM_ARG_TARGET && ab_rec35(a->c) &&
        a->gto_dot == 0) {
        /* 35s and STU (unit 036b; 35s p.14-6, p.13-10): GTO and XEQ take a letter, then ENTER for its first
           line or three digits for a line; a line 000 is NONEXISTENT. In program entry they record the line
           (XEQ A001, GTO A003). Backspace steps back, C backs out. Other keys before the letter fall through:
           (I) and (J) act at once, as before, and GTO's . opens the address form (036) */
        int op = a->prompt_op;
        if (a->tgt_label < 0) {
            const km_letter *l = km_letter_of(key);
            if (l->kind == KL_LETTER) {
                a->tgt_label = l->value;
                a->prompt_n = a->prompt_val = 0;
                tgt_text(a);
                return;
            }
        } else {
            if (face->kind == KM_OP && face->op >= AB_K0 && face->op <= AB_K9) {
                a->prompt_val = a->prompt_val * 10 + face->op - AB_K0;
                if (++a->prompt_n == 3) {
                    int line = a->prompt_val, lab = a->tgt_label;
                    prompt_close(a);
                    if (line == 0) do_op(a, AB_GTOPOS, -2);     /* no line 000: NONEXISTENT, never a line */
                    else do_op(a, op, lab * 1000 + line);
                } else {
                    tgt_text(a);
                }
            } else if (face->kind == KM_OP && face->op == AB_ENTER && a->prompt_n == 0) {
                int lab = a->tgt_label;
                prompt_close(a);
                do_op(a, op, lab * 1000);
            } else if (face->kind == KM_OP && face->op == AB_KBKSP) {
                if (a->prompt_n > 0) {
                    a->prompt_n--;
                    a->prompt_val /= 10;
                    tgt_text(a);
                } else {
                    a->tgt_label = -1;
                    prompt_open(a, op, KM_ARG_TARGET);
                }
            } else if (face->kind == KM_OP && face->op == AB_CANCEL) {
                prompt_close(a);
            }
            return;
        }
    }
    if (a->prompt_op == AB_GTO && a->prompt_kind == KM_ARG_TARGET) {
        /* GTO . from the keyboard (unit 036; 33s p.13-5, 35s p.14-6): . . to PRGM TOP; . label and the
           line's digits to that line, the pointer only (AB_GTOPOS: never a line, in program entry too);
           backspace steps back, C backs out */
        int need = a->c->m35 ? 3 : 4;
        bool dot = face->kind == KM_OP && face->op == AB_KPOINT && a->shift == 0;
        bool bk = face->kind == KM_OP && face->op == AB_KBKSP, cancel = face->kind == KM_OP && face->op == AB_CANCEL;
        if (a->gto_dot == 0 && dot) {
            a->gto_dot = 1;
            gto_text(a);
            return;
        }
        if (a->gto_dot == 1) {
            const km_letter *l = km_letter_of(key);
            if (dot) {
                prompt_close(a);
                do_op(a, AB_GTOPOS, AB_PC_TOP);
            } else if (bk) {
                a->gto_dot = 0;
                prompt_open(a, AB_GTO, KM_ARG_TARGET);
            } else if (cancel) {
                prompt_close(a);
            } else if (l->kind == KL_LETTER) {
                a->gto_dot = 2;
                a->gto_label = l->value;
                a->prompt_n = a->prompt_val = 0;
                gto_text(a);
            }
            return;
        }
        if (a->gto_dot == 2) {
            if (face->kind == KM_OP && face->op >= AB_K0 && face->op <= AB_K9) {
                a->prompt_val = a->prompt_val * 10 + face->op - AB_K0;
                if (++a->prompt_n == need) {
                    int line = a->prompt_val, lab = a->gto_label;
                    prompt_close(a);
                    do_op(a, AB_GTOPOS, line >= 1 && line <= 999 ? lab * 1000 + line : -2);   /* -2: NONEXISTENT */
                } else {
                    gto_text(a);
                }
            } else if (bk) {
                if (a->prompt_n > 0) {
                    a->prompt_n--;
                    a->prompt_val /= 10;
                } else {
                    a->gto_dot = 1;
                }
                gto_text(a);
            } else if (cancel) {
                prompt_close(a);
            }
            return;
        }
    }
    if (a->prompt_op >= 0) {                    /* the letter layer */
        const km_letter *l = km_letter_of(key);
        if (l->kind == KL_LETTER || l->kind == KL_IND_I || l->kind == KL_IND_J) {
            int arg = km_prompt_arg(a->prompt_kind, l);
            if (!ab_rec35(a->c) && l->kind != KL_LETTER) {    /* (in program entry, the edited label's: 041 C) */
                /* 33s mode (unit 025): the (I) key gives i, the (J) key (i), the 33s's one index */
                bool var = a->prompt_kind == KM_ARG_VAR,
                     tgt = a->prompt_kind == KM_ARG_TARGET || a->prompt_kind == KM_ARG_FN;
                arg = l->kind == KL_IND_I ? (var ? AB_VAR_SI : -1) : var ? AB_VAR_IND_I : tgt ? AB_TGT_IND_I : -1;
            }
            if (arg != -1) {
                int op = a->prompt_op;
                prompt_close(a);
                do_op(a, op, arg);
            }
        } else if (l->kind == KL_ARITH) {
            int op = km_arith_op(a->prompt_op, l->value);
            if (op >= 0) prompt_open(a, op, a->prompt_kind);
        } else if (l->kind == KL_ESC) {
            prompt_close(a);
        }
        return;
    }
    if (a->cat && face->kind != KM_SHIFT && face->kind != KM_SOFT) {
        /* unit 041 C (B5): MEM's catalog (33s p.3-3, p.12-20): C cancels; CLEAR clears the variable shown or deletes
           the program; in VAR, ENTER copies the variable to X; in PGM, XEQ or R/S runs the label. The soft keys are
           the catalog's bar; a shift arms; any other key leaves the catalog and acts. */
        const km_action *act = km_lookup(key, a->shift);
        if (act->kind == KM_NONE) act = face;
        int sh = a->shift, bar = a->menu;
        a->shift = 0;
        if (face->kind == KM_OP && face->op == AB_CANCEL && sh == 0) {
            a->cat = 0;
            a->menu = -1;
            return;
        }
        if (act->kind == KM_MENU && act->arg >= 0 && act->arg == menu_named("CLEAR")) {
            int i = cat_pos(a);
            undo_mark(a);
            if (a->cat == 1) ab_var_clear(a->c, a->cat_at);
            else ab_prog_delete_label(a->c, a->cat_at);
            a->dirty = true;
            cat_show(a, i < 0 ? 0 : i);         /* the item that followed it, or the first */
            return;
        }
        if (face->kind == KM_OP && face->op == AB_KBKSP && sh == 0) {
            a->cat = 0;                         /* back-arrow backs out to MEM (33s p.12-20; 35s p.1-9) */
            a->menu = menu_named("MEM");
            a->menu_page = 0;
            mem_fill(a);
            return;
        }
        if (a->cat == 1 && act->kind == KM_OP && act->op == AB_ENTER) {
            if (!a->c->prgm) do_op(a, AB_RCL, a->cat_at == 26 ? AB_VAR_SI : AB_VAR_A + a->cat_at);
            a->menu = bar;                      /* in program entry: no line (ours, fresh eyes) */
            return;
        }
        if (a->cat == 2 && ((act->kind == KM_PROMPT && act->op == AB_XEQ) || (act->kind == KM_OP && act->op == AB_RS))) {
            if (a->c->prgm) {                   /* in program entry: no line (ours, fresh eyes) */
                a->menu = bar;
                return;
            }
            int l = a->cat_at;
            a->cat = 0;
            a->menu = -1;
            do_op(a, AB_XEQ, l * 1000);
            return;
        }
        a->cat = 0;                             /* any other key: the catalog left, the key acts */
        a->shift = sh;
    }
    if (a->menu >= 0 && face->kind == KM_SHIFT) {     /* a shift arms, switches or cancels; the menu stays
                                                         (ruling #3828, abacus #3971) */
        a->shift = a->shift == face->arg ? 0 : face->arg;
        return;
    }
    if (a->menu >= 0) {                         /* a menu on the soft keys */
        int at = a->menu, page = a->menu_page;
        const km_menu *mn = km_menu_at(at);
        a->menu = -1;
        if (face->kind == KM_SOFT && mn && mn->paged) {     /* CONST, APPS: five a page, then more */
            bool apps = mn->paged == KM_PAGED_APPS;
            int n = apps ? a->napps : ab_consts(a->c), i = page * 5 + face->arg;
            a->shift = 0;
            if (face->arg == 5) {
                a->menu = at;
                a->menu_page = (page + 1) * 5 < n ? page + 1 : 0;
            } else if (i < n) {
                if (apps) app_start(a, a->apps[i]);     /* unit 037 */
                else do_op(a, mn->paged, i);
            }
            return;
        }
        if (face->kind == KM_SOFT && mn) {
            const km_action *it = &mn->item[face->arg];
            a->shift = 0;
            if (item_hidden(a->c, it)) {        /* 33s mode: a 35s feature does nothing, the menu kept (unit 041 A) */
                a->menu = at;
                a->menu_page = page;
                return;
            }
            do_action(a, it);
            if ((it->kind == KM_DEVICE && (it->arg == KM_DEV_USE || it->arg == KM_DEV_USEALL)) ||
                (it->kind == KM_OP && (it->op == AB_ENGL || it->op == AB_ENGR)))
                a->menu = at;                   /* the buzzer's uses: toggled with the menu open (ours); ENG→ / ←ENG
                                                   pressed again from DISP (ours, 041 C) */
            return;
        }
        if (cx_prefix(a, at)) {
            /* 33s mode: CMPLX is also the prefix (unit 039; 33s p.9-2): an operator key does it on the stack
               pairs; the soft keys stay the menu's (x i y, rule 5); any other key closes it, as a menu */
            const km_action *op = km_lookup(key, a->shift);
            if (op->kind == KM_NONE) op = face;
            int k = cx_pair_of(op);
            if (k >= 0) {
                a->shift = 0;
                do_op(a, AB_CMPLX, k);
                return;
            }
        }
        if (at == menu_named("HYP") && !ab_rec35(a->c)) {
            /* 33s mode: HYP is also the prefix (unit 041 C, B10; 33s p.4-6): a trig key, or its gold inverse, is
               the hyperbolic function, on the stack, in program entry or typed; the soft keys stay the menu's; any
               other key closes it, as a menu. 35s mode and STU keep the menu only. */
            const km_action *op = km_lookup(key, a->shift);
            if (op->kind == KM_NONE) op = face;
            int h = hyp_of(op);
            if (h >= 0) {
                a->shift = 0;
                do_op(a, h, 0);
                return;
            }
        }
        if (face->kind == KM_OP && (face->op == AB_CANCEL || face->op == AB_KBKSP) && a->shift == 0)
            return;                             /* C or back-arrow only closes it (35s p.1-9: "cancels any other
                                                   menu"; back-arrow backs out of MEM one level: 041 C) */
    }
    if (face->kind == KM_SOFT && eqn_bar(a) >= 0) {     /* Equation mode's bar (unit 020) */
        int bar = eqn_bar(a);                   /* (with the shift's bank: unit 040) */
        a->shift = 0;
        do_action(a, &km_menu_at(bar)->item[face->arg]);
        return;
    }
    if (a->c->gr_on && face->kind == KM_OP && face->op >= AB_K1 && face->op <= AB_K9 && a->shift != 2) {
        /* unit 040: the open graph's digits move the cursor (the STU-32 has no arrows): 8 up, 2 down, 4 left,
           6 right, the corners diagonal, 5 home; GOLD first, ten pixels */
        int d = face->op - AB_K0 + (a->shift == 1 ? 10 : 0);
        a->shift = 0;
        do_op(a, AB_GMOVE, d);
        return;
    }
    if (face->kind == KM_SHIFT) {               /* the same shift cancels, the other switches */
        a->shift = a->shift == face->arg ? 0 : face->arg;
        return;
    }
    if (a->shift == 0 && a->c->base == 16 && key >= 6 && key <= 11) {     /* HEX: A-F (UG p.11-1) */
        const km_letter *l = km_letter_of(key);
        if (l->kind == KL_LETTER && l->value < 6) {
            do_op(a, AB_KA + l->value, 0);
            return;
        }
    }
    const km_action *act = km_lookup(key, a->shift);
    if (act->kind == KM_NONE) act = face;       /* no separate legend: the face action (#3827) */
    a->shift = 0;
    do_action(a, act);
}

/* SYS's views (unit 027 rule 5), from the device's facts, as lines any key leaves. */
static void view_open(app_state *a, int view)
{
    app_sysinfo si;
    memset(&si, 0, sizeof si);
    si.soc = -1;
    if (a->sysinfo) a->sysinfo(a->ctx, &si);
    memset(a->view_text, 0, sizeof a->view_text);
    char (*t)[40] = a->view_text;
    if (view == APP_VIEW_BATT) {
        if (si.soc >= 0) snprintf(t[0], sizeof t[0], "BATT %d%%", si.soc);
        else snprintf(t[0], sizeof t[0], "BATT --");
        snprintf(t[1], sizeof t[1], "CHG %s", si.chg_n ? "OFF" : "ON");     /* the pins are active low */
        snprintf(t[2], sizeof t[2], "PGOOD %s", si.pgood_n ? "NO" : "YES");
        if (si.vbat_uv) snprintf(t[3], sizeof t[3], "%u.%02u V", (unsigned)(si.vbat_uv / 1000000u),
                                 (unsigned)(si.vbat_uv % 1000000u / 10000u));
    } else if (view == APP_VIEW_VER) {
        snprintf(t[0], sizeof t[0], "VER %s", si.version ? si.version : "?");
        snprintf(t[1], sizeof t[1], "CORE %s", si.commit ? si.commit : "?");
        snprintf(t[2], sizeof t[2], "BUILT %s", si.built ? si.built : "?");
    } else if (view == APP_VIEW_FLASH) {
        snprintf(t[0], sizeof t[0], "FLASH %lu B FREE", (unsigned long)si.flash_free);
        snprintf(t[1], sizeof t[1], "HEAP %lu B", (unsigned long)si.heap_free);
        snprintf(t[2], sizeof t[2], "LARGEST %lu B", (unsigned long)si.heap_largest);
    } else {
        return;
    }
    a->view = view;
}

/* RADIX and SETUP (unit 022): the device's settings change, are saved with the image, and are not
   undo points (UNDO is the calculator's). */
static void setting(app_state *a, const km_action *act)
{
    dev_settings *s = &a->set;
    switch (act->arg) {
    case KM_DEV_RADIX: s->radix = (char)act->op; break;
    case KM_DEV_GROUP: s->group = (char)act->op; break;
    case KM_DEV_OFFTIME:                        /* 2, 5, 10, 30 minutes, wrapping */
        s->off_min = s->off_min == 2 ? 5 : s->off_min == 5 ? 10 : s->off_min == 10 ? 30 : 2;
        break;
    case KM_DEV_VOL:                            /* the next level, sounded */
        s->vol = (uint8_t)((s->vol + 1) % PIEZO_LEVELS);
        a->tone = s->vol;
        a->tone_use = 0;
        break;
    case KM_DEV_USE: s->uses ^= (uint8_t)act->op; break;            /* one use on or off (unit 027) */
    case KM_DEV_USEALL: s->uses = s->uses ? 0 : DEV_USE_ALL; break; /* any on: all off; else all on */
    case KM_DEV_H24: s->h24 = !s->h24; break;
    case KM_DEV_CLOCKSET:
        a->prompt_n = a->prompt_val = 0;
        prompt_open(a, 0, KM_ARG_TIME);
        return;
    case KM_DEV_VIEW:
        view_open(a, act->op);
        return;
    default: return;
    }
    a->pwr.idle_ms = s->off_min * 60000u;
    a->dirty = true;
}

static void do_action(app_state *a, const km_action *act)
{
    if (item_hidden(a->c, act)) return;         /* 33s mode: a 35s feature does nothing (unit 041 A) */
    switch (act->kind) {
    case KM_OP:
        if (act->op == AB_ENTRYM && act->arg == 1 && !a->c->stu) break;    /* ALG: shown in STU only */
        if (act->op == AB_SHOW) {               /* unit 030: held, X's full mantissa; changes nothing, so
                                                   no UNDO mark (a message showing: cleared, as any key) */
            char t[2][AB_SHOW_LEN];
            bool msg = a->c->msg;               /* (a 35s message: app_key cleared it before this) */
            ab_do_arg(a->c, AB_SHOW, 0);
            if (msg) a->dirty = true;
            if (ab_show_text(a->c, t) > 0) {
                a->show = true;
                a->show_key = a->key_now;
            }
            break;
        }
        do_op(a, act->op, act->arg);
        break;
    case KM_PROMPT:
        if (ab_rpn_only(a->c, (ab_op)act->op)) {   /* refused before its argument (unit 029) */
            a->notice = ab_status_message(ABN_RPN_ONLY);
            a->notice_ms = a->last_key_ms;
            break;
        }
        if (act->op == AB_XEQ && a->c->eqn_mode && !a->c->prgm)
            do_op(a, AB_XEQ, AB_TGT_EQN);       /* evaluates the equation shown (UG p.G-14); while one is typed, ends,
                                                   stores and evaluates it, as the core's XEQ (abacus #4799, 3) */
        else if (act->op == AB_SOLVE && !a->c->prgm && ab_builtin_shown(a->c))
            do_op(a, AB_SOLVE, -1);             /* a built-in solver: straight to A's prompt (35s p.7-6; unit 043) */
        else
            prompt_open(a, act->op, act->prompt);
        break;
    case KM_MENU:
        if (act->arg >= 0 && km_menu_at(act->arg)) {
            a->menu = act->arg;
            a->menu_page = 0;
            if (act->arg == menu_named("MEM")) mem_fill(a);    /* unit 041 C (B5) */
        }
        break;
    case KM_CONFIRM:                            /* ALL is PGM in program entry, EQN in Equation
                                                   mode (UG p.1-5) */
        a->confirm_op = act->op != AB_CLALL ? act->op : a->c->prgm ? AB_CLPGM : a->c->eqn_mode ? AB_CLEQN : AB_CLALL;
        break;
    case KM_DEVICE:
        if (act->arg == KM_DEV_GPAGE) {         /* unit 040: the graph's next page, its readout, ZFACT */
            a->gpage = (a->gpage + 1) % 3;
            break;
        }
        if (act->arg == KM_DEV_GXY) {
            a->gxy_off = !a->gxy_off;
            break;
        }
        if (act->arg == KM_DEV_GFACT) {         /* 2, 4, 8, 10 in turn (the digits move the cursor: no typing) */
            abn k2 = abn_from_int(2), k4 = abn_from_int(4), k8 = abn_from_int(8);
            int f = !abn_cmp(&a->c->gr_hf, &k2) ? 4 : !abn_cmp(&a->c->gr_hf, &k4) ? 8 : !abn_cmp(&a->c->gr_hf, &k8) ? 10 : 2;
            static char note[16];
            do_op(a, AB_GFACT, f);
            snprintf(note, sizeof note, "ZFACT %d", f);
            a->notice = note;
            a->notice_ms = a->last_key_ms;
            break;
        }
        if (act->arg == KM_DEV_CAT) {           /* unit 041 C (B5): MEM's catalogs */
            cat_open(a, act->op);
            break;
        }
        if (act->arg == KM_DEV_CATUP || act->arg == KM_DEV_CATDN) {
            int i = cat_pos(a);
            cat_show(a, (i < 0 ? 0 : i) + (act->arg == KM_DEV_CATUP ? -1 : 1));
            break;
        }
        if (act->arg == KM_DEV_CATXEQ) {
            if (a->c->prgm) {                   /* in program entry: no line (ours, fresh eyes) */
                a->menu = menu_named("CAT PGM");
                break;
            }
            int l = a->cat_at;
            a->cat = 0;
            do_op(a, AB_XEQ, l * 1000);
            break;
        }
        if (act->arg == KM_DEV_CATPRGM) {       /* the label in program entry: the pointer on its LBL line */
            int l = a->cat_at;
            a->cat = 0;
            do_op(a, AB_GTOPOS, l * 1000);
            if (!a->c->prgm) do_op(a, AB_PRGM, 0);
            break;
        }
        if (act->arg == KM_DEV_OFF) a->off = true;
        if (act->arg == KM_DEV_UNDO) undo(a);
        if (act->arg >= KM_DEV_RADIX) setting(a, act);
        break;
    default:                                    /* a menu not defined yet, a soft key with none open */
        break;
    }
}

app_action app_tick(app_state *a, const pwr_inputs *in, uint32_t now_ms)
{
    bool usb = pwr_external(in);
    if (usb != a->usb_was) {                    /* the power source changed */
        if (a->usb_was && !a->soft_off)
            pwr_activity(&a->pwr, now_ms);      /* 068: unplugged while on: the idle clock from here (abacus #7571) */
        a->usb_was = usb;
        a->render = true;                       /* (the status band may change) */
    }
    if (a->off && pwr_external(in)) {           /* 068: OFF on USB power is a soft off; the chip stays up */
        a->off = false;
        if (a->app) app_exit(a);                /* (the app saved first) */
        a->soft_off = true;
        a->shift = 0;
        a->render = true;
    }
    if (a->soft_off) {
        if (!pwr_external(in)) return APP_OFF;  /* USB unplugged meanwhile: the deep sleep */
        if (a->dirty) return APP_SAVE;          /* saved first, as OFF saves before its sleep */
        if (a->render) {
            a->render = false;
            a->now_ms = now_ms;
            return APP_RENDER;
        }
        return APP_IDLE;
    }
    bool msg = a->c->msg;
    if (a->app && a->idle_limit && now_ms - a->last_key_ms >= 2u * a->set.off_min * 60000u)
        app_exit(a);                            /* the idle limit (abacus #4639): twice auto-off with no key, the
                                                   app saved and stopped; the calculator's auto-off resumes below */
    if (a->app) {                               /* unit 037: an app's tick; the calculator idle */
        app_tick_app(a, now_ms);
        pwr_inputs p = *in;
        p.busy = true;                          /* auto-off suspended; a low battery still turns it off */
        a->pwr.idle_ms = a->set.off_min * 60000u;
        if (a->off || pwr_check(&a->pwr, &p, now_ms) != PWR_STAY) {
            app_exit(a);                        /* (the app saved first) */
            return APP_OFF;
        }
        if (a->render) {
            a->render = false;
            return APP_RENDER;
        }
        return APP_IDLE;
    }
    if (a->c->gr_on && a->graph && !a->gr_live) {  /* opened by an image or UNDO (review F1): the first page */
        a->gpage = 0;
        a->gxy_off = false;
        graph_start(a);
    }
    if (!a->c->gr_on) a->gr_live = a->graphing = false;
    if (a->graphing) graph_step(a);             /* unit 033: a slice of the graph's columns */
    if (a->running) {
        ab_continue(a->c);
        if (a->c->ovf > 0) a->ovf_pending = true;
        a->dirty = true;
        a->render = true;
        if (a->c->stop != AB_STOP_SLICE) {
            a->running = false;
            a->busy_op = 0;
        }
        if (a->c->stop == AB_STOP_PSE) {
            a->pause = true;
            a->pause_ms = now_ms;
        }
    } else if (a->pause && (uint32_t)(now_ms - a->pause_ms) >= APP_PSE_MS) {
        /* the pause is over: the program goes on, part of the same run (no undo point); its end is
           activity for the power policy (unit 023: a pause counts once, here, so app_tick before it
           changes nothing) */
        a->pause = false;
        pwr_activity(&a->pwr, now_ms);
        ab_do_arg(a->c, AB_RS, 0);
        if (a->c->ovf > 0) a->ovf_pending = true;
        a->dirty = true;
        a->render = true;
        if (a->c->stop == AB_STOP_SLICE) {
            a->running = true;
            a->busy_op = AB_RS;
        }
        if (a->c->stop == AB_STOP_PSE) {
            a->pause = true;
            a->pause_ms = now_ms;
        }
    }
    /* a run that ended in this tick is timed by the device's clock when there is one: its last slice
       may have held a long SOLVE or integral (unit 027 review) */
    after_core(a, msg, a->run_timed && !a->running && !a->pause && a->clock_ms ? a->clock_ms(a->ctx) : now_ms);
    if (in->soc >= 0) {                         /* BATT: once when it first reads low (unit 027) */
        if (in->soc <= APP_BATT_LOW_SOC && !a->batt_low) {
            a->batt_low = true;
            sound(a, DEV_USE_BATT);
            a->render = true;                   /* acted on: the device looks for its tone */
        } else if (in->soc > APP_BATT_OK_SOC && a->batt_low) {
            a->batt_low = false;
            a->render = true;
        }
    }
    {
        int64_t w;                              /* the clock's minute changed: draw it */
        if (app_wall(a, now_ms, &w) && w / 60 != a->clock_min) {
            a->clock_min = w / 60;
            a->render = true;
            uint32_t gone = (now_ms - a->wall_ms) / 1000u;  /* re-anchored, its phase kept: the ms
                                                               clock may run 49 days, an int32 25 */
            a->wall_s += gone;
            a->wall_ms += gone * 1000u;
        }
    }
    overflow_notice(a, now_ms);
    if (a->notice && (uint32_t)(now_ms - a->notice_ms) >= APP_NOTICE_MS) {
        a->notice = NULL;                       /* the brief notice is over */
        a->render = true;
    }
    pwr_inputs p = *in;
    p.busy = a->running;                        /* a pause is counted at its end, above */
    a->pwr.idle_ms = a->set.off_min * 60000u;   /* settings the device loaded take effect */
    if (a->off || pwr_check(&a->pwr, &p, now_ms) != PWR_STAY) return APP_OFF;
    if (a->dirty && !a->running && (uint32_t)(now_ms - a->last_key_ms) >= APP_QUIET_MS) return APP_SAVE;
    if (a->render) {
        a->render = false;
        a->now_ms = now_ms;                     /* the time the screen is drawn at (its clock) */
        return APP_RENDER;
    }
    return APP_IDLE;
}

void app_saved(app_state *a)
{
    a->dirty = false;
}

/* the ms left of span since `since` (0 when it has run out); the clock may wrap */
static uint32_t left(uint32_t now_ms, uint32_t since, uint32_t span)
{
    uint32_t gone = now_ms - since;
    return gone >= span ? 0 : span - gone;
}

uint32_t app_next_wake(const app_state *a, const pwr_inputs *in, uint32_t now_ms)
{
    if (pwr_external(in) != a->usb_was) return 0;      /* the power source changed: seen now (068) */
    if (a->soft_off) return a->render || a->dirty ? 0 : UINT32_MAX;   /* 068: only a key or USB going away (the device looks) */
    if (a->running || a->render || a->off || a->graphing || (a->c->gr_on && a->graph && !a->gr_live)) return 0;
    if (a->app) {                               /* unit 037: the app's next tick, or keys alone; a tune's next note */
        uint32_t t = now_ms - a->app_t0, w = 3600000u;
        if (a->app_next) {
            uint32_t due = a->app_last + a->app_next;
            w = due > t ? due - t : 0;
        }
        if (a->tune_n) {
            uint32_t tw = (int32_t)(a->tune_end - t) > 0 ? a->tune_end - t : 0;
            if (tw < w) w = tw;
        }
        return w;
    }
    if (in->soc >= 0 && ((in->soc <= APP_BATT_LOW_SOC && !a->batt_low) || (in->soc > APP_BATT_OK_SOC && a->batt_low)))
        return 0;                               /* BATT's latch changes now (unit 027) */
    pwr_state p = a->pwr;
    p.idle_ms = a->set.off_min * 60000u;        /* as app_tick sets it */
    uint32_t w = pwr_next(&p, in, now_ms), t;
    if (a->pause && (t = left(now_ms, a->pause_ms, APP_PSE_MS)) < w) w = t;
    if (a->dirty && (t = left(now_ms, a->last_key_ms, APP_QUIET_MS)) < w) w = t;
    if (a->notice && (t = left(now_ms, a->notice_ms, APP_NOTICE_MS)) < w) w = t;
    int64_t ws;
    if (app_wall(a, now_ms, &ws)) {             /* the minute after the one drawn (unit 027 rule 4) */
        if (ws / 60 != a->clock_min) return 0;  /* already past: draw it now */
        int64_t gone = (int32_t)(now_ms - a->wall_ms) > 0 ? (int32_t)(now_ms - a->wall_ms) : 0;
        int64_t to = ((a->clock_min + 1) * 60 - a->wall_s) * 1000 - gone;
        if (to < 1) to = 1;
        if ((uint64_t)to < w) w = (uint32_t)to;
    }
    return w;
}

void app_graph_buffer(app_state *a, app_graph *g)
{
    a->graph = g;
    a->gr_live = false;
    if (g) memset(&g->s, 0, sizeof g->s), g->rd_ok = false;     /* (a device's buffer comes unzeroed) */
    if (a->c->gr_on) graph_start(a);            /* a graph open in the image loaded at boot (review F1) */
}

/* The graph's rows once its window is known: each computed column's row and join (the core's model). */
static void graph_rows(app_state *a, int from, int to)
{
    app_graph *g = a->graph;
    for (int i = from; i < to; i++) {
        g->s.lo[i] = 1;
        g->s.hi[i] = 0;
        if (!g->ok[i]) continue;
        g->row[i] = (int16_t)ab_graph_row(&g->y[i], &g->s.ymin, &g->s.ymax);
        int lo, hi;
        bool pok = i > 0 && g->ok[i - 1];
        if (ab_graph_join(pok, pok ? g->row[i - 1] : 0, g->row[i], &lo, &hi)) {
            g->s.lo[i] = (int16_t)lo;
            g->s.hi[i] = (int16_t)hi;
        }
    }
}

static void graph_ready(app_state *a)
{
    app_graph *g = a->graph;
    g->s.ready = true;
    ab_graph_axes(a->c, &g->s.ymin, &g->s.ymax, &g->s.xrow, &g->s.ycol);
}

/* GO accepted (unit 033 rule 8): with Y set the window is known and columns draw as computed; with AUTO
   the samples come first (a progress bar), then the fit, then the drawing. */
static void graph_start(app_state *a)
{
    app_graph *g = a->graph;
    const ab_calc *c = a->c;
    if (!g) return;
    memset(&g->s, 0, sizeof g->s);
    g->rd_ok = false;
    g->s.xmin = c->gr_xmin;
    g->s.xmax = c->gr_xmax;
    g->s.ymin = c->gr_ymin;
    g->s.ymax = c->gr_ymax;
    {   /* the form's note: V=expr labels its axis V, any other L=R says L-R (rule 1; the core's own test:
           review F4) */
        int form = ab_graph_form(c, c->gr_eq);
        if (form > 0) snprintf(g->s.note, sizeof g->s.note, "%c", 'A' + form - 1);
        else if (form < 0) snprintf(g->s.note, sizeof g->s.note, "L\xe2\x88\x92R");
    }
    if (!c->gr_auto) graph_ready(a);
    a->graphing = true;
    a->gr_live = true;
    a->render = true;
}

/* A slice of columns (at most 8 a tick, so C and the screen keep up). */
static void graph_step(app_state *a)
{
    app_graph *g = a->graph;
    if (!g || !a->c->gr_on) {
        a->graphing = false;
        return;
    }
    int from = g->s.done;
    for (int k = 0; k < 8 && g->s.done < AB_GRAPH_W; k++) {
        abn x;
        ab_val v;
        int i = g->s.done;
        g->ok[i] = ab_graph_sample(a->c, i, &x, &v) == ABN_OK;
        g->y[i] = v.re;
        g->s.done++;
    }
    if (g->s.ready) graph_rows(a, from, g->s.done);
    if (g->s.done == AB_GRAPH_W) {
        a->graphing = false;
        if (!g->s.ready) {
            bool okb[AB_GRAPH_W];
            for (int i = 0; i < AB_GRAPH_W; i++) okb[i] = g->ok[i];
            ab_graph_fit(g->y, okb, AB_GRAPH_W, &g->s.ymin, &g->s.ymax);
            ab_graph_fit_known(a->c, &g->s.ymin, &g->s.ymax);     /* the core knows it too (040 review) */
            int finite = 0;
            for (int i = 0; i < AB_GRAPH_W; i++) finite += okb[i];
            if (!finite) snprintf(g->s.note, sizeof g->s.note, "NO VALUES");    /* -10..10 (answer 2) */
            graph_ready(a);
            graph_rows(a, 0, AB_GRAPH_W);
        }
    }
    a->render = true;
}

void app_undo_buffers(app_state *a, uint8_t *b0, uint8_t *b1, size_t cap)
{
    a->undo[0] = b0;
    a->undo[1] = b1;
    a->undo_cap = b0 && b1 ? cap : 0;
    a->undo_len = 0;
    if (!a->undo_cap) a->undo[0] = a->undo[1] = NULL;
}

void app_ui(const app_state *a, screen_ui *ui)
{
    screen_ui_default(ui);
    if (a->soft_off) {                          /* 068: the soft off draws nothing (the off screen: card off-screen) */
        ui->blank = true;
        return;
    }
    ui->shift = a->shift;
    ui->radix = a->set.radix;
    ui->separator = a->set.group;
    ui->busy = a->running ? a->busy_op : 0;
    ui->prompt = a->prompt_op >= 0 ? a->prompt_text : NULL;
    if (!ui->prompt && cx_prefix(a, a->menu))
        ui->prompt = "CMPLX_";               /* 33s mode's prefix (unit 039; 33s p.G-6) */
    if (!ui->prompt && a->menu >= 0 && a->menu == menu_named("MEM"))
        ui->prompt = a->mem_text;            /* unit 041 C (B5): the bytes free */
    if (a->cat) {                            /* unit 041 C (B5): MEM's catalog on Y's and X's lines */
        ui->view[2] = a->cat_text[0][0] ? a->cat_text[0] : NULL;
        ui->view[3] = a->cat_text[1];
    }
    ui->notice = a->notice;
    {
        int64_t w;                              /* the status band's clock (unit 027 rule 4) */
        if (!app_wall(a, a->now_ms, &w)) {
            snprintf(ui->clock, sizeof ui->clock, "--:--");
        } else {
            int sod = (int)(w % 86400), h = sod / 3600, m = sod / 60 % 60;
            if (a->set.h24) {
                snprintf(ui->clock, sizeof ui->clock, "%02d:%02d", h, m);
            } else {
                snprintf(ui->clock, sizeof ui->clock, "%d:%02d", h % 12 ? h % 12 : 12, m);
                ui->ampm = h < 12 ? 'a' : 'p';
            }
        }
    }
    if (a->view) {                              /* SYS's view in place of the stack */
        for (int i = 0; i < 4; i++) ui->view[i] = a->view_text[i][0] ? a->view_text[i] : NULL;
        return;
    }
    if (a->c->gr_on && a->graph && a->gr_live) {    /* a graph (unit 033): its buffer, the trace's sample */
        app_graph *g = a->graph;
        g->s.cur = a->c->gr_cur;
        if (a->c->gr_trace) {                   /* the readout correctly rounded, the plot's pixels not (044 rule 5) */
            abn x;
            const ab_calc *c = a->c;
            bool same = g->rd_ok && g->rd_col == c->gr_cur && g->rd_eq == c->gr_eq && g->rd_ang == (int)c->angle &&
                        ab_graph_x(a->c, c->gr_cur, &x) == ABN_OK && abn_cmp(&x, &g->rd_x) == 0 &&
                        abn_is_negative(&x) == abn_is_negative(&g->rd_x);
            if (!same) {
                g->rd_y = abn_zero();
                g->rd_st = ab_graph_cursor(a->c, &g->rd_x, &g->rd_y);
                g->rd_col = c->gr_cur, g->rd_eq = c->gr_eq, g->rd_ang = (int)c->angle, g->rd_ok = true;
            }
            g->s.cst = g->rd_st;
            g->s.cx = g->rd_x;
            memset(&g->s.cy, 0, sizeof g->s.cy);
            g->s.cy.re = g->rd_y;
            g->s.crow = -1;
        } else {                                /* unit 040: the free cursor, its pixel's x and y (the window
                                                   known: a fitted Y once plotted, never sampled again here) */
            abn yv = abn_zero();
            if (g->s.ready || !a->c->gr_auto) g->s.cst = ab_graph_cursor(a->c, &g->s.cx, &yv);
            else g->s.cst = ab_graph_x(a->c, a->c->gr_cur, &g->s.cx);
            memset(&g->s.cy, 0, sizeof g->s.cy);
            g->s.cy.re = yv;
            g->s.crow = a->c->gr_row;
        }
        g->s.mcol = a->c->gr_mark ? a->c->gr_mcol : -1;
        g->s.mrow = a->c->gr_mark ? a->c->gr_mrow : -1;
        g->s.noxy = a->gxy_off;
        if (a->c->gr_res_kind == AB_F_TANL + 1 && g->s.ready) {    /* TANL's line, y = m x + b, by column */
            if (!g->s.tl_on || abn_cmp(&g->s.tl_m, &a->c->gr_res) || abn_cmp(&g->s.tl_b, &a->c->gr_res_b)) {
                g->s.tl_m = a->c->gr_res;       /* (the line drawn is this tangent's: 040 review) */
                g->s.tl_b = a->c->gr_res_b;
                for (int i = 0; i < AB_GRAPH_W; i++) {
                    abn x, t, y;
                    g->s.tl[i] = -1;
                    if (ab_graph_x(a->c, i, &x) != ABN_OK || abn_mul(&t, &a->c->gr_res, &x) != ABN_OK ||
                        abn_add(&y, &t, &a->c->gr_res_b) != ABN_OK)
                        continue;
                    int r = ab_graph_row(&y, &g->s.ymin, &g->s.ymax);
                    if (r >= 0 && r <= AB_GRAPH_H - 1) g->s.tl[i] = (int16_t)r;
                }
                g->s.tl_on = true;
            }
        } else {
            g->s.tl_on = false;
        }
        ui->graph = &g->s;
    }
    if (a->show) {                              /* SHOW held in place of the stack (unit 030) */
        ui->show_n = ab_show_text(a->c, ui->show);
        return;
    }
    if (a->confirm_op >= 0) {
        ui->prompt = a->confirm_op == AB_CLPGM ? "CLR PGMS?" : a->confirm_op == AB_CLEQN ? "CLR EQN?" : "CLR ALL?";
        ui->soft[0] = "Y";
        ui->soft[1] = "N";
        return;
    }
    const km_menu *mn = a->menu >= 0 ? km_menu_at(a->menu) : eqn_bar(a) >= 0 ? km_menu_at(eqn_bar(a)) : NULL;
    for (int i = 0; mn && i < SCREEN_SOFT && i < KM_SOFT_KEYS; i++) {
        const km_action *it = &mn->item[i];
        if (mn->paged && i < 5) {               /* CONST's page: its constants by name; APPS's: the apps */
            int k = a->menu_page * 5 + i;
            if (mn->paged == KM_PAGED_APPS) ui->soft[i] = k < a->napps ? a->apps[k]->name : NULL;
            else ui->soft[i] = k < ab_consts(a->c) ? ab_const_symbol(k) : NULL;     /* by symbol (#3958) */
            continue;
        }
        bool all = it->kind == KM_CONFIRM && it->op == AB_CLALL;
        ui->soft[i] = all && a->c->prgm ? "PGM" : all && a->c->eqn_mode ? "EQN" : it->legend;
        {
            /* the mode MODE's keys would change: on a LBL line in program entry, its label's
               (unit 025 rule 5); otherwise the calculator's */
            const ab_calc *c = a->c;
            int mode = c->stu ? AB_MODEL_STU : c->m35 ? 35 : 33;
            if (c->prgm && c->pc >= 0 && c->pc < c->prog_len && c->prog[c->pc].op == AB_LBL &&
                c->lmode[c->prog[c->pc].arg])
                mode = c->lmode[c->prog[c->pc].arg];
            ui->soft_on[i] = it->kind == KM_OP && ((it->op == AB_MODEL && it->arg == mode) ||
                                                   (it->op == AB_ENTRYM && (it->arg == 1) == c->alg));
            if (it->kind == KM_OP && it->op == AB_ENTRYM && it->arg == 1 && !c->stu) ui->soft[i] = NULL;   /* STU's */
        }
        ui->soft_sub[i] = it->kind == KM_MENU;  /* a submenu: its box marked (#3958) */
        ui->soft_dim[i] = item_hidden(a->c, it);   /* unit 041 A */
        if (it->kind == KM_MENU && it->arg >= 0 && !strcmp(km_menu_at(it->arg)->name, "CAS") && !a->c->stu)
            ui->soft[i] = NULL, ui->soft_dim[i] = false;    /* CAS 004: no CAS outside STU, not even dimmed */
        if (it->kind == KM_DEVICE) {            /* the settings in effect (unit 022) */
            static char off[8], vol[8];
            switch (it->arg) {
            case KM_DEV_RADIX: ui->soft_on[i] = a->set.radix == (char)it->op; break;
            case KM_DEV_GROUP: ui->soft_on[i] = a->set.group == (char)it->op; break;
            case KM_DEV_OFFTIME:
                snprintf(off, sizeof off, "OFF%u", (unsigned)a->set.off_min);
                ui->soft[i] = off;
                break;
            case KM_DEV_VOL:
                snprintf(vol, sizeof vol, "VOL %u", (unsigned)a->set.vol);
                ui->soft[i] = vol;
                break;
            case KM_DEV_USE: ui->soft_on[i] = (a->set.uses & it->op) != 0; break;
            case KM_DEV_H24: ui->soft_on[i] = a->set.h24; break;
            default: break;
            }
        }
    }
}

void app_set_wall(app_state *a, int64_t secs, uint32_t now_ms)
{
    a->wall_set = secs >= APP_CLOCK_UNSET_S;
    a->wall_s = secs;
    a->wall_ms = a->now_ms = now_ms;
    a->clock_min = secs / 60;
    a->render = true;
}

bool app_wall(const app_state *a, uint32_t now_ms, int64_t *secs)
{
    if (!a->wall_set) return false;
    int32_t gone = (int32_t)(now_ms - a->wall_ms);  /* the clock may wrap; never before wall_ms */
    *secs = a->wall_s + (gone > 0 ? gone / 1000 : 0);
    return true;
}

void app_interrupt(app_state *a)
{
    if (a->computing) a->c->intr = true;        /* inside the call: read between evaluations or lines (040b) */
    else ab_interrupt(a->c);
    if (a->run_timed) a->run_stopped = true;
}

void app_powered_on(app_state *a)
{
    sound(a, DEV_USE_ON);
}

void app_key_up(app_state *a, int key, uint32_t now_ms)
{
    if (a->soft_off) return;                    /* 068: the soft off drops releases too */
    if (a->app) {                               /* unit 037: the release to the app (C's never) */
        app_key_to(a, key, false, now_ms);
        return;
    }
    if (a->show && key == a->show_key) {        /* SHOW's key released: the normal display (unit 030) */
        a->show = false;
        a->render = true;
    }
}

/* ---- APPS (unit 037): programs that own the screen and the keys ---- */

#define APP_EXIT_KEY 42                         /* C, the HP's ON position: always ends an app (rule 4) */

static app_state *app_fw;                       /* the framework running an app: app_beep's and app_now_ms's */
static app_state *seed_src;                     /* the framework that last ran an app: app_seed's RNG outside a run */

static uint32_t app_hash(const char *name)      /* FNV-1a of the name: an app's save's key */
{
    uint32_t h = 2166136261u;
    for (; *name; name++) h = (h ^ (uint8_t)*name) * 16777619u;
    return h;
}

/* apps.h v1.2: the saves are records of (hash, 16-bit length, bytes) in the device's buffer. The record of hash h:
   the offset of its bytes and their length, or -1. */
static long save_find(const app_state *a, uint32_t h, uint32_t *len)
{
    size_t p = 0;
    while (a->saves && p + 6 <= a->saves_len) {
        const uint8_t *r = a->saves + p;
        uint32_t rh = (uint32_t)r[0] | (uint32_t)r[1] << 8 | (uint32_t)r[2] << 16 | (uint32_t)r[3] << 24;
        uint32_t rl = (uint32_t)r[4] | (uint32_t)r[5] << 8;
        if (rh == h) {
            *len = rl;
            return (long)(p + 6);
        }
        p += 6 + rl;
    }
    return -1;
}

static int save_count(const app_state *a)
{
    int n = 0;
    for (size_t p = 0; a->saves && p + 6 <= a->saves_len; p += 6 + ((size_t)a->saves[p + 4] | (size_t)a->saves[p + 5] << 8))
        n++;
    return n;
}

/* h's record set to d[0..n): replaced, or added; false (nothing changed) with no room or APP_SAVES_MAX apps kept */
static bool save_set(app_state *a, uint32_t h, const uint8_t *d, uint32_t n)
{
    uint32_t old = 0;
    long at = save_find(a, h, &old);
    size_t without = at >= 0 ? a->saves_len - 6 - old : a->saves_len;
    if (!a->saves || n > APP_SAVE_MAX || without + 6 + n > a->saves_cap || (at < 0 && save_count(a) >= APP_SAVES_MAX))
        return false;
    if (at >= 0) {                              /* the old record out, the rest closed up */
        size_t start = (size_t)at - 6, end = (size_t)at + old;
        memmove(a->saves + start, a->saves + end, a->saves_len - end);
        a->saves_len = without;
    }
    uint8_t *r = a->saves + a->saves_len;
    for (int k = 0; k < 4; k++) r[k] = (uint8_t)(h >> (8 * k));
    r[4] = (uint8_t)n;
    r[5] = (uint8_t)(n >> 8);
    memcpy(r + 6, d, n);
    a->saves_len += 6 + n;
    return true;
}

void app_save_buffers(app_state *a, uint8_t *img, size_t img_cap, uint8_t *run, uint32_t run_cap)
{
    a->saves = img;
    a->saves_cap = img ? img_cap : 0;
    a->saves_len = 0;
    a->save_run = run;
    a->save_run_cap = run ? run_cap : 0;
}

static uint32_t app_save_size(const app_def *d)
{
    return d->save_size ? d->save_size : APP_SAVE_SIZE;
}

void app_apps(app_state *a, const app_def *const *list, int n)
{
    a->apps = list;
    a->napps = n;
}

void app_ctx_buffer(app_state *a, void *buf, uint32_t cap)
{
    a->app_ctx = buf;
    a->app_ctx_cap = buf ? cap : 0;
}

bool app_running(const app_state *a)
{
    return a->app != NULL;
}

bool app_ticking(const app_state *a)
{
    return a->app != NULL && (a->app_next > 0 || a->tune_n);    /* (a tune playing: v1.2) */
}

/* Starts an app (APPS's soft key): its state zeroed, its save loaded, started, the first tick at once. */
static void app_start(app_state *a, const app_def *d)
{
    uint32_t now = a->now_ms;
    uint32_t ss = app_save_size(d);
    if (!a->app_ctx || d->ctx_size > a->app_ctx_cap || d->ctx_size > APP_CTX_MAX || ss > APP_SAVE_MAX ||
        !a->save_run || ss > a->save_run_cap) {
        a->notice = "NO MEMORY";
        a->notice_ms = now;
        return;
    }
    memset(a->app_ctx, 0, d->ctx_size);
    a->app = d;
    app_fw = a;
    seed_src = a;
    a->app_t0 = now;
    a->menu = -1;
    a->shift = 0;
    uint32_t len = 0;                           /* its save into the running buffer, the rest zero (v1.2) */
    long at = save_find(a, app_hash(d->name), &len);
    if (len > ss) len = ss;
    memset(a->save_run, 0, ss);
    if (at >= 0) memcpy(a->save_run, a->saves + at, len);
    a->tune_n = 0;
    d->load(a->app_ctx, at >= 0 ? a->save_run : NULL, at >= 0 ? len : 0);
    d->start(a->app_ctx);
    a->app_last = 0;
    a->app_next = d->tick(a->app_ctx, 0);       /* the first tick right after start */
    a->render = true;
}

/* C: the app saves and stops, the calculator's screen as it was. */
static void app_exit(app_state *a)
{
    const app_def *d = a->app;
    uint32_t h = app_hash(d->name), ss = app_save_size(d), len = 0;
    long at = save_find(a, h, &len);
    a->app_exiting = true;                      /* C stops sound: none from save or stop either (v1.2) */
    a->tune_n = 0;
    d->save(a->app_ctx, a->save_run, ss);       /* its save, apart from the calculator's image (rule 5), into the
                                                   running buffer holding what it loaded (v1.2) */
    if ((at < 0 || len != ss || memcmp(a->saves + at, a->save_run, ss)) && save_set(a, h, a->save_run, ss))
        a->apps_dirty = true;                   /* stored only when it changed (the flash's wear) */
    d->stop(a->app_ctx);
    a->app_exiting = false;
    a->app = NULL;
    app_fw = NULL;
    a->app_next = 0;
    a->render = true;
}

/* A key while an app runs: C ends it (never delivered); any other key goes to it, down or up, on its
   clock; drawn when the app asks. The core is never called (rule 3). */
static void app_key_to(app_state *a, int key, bool down, uint32_t now_ms)
{
    a->now_ms = now_ms;
    pwr_activity(&a->pwr, now_ms);
    if (key == APP_EXIT_KEY) {
        if (down) app_exit(a);
        return;
    }
    if (a->app->key(a->app_ctx, key, down, now_ms - a->app_t0)) a->render = true;
}

/* v1.2: a tune's note i starting at t on the app's clock (a rest sounds nothing); past the last, the tune is over */
static void tune_note(app_state *a, uint32_t t)
{
    if (a->tune_i >= a->tune_n) {
        a->tune_n = 0;
        return;
    }
    uint16_t hz = a->tune[2 * a->tune_i], ms = a->tune[2 * a->tune_i + 1];
    if (hz) app_beep(hz, ms);
    a->tune_end = t + ms;
}

/* A tick due: the app's tick with the time since the last; auto-off kept away (rule 3). A tune steps on. */
static void app_tick_app(app_state *a, uint32_t now_ms)
{
    uint32_t t = now_ms - a->app_t0;
    a->now_ms = now_ms;                         /* (auto-off: app_tick counts an app as busy, rule 3) */
    while (a->tune_n && (int32_t)(t - a->tune_end) >= 0) {
        a->tune_i++;
        tune_note(a, a->tune_end);
    }
    if (a->app_next && t - a->app_last >= a->app_next) {
        uint32_t ms = t - a->app_last;
        a->app_last = t;
        a->app_next = a->app->tick(a->app_ctx, ms);
        a->render = true;
    }
}

bool app_draw(app_state *a, ls027_frame *f)
{
    if (!a->app) return false;
    a->app->draw(a->app_ctx, f);
    return true;
}

/* The saves' image: "ABAP", version 2, the records' count, the records (hash, 16-bit length, bytes), CRC-32 of the
   rest (apps.h v1.2). Version 1 (unit 037: eight records of hash, used, 64 bytes) still loads, its saves unchanged. */
#define APP_SAVES_V1_LEN (6 + 8 * (4 + 1 + APP_SAVE_SIZE) + 4)

size_t app_saves_image(const app_state *a, uint8_t *buf, size_t cap)
{
    size_t n = 6 + a->saves_len + 4;
    if (cap < n) return 0;
    memcpy(buf, "ABAP", 4);
    buf[4] = 2;
    buf[5] = (uint8_t)save_count(a);
    if (a->saves_len) memcpy(buf + 6, a->saves, a->saves_len);
    uint32_t crc = ab_crc32(buf, n - 4);
    for (int k = 0; k < 4; k++) buf[n - 4 + k] = (uint8_t)(crc >> (8 * k));
    return n;
}

bool app_saves_load(app_state *a, const uint8_t *buf, size_t n)
{
    if (n < 10 || memcmp(buf, "ABAP", 4)) return false;
    uint32_t crc = (uint32_t)buf[n - 4] | (uint32_t)buf[n - 3] << 8 | (uint32_t)buf[n - 2] << 16 | (uint32_t)buf[n - 1] << 24;
    if (ab_crc32(buf, n - 4) != crc || !a->saves) return false;
    if (buf[4] == 1) {                          /* version 1: each used record, 64 bytes */
        if (n != APP_SAVES_V1_LEN || buf[5] != 8) return false;
        size_t need = 0;                        /* (it must fit before anything changes: fresh eyes) */
        for (size_t p = 6; p + 5 + APP_SAVE_SIZE <= n - 4; p += 5 + APP_SAVE_SIZE) need += buf[p + 4] ? 6 + APP_SAVE_SIZE : 0;
        if (need > a->saves_cap) return false;
        a->saves_len = 0;
        for (size_t p = 6; p + 5 + APP_SAVE_SIZE <= n - 4; p += 5 + APP_SAVE_SIZE) {
            uint32_t h = (uint32_t)buf[p] | (uint32_t)buf[p + 1] << 8 | (uint32_t)buf[p + 2] << 16 | (uint32_t)buf[p + 3] << 24;
            if (buf[p + 4] && !save_set(a, h, buf + p + 5, APP_SAVE_SIZE)) return false;
        }
        return true;
    }
    if (buf[4] != 2 || buf[5] > APP_SAVES_MAX || n - 10 > a->saves_cap) return false;
    size_t p = 6, end = n - 4;
    int count = 0;
    while (p < end) {                           /* every record whole, none past APP_SAVE_MAX, no hash twice */
        if (p + 6 > end) return false;
        uint32_t len = (uint32_t)buf[p + 4] | (uint32_t)buf[p + 5] << 8;
        if (len == 0 || len > APP_SAVE_MAX || p + 6 + len > end) return false;   /* (a save is never empty) */
        for (size_t q = 6; q < p; q += 6 + ((size_t)buf[q + 4] | (size_t)buf[q + 5] << 8))
            if (!memcmp(buf + q, buf + p, 4)) return false;
        p += 6 + len;
        count++;
    }
    if (count != buf[5]) return false;
    memcpy(a->saves, buf + 6, n - 10);
    a->saves_len = n - 10;
    return true;
}

void app_beep(uint16_t hz, uint16_t ms)
{
    app_state *a = app_fw;
    if (!a || a->app_exiting || !(a->set.uses & DEV_USE_APP) || a->set.vol == 0) return;
    a->tone = a->set.vol;
    a->tone_use = DEV_USE_APP;
    a->tone_hz = hz;
    a->tone_ms = ms;
}

void app_tune(const uint16_t *notes, uint8_t n)
{
    app_state *a = app_fw;
    if (!a || a->app_exiting) return;
    if (!notes || n == 0) {                     /* nothing: what was playing stops (after the note sounding) */
        a->tune_n = 0;
        return;
    }
    if (n > APP_TUNE_MAX) n = APP_TUNE_MAX;
    memcpy(a->tune, notes, (size_t)n * 2 * sizeof notes[0]);
    a->tune_n = n;
    a->tune_i = 0;
    tune_note(a, a->now_ms - a->app_t0);        /* the first note now (a new call replaces what was playing) */
}

static bool seed_fixed;                         /* a host's test fixed it (app_seed_set) */
static uint32_t seed_value;

uint32_t app_seed(void)
{
    app_state *a = app_fw ? app_fw : seed_src;
    if (seed_fixed) return seed_value;
    if (a && a->rng) return a->rng(a->ctx);     /* the device's hardware RNG */
    uint32_t x = (a ? (a->clock_ms ? a->clock_ms(a->ctx) : a->now_ms) : 0) ^ 0x9E3779B9u;   /* a host's choice */
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    return x;
}

#ifndef ESP_PLATFORM
void app_seed_set(uint32_t seed)
{
    seed_fixed = true;
    seed_value = seed;
}
#endif

uint32_t app_now_ms(void)
{
    app_state *a = app_fw;
    if (!a) return 0;
    uint32_t now = a->clock_ms ? a->clock_ms(a->ctx) : a->now_ms;
    return now - a->app_t0;
}
