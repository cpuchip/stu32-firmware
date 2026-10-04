/* app: from a key press to the core (see app.h). */
#include "app.h"

#include <stdio.h>
#include <string.h>

#include "keymap.h"

void app_init(app_state *a, ab_calc *c, uint32_t now_ms)
{
    memset(a, 0, sizeof *a);
    a->c = c;
    a->prompt_op = -1;
    a->menu = -1;
    a->last_key_ms = now_ms;
    pwr_init(&a->pwr, now_ms);
    c->slice = APP_SLICE_LINES;
}

static void prompt_close(app_state *a)
{
    a->prompt_op = -1;
    a->prompt_tens = false;
    a->prompt_text[0] = '\0';
}

/* "STO _", "STO+ _": the op's name as listed (UG Appendix G; list.c's spacing), then the cursor */
static void prompt_open(app_state *a, int op, int kind)
{
    static const char *const ARITH[4] = {"+", "-", "\xc3\x97", "\xc3\xb7"};
    const char *name = op == AB_STO ? "STO" : op == AB_RCL ? "RCL" : op == AB_VIEW ? "VIEW" : op == AB_ISG ? "ISG"
                       : op == AB_DSE ? "DSE" : op == AB_LBL ? "LBL" : op == AB_XEQ ? "XEQ" : op == AB_GTO ? "GTO"
                       : op == AB_SETFIX ? "FIX" : op == AB_SETSCI ? "SCI" : op == AB_SETENG ? "ENG" : "";
    char buf[12] = "";
    if (op >= AB_STOADD && op <= AB_STODIV) snprintf(buf, sizeof buf, "STO%s", ARITH[op - AB_STOADD]);
    else if (op >= AB_RCLADD && op <= AB_RCLDIV) snprintf(buf, sizeof buf, "RCL%s", ARITH[op - AB_RCLADD]);
    else snprintf(buf, sizeof buf, "%s", name);
    a->prompt_op = op;
    a->prompt_kind = kind;
    snprintf(a->prompt_text, sizeof a->prompt_text, a->prompt_tens ? "%s 1_" : "%s _", buf);
}

/* An op from the keys: the busy callback first when it may take long, and a run that has not
   ended goes on from app_tick. */
static void do_op(app_state *a, int op, int arg)
{
    ab_calc *c = a->c;
    bool runs = !c->prgm && (op == AB_XEQ || op == AB_RS || op == AB_SST || op == AB_SOLVE || op == AB_INTEG);
    if (runs && a->busy) a->busy(a->ctx, op);
    ab_do_arg(c, (ab_op)op, arg);
    a->dirty = true;
    if (c->stop == AB_STOP_SLICE) {
        a->running = true;
        a->busy_op = op;
    }
}

static void do_action(app_state *a, const km_action *act);

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
        const km_menu *mn = km_menu_at(a->menu);
        a->menu = -1;
        if (face->kind == KM_SOFT && mn) {
            a->shift = 0;
            do_action(a, &mn->item[face->arg]);
            return;
        }
        if (face->kind == KM_OP && face->op == AB_CANCEL && a->shift == 0) return;   /* C: only closes it */
    }
    if (face->kind == KM_SHIFT) {               /* the same shift cancels, the other switches */
        a->shift = a->shift == face->arg ? 0 : face->arg;
        return;
    }
    const km_action *act = km_lookup(key, a->shift);
    if (act->kind == KM_NONE) act = face;       /* no separate legend: the face action (#3827) */
    a->shift = 0;
    do_action(a, act);
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
        if (act->arg >= 0 && km_menu_at(act->arg)) a->menu = act->arg;
        break;
    case KM_DEVICE:
        if (act->arg == KM_DEV_OFF) a->off = true;
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
    }
    pwr_inputs p = *in;
    p.busy = a->running;
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
    (void)a;
    (void)b0;
    (void)b1;
    (void)cap;
}

void app_ui(const app_state *a, screen_ui *ui)
{
    screen_ui_default(ui);
    ui->shift = a->shift;
    ui->busy = a->running ? a->busy_op : 0;
    ui->prompt = a->prompt_op >= 0 ? a->prompt_text : NULL;
    const km_menu *mn = a->menu >= 0 ? km_menu_at(a->menu) : NULL;
    for (int i = 0; mn && i < SCREEN_SOFT && i < KM_SOFT_KEYS; i++) ui->soft[i] = mn->item[i].legend;
}
