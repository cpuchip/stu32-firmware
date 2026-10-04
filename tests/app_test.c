/* app_test: the device's behaviour from key presses (app.h), driven by legends the way a person
 * presses keys: "GOLD" "√x" is the gold shift then the square-root key. Results are read from
 * the core. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "keymap.h"

static int bad, checks;
static uint32_t now;
static int busy_calls, busy_last;

static void expect(int ok, const char *what)
{
    checks++;
    if (!ok && bad++ < 40) printf("app_test: %s\n", what);
}

static void on_busy(void *ctx, int op)
{
    (void)ctx;
    busy_calls++;
    busy_last = op;
}

/* the key whose face legend is l */
static int face(const char *l)
{
    for (int k = 0; k < KB_KEYS; k++)
        if (km_lookup(k, KM_FACE)->kind != KM_NONE && strcmp(km_lookup(k, KM_FACE)->legend, l) == 0) return k;
    fprintf(stderr, "app_test: no face legend %s\n", l);
    return -1;
}

static int shift_key(int s)
{
    for (int k = 0; k < KB_KEYS; k++)
        if (km_lookup(k, KM_FACE)->kind == KM_SHIFT && km_lookup(k, KM_FACE)->arg == s) return k;
    return -1;
}

static void tap(app_state *a, int key)
{
    now += 100;
    app_key(a, key, now);
}

/* press legends separated by blanks; GOLD and BLUE are the shift keys, "k:N" key N, "ltr:X" the
   letter layer's X */
static void press(app_state *a, const char *s)
{
    char buf[256];
    snprintf(buf, sizeof buf, "%s", s);
    for (char *t = strtok(buf, " "); t; t = strtok(NULL, " ")) {
        if (!strcmp(t, "GOLD")) tap(a, shift_key(1));
        else if (!strcmp(t, "BLUE")) tap(a, shift_key(2));
        else if (!strncmp(t, "k:", 2)) tap(a, atoi(t + 2));
        else if (!strncmp(t, "ltr:", 4)) {
            for (int k = 0; k < KB_KEYS; k++)
                if (km_letter_of(k)->kind == KL_LETTER && km_letter_of(k)->value == t[4] - 'A') tap(a, k);
        } else tap(a, face(t));
    }
}

static double x_of(const ab_calc *c)
{
    char t[64];
    double v = 0;
    if (abn_to_text(&c->x.re, t, sizeof t) > 0) sscanf(t, "%lf", &v);
    return v;
}

static double var_of(const ab_calc *c, char name)
{
    char t[64];
    double v = 0;
    if (abn_to_text(&c->var[name - 'A'].re, t, sizeof t) > 0) sscanf(t, "%lf", &v);
    return v;
}

static void run_out(app_state *a, pwr_inputs *in)
{
    for (int i = 0; i < 100000 && a->running; i++) {
        now += 1;
        app_tick(a, in, now);
    }
}

int main(void)
{
    static ab_calc c;
    static app_state a;
    pwr_inputs in = {true, true, 80, false};
    screen_ui ui;

    /* arithmetic, from the keys */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "2 ENTER 3 +");
    expect(x_of(&c) == 5, "2 ENTER 3 +: 5");
    expect(a.dirty, "a change marks the state for saving");

    /* shifts: gold then the root key is x^2; gold twice cancels; gold then blue is blue (#3824) */
    press(&a, "C 3 GOLD \xe2\x88\x9ax");
    expect(x_of(&c) == 9, "GOLD then the root key: x^2");
    press(&a, "C 9 GOLD GOLD \xe2\x88\x9ax");
    expect(x_of(&c) == 3, "GOLD GOLD then the root key: the shift cancelled, the root");
    press(&a, "C 4 +/\xe2\x88\x92 GOLD BLUE \xe2\x88\x9ax");
    expect(x_of(&c) == 4, "GOLD BLUE then the root key: blue, ABS");
    press(&a, "GOLD");
    app_ui(&a, &ui);
    expect(ui.shift == 1, "the screen shows the shift held");
    press(&a, "5");
    app_ui(&a, &ui);
    expect(ui.shift == 0, "any other key clears the shift");

    /* prompts and the letter layer */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "7 STO");
    app_ui(&a, &ui);
    expect(ui.prompt && strcmp(ui.prompt, "STO _") == 0, "STO waits: 'STO _' on the screen");
    press(&a, "ltr:G");
    expect(var_of(&c, 'G') == 7, "STO then G: G = 7");
    app_ui(&a, &ui);
    expect(!ui.prompt, "the prompt closes with its letter");
    press(&a, "STO + ltr:G");
    expect(var_of(&c, 'G') == 14, "STO + G: G = 14 (UG p.3-6)");
    press(&a, "C 2 RCL \xc3\x97 ltr:G");
    expect(x_of(&c) == 28, "RCL x G: x = 28 (UG p.3-7)");
    press(&a, "C 1 STO");
    press(&a, "\xe2\x88\x9ax");                     /* key 6 is A in the letter layer */
    expect(var_of(&c, 'A') == 1 && a.prompt_op < 0, "during a prompt the root key types its letter, A");
    press(&a, "STO C");
    expect(a.prompt_op < 0 && var_of(&c, 'G') == 14, "ESC (the C key) cancels the prompt, nothing stored");
    press(&a, "STO ltr:I");
    expect(var_of(&c, 'I') == x_of(&c), "STO I (the letter): variable I");
    press(&a, "3 STO 3");                           /* key 39: (I) */
    expect(a.prompt_op < 0, "STO (I): the prompt closes");

    /* a program: keyed, run in slices, interrupted. PRGM is a menu for now (the layout's menus
       are not defined), so program entry is switched by the core; the lines come from the keys:
       LBL A (gold XEQ), 1, +, GTO A (gold 4) */
    ab_init(&c);
    app_init(&a, &c, now);
    a.busy = on_busy;
    ab_do(&c, AB_PRGM);
    press(&a, "GOLD XEQ ltr:A 1 + GOLD 4 ltr:A");
    ab_do(&c, AB_PRGM);
    expect(c.prog_len == 4, "LBL A 1 + GTO A keyed from the letters");
    busy_calls = 0;
    press(&a, "0 XEQ ltr:A");
    expect(a.running, "XEQ A: the run goes on in slices");
    expect(busy_calls == 1 && busy_last == AB_XEQ, "the busy callback before the run");
    app_ui(&a, &ui);
    expect(ui.busy == AB_XEQ, "the screen shows RUNNING");
    for (int i = 0; i < 20; i++) app_tick(&a, &in, now += 10);
    expect(a.running && x_of(&c) > 0, "after 20 ticks: still running, X counting");
    press(&a, "5");
    for (int i = 0; i < 5; i++) app_tick(&a, &in, now += 10);
    expect(a.running, "a digit during a run: ignored, the run goes on");
    {
        /* a long run is activity and is not saved under: 11 minutes of it, no key */
        bool off = false, saved = false;
        for (int i = 0; i < 660; i++) {
            app_action r = app_tick(&a, &in, now += 1000);
            off |= r == APP_OFF;
            saved |= r == APP_SAVE;
        }
        expect(a.running && !off, "11 minutes of a run with no key: still on (a run is activity)");
        expect(!saved, "no save while a run goes on");
    }
    press(&a, "R/S");
    run_out(&a, &in);
    expect(!a.running && c.stop == AB_STOP_STOP, "R/S interrupts the run (UG p.13-19)");
    double x1 = x_of(&c);
    expect(x1 > 20 * APP_SLICE_LINES / 4.0 / 2, "it ran many lines in its slices");
    press(&a, "R/S");
    expect(a.running, "R/S resumes");
    for (int i = 0; i < 5; i++) app_tick(&a, &in, now += 10);
    press(&a, "C");
    run_out(&a, &in);
    expect(!a.running && x_of(&c) > x1, "C interrupts too, and the run had gone on");

    /* in program entry XEQ records a line: no busy callback; a label prompt ignores (I) */
    ab_init(&c);
    app_init(&a, &c, now);
    a.busy = on_busy;
    busy_calls = 0;
    ab_do(&c, AB_PRGM);
    press(&a, "GOLD XEQ ltr:A XEQ ltr:A");
    expect(busy_calls == 0 && c.prog_len == 2, "program entry: XEQ A recorded, no busy callback");
    press(&a, "GOLD XEQ 3");                        /* key 39 is (I): no label */
    expect(a.prompt_op == AB_LBL && c.prog_len == 2, "LBL then (I): ignored, the prompt stays open");
    press(&a, "ltr:B");
    expect(a.prompt_op < 0 && c.prog_len == 3, "then B: LBL B");
    ab_do(&c, AB_PRGM);

    /* a run that ends by itself gives what the unsliced run gives: LBL B, 1000, STO N, DSE N,
       GTO B004, RTN (about 2,000 lines: ten slices) */
    {
        static ab_calc d;
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 1);
        ab_do(&c, AB_K1); ab_do(&c, AB_K0); ab_do(&c, AB_K0); ab_do(&c, AB_K0);
        ab_do_arg(&c, AB_STO, 'N' - 'A');
        ab_do_arg(&c, AB_DSE, 'N' - 'A');
        ab_do_arg(&c, AB_GTO, 1 * 1000 + 4);
        ab_do(&c, AB_RTN);
        ab_do(&c, AB_PRGM);
        expect(c.prog_len == 6, "the DSE loop keyed");
        d = c;
        d.slice = 0;
        ab_do_arg(&d, AB_XEQ, 1 * 1000);            /* the unsliced run */
        press(&a, "XEQ ltr:B");
        expect(a.running, "XEQ B: in slices");
        run_out(&a, &in);
        expect(!a.running && c.stop == d.stop && c.pc == d.pc &&
                   abn_cmp(&c.var['N' - 'A'].re, &d.var['N' - 'A'].re) == 0 && abn_cmp(&c.x.re, &d.x.re) == 0,
               "a run in slices ends where the unsliced run ends");
    }

    /* XEQ in Equation mode evaluates the equation shown: no prompt (UG p.G-14) */
    ab_init(&c);
    app_init(&a, &c, now);
    ab_eqn_add(&c, "2+3");
    press(&a, "XEQ");
    expect(a.prompt_op < 0 && x_of(&c) == 5, "XEQ in Equation mode: 2+3 evaluated");

    /* power: 10 minutes idle (p.1-1), a run is activity */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "1");
    expect(app_tick(&a, &in, now + PWR_IDLE_MS - 1) != APP_OFF, "a millisecond short of 10 minutes: on");
    expect(app_tick(&a, &in, now + PWR_IDLE_MS) == APP_OFF, "10 minutes idle: off");
    press(&a, "GOLD C");
    expect(app_tick(&a, &in, now) == APP_OFF, "the OFF key: off");

    /* saving: after a change, once 30 s pass quietly; not again until another change */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "4 2");
    uint32_t t0 = now;
    expect(app_tick(&a, &in, t0 + APP_QUIET_MS - 1) != APP_SAVE, "29.999 s after a key: no save");
    expect(app_tick(&a, &in, t0 + APP_QUIET_MS) == APP_SAVE, "30 s quiet after a change: save");
    app_saved(&a);
    expect(app_tick(&a, &in, t0 + APP_QUIET_MS + 1000) != APP_SAVE, "saved: no second save");
    press(&a, "ENTER");
    expect(app_tick(&a, &in, now + APP_QUIET_MS) == APP_SAVE, "a new change: saved again");

    /* the PRGM menu (#3877): GOLD 9 opens it, its soft keys act and close it, C only closes it,
       another key closes it and acts; GOLD 0 is the Equation-mode toggle (#3867) */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "GOLD 9");
    app_ui(&a, &ui);
    expect(ui.soft[0] && !strcmp(ui.soft[0], "PRGM") && ui.soft[5] && !strcmp(ui.soft[5], "CLPGM"),
           "GOLD 9: the PRGM menu's labels on the soft keys");
    press(&a, "k:0");
    app_ui(&a, &ui);
    expect(c.prgm && !ui.soft[0], "soft key 1 (PRGM): program entry on, the menu closed");
    press(&a, "GOLD 9 k:1 ltr:A GOLD 9 k:2 ltr:R");
    {
        char t[32] = "";
        ab_prog_line(&c, 0, NULL, t, sizeof t);
        bool l1 = !strcmp(t, "LBL A");
        ab_prog_line(&c, 1, NULL, t, sizeof t);
        expect(l1 && !strcmp(t, "INPUT R") && c.prog_len == 2, "the menu's LBL A and INPUT R recorded");
    }
    press(&a, "GOLD 9 C");
    app_ui(&a, &ui);
    expect(c.prgm && !ui.soft[0] && c.prog_len == 2, "C closes the menu only: still in program entry, nothing recorded");
    press(&a, "GOLD 9 5");
    app_ui(&a, &ui);
    expect(!ui.soft[0] && c.entry.active, "another key closes the menu and acts: 5 typed");
    press(&a, "ENTER GOLD 9 k:0");
    expect(!c.prgm, "soft key 1 again: program entry off");
    press(&a, "GOLD 0");
    expect(c.eqn_mode, "GOLD 0: Equation mode on (the provisional toggle)");
    press(&a, "GOLD 0");
    expect(!c.eqn_mode, "GOLD 0 again: off");

    /* the DISP menu (Michael, #3901): FIX, SCI, ENG then a digit setting (UG p.1-21's FIX_ prompt;
       . then a digit is 10-19, extending the guide's .0 and .1), ALL, xiy, rθa */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "GOLD 2");
    app_ui(&a, &ui);
    expect(ui.soft[0] && !strcmp(ui.soft[0], "FIX") && ui.soft[3] && !strcmp(ui.soft[3], "ALL"),
           "GOLD 2: the DISP menu's labels");
    press(&a, "k:0");
    app_ui(&a, &ui);
    expect(ui.prompt && !strcmp(ui.prompt, "FIX _"), "FIX waits for its digit: 'FIX _'");
    press(&a, "STO");
    expect(a.prompt_op == AB_SETFIX, "a digit prompt ignores other keys (STO)");
    press(&a, "4");
    expect(c.disp_mode == AB_FIX && c.disp_n == 4 && a.prompt_op < 0, "FIX 4");
    press(&a, "GOLD 2 k:1 .");
    app_ui(&a, &ui);
    expect(ui.prompt && !strcmp(ui.prompt, "SCI 1_"), "SCI . waits for the second digit: 'SCI 1_'");
    press(&a, "2");
    expect(c.disp_mode == AB_SCI && c.disp_n == 12, "SCI . 2: SCI 12");
    press(&a, "GOLD 2 k:2 C");
    expect(c.disp_mode == AB_SCI && c.disp_n == 12 && a.prompt_op < 0, "ENG then C: cancelled, SCI 12 kept");
    press(&a, "GOLD 2 k:2 0");
    expect(c.disp_mode == AB_ENG && c.disp_n == 0, "ENG 0");
    press(&a, "GOLD 2 k:3");
    expect(c.disp_mode == AB_ALL, "ALL");
    press(&a, "GOLD 2 k:5");
    expect(c.cpolar, "r\xce\xb8" "a: complex numbers in polar form");
    press(&a, "GOLD 2 k:4");
    expect(!c.cpolar, "xiy: back to rectangular");
    ab_do(&c, AB_PRGM);
    press(&a, "GOLD 2 k:0 7");
    {
        char t[32] = "";
        ab_prog_line(&c, c.pc, NULL, t, sizeof t);
        expect(!strcmp(t, "FIX 7"), "in program entry: the line FIX 7");
    }
    ab_do(&c, AB_PRGM);

    /* the angle-mode menu (decision 33): BLUE 2, then DEG, RAD or GRAD; an empty soft key closes it */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "BLUE 2");
    app_ui(&a, &ui);
    expect(ui.soft[1] && !strcmp(ui.soft[1], "RAD") && (!ui.soft[3] || !ui.soft[3][0]), "BLUE 2: DEG RAD GRAD on the soft keys");
    press(&a, "k:1");
    expect(c.angle == AB_ANGLE_RAD, "RAD");
    press(&a, "BLUE 2 k:2");
    expect(c.angle == AB_ANGLE_GRAD, "GRAD");
    press(&a, "BLUE 2 k:4");
    app_ui(&a, &ui);
    expect(c.angle == AB_ANGLE_GRAD && !ui.soft[0], "an empty soft key: nothing, the menu closed");
    press(&a, "BLUE 2 k:0");
    expect(c.angle == AB_ANGLE_DEG, "DEG");

    /* other menus and the soft keys alone do nothing yet; nothing crashes on any key in any shift */
    ab_init(&c);
    app_init(&a, &c, now);
    for (int s = 0; s < 3; s++)
        for (int k = 0; k < KB_KEYS; k++) {
            if (s) tap(&a, shift_key(s));
            tap(&a, k);
            app_tick(&a, &in, now);
            run_out(&a, &in);
        }
    expect(1, "every key in every shift, once");

    printf("app_test: %d/%d checks\n", checks - bad, checks);
    return bad != 0;
}
