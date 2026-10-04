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
        expect(ui.soft[4] && !strcmp(ui.soft[4], "ALL") && ui.soft[5] && !strcmp(ui.soft[5], "CLVARx") &&
               ui.soft[1] && !strcmp(ui.soft[1], "CLSTK"), "GOLD \xe2\x86\x90: the CLEAR menu, ALL fifth and CLVARx sixth");
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
                const char *want = mn->paged ? (i < 5 ? ab_const_name(i) : "more") : mn->item[i].legend;
                ok = ok && ui.soft[i] && !strcmp(ui.soft[i], want);
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

    /* CONST pages: five constants and "more", wrapping after the last (41 constants: nine pages,
       the last holding one) */
    ab_init(&c);
    app_init(&a, &c, now);
    press(&a, "GOLD RCL k:5");
    app_ui(&a, &ui);
    expect(ui.soft[0] && !strcmp(ui.soft[0], ab_const_name(5)) && ui.soft[5] && !strcmp(ui.soft[5], "more"),
           "CONST more: the next five");
    for (int i = 0; i < 7; i++) press(&a, "k:5");
    app_ui(&a, &ui);
    expect(ab_const_count() == 41 && ui.soft[0] && !strcmp(ui.soft[0], ab_const_name(40)) && (!ui.soft[1] || !ui.soft[1][0]) &&
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
    expect(ui.soft[0] && !strcmp(ui.soft[0], ab_const_name(0)), "CONST wraps from the last page to the first");
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
