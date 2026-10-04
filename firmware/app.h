/* app: the device's behaviour from a key press to the core, the same on every board and SDK
 * (unit 018 scopes 5-7). Portable C: the device gives it key-down events and the time, calls
 * app_tick between events, and acts on what it returns (render, save, power off); the host test
 * drives it with key numbers.
 *
 * What it does:
 * - Shifts (keymap.h): the same shift cancels, the other switches; any other key clears it.
 * - A key whose action asks for an argument (STO, RCL, VIEW, ISG, DSE, LBL, XEQ, GTO) opens a
 *   prompt; the next keys are read in the letter layer: a letter, (I) or (J) completes it, + - x /
 *   turns STO and RCL into STO+ ... RCL/ (UG p.3-6, 3-7), ESC (the C key) cancels it, other keys
 *   do nothing. XEQ in Equation mode evaluates the equation shown, no prompt (UG p.G-14).
 * - A run the keys start (XEQ, R/S, SST of a long line) goes on in slices from app_tick (unit 016);
 *   while it runs, C or R/S interrupt it (UG p.13-19) and other keys are ignored (ours). SOLVE and
 *   integrate stay one call: the busy callback lets the device draw SOLVING first.
 * - Power (power.h): keys are activity, a run is activity; app_tick says when to turn off.
 * - Saving (store.h, by the device): after a change, once APP_QUIET_MS pass with no key and no run
 *   (ours: few writes to flash), and always before turning off.
 * - A digit prompt (DISP's FIX, SCI, ENG; Michael #3901) reads the digit keys: a digit completes
 *   it, the point then a digit gives 10-19 (the guide's .0 and .1 for 10 and 11, UG p.1-21,
 *   extended), C cancels it, other keys do nothing.
 * - Menus (keymap.h's MENUS; only PRGM is defined, abacus #3877): a menu key opens its menu and
 *   its labels go to the soft keys; a soft key does its item and closes it; C only closes it
 *   (ours: in program entry C would otherwise leave it); any other key closes it and acts.
 *   A menu not defined yet, and a soft key with no menu open, do nothing; UNDO is the device's. */
#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "calc.h"
#include "power.h"
#include "screen.h"

#define APP_QUIET_MS 30000u                 /* a change saved after 30 s without keys (ours) */
#define APP_SLICE_LINES 200                 /* program lines per app_tick slice (ours) */

typedef enum {
    APP_IDLE = 0,           /* nothing to do */
    APP_RENDER,             /* the screen changed: draw it */
    APP_SAVE,               /* save the state image now (then call app_saved) */
    APP_OFF                 /* save, then power off (idle, low battery or the OFF key) */
} app_action;

typedef struct {
    ab_calc *c;
    int shift;              /* 0, 1 gold, 2 blue */
    int prompt_op;          /* the op waiting for its argument, -1 none */
    int prompt_kind;        /* KM_ARG_* */
    char prompt_text[16];   /* what the X line shows meanwhile: "STO _" */
    bool prompt_tens;       /* a digit prompt has had its point: the next digit is 10-19 */
    int menu;               /* the menu open on the soft keys (km_menu_at), -1 none */
    bool running;           /* a run in slices */
    int busy_op;            /* the op that started it, for ab_busy_message */
    bool dirty;             /* changed since the last save */
    bool off;               /* the OFF key was pressed */
    bool render;            /* the screen changed since the last APP_RENDER */
    uint32_t last_key_ms;
    pwr_state pwr;
    void *ctx;
    void (*busy)(void *ctx, int op);        /* before a call that may take long; may be NULL */
} app_state;

void app_init(app_state *a, ab_calc *c, uint32_t now_ms);
void app_key(app_state *a, int key, uint32_t now_ms);
/* Between events: runs one slice of a run in progress, and says what the device should do. */
app_action app_tick(app_state *a, const pwr_inputs *in, uint32_t now_ms);
void app_saved(app_state *a);               /* the device saved the image */
/* UNDO's two image buffers of cap bytes each (unit 021; the device's PSRAM): without them UNDO
   does nothing. */
void app_undo_buffers(app_state *a, uint8_t *b0, uint8_t *b1, size_t cap);
/* What the screen needs that the core does not hold. */
void app_ui(const app_state *a, screen_ui *ui);

#endif
