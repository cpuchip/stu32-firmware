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
    if (n == 0 || n > a->undo_cap || ab_state_load(a->c, a->undo[0], a->undo_len) != ABN_OK) return;
    undo_swap(a, n);
    a->dirty = true;
}

/* An op from the keys: the state kept for UNDO first (VIEW changes nothing), the busy callback
   when it may take long, and a run that has not ended goes on from app_tick. */
static void do_op(app_state *a, int op, int arg)
{
    ab_calc *c = a->c;
    bool runs = !c->prgm && (op == AB_XEQ || op == AB_RS || op == AB_SST || op == AB_SOLVE || op == AB_INTEG);
    if (op != AB_VIEW) undo_mark(a);
    if (runs && a->busy) a->busy(a->ctx, op);
    bool msg = c->msg;
    ab_do_arg(c, (ab_op)op, arg);
    a->dirty = true;
    if (c->msg && !msg && a->set.beep && a->set.vol > 0) a->tone = a->set.vol;   /* the error tone (unit 022) */
    if (c->stop == AB_STOP_SLICE) {
        a->running = true;
        a->busy_op = op;
    }
    if (c->stop == AB_STOP_PSE) {               /* PSE: going on by itself after the pause */
        a->pause = true;
        a->pause_ms = a->last_key_ms;
    }
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
    if (a->c->eqn_typing) return menu_named("EQN TYPE");
    if (a->c->eqn_mode && !a->c->prgm) return menu_named("EQN LIST");
    return -1;
}

void app_key(app_state *a, int key, uint32_t now_ms)
{
    const km_action *face = km_lookup(key, KM_FACE);
    a->last_key_ms = now_ms;
    pwr_activity(&a->pwr, now_ms);
    a->render = true;

    if (a->running) {                           /* only C and R/S, which interrupt (UG p.13-19) */
        if (face->kind == KM_OP && (face->op == AB_CANCEL || face->op == AB_RS)) ab_interrupt(a->c);
        return;
    }
    if (a->pause) {                             /* PSE's pause: C and R/S stop it (UG p.13-19) */
        if (face->kind == KM_OP && (face->op == AB_CANCEL || face->op == AB_RS)) a->pause = false;
        return;
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
            a->shift = 0;
            do_action(a, &mn->item[face->arg]);
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
        break;
    case KM_DEV_BEEP: s->beep = !s->beep; break;
    default: return;
    }
    a->pwr.idle_ms = s->off_min * 60000u;
    a->dirty = true;
}

static void do_action(app_state *a, const km_action *act)
{
    switch (act->kind) {
    case KM_OP:
        do_op(a, act->op, act->arg);
        break;
    case KM_PROMPT:
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
    if (a->running) {
        ab_continue(a->c);
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
        /* the pause is over: the program goes on, part of the same run (no undo point) */
        a->pause = false;
        ab_do_arg(a->c, AB_RS, 0);
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
    pwr_inputs p = *in;
    p.busy = a->running || a->pause;
    a->pwr.idle_ms = a->set.off_min * 60000u;   /* settings the device loaded take effect */
    if (a->off || pwr_check(&a->pwr, &p, now_ms) != PWR_STAY) return APP_OFF;
    if (a->dirty && !a->running && (uint32_t)(now_ms - a->last_key_ms) >= APP_QUIET_MS) return APP_SAVE;
    if (a->render) {
        a->render = false;
        return APP_RENDER;
    }
    return APP_IDLE;
}

void app_saved(app_state *a)
{
    a->dirty = false;
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
        ui->soft_on[i] = it->kind == KM_OP && it->op == AB_MODEL && it->arg == (a->c->m35 ? 35 : 33);
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
            case KM_DEV_BEEP: ui->soft_on[i] = a->set.beep; break;
            default: break;
            }
        }
    }
}
