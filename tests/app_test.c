/* app_test: the device's behaviour from key presses (app.h), driven by legends the way a person
 * presses keys: "GOLD" "√x" is the gold shift then the square-root key. Results are read from
 * the core. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "keymap.h"
#include "const.h"
#include "state.h"

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

/* the state's identity: the checksum of its image (unit 016) without the image's own closing
   checksum, since a CRC-32 over data and its CRC is the same constant for every image */
static uint32_t img_crc(const ab_calc *c)
{
    static uint8_t img[1 << 18];
    size_t n = ab_state_save(c, img, sizeof img);
    return n > 4 && n <= sizeof img ? ab_crc32(img, n - 4) : 0;
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

    /* unit 021: UNDO, one level (ours): the state before the last key that reached the core; a
       second UNDO puts it back. Compared as whole state images. */
    {
        static uint8_t u0[1 << 18], u1[1 << 18];
        uint32_t before, after;
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "BLUE \xe2\x86\x90");
        app_undo_buffers(&a, u0, u1, sizeof u0);
        press(&a, "2 ENTER 3");
        before = img_crc(&c);
        press(&a, "+");
        after = img_crc(&c);
        expect(x_of(&c) == 5 && before != after, "2 ENTER 3 +");
        press(&a, "BLUE \xe2\x86\x90");
        expect(img_crc(&c) == before, "UNDO after +: the state before + back");
        press(&a, "BLUE \xe2\x86\x90");
        expect(img_crc(&c) == after && x_of(&c) == 5, "UNDO twice: + again (a redo)");

        /* a shift, a menu opening, a menu closed with C and VIEW are not undo points */
        press(&a, "7");
        before = img_crc(&c);
        press(&a, "STO ltr:A");
        after = img_crc(&c);
        press(&a, "GOLD GOLD BLUE GOLD \xe2\x86\x90 C GOLD STO ltr:A");
        press(&a, "BLUE \xe2\x86\x90");
        expect(img_crc(&c) == before && var_of(&c, 'A') == 0, "UNDO passes over shifts, a menu and VIEW to STO");

        /* UNDO after CLSTK (the CLEAR menu's second item) */
        press(&a, "1 ENTER 2 ENTER 3");
        before = img_crc(&c);
        press(&a, "GOLD \xe2\x86\x90 k:1");
        expect(x_of(&c) == 0 && img_crc(&c) != before, "CLEAR CLSTK");
        press(&a, "BLUE \xe2\x86\x90");
        expect(img_crc(&c) == before, "UNDO after CLSTK");

        /* CLEAR ALL asks first (UG p.1-5: CLR ALL? Y N); yes clears, and UNDO brings it all back */
        ab_init(&c);
        app_init(&a, &c, now);
        app_undo_buffers(&a, u0, u1, sizeof u0);
        press(&a, "4 STO ltr:A 9");
        before = img_crc(&c);
        press(&a, "GOLD \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(ui.soft[4] && !strcmp(ui.soft[4], "ALL") && ui.soft[5] && !strcmp(ui.soft[5], "VARx") &&
               ui.soft[1] && !strcmp(ui.soft[1], "STK"), "GOLD \xe2\x86\x90: the CLEAR menu, ALL fifth and VARx sixth (#3958)");
        press(&a, "k:4");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "CLR ALL?") && ui.soft[0] && !strcmp(ui.soft[0], "Y") &&
               ui.soft[1] && !strcmp(ui.soft[1], "N") && var_of(&c, 'A') == 4, "ALL: CLR ALL? with Y and N, nothing cleared yet");
        press(&a, "k:0");
        app_ui(&a, &ui);
        expect(var_of(&c, 'A') == 0 && !c.entry.active && !ui.prompt && !ui.soft[0], "Y: all cleared, the prompt gone");
        press(&a, "BLUE \xe2\x86\x90");
        expect(img_crc(&c) == before && var_of(&c, 'A') == 4, "UNDO after CLEAR ALL: everything back");
        press(&a, "GOLD \xe2\x86\x90 k:4 k:1");
        app_ui(&a, &ui);
        expect(img_crc(&c) == before && !ui.prompt && !ui.soft[0], "N: nothing cleared, the prompt gone");
        press(&a, "GOLD \xe2\x86\x90 k:4 5");
        app_ui(&a, &ui);
        expect(img_crc(&c) == before && !ui.prompt, "any other key cancels, and only cancels: 5 not typed");
        press(&a, "GOLD \xe2\x86\x90 k:4 BLUE");
        app_ui(&a, &ui);
        expect(img_crc(&c) == before && !ui.prompt && a.shift == 0, "a shift at the prompt only cancels it, unarmed");

        /* in program entry the menu offers PGM instead (UG p.1-5: CLR PGMS? Y N) */
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:A 2 BLUE XEQ");
        expect(c.prgm && c.prog_len == 3, "a program keyed");
        press(&a, "GOLD \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(ui.soft[4] && !strcmp(ui.soft[4], "PGM"), "in program entry F5 is PGM");
        press(&a, "k:4");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "CLR PGMS?") && c.prog_len == 3, "PGM: CLR PGMS?");
        press(&a, "k:0");
        expect(c.prog_len == 0 && c.prgm, "Y: the programs cleared, still in program entry");
        press(&a, "GOLD 9 k:0");

        /* UNDO after a short program: a run is one key */
        ab_init(&c);
        app_init(&a, &c, now);
        app_undo_buffers(&a, u0, u1, sizeof u0);
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:B 3 \xc3\x97 BLUE XEQ GOLD 9 k:0 5");
        before = img_crc(&c);
        press(&a, "XEQ ltr:B");
        run_out(&a, &in);
        expect(x_of(&c) == 15, "XEQ B: 5 times 3");
        press(&a, "BLUE \xe2\x86\x90");
        expect(img_crc(&c) == before, "UNDO after the run: the state before XEQ");

        /* a run over several slices is still one key (the program counts A down from 300: 600
           lines, three slices of APP_SLICE_LINES) */
        ab_init(&c);
        app_init(&a, &c, now);
        app_undo_buffers(&a, u0, u1, sizeof u0);
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:D GOLD 5 ltr:A GOLD 4 ltr:D BLUE XEQ GOLD 9 k:0 3 0 0 STO ltr:A 7");
        before = img_crc(&c);
        press(&a, "XEQ ltr:D");
        expect(a.running, "XEQ D runs on in slices");
        int ticks = 0;
        while (a.running && ticks < 100) {
            app_tick(&a, &in, now);
            ticks++;
        }
        expect(!a.running && ticks >= 2 && var_of(&c, 'A') == 0, "the countdown ran over several slices");
        press(&a, "BLUE \xe2\x86\x90");
        expect(img_crc(&c) == before, "UNDO after a run in slices: the state before XEQ");

        /* with no buffers UNDO does nothing */
        app_init(&a, &c, now);
        before = img_crc(&c);
        press(&a, "BLUE \xe2\x86\x90");
        expect(img_crc(&c) == before, "no undo buffers: UNDO does nothing");
    }

    /* the review of 021: on the 35s a key with a message showing only clears it (unit 017 rule 4),
       and that holds for a key that would open a menu or a prompt, so an op a menu confirms is
       never swallowed by the message */
    ab_init(&c);
    app_init(&a, &c, now);
    ab_do_arg(&c, AB_MODEL, 35);
    press(&a, "4 STO ltr:A 1 ENTER 0 \xc3\xb7");
    expect(c.msg, "35s: 1 / 0 leaves its message");
    press(&a, "GOLD \xe2\x86\x90");
    app_ui(&a, &ui);
    expect(!c.msg && !ui.soft[0], "35s: GOLD then the CLEAR key only clears the message");
    press(&a, "GOLD \xe2\x86\x90 k:4 k:0");
    expect(var_of(&c, 'A') == 0, "then CLEAR ALL clears");
    press(&a, "1 ENTER 0 \xc3\xb7 STO");
    app_ui(&a, &ui);
    expect(!c.msg && !ui.prompt, "35s: STO with a message showing only clears it");
    ab_do_arg(&c, AB_MODEL, 33);
    press(&a, "4 STO ltr:A 1 ENTER 0 \xc3\xb7 GOLD \xe2\x86\x90 k:4 k:0");
    expect(var_of(&c, 'A') == 0 && !c.msg, "33s: the message goes and CLEAR ALL acts");

    /* in Equation mode CLEAR's F5 is EQN (UG p.1-5: CLR EQN?) */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "5 STO ltr:A");
    ab_do(&c, AB_EQN);
    ab_do(&c, AB_KEQ);
    ab_do(&c, AB_ENTER);
    expect(c.eqn_mode && c.eqn_count == 3, "an equation keyed in Equation mode");
    press(&a, "GOLD \xe2\x86\x90");
    app_ui(&a, &ui);
    expect(ui.soft[4] && !strcmp(ui.soft[4], "EQN"), "in Equation mode F5 is EQN");
    press(&a, "k:4");
    app_ui(&a, &ui);
    expect(ui.prompt && !strcmp(ui.prompt, "CLR EQN?"), "EQN: CLR EQN?");
    press(&a, "k:0");
    expect(c.eqn_count == 2 && var_of(&c, 'A') == 5, "Y: the equations cleared, the built-in solvers and the data kept");

    /* unit 021: CLVARx asks for a three-digit address (UG p.1-5), from the digit keys */
    ab_init(&c);
    app_init(&a, &c, now);
    ab_do_arg(&c, AB_K5, 0);
    ab_do_arg(&c, AB_K0, 0);
    ab_do_arg(&c, AB_STO, 'I' - 'A');
    ab_do_arg(&c, AB_STO, AB_VAR_IND_I);
    ab_do_arg(&c, AB_K6, 0);
    ab_do_arg(&c, AB_K0, 0);
    ab_do_arg(&c, AB_STO, 'I' - 'A');
    ab_do_arg(&c, AB_STO, AB_VAR_IND_I);
    expect(c.ind_top == 60, "50 and 60 stored indirectly");
    press(&a, "GOLD \xe2\x86\x90 k:5");
    app_ui(&a, &ui);
    expect(ui.prompt && !strcmp(ui.prompt, "CLVAR___"), "CLVARx: CLVAR___");
    press(&a, "0");
    app_ui(&a, &ui);
    expect(ui.prompt && !strcmp(ui.prompt, "CLVAR0__"), "one digit: CLVAR0__");
    press(&a, "+ 5");
    app_ui(&a, &ui);
    expect(ui.prompt && !strcmp(ui.prompt, "CLVAR05_") && c.ind_top == 60, "another key does nothing; two digits");
    press(&a, "5");
    app_ui(&a, &ui);
    expect(!ui.prompt && c.ind_top == 50, "the third digit: CLVAR055 clears 60, keeps 50");
    press(&a, "GOLD \xe2\x86\x90 k:5 0 C 0");
    app_ui(&a, &ui);
    expect(!ui.prompt && c.ind_top == 50 && c.entry.active, "C cancels it; the next 0 is typed");

    /* unit 019: every menu on a key opens with its items on the soft keys (CONST shows its first
       five constants and "more") */
    for (int k = 0; k < KB_KEYS; k++)
        for (int sh = 1; sh < 3; sh++) {
            const km_action *act = km_lookup(k, sh);
            if (act->kind != KM_MENU || act->arg < 0) continue;
            const km_menu *mn = km_menu_at(act->arg);
            ab_init(&c);
            app_init(&a, &c, now);
            tap(&a, shift_key(sh));
            tap(&a, k);
            app_ui(&a, &ui);
            bool ok = true;
            for (int i = 0; i < 6; i++) {
                const char *want = mn->paged ? (i < 5 ? ab_const_symbol(i) : "more") : mn->item[i].legend;
                /* SETUP's OFF and VOL show their values after the legend (unit 022) */
                ok = ok && ui.soft[i] && !strncmp(ui.soft[i], want, strlen(want)) &&
                     (!strcmp(ui.soft[i], want) || mn->item[i].kind == KM_DEVICE);
            }
            char m[96];
            snprintf(m, sizeof m, "%s opens with its items on the soft keys", mn->name);
            expect(ok, m);
        }

    /* each soft key does its op: one per menu (work/019 acceptance) */
    {
        static const struct { const char *keys; double x; const char *what; } S[] = {
            {"1 BLUE LN k:0", 1.1752011936438014, "HYP SINH 1"},
            {"1 \xce\xa3+ 3 \xce\xa3+ BLUE \xce\xa3+ k:0", 2, "STAT x-bar of 1 and 3"},
            {"2 ENTER 1 \xce\xa3+ 4 ENTER 2 \xce\xa3+ BLUE + k:3", 2, "L.R. m of (1,2) (2,4)"},
            {"2 ENTER 1 \xce\xa3+ 4 ENTER 2 \xce\xa3+ GOLD + k:0", 2, "SUMS n"},
            {"5 ENTER 2 BLUE \xc3\x97 k:0", 10, "PROB Cn,r 5 2"},
            {"1 ENTER 2 GOLD 6 k:2 1 0", 10, "x?y: a test from the keys leaves the stack"},
            {"1 0 BLUE RCL k:4", 25.4, "CONV 10 in to cm"},
            {"1 GOLD 1 k:2", 1.609344, "UNITS 1 mile to km"},
            {"1 BLUE SIN k:4", 57.295779513082321, "ANGLE 1 rad to degrees"},
            {"5 BLUE R\xe2\x86\x93 k:0", 0, "STACK CLSTK"},
            {"2 . 5 BLUE y\xcb\xa3 k:0", 2, "POW IP 2.5"},
            {"GOLD RCL k:0", 299792458, "CONST c"},
        };
        for (size_t i = 0; i < sizeof S / sizeof S[0]; i++) {
            ab_init(&c);
            app_init(&a, &c, now);
            press(&a, S[i].keys);
            double x = x_of(&c), e = S[i].x == 0 ? 1e-12 : 1e-12 * (S[i].x < 0 ? -S[i].x : S[i].x);
            app_ui(&a, &ui);
            expect(x > S[i].x - e && x < S[i].x + e && !ui.soft[0], S[i].what);
        }
        /* the tests set YES / NO */
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "1 ENTER 2 GOLD 6 k:3");
        expect(c.yes, "x?y x>y? with y 1, x 2: yes");
        press(&a, "GOLD 6 k:2");
        expect(!c.yes, "x?y x<y?: no");
        press(&a, "5 BLUE 5 k:3");
        expect(c.yes, "x?0 x>0? of 5: yes");
        press(&a, "0 GOLD x\xe2\x86\x94y k:2");
        expect(c.yes, "TEST x=0? of 0: yes");
        /* FLAGS: SF, CF, FS? take a flag from the digit prompt; SF then .1 is flag 11 */
        press(&a, "BLUE 6 k:0 3");
        expect(ab_flag(&c, 3), "FLAGS SF 3");
        press(&a, "BLUE 6 k:1 3");
        expect(!ab_flag(&c, 3), "FLAGS CF 3");
        press(&a, "BLUE 6 k:0");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "SF _"), "SF _");
        press(&a, ". 1");
        expect(ab_flag(&c, 11), "SF then .1 sets flag 11");
        press(&a, "BLUE 6 k:2 . 1");
        expect(c.yes, "FS? 11: yes");
        press(&a, "GOLD x\xe2\x86\x94y k:4 . 1");
        expect(c.yes, "TEST's FS? 11: yes");
        /* BASE, and BASE > LOGIC > AND (12 AND 10 is 8) */
        press(&a, "BLUE x\xe2\x86\x94y k:1");
        expect(c.base == 16, "BASE HEX");
        press(&a, "BLUE x\xe2\x86\x94y k:0 1 2 ENTER 1 0 BLUE x\xe2\x86\x94y k:4");
        app_ui(&a, &ui);
        expect(c.base == 10 && ui.soft[0] && !strcmp(ui.soft[0], "AND") && ui.soft[5] && !strcmp(ui.soft[5], "NOR"),
               "BASE > LOGIC: the logic menu on the soft keys");
        press(&a, "C BLUE x\xe2\x86\x94y");
        app_ui(&a, &ui);
        expect(ui.soft_sub[4] && ui.soft_sub[5] && !ui.soft_sub[0] && !strcmp(ui.soft[4], "LOGIC") && !strcmp(ui.soft[5], "SUFX"),
               "BASE: LOGIC and SUFX boxed as submenus (#3958)");
        press(&a, "k:4");
        press(&a, "k:0");
        expect(x_of(&c) == 8, "BASE > LOGIC > AND: 12 AND 10 is 8");
        press(&a, "BLUE x\xe2\x86\x94y k:5");
        app_ui(&a, &ui);
        expect(ui.soft[1] && !strcmp(ui.soft[1], "h"), "BASE > SUFFIX: d h o b");
        press(&a, "C");
        /* CMPLX: i between the parts */
        press(&a, "1 BLUE +/\xe2\x88\x92 k:0 2 ENTER");
        expect(c.x.kind == AB_COMPLEX, "CMPLX i: 1 i 2 is complex");
        /* VARS x<> takes a variable */
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "7 BLUE STO k:0");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "x<> _"), "VARS x<>: x<> _");
        press(&a, "ltr:A");
        expect(var_of(&c, 'A') == 7 && x_of(&c) == 0, "VARS x<> A");
        /* FRAC's three formats, each both flags (rule 2) */
        press(&a, "BLUE E k:5");
        expect(c.flag8 && c.flag9, "FRAC fixed: flags 8 and 9 set");
        press(&a, "BLUE E k:3");
        expect(!c.flag8, "FRAC precise: flag 8 clear");
        press(&a, "BLUE E k:5 BLUE E k:4");
        expect(c.flag8 && !c.flag9, "FRAC factors: 8 set, 9 clear");
        /* MODE: both items, and the one in effect marked */
        press(&a, "BLUE ENTER k:1");
        expect(c.m35, "MODE 35s");
        press(&a, "BLUE ENTER");
        app_ui(&a, &ui);
        expect(ui.soft_on[1] && !ui.soft_on[0], "MODE: 35s marked in effect");
        press(&a, "k:0");
        expect(!c.m35, "MODE 33s");
        press(&a, "BLUE ENTER");
        app_ui(&a, &ui);
        expect(ui.soft_on[0] && !ui.soft_on[1], "MODE: 33s marked in effect");
        press(&a, "C");
        app_ui(&a, &ui);
        expect(!ui.soft_on[0] && !ui.soft[0], "no menu, no mark");
    }

    /* abacus #3958: EXP (blue eˣ) as drafted */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "1 0 BLUE e\xcb\xa3 k:0");
    expect(x_of(&c) == 1024, "EXP 2^x of 10");
    press(&a, "BLUE e\xcb\xa3 k:1");
    expect(x_of(&c) == 10, "EXP LOG2 of 1024");

    /* CONST pages: five constants and "more", wrapping after the last (41 constants: nine pages,
       the last holding one) */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "GOLD RCL k:5");
    app_ui(&a, &ui);
    expect(ui.soft[0] && !strcmp(ui.soft[0], ab_const_symbol(5)) && !strcmp(ab_const_symbol(5), "R\xe2\x88\x9e") &&
           ui.soft[5] && !strcmp(ui.soft[5], "more"), "CONST more: the next five, by symbol (R\xe2\x88\x9e, #3958)");
    for (int i = 0; i < 7; i++) press(&a, "k:5");
    app_ui(&a, &ui);
    expect(ab_const_count() == 41 && ui.soft[0] && !strcmp(ui.soft[0], ab_const_symbol(40)) && (!ui.soft[1] || !ui.soft[1][0]) &&
               ui.soft[5] && !strcmp(ui.soft[5], "more"), "CONST's last page: the 41st alone");
    press(&a, "k:0");
    {
        uint32_t before = img_crc(&c);
        abn want;
        ab_calc t;
        ab_init(&t);
        ab_do_arg(&t, AB_CONST, 40);
        want = t.x.re;
        expect(abn_cmp(&c.x.re, &want) == 0 && before == img_crc(&c), "the last page's constant recalled");
    }
    press(&a, "GOLD RCL");
    for (int i = 0; i < 9; i++) press(&a, "k:5");
    app_ui(&a, &ui);
    expect(ui.soft[0] && !strcmp(ui.soft[0], ab_const_symbol(0)), "CONST wraps from the last page to the first");
    press(&a, "k:1");
    {
        ab_calc t;
        ab_init(&t);
        ab_do_arg(&t, AB_CONST, 1);
        expect(abn_cmp(&c.x.re, &t.x.re) == 0, "after wrapping, soft key 2 recalls the second constant");
    }
    press(&a, "GOLD RCL");
    for (int i = 0; i < 8; i++) press(&a, "k:5");
    press(&a, "k:2");
    app_ui(&a, &ui);
    expect(!ui.soft[0], "an empty soft key on the last page closes it");

    /* C closes a menu and does nothing else */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "4");
    {
        uint32_t before = img_crc(&c);
        press(&a, "BLUE LN C");
        app_ui(&a, &ui);
        expect(!ui.soft[0] && img_crc(&c) == before && c.entry.active, "C closes HYP, the 4 still being typed");
    }

    /* SOLVE (gold 7) and integrate (gold 8) ask for the variable (rule 4) */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "GOLD 7");
    app_ui(&a, &ui);
    expect(ui.prompt && !strcmp(ui.prompt, "SOLVE _"), "SOLVE _");
    press(&a, "C GOLD 8");
    app_ui(&a, &ui);
    expect(ui.prompt && !strcmp(ui.prompt, "\xe2\x88\xab" "FN d _"), "integrate: \xe2\x88\xab" "FN d _");
    press(&a, "C");

    /* unit 020: in Equation mode the soft keys are the editor's bar (the list's, or typing's) */
    {
        static const char *const LIST[6] = {"\xe2\x96\xb2", "\xe2\x96\xbc", "EDIT", "NEW", NULL, "EXIT"};
        static const char *const TYPE[6] = {"\xe2\x97\x80", "\xe2\x96\xb6", "( )", "=", "SPC", "REG"};   /* REG boxed (#3958) */
        bool ok;
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "GOLD 0");
        app_ui(&a, &ui);
        ok = c.eqn_mode;
        for (int i = 0; i < 6; i++) ok = ok && (LIST[i] ? ui.soft[i] && !strcmp(ui.soft[i], LIST[i]) : !ui.soft[i] || !ui.soft[i][0]);
        expect(ok, "EQN: the list's bar, \xe2\x96\xb2 \xe2\x96\xbc EDIT NEW - EXIT");
        /* the guide's first equation, key by key (p.6-2; unit 014's M01-M04) */
        press(&a, "RCL ltr:V");
        app_ui(&a, &ui);
        ok = c.eqn_typing;
        for (int i = 0; i < 6; i++) ok = ok && ui.soft[i] && !strcmp(ui.soft[i], TYPE[i]);
        expect(ok && ui.soft_sub[5] && !ui.soft_sub[4], "typing: the bar \xe2\x97\x80 \xe2\x96\xb6 ( ) = SPC REG, REG boxed as a submenu");
        press(&a, "k:3 . 2 5 \xc3\x97 GOLD E \xc3\x97 RCL ltr:D y\xcb\xa3 2 \xc3\x97 RCL ltr:L ENTER");
        expect(!c.eqn_typing && c.eqn_cur >= 0 && !strcmp(c.eqn[c.eqn_cur], "V=0.25\xc3\x97\xcf\x80\xc3\x97" "D^2\xc3\x97L"),
               "V=0.25\xc3\x97\xcf\x80\xc3\x97" "D^2\xc3\x97L typed from the keys, = from the bar");
        press(&a, "ENTER");
        expect(c.stop == AB_STOP_INPUT, "ENTER evaluates: the prompt for D");
        press(&a, "2 . 5 R/S 1 6 R/S");
        expect(x_of(&c) > 78.539816339744 && x_of(&c) < 78.539816339745 && var_of(&c, 'V') > 78.5398,
               "D 2.5, L 16: V = 78.5398..., stored (unit 014's M04)");

        /* the list wraps through EQN LIST TOP (p.6-7); NEW goes to TOP; EDIT on a built-in does nothing */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_eqn_add(&c, "A+1");                  /* (which turns Equation mode on) */
        int n = c.eqn_count, top = c.eqn_cur;
        press(&a, "k:3");
        expect(c.eqn_cur == -1, "NEW: EQN LIST TOP");
        press(&a, "k:0");
        expect(c.eqn_cur == n - 1, "\xe2\x96\xb2 from TOP: the last equation");
        press(&a, "k:1");
        expect(c.eqn_cur == -1, "\xe2\x96\xbc from the last: TOP");
        press(&a, "k:1");
        expect(c.eqn_cur == 0, "\xe2\x96\xbc from TOP: the first");
        int b = -1;
        for (int i = 0; i < n; i++)
            if (c.eqn[i][0] == '\x01' && b < 0) b = i;
        for (int i = 0; i < n + 2 && c.eqn_cur != b; i++) press(&a, "k:1");
        expect(c.eqn_cur == b, "\xe2\x96\xbc reaches a built-in solver");
        press(&a, "k:2");
        expect(!c.eqn_typing, "EDIT on a built-in solver does nothing (p.6-8)");
        (void)top;

        /* EDIT, then the cursor and backspace fix a typing mistake, and the equation evaluates */
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "4 STO ltr:A GOLD 0 RCL ltr:A + 2 3");
        press(&a, "k:0 \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(!strcmp(c.ebuf, "A+3"), "\xe2\x97\x80 then \xe2\x86\x90 deletes the 2 before the cursor: A+3");
        press(&a, "ENTER XEQ R/S");
        expect(x_of(&c) == 7, "A+3 with A = 4: 7");
        press(&a, "GOLD 0 k:2");                /* the evaluation left Equation mode (unit 014): back to it */
        expect(c.eqn_typing && c.ecur == 0, "EDIT: editing at the start of the equation (p.6-7)");
        press(&a, "k:1 k:1 \xe2\x86\x90 C");
        expect(!c.eqn_mode && !strcmp(c.eqn[c.eqn_cur], "A3"), "C in the middle of typing stores the edit and leaves");
        app_ui(&a, &ui);
        expect(!ui.soft[0], "after C the bar is gone");

        /* REG > REGY types REGY, and the typing bar comes back */
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "GOLD 0 RCL ltr:A + k:5");
        app_ui(&a, &ui);
        expect(ui.soft[1] && !strcmp(ui.soft[1], "REGY") && ui.soft[3] && !strcmp(ui.soft[3], "REGT"), "REG\xe2\x96\xb8: REGX REGY REGZ REGT");
        press(&a, "k:1");
        app_ui(&a, &ui);
        expect(strstr(c.ebuf, "REGY") != NULL && ui.soft[0] && !strcmp(ui.soft[0], TYPE[0]), "REGY typed; the typing bar back");

        /* RCL types a variable, (I) included; SPC; a function key types its name; a menu's item types too */
        int ki = -1;
        for (int k = 0; k < KB_KEYS; k++)
            if (km_letter_of(k)->kind == KL_IND_I) ki = k;
        char keys_[48];
        snprintf(keys_, sizeof keys_, "\xc3\x97 RCL k:%d k:4 SIN", ki);
        press(&a, keys_);
        /* (I) is the guide's single glyph in an equation (p.14-23; the core keeps it as U+24A4) */
        expect(strstr(c.ebuf, "\xe2\x92\xa4") != NULL && strstr(c.ebuf, " ") != NULL && strstr(c.ebuf, "SIN(") != NULL,
               "RCL (I) types (I), SPC a blank, SIN its name");
        press(&a, "BLUE LN k:0");
        expect(strstr(c.ebuf, "SINH(") != NULL, "HYP's SINH types SINH( (p.6-4)");
        press(&a, "k:2");
        expect(strstr(c.ebuf, "()") != NULL || strstr(c.ebuf, "(") != NULL, "( ) types a pair");

        /* EXIT leaves with the list unchanged */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_eqn_add(&c, "B+2");
        press(&a, "k:1 k:1");
        int count = c.eqn_count;
        press(&a, "k:5");
        app_ui(&a, &ui);
        expect(!c.eqn_mode && c.eqn_count == count && !ui.soft[0], "EXIT: Equation mode off, the list as it was");
    }

    /* unit 022: RADIX and SETUP, the device's settings */
    {
        /* each RADIX choice changes the number as the screen formats it (the guide's p.1-24 example) */
        static const struct { const char *keys; const char *want; } R[] = {
            {"GOLD 3 k:2", "12,345,678.9"},             /* 1,000: the guide's mark */
            {"GOLD 3 k:1", "12.345.678,9"},             /* , radix: the mark becomes . (p.1-24) */
            {"GOLD 3 k:4", "12345678,9"},               /* none */
            {"GOLD 3 k:3", "12 345 678,9"},             /* 1 000: a space (ours) */
            {"GOLD 3 k:0", "12 345 678.9"},             /* . radix again, the space kept */
        };
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "1 2 3 4 5 6 7 8 . 9 ENTER");
        {   /* the default groups, as both machines do (abacus #3965; UG p.1-23, p.B-3) */
            app_ui(&a, &ui);
            ab_fmt f = {AB_ALL, 0, 40, ui.radix, ui.separator};
            char t[48];
            ab_format(&c.x.re, &f, t, sizeof t);
            expect(!strcmp(t, "12,345,678.9"), "a new state groups by default: 12,345,678.9");
        }
        for (size_t i = 0; i < sizeof R / sizeof R[0]; i++) {
            press(&a, R[i].keys);
            app_ui(&a, &ui);
            ab_fmt f = {AB_ALL, 0, 40, ui.radix, ui.separator};
            char t[48];
            ab_format(&c.x.re, &f, t, sizeof t);
            char m[96];
            snprintf(m, sizeof m, "%s: %s (got %s)", R[i].keys, R[i].want, t);
            expect(!strcmp(t, R[i].want), m);
        }
        press(&a, "GOLD 3");
        app_ui(&a, &ui);
        expect(ui.soft_on[0] && !ui.soft_on[1] && ui.soft_on[3] && !ui.soft_on[2] && !ui.soft_on[4],
               "RADIX marks the mark and the grouping in effect");
        press(&a, "C");
        /* SETUP: OFF cycles 2, 5, 10, 30 and wraps; the idle time follows */
        expect(a.set.off_min == 10, "OFF starts at 10 minutes");
        static const int OFF[] = {30, 2, 5, 10};
        bool ok = true;
        for (int i = 0; i < 4; i++) {
            press(&a, "BLUE 3 k:0");
            ok = ok && a.set.off_min == OFF[i] && a.pwr.idle_ms == OFF[i] * 60000u;
        }
        expect(ok, "OFF: 30, 2, 5, 10, wrapping, the idle time with it");
        press(&a, "BLUE 3");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "OFF10") && ui.soft[1] && !strcmp(ui.soft[1], "VOL 4") &&
               ui.soft[2] && !strcmp(ui.soft[2], "BEEP") && ui.soft_on[2], "SETUP shows OFF10, VOL 4, BEEP on");
        press(&a, "C");
        /* VOL cycles the five levels, sounding each */
        ok = true;
        for (int i = 0; i < 5; i++) {
            a.tone = -1;
            press(&a, "BLUE 3 k:1");
            ok = ok && a.set.vol == (unsigned)i && a.tone == i;
        }
        expect(ok, "VOL: 0, 1, 2, 3, 4, a tone at each new level");
        /* the error tone, only with BEEP on */
        a.tone = -1;
        press(&a, "1 ENTER 0 \xc3\xb7");
        expect(a.tone == 4, "an error sounds at the level set");
        press(&a, "C BLUE 3 k:2");
        expect(!a.set.beep, "BEEP off");
        a.tone = -1;
        press(&a, "1 ENTER 0 \xc3\xb7");
        expect(a.tone == -1, "BEEP off: no error tone");
        press(&a, "C");
        /* CLEAR ALL keeps them, and a settings change is saved */
        press(&a, "GOLD 3 k:1 GOLD \xe2\x86\x90 k:4 k:0");
        expect(a.set.radix == ',' && !a.set.beep && a.set.off_min == 10, "CLEAR ALL keeps the settings");
        app_saved(&a);
        press(&a, "GOLD 3 k:0");
        expect(a.dirty, "a settings change is saved (dirty)");
    }

    /* abacus #3962: in HEX the unshifted keys 6-11 type A-F (UG p.11-1's shortcuts); shifted they keep
       their functions */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "BLUE x\xe2\x86\x94y k:1 \xe2\x88\x9ax ENTER e\xcb\xa3 +");
    expect(c.base == 16 && x_of(&c) == 21, "HEX: A ENTER B + is 21 (15 hex)");
    press(&a, "C 3 GOLD \xe2\x88\x9ax");
    expect(x_of(&c) == 9, "HEX: gold on the A key is still x squared");
    press(&a, "C LN y\xcb\xa3 1/x \xce\xa3+");
    expect(x_of(&c) == 0xCDEF, "HEX: C D E F from LN, y^x, 1/x, Sigma+");
    press(&a, "BLUE x\xe2\x86\x94y k:0 C 4 \xe2\x88\x9ax");
    expect(c.base == 10 && x_of(&c) == 2, "DEC again: the key is the square root");

    /* abacus #3962: PSE pauses about 1 s, then the program goes on by itself (UG p.13-19); C or R/S in the
       pause stops it, and R/S then resumes */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "GOLD 9 k:0 GOLD XEQ ltr:P 5 GOLD R/S 6 BLUE XEQ GOLD 9 k:0 XEQ ltr:P");
    expect(c.stop == AB_STOP_PSE && x_of(&c) == 5, "XEQ P: the pause, showing 5");
    app_tick(&a, &in, now + 500);
    expect(c.stop == AB_STOP_PSE, "half a second on: still pausing");
    run_out(&a, &in);
    app_tick(&a, &in, now + 1100);
    run_out(&a, &in);
    expect(c.stop == AB_STOP_DONE && x_of(&c) == 6, "after the pause the program goes on to its end");
    press(&a, "XEQ ltr:P");
    expect(c.stop == AB_STOP_PSE, "again: the pause");
    press(&a, "C");
    app_tick(&a, &in, now + 3000);
    expect(c.stop == AB_STOP_PSE && x_of(&c) == 5, "C in the pause stops it: no going on");
    press(&a, "R/S");
    run_out(&a, &in);
    expect(c.stop == AB_STOP_DONE && x_of(&c) == 6, "R/S then resumes it");

    /* abacus #3971 (ruling #3828): with a menu open a shift arms, switches or cancels and the menu stays;
       a soft key then acts and clears the shift; any other key closes the menu and acts in the shift */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "1 BLUE LN GOLD");
    app_ui(&a, &ui);
    expect(a.shift == 1 && ui.soft[0] && !strcmp(ui.soft[0], "SINH"), "HYP open, gold: armed, the menu stays");
    press(&a, "BLUE");
    app_ui(&a, &ui);
    expect(a.shift == 2 && ui.soft[0] && !strcmp(ui.soft[0], "SINH"), "then blue: switched, the menu stays");
    press(&a, "BLUE");
    app_ui(&a, &ui);
    expect(a.shift == 0 && ui.soft[0] && !strcmp(ui.soft[0], "SINH"), "blue again: cancelled, the menu stays");
    press(&a, "GOLD k:0");
    app_ui(&a, &ui);
    expect(a.shift == 0 && !ui.soft[0] && x_of(&c) > 1.1752 && x_of(&c) < 1.1753, "gold, then a soft key: SINH acts, the shift cleared");
    press(&a, "C 3 BLUE LN GOLD \xe2\x88\x9ax");
    app_ui(&a, &ui);
    expect(a.shift == 0 && !ui.soft[0] && x_of(&c) == 9, "gold, then another key: the menu closes and x squared acts");

    /* abacus #3971: MEMORY CLEAR restores the core's run limits, so a calloc'd calculator runs programs */
    {
        static ab_calc z;
        memset(&z, 0, sizeof z);
        ab_memory_clear(&z);
        ab_do(&z, AB_PRGM);
        ab_do_arg(&z, AB_LBL, 0);
        for (int i = 0; i < 40; i++) ab_do(&z, AB_K1);      /* a few lines of work */
        ab_do(&z, AB_RTN);
        ab_do(&z, AB_PRGM);
        ab_do_arg(&z, AB_XEQ, 0);
        while (z.stop == AB_STOP_SLICE) ab_continue(&z);
        expect(z.stop == AB_STOP_DONE, "calloc then MEMORY CLEAR: a program runs to its end, no RUN LIMIT");
    }

    /* abacus #3979: a keyboard VIEW shows Variable=Value on the X line (UG p.3-4), the stack untouched;
       C or backspace only clears it (p.3-6); any other key clears it and acts */
    {
        static screen_page pg;
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "5 STO ltr:A 7 GOLD STO ltr:A");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(pg.x.kind == SCREEN_VIEW && !strcmp(pg.x.text, "A=5") && !pg.x.label && x_of(&c) == 7,
               "VIEW A: A=5 on the X line, X still 7");
        press(&a, "C");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(pg.x.kind != SCREEN_VIEW && x_of(&c) == 7 && !c.entry.active, "C only clears the VIEW display: X 7");
        press(&a, "GOLD STO ltr:A \xe2\x86\x90");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(pg.x.kind != SCREEN_VIEW && x_of(&c) == 7, "backspace only clears it too: X 7, not cleared");
        press(&a, "GOLD STO ltr:A +");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(pg.x.kind != SCREEN_VIEW && x_of(&c) == 12, "VIEW then +: the display goes and + adds (5 + 7)");
        /* VIEW (I) names the variable it points at */
        press(&a, "C 3 STO ltr:I 9 STO k:39 GOLD STO k:39");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(pg.x.kind == SCREEN_VIEW && !strcmp(pg.x.text, "(3)=9"), "VIEW (I) with I = 3: (3)=9");
        press(&a, "C");
    }

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
