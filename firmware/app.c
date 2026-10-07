/* app: from a key press to the core (see app.h). */
#include "app.h"

#include <stdio.h>
#include <string.h>

#include "const.h"
#include "keymap.h"
#include "piezo.h"
#include "state.h"

void app_init(app_state *a, ab_calc *c, uint32_t now_ms)
{
    memset(a, 0, sizeof *a);
    a->c = c;
    a->prompt_op = -1;
    a->confirm_op = -1;
    a->menu = -1;
    a->last_key_ms = now_ms;
    a->tone = -1;
    a->now_ms = now_ms;
    a->beeps = c->beeps;
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
                       : op == AB_INTEG ? "\xe2\x88\xab" "FN d" : "";
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
    if (op != AB_VIEW) undo_mark(a);
    if (runs && a->busy) a->busy(a->ctx, op);
    bool msg = c->msg;
    bool on_lbl = c->prgm && c->pc >= 0 && c->pc < c->prog_len && c->prog[c->pc].op == AB_LBL;
    int was = on_lbl ? c->lmode[c->prog[c->pc].arg] : 0;
    abn_status st = ab_do_arg(c, (ab_op)op, arg);
    a->dirty = true;
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

/* The menu named name, -1 if there is none. */
static int menu_named(const char *name)
{
    for (int i = 0; km_menu_at(i); i++)
        if (!strcmp(km_menu_at(i)->name, name)) return i;
    return -1;
}

/* Equation mode's bar on the soft keys when no menu is open (unit 020): typing's while an equation
   is typed (in program entry too), the list's while Equation mode shows the list; -1 none. */
static int eqn_bar(const app_state *a)
{
    if (a->c->tbl_on) return menu_named("TABLE BAR");                /* a table open (unit 032) */
    if (a->c->alg && !a->c->prgm) return menu_named("ALG LINE");    /* STU's line (unit 029) */
    if (a->c->eqn_typing) return menu_named("EQN TYPE");
    if (a->c->eqn_mode && !a->c->prgm) return menu_named("EQN LIST");
    return -1;
}

void app_key(app_state *a, int key, uint32_t now_ms)
{
    const km_action *face = km_lookup(key, KM_FACE);
    a->last_key_ms = a->now_ms = now_ms;
    pwr_activity(&a->pwr, now_ms);
    a->render = true;
    a->notice = NULL;                           /* a key ends a brief notice */
    sound(a, DEV_USE_CLICK);                    /* every key ticks (unit 027) */
    a->key_now = key;
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
    if (face->kind != KM_SHIFT &&
        ab_view_key(a->c, a->shift == 0 && face->kind == KM_OP ? face->op : -1))
        return;                                 /* a keyboard VIEW's display: the core's rule, applied
                                                   here too for keys that never reach it (a menu's,
                                                   a prompt's) */
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
    if (a->prompt_op >= 0) {                    /* the letter layer */
        const km_letter *l = km_letter_of(key);
        if (l->kind == KL_LETTER || l->kind == KL_IND_I || l->kind == KL_IND_J) {
            int arg = km_prompt_arg(a->prompt_kind, l);
            if (!a->c->m35 && l->kind != KL_LETTER) {
                /* 33s mode (unit 025): the (I) key gives i, the (J) key (i), the 33s's one index */
                bool var = a->prompt_kind == KM_ARG_VAR, tgt = a->prompt_kind == KM_ARG_TARGET;
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
    if (a->menu >= 0 && face->kind == KM_SHIFT) {     /* a shift arms, switches or cancels; the menu stays
                                                         (ruling #3828, abacus #3971) */
        a->shift = a->shift == face->arg ? 0 : face->arg;
        return;
    }
    if (a->menu >= 0) {                         /* a menu on the soft keys */
        int at = a->menu, page = a->menu_page;
        const km_menu *mn = km_menu_at(at);
        a->menu = -1;
        if (face->kind == KM_SOFT && mn && mn->paged) {     /* CONST: five a page, then more */
            int n = ab_const_count(), i = page * 5 + face->arg;
            a->shift = 0;
            if (face->arg == 5) {
                a->menu = at;
                a->menu_page = (page + 1) * 5 < n ? page + 1 : 0;
            } else if (i < n) {
                do_op(a, mn->paged, i);
            }
            return;
        }
        if (face->kind == KM_SOFT && mn) {
            const km_action *it = &mn->item[face->arg];
            a->shift = 0;
            do_action(a, it);
            if (it->kind == KM_DEVICE && (it->arg == KM_DEV_USE || it->arg == KM_DEV_USEALL))
                a->menu = at;                   /* the buzzer's uses: toggled with the menu open (ours) */
            return;
        }
        if (face->kind == KM_OP && face->op == AB_CANCEL && a->shift == 0) return;   /* C: only closes it */
    }
    if (face->kind == KM_SOFT && eqn_bar(a) >= 0) {     /* Equation mode's bar (unit 020) */
        a->shift = 0;
        do_action(a, &km_menu_at(eqn_bar(a))->item[face->arg]);
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
        if (act->op == AB_XEQ && a->c->eqn_mode && !a->c->eqn_typing && !a->c->prgm)
            do_op(a, AB_XEQ, AB_TGT_EQN);       /* evaluates the equation shown (UG p.G-14) */
        else
            prompt_open(a, act->op, act->prompt);
        break;
    case KM_MENU:
        if (act->arg >= 0 && km_menu_at(act->arg)) {
            a->menu = act->arg;
            a->menu_page = 0;
        }
        break;
    case KM_CONFIRM:                            /* ALL is PGM in program entry, EQN in Equation
                                                   mode (UG p.1-5) */
        a->confirm_op = act->op != AB_CLALL ? act->op : a->c->prgm ? AB_CLPGM : a->c->eqn_mode ? AB_CLEQN : AB_CLALL;
        break;
    case KM_DEVICE:
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
    bool msg = a->c->msg;
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
    if (a->running || a->render || a->off) return 0;
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
    ui->shift = a->shift;
    ui->radix = a->set.radix;
    ui->separator = a->set.group;
    ui->busy = a->running ? a->busy_op : 0;
    ui->prompt = a->prompt_op >= 0 ? a->prompt_text : NULL;
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
        if (mn->paged && i < 5) {               /* CONST's page: its constants by name */
            int k = a->menu_page * 5 + i;
            ui->soft[i] = k < ab_const_count() ? ab_const_symbol(k) : NULL;     /* by symbol (#3958) */
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
    ab_interrupt(a->c);
    if (a->run_timed) a->run_stopped = true;
}

void app_powered_on(app_state *a)
{
    sound(a, DEV_USE_ON);
}

void app_key_up(app_state *a, int key, uint32_t now_ms)
{
    (void)now_ms;
    if (a->show && key == a->show_key) {        /* SHOW's key released: the normal display (unit 030) */
        a->show = false;
        a->render = true;
    }
}
