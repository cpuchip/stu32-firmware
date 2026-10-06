/* app: the device's behaviour from a key press to the core, the same on every board and SDK
 * (unit 018 scopes 5-7). Portable C: the device gives it key-down events and the time, calls
 * app_tick between events, and acts on what it returns (render, save, power off); the host test
 * drives it with key numbers.
 *
 * What it does:
 * - Shifts (keymap.h): the same shift cancels, the other switches; any other key clears it. With a
 *   menu open a shift leaves it open: a soft key then acts and clears the shift, any other key closes
 *   the menu and acts in the shift (ruling #3828, abacus #3971).
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
 * - Menus (keymap.h's MENUS): a menu key opens its menu and its labels go to the soft keys; a
 *   soft key does its item and closes it; C only closes it (ours: in program entry C would
 *   otherwise leave it); any other key closes it and acts. A menu not defined yet, and a soft key
 *   with no menu open, do nothing. A soft key may open another menu (BASE's LOGIC and SUFFIX).
 *   CONST is paged (unit 019): soft keys 1-5 recall five constants, 6 shows the next five, and
 *   after the last page the first. MODE marks the model in effect.
 * - An address prompt (CLVARx, unit 021; UG p.1-5's three placeholders) reads three digit keys,
 *   "CLVAR___" filling as they come; the third completes it, backspace takes one back, C cancels,
 *   other keys do nothing.
 * - A yes/no prompt (CLEAR ALL, unit 021; UG p.1-5's CLR ALL? Y N; the item is PGM, CLR PGMS?,
 *   in program entry and EQN, CLR EQN?, in Equation mode): soft key 1 is yes, and any other key,
 *   a shift included, only cancels.
 * - Equation mode's bar (unit 020, provisional): with no menu open, the soft keys are the editor's,
 *   from keymap.h's EQN LIST (up, down, EDIT, NEW, -, EXIT) while the list shows and EQN TYPE
 *   (left, right, ( ), =, SPC, REG >) while an equation is typed; it follows the core's state, so
 *   C or EXIT leaving Equation mode takes it away.
 * - HEX (abacus #3962; UG p.11-1's shortcuts): with no shift, the keys lettered A-F (6-11, whose
 *   functions the core refuses in HEX, p.11-4) type the digits A-F; shifted they keep their
 *   functions.
 * - PSE (UG p.13-19): a program paused at PSE goes on by itself after APP_PSE_MS, from app_tick, so
 *   keys are never blocked; in the pause C or R/S stops it (R/S then resumes it) and other keys are
 *   ignored, as during a run.
 * - On the 35s a key with a message showing only clears it, whatever it would have done (a menu
 *   or a prompt included), as the core does for the keys it sees (unit 017 rule 4).
 * - UNDO (unit 021, ours): before each key that reaches the core (VIEW excepted: it changes
 *   nothing) the state image is kept, and UNDO swaps it with the state, so a second UNDO puts it
 *   back. A run is one key. Shifts, menus and prompts opening are not undo points. The device gives
 *   the two image buffers; the images are not kept through power off. */
#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "calc.h"
#include "power.h"
#include "screen.h"
#include "settings.h"

#define APP_QUIET_MS 30000u                 /* a change saved after 30 s without keys (ours) */
#define APP_SLICE_LINES 200                 /* program lines per app_tick slice (ours) */
#define APP_PSE_MS 1000u                    /* PSE's pause, "about 1 second" (UG p.13-19) */
#define APP_NOTICE_MS 1000u                 /* a brief notice: OVERFLOW "briefly displayed" (33s p.10-3) */

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
    char prompt_text[24];   /* what the X line shows meanwhile: "STO _" */
    bool prompt_tens;       /* a digit prompt has had its point: the next digit is 10-19 */
    int prompt_n, prompt_val;   /* an address prompt's digits so far, and their value */
    int confirm_op;         /* the op a yes/no prompt waits to do (CLR ALL?), -1 none */
    int menu;               /* the menu open on the soft keys (km_menu_at), -1 none */
    int menu_page;          /* a paged menu's page (CONST, unit 019) */
    bool running;           /* a run in slices */
    bool pause;             /* a program paused at PSE, going on by itself after APP_PSE_MS */
    const char *notice;     /* a brief notice on the X line, NULL none (unit 024: a base result clamped) */
    uint32_t notice_ms;     /* when it began; it goes after APP_NOTICE_MS or at the next key */
    bool ovf_pending;       /* an overflow in the run going on: its OVERFLOW notice when the run
                               ends (33s p.13-9; unit 026) */
    uint32_t pause_ms;      /* when the pause began */
    int busy_op;            /* the op that started it, for ab_busy_message */
    bool dirty;             /* changed since the last save */
    bool off;               /* the OFF key was pressed */
    bool render;            /* the screen changed since the last APP_RENDER */
    uint8_t *undo[2];       /* UNDO's images: [0] the state before the last change, [1] scratch */
    size_t undo_cap, undo_len;  /* undo_len 0: nothing to undo */
    uint32_t last_key_ms;
    pwr_state pwr;
    dev_settings set;       /* the device's settings (unit 022): RADIX and SETUP change them */
    int tone;               /* a tone for the device to sound at this level, -1 none (it clears it):
                               VOL's new level, or an error with BEEP on (unit 022) */
    void *ctx;
    void (*busy)(void *ctx, int op);        /* before a call that may take long; may be NULL */
} app_state;

void app_init(app_state *a, ab_calc *c, uint32_t now_ms);
void app_key(app_state *a, int key, uint32_t now_ms);
/* Between events: runs one slice of a run in progress, and says what the device should do. */
app_action app_tick(app_state *a, const pwr_inputs *in, uint32_t now_ms);
void app_saved(app_state *a);               /* the device saved the image */
/* The ms until app_tick has anything to do (unit 023): 0 while a program runs or a render, save
   or power-off is due; else the nearest of the PSE pause's end, the quiet save and pwr_next's
   auto-off or low battery. The contract: app_tick at any t before now_ms plus this, with the same
   inputs, returns APP_IDLE and changes nothing. The device sleeps until then or the next key. */
uint32_t app_next_wake(const app_state *a, const pwr_inputs *in, uint32_t now_ms);
/* UNDO's two image buffers of cap bytes each (unit 021; the device's PSRAM): without them UNDO
   does nothing. */
void app_undo_buffers(app_state *a, uint8_t *b0, uint8_t *b1, size_t cap);
/* What the screen needs that the core does not hold. */
void app_ui(const app_state *a, screen_ui *ui);

#endif
