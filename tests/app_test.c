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
#include "eqn.h"

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

/* unit 027: the app's clock for a one-call SOLVE's length, and SYS's facts */
static uint32_t fake_ms;
static uint32_t fake_clock(void *ctx)
{
    (void)ctx;
    return fake_ms;
}

static int64_t rtc_set = -1;
static void fake_set_wall(void *ctx, int64_t secs)
{
    (void)ctx;
    rtc_set = secs;
}

static void fake_sysinfo(void *ctx, app_sysinfo *o)
{
    (void)ctx;
    memset(o, 0, sizeof *o);
    o->soc = 87;
    o->chg_n = false;           /* charging */
    o->pgood_n = false;
    o->vbat_uv = 3920000;
    o->version = "0.27";
    o->commit = "abc1234";
    o->built = "Oct  6 2026";
    o->flash_free = 98304;
    o->heap_free = 66771;
    o->heap_largest = 31744;
}

/* the soft key whose label is l, -1 none */
static int soft_of(const screen_ui *ui, const char *l)
{
    for (int i = 0; i < SCREEN_SOFT; i++)
        if (ui->soft[i] && !strcmp(ui->soft[i], l)) return i;
    return -1;
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
        bool l1 = !strcmp(t, "LBL A \xc2\xb7" "33");   /* stamped with the mode it was entered in (unit 025) */
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
    ab_do_arg(&c, AB_MODEL, 35);                /* the unnamed registers are 35s mode's (unit 025) */
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
                if (!strcmp(mn->name, "MODE") && i == 4) {     /* ALG: hidden outside STU (unit 029) */
                    ok = ok && (!ui.soft[i] || !*ui.soft[i]);
                    continue;
                }
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
        ab_do_arg(&c, AB_MODEL, 35);            /* (I) below is 35s mode's key (unit 025) */
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

        /* 33s mode: RCL i types i, RCL (i) the glyph; i is a variable, prompted for, (i) is not
           (33s p.13-23; unit 025) */
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "5 STO ltr:A 1 STO k:39 GOLD 0 RCL k:39 + RCL k:43");
        expect(!strcmp(c.ebuf, "i+\xe2\x92\xa4"), "33s: RCL i types i, RCL (i) types (i)");
        press(&a, "ENTER XEQ");
        expect(c.stop == AB_STOP_INPUT && c.stop_var == 26, "33s: XEQ prompts for i, not for (i)");
        press(&a, "R/S");
        expect(x_of(&c) == 6, "i+(i) with i = 1, A = 5: 6");

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
               ui.soft[2] && !strcmp(ui.soft[2], "BEEP") && ui.soft_sub[2], "SETUP shows OFF10, VOL 4, BEEP> (unit 027: its uses)");
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
        press(&a, "C BLUE 3 k:2 k:5 k:1 C");      /* unit 027: BEEP>'s second page, ALL */
        expect(a.set.uses == 0, "BEEP off");
        a.tone = -1;
        press(&a, "1 ENTER 0 \xc3\xb7");
        expect(a.tone == -1, "BEEP off: no error tone");
        press(&a, "C");
        /* CLEAR ALL keeps them, and a settings change is saved */
        press(&a, "GOLD 3 k:1 GOLD \xe2\x86\x90 k:4 k:0");
        expect(a.set.radix == ',' && a.set.uses == 0 && a.set.off_min == 10, "CLEAR ALL keeps the settings");
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
        press(&a, "C");
        ab_do_arg(&c, AB_MODEL, 35);            /* the unnamed registers are 35s mode's (unit 025) */
        press(&a, "3 STO ltr:I 9 STO k:39 GOLD STO k:39");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(pg.x.kind == SCREEN_VIEW && !strcmp(pg.x.text, "(3)=9"), "VIEW (I) with I = 3: (3)=9");
        press(&a, "C");
        ab_do_arg(&c, AB_MODEL, 33);
        press(&a, "3 STO k:39 8 STO k:43 GOLD STO k:43");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(pg.x.kind == SCREEN_VIEW && !strcmp(pg.x.text, "C=8"), "33s: VIEW (i) with i = 3 names C: C=8");
        press(&a, "C GOLD STO k:39");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(pg.x.kind == SCREEN_VIEW && !strcmp(pg.x.text, "i=3"), "33s: VIEW i: i=3");
        press(&a, "C");
    }

    /* unit 023: app_next_wake, each deadline */
    {
        pwr_inputs bat = {true, true, 80, false};
        ab_init(&c);
        app_init(&a, &c, now);
        expect(app_next_wake(&a, &bat, now) == PWR_IDLE_MS, "a fresh app: only the auto-off, 10 minutes");
        press(&a, "7");
        expect(app_next_wake(&a, &bat, now) == 0, "a key: a render is due now");
        app_tick(&a, &bat, now);                /* the render */
        expect(app_next_wake(&a, &bat, now + 1000) == APP_QUIET_MS - 1000, "dirty: the quiet save, 29 s on");
        expect(app_tick(&a, &bat, now + APP_QUIET_MS) == APP_SAVE, "at that time the save is due");
        app_saved(&a);
        static const int OFF[] = {30, 2, 5, 10};
        for (int i = 0; i < 4; i++) {
            press(&a, "BLUE 3 k:0");
            app_tick(&a, &bat, now);
            app_saved(&a);
            char m[80];
            snprintf(m, sizeof m, "OFF %d: the auto-off %d minutes on", OFF[i], OFF[i]);
            expect(app_next_wake(&a, &bat, now) == OFF[i] * 60000u, m);
        }
        pwr_inputs low = {true, true, PWR_LOW_SOC - 1, false};
        expect(app_next_wake(&a, &low, now) == 0, "a low battery with no input: off is due now");
        /* a running program: 0; its PSE: the rest of the pause */
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:P 5 GOLD R/S 6 BLUE XEQ GOLD 9 k:0");
        run_out(&a, &in);
        press(&a, "XEQ ltr:P");
        app_tick(&a, &bat, now);                /* the render */
        expect(a.pause && app_next_wake(&a, &bat, now + 300) == APP_PSE_MS - 300, "PSE: the rest of the pause, 700 ms");
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:D GOLD 5 ltr:A GOLD 4 ltr:D BLUE XEQ GOLD 9 k:0 3 0 0 STO ltr:A XEQ ltr:D");
        expect(a.running && app_next_wake(&a, &bat, now) == 0, "a program running in slices: 0");
        run_out(&a, &in);
    }

    /* unit 023: the contract, by property. Random keys and time jumps; at a random t before the
       deadline app_tick returns APP_IDLE and changes nothing (the app's state and the core's image),
       and at the deadline it acts */
    {
        static const char *const KEYS[] = {"1", "2", "ENTER", "+", "\xc3\x97", "C", "STO ltr:A", "XEQ ltr:P", "XEQ ltr:D",
                                           "BLUE 3 k:0", "GOLD 9 k:0", "BLUE LN", "k:0", "BLUE \xe2\x86\x90", "R/S"};
        static app_state before;
        unsigned seed = 2026;
        int checked = 0, acted = 0, broken = 0;
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:P 5 GOLD R/S 6 BLUE XEQ GOLD XEQ ltr:D GOLD 5 ltr:A GOLD 4 ltr:D BLUE XEQ GOLD 9 k:0 4 0 STO ltr:A");
        app_set_wall(&a, (int64_t)APP_CLOCK_KEYS_DATE + 12 * 3600 + 59, now);    /* unit 027: the clock set */
        for (int step = 0; step < 6000; step++) {
            seed = seed * 1103515245u + 12345u;
            static const int SOC[] = {PWR_LOW_SOC - 1, 70, APP_BATT_LOW_SOC, APP_BATT_LOW_SOC + 2, APP_BATT_OK_SOC + 1};
            pwr_inputs pin = {true, ((seed >> 3) & 7) != 0, ((seed >> 6) & 31) == 0 ? SOC[(seed >> 11) % 5] : 70, false};
            seed = seed * 1103515245u + 12345u;
            if ((seed >> 9) % 3 == 0) {
                press(&a, KEYS[(seed >> 12) % (sizeof KEYS / sizeof KEYS[0])]);
            } else {
                seed = seed * 1103515245u + 12345u;
                now += (seed >> 10) % 4 == 0 ? (seed >> 12) % 3000000u : (seed >> 12) % 2000u;
            }
            uint32_t w = app_next_wake(&a, &pin, now);
            if (w > 0 && w != UINT32_MAX) {
                seed = seed * 1103515245u + 12345u;
                uint32_t t = now + ((seed >> 8) & 1 ? w - 1 : (seed >> 9) % w);
                uint32_t crc = img_crc(&c);
                before = a;
                app_action r = app_tick(&a, &pin, t);
                checked++;
                if (r != APP_IDLE || memcmp(&before, &a, sizeof a) != 0 || img_crc(&c) != crc) {
                    if (broken++ < 3) printf("app_test: step %d: app_tick at %u ms before the deadline (%u) acted (%d)\n",
                                             step, (unsigned)(t - now), (unsigned)w, (int)r);
                }
            }
            if (w != UINT32_MAX) {
                app_action r = app_tick(&a, &pin, now + w);
                acted += r != APP_IDLE;
                if (r == APP_IDLE && broken++ < 3) printf("app_test: step %d: idle at the deadline (%u)\n", step, (unsigned)w);
                now += w;
                if (r == APP_SAVE) app_saved(&a);
                if (r == APP_OFF) {             /* power off, and on again */
                    ab_init(&c);
                    app_init(&a, &c, now);
                    press(&a, "GOLD 9 k:0 GOLD XEQ ltr:P 5 GOLD R/S 6 BLUE XEQ GOLD XEQ ltr:D GOLD 5 ltr:A GOLD 4 ltr:D BLUE XEQ GOLD 9 k:0 4 0 STO ltr:A");
                }
            }
        }
        char m[120];
        snprintf(m, sizeof m, "app_next_wake's contract: %d checks before deadlines, %d actions at them", checked, acted);
        expect(broken == 0 && checked > 500 && acted > 500, m);
    }

    /* unit 024 rule 3: a base result beyond the word shows OVERFLOW briefly, then the largest number
       (33s p.10-3: "OVERFLOW to be briefly displayed"); 1 s, or until the next key */
    {
        pwr_inputs bat = {true, true, 80, false};
        static screen_page pg;
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "BLUE x\xe2\x86\x94y k:1 7");
        for (int i = 0; i < 8; i++) press(&a, "\xce\xa3+");     /* the HEX F key (Sigma+) */
        press(&a, "ENTER 1 +");
        uint32_t t0 = now;
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(ui.notice && !strcmp(ui.notice, "OVERFLOW") && pg.x.kind == SCREEN_MESSAGE && !strcmp(pg.x.text, "OVERFLOW"),
               "HEX 7FFFFFFFF + 1: OVERFLOW on the X line");
        expect(x_of(&c) == 34359738367.0, "the largest number kept in X");
        app_tick(&a, &bat, t0);
        expect(app_next_wake(&a, &bat, t0 + 400) == APP_NOTICE_MS - 400, "the notice is a deadline: 600 ms left");
        expect(app_tick(&a, &bat, t0 + APP_NOTICE_MS - 1) == APP_IDLE && a.notice, "1 ms before: still shown");
        expect(app_tick(&a, &bat, t0 + APP_NOTICE_MS) == APP_RENDER && !a.notice, "after 1 s it goes (a render)");
        press(&a, "ENTER 1 +");
        press(&a, "C");
        expect(!a.notice, "a key clears it at once");
    }

    /* unit 029: STU and its ALG on the device: MODE's keys, the line's bar, ANS's key, RPN ONLY, the screen */
    {
        static screen_page pg;
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "BLUE ENTER");
        app_ui(&a, &ui);
        expect(ui.soft[2] && !strcmp(ui.soft[2], "STU") && ui.soft[3] && !strcmp(ui.soft[3], "RPN") && ui.soft_on[3] &&
                   (!ui.soft[4] || !*ui.soft[4]),
               "029 MODE in 33s: STU, RPN marked, ALG blank");
        press(&a, "k:2");
        expect(c.stu && c.m35, "029: STU chosen, behaving as 35s");
        press(&a, "BLUE ENTER");
        app_ui(&a, &ui);
        expect(ui.soft_on[2] && ui.soft[4] && !strcmp(ui.soft[4], "ALG") && !ui.soft_on[0] && !ui.soft_on[1],
               "029 MODE in STU: STU marked, ALG offered");
        press(&a, "k:4");
        app_ui(&a, &ui);
        expect(c.alg && ui.soft[0] && !strcmp(ui.soft[0], "\xe2\x97\x80") && ui.soft[1] && !strcmp(ui.soft[1], "\xe2\x96\xb6") &&
                   ui.soft[2] && !strcmp(ui.soft[2], "( )") && ui.soft[4] && !strcmp(ui.soft[4], "\xe2\x96\xb2") &&
                   ui.soft[5] && !strcmp(ui.soft[5], "\xe2\x96\xbc"),
               "029 ALG: the line's bar, left right ( ) up down");
        press(&a, "2 + 3 ENTER");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(pg.y.text, "2+3") && pg.y.left && strstr(pg.x.text, "5") && !pg.x.left,
               "029: the line in Y's place, the result right in X's");
        expect(strstr(pg.status.text, "STU") && strstr(pg.status.text, "ALG"), "029: the status line shows STU and ALG");
        press(&a, "GOLD ENTER");                /* LASTx's key types ANS (proposal 1) */
        expect(!strcmp(c.ebuf, "ANS"), "029: LASTx's key types ANS");
        press(&a, "C");
        press(&a, "R\xe2\x86\x93");
        app_ui(&a, &ui);
        expect(ui.notice && !strcmp(ui.notice, "RPN ONLY") && !c.msg, "029: R-down shows RPN ONLY briefly, not a message");
        press(&a, "k:4");                       /* up: the history */
        expect(c.hist_at == 0, "029: the bar's up shows the last entry");
        press(&a, "C BLUE ENTER k:3");          /* back to RPN */
        expect(!c.alg && x_of(&c) == 5, "029: ALG to RPN puts ANS in X");
        /* a label written in STU lists its stamp */
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:A GOLD 9 k:0");
        char t[32];
        ab_prog_line(&c, 0, NULL, t, sizeof t);
        expect(!strcmp(t, "LBL A \xc2\xb7STU"), "029: a label written in STU lists LBL A \xc2\xb7STU");
    }
    /* unit 030: SHOW, STACK's F5 held: X's 34 digits, 17 on Y's line and 17 on X's; changes nothing;
       its release ends it; without a release the next key only ends it; a complex X on all four lines */
    {
        static screen_page pg;
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "BLUE R\xe2\x86\x93");
        app_ui(&a, &ui);
        expect(ui.soft[4] && !strcmp(ui.soft[4], "SHOW"), "030: STACK's F5 is SHOW");
        press(&a, "C 2 \xe2\x88\x9ax");
        uint32_t before = img_crc(&c);
        a.dirty = false;
        press(&a, "BLUE R\xe2\x86\x93 k:4");
        expect(!a.dirty, "030: SHOW leaves nothing to save");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(ui.show_n == 1 && !strcmp(pg.y.text, "14142135623730950") && !strcmp(pg.x.text, "48801688724209698") &&
                   pg.y.left && pg.x.left && !pg.x.label,
               "030: SHOW held: 17 digits on Y's line, 17 on X's, no letters");
        expect(img_crc(&c) == before, "030: SHOW changes nothing");
        app_key_up(&a, 7, now);
        app_ui(&a, &ui);
        expect(ui.show_n == 1, "030: another key's release leaves it");
        app_key_up(&a, 4, now);
        app_ui(&a, &ui);
        expect(ui.show_n == 0 && !a.show, "030: its release ends it");
        press(&a, "BLUE R\xe2\x86\x93 k:4");
        app_ui(&a, &ui);
        expect(ui.show_n == 1, "030: SHOW again");
        press(&a, "5");                         /* the console: no release; the next key only ends it */
        app_ui(&a, &ui);
        expect(ui.show_n == 0 && !c.entry.active && img_crc(&c) == before, "030: without a release the next key only ends it");
        a.key_ups = true;                       /* Rev A's matrix: the finger rolls on from SHOW (review C1) */
        press(&a, "BLUE R\xe2\x86\x93 k:4 5");
        app_ui(&a, &ui);
        expect(ui.show_n == 0 && c.entry.active, "030: with releases, a key while SHOW is held ends it and acts");
        a.key_ups = false;
        press(&a, "C");
        press(&a, "C 3 BLUE +/\xe2\x88\x92 k:0 4 ENTER BLUE R\xe2\x86\x93 k:4");    /* 3 i 4 (CMPLX's i, blue +/-) */
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(ui.show_n == 2 && !strcmp(pg.t.text, "30000000000000000") && !strcmp(pg.z.text, "00000000000000000") &&
                   !strcmp(pg.y.text, "40000000000000000") && !strcmp(pg.x.text, "00000000000000000"),
               "030: a complex X: the real part on T and Z, the imaginary on Y and X");
        app_key_up(&a, 4, now);
    }

    /* unit 032: TABLE: blue 8's menu (VAR START STEP GO); the table on the four lines, x left and the
       value right, row k on X; the bar's up and down; ENTER takes the row; the status band says TABLE;
       STU ONLY briefly in 35s mode */
    {
        static screen_page pg;
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "BLUE 8");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "VAR") && ui.soft[1] && !strcmp(ui.soft[1], "START") && ui.soft[2] &&
                   !strcmp(ui.soft[2], "STEP") && ui.soft[3] && !strcmp(ui.soft[3], "GO"),
               "032: blue 8 is TABLE: VAR START STEP GO");
        press(&a, "C");
        ab_eqn_add(&c, "X^2");
        ab_do(&c, AB_EQN);                      /* out of Equation mode, X^2 last shown */
        press(&a, "3 STO ltr:X");
        c.stu = true;                           /* STU (MODE's keys tested elsewhere) */
        press(&a, "BLUE 8 k:3");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(c.tbl_on && strstr(pg.x.text, "3.0000") && strstr(pg.x.text, "9.0000") && strstr(pg.t.text, "0.0000"),
               "032: the table: row 0 (3, 9) on X, row -3 (0, 0) on T");
        expect(strstr(pg.status.text, "TABLE") != NULL, "032: the status band says TABLE");
        expect(ui.soft[0] && !strcmp(ui.soft[0], "\xe2\x96\xb2") && ui.soft[1] && !strcmp(ui.soft[1], "\xe2\x96\xbc"),
               "032: the table's bar: up, down");
        press(&a, "k:1");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(c.tbl_k == 1 && strstr(pg.x.text, "16.0000"), "032: down: row 1 (4, 16) on X");
        press(&a, "ENTER");
        expect(!c.tbl_on && x_of(&c) == 16, "032: ENTER takes the row's value into X");
        c.stu = false;
        c.m35 = true;
        press(&a, "BLUE 8 k:3");
        app_ui(&a, &ui);
        expect(!c.tbl_on && ui.notice && !strcmp(ui.notice, "STU ONLY") && !c.msg, "032: 35s mode: STU ONLY briefly");
    }

    /* unit 031 rule 3, answer C: a chain shown under the other grouping is marked with the model it was
       typed in; under its own grouping, or with no chain, no mark */
    {
        static screen_page pg;
        char t[64];
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c, "2^3^2");
        c.stu = false;                          /* (MODE's keys do nothing in Equation mode: set directly) */
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(pg.x.text, "2^3^2 \xc2\xb7STU"), "031: an STU chain shown in 35s mode: 2^3^2 \xc2\xb7STU");
        c.stu = true;
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(pg.x.text, "2^3^2"), "031: shown in STU: no mark");
        c.stu = false;
        ab_eqn_add(&c, "2^3");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(pg.x.text, "2^3"), "031: no chain, no mark");
        ab_eqn_add(&c, "4^3^2");
        c.stu = true;
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(pg.x.text, "4^3^2 \xc2\xb7" "35"), "031: a 35s chain shown in STU: 4^3^2 \xc2\xb7" "35");
        /* a program's equation line, listed */
        ab_init(&c);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 0);
        ab_eqn_add(&c, "2^3^2");
        c.stu = c.m35 = false;                  /* 33s mode (MODE is not a program line: set directly) */
        ab_prog_line(&c, 1, NULL, t, sizeof t);
        expect(!strcmp(t, "2^3^2 \xc2\xb7STU"), "031: an STU program line listed in 33s mode is marked");
        /* rule 6 in the parser itself, on text not yet written with its x: an implied product ranks
           exactly as a typed x (every stored text has its x written first: rule 7) */
        {
            ab_val v;
            int pos;
            abn two = abn_from_int(2), eighteen = abn_from_int(18);
            ab_do(&c, AB_PRGM);                 /* (out of program entry) */
            ab_do(&c, AB_K4);
            ab_do_arg(&c, AB_STO, AB_VAR_A);
            expect(eqn_eval(&c, "1\xc3\xb7" "2A", AB_MODEL_STU, false, false, &v, &pos) == ABN_OK && v.kind == AB_REAL &&
                       abn_cmp(&v.re, &two) == 0,
                   "031 rule 6: 1/2A read as (1/2)xA = 2 (A = 4)");
            ab_do(&c, AB_K3);
            ab_do_arg(&c, AB_STO, AB_VAR_A);
            expect(eqn_eval(&c, "2A^2", AB_MODEL_STU, false, false, &v, &pos) == ABN_OK && abn_cmp(&v.re, &eighteen) == 0,
                   "031 rule 6: 2A^2 read as 2x(A^2) = 18 (A = 3)");
        }
    }

    /* unit 030's review: an equation line typed in program entry is shown as it is typed (UG p.13-7),
       not the program line before it */
    {
        static screen_page pg;
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 0);
        ab_do(&c, AB_K1);
        ab_do(&c, AB_EQN);
        ab_do(&c, AB_K2);
        ab_do(&c, AB_ADD);
        ab_do(&c, AB_K3);
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(c.eqn_typing && !strcmp(pg.x.text, "2+3_") && pg.x.left, "program entry: the equation line being typed is shown");
    }

    /* unit 029, ours: ALG's hidden key does nothing outside STU; XEQ is refused before its prompt;
       UNDO's image leaves the line's history (RAM, not state) */
    {
        static uint8_t u0[1 << 18], u1[1 << 18];
        ab_init(&c);
        app_init(&a, &c, now);
        app_undo_buffers(&a, u0, u1, sizeof u0);
        press(&a, "BLUE ENTER k:4");
        expect(!c.alg && !c.msg && !a.notice, "029: ALG's hidden key does nothing outside STU");
        press(&a, "C BLUE ENTER k:2 BLUE ENTER k:4 XEQ");
        app_ui(&a, &ui);
        expect(c.alg && ui.notice && !strcmp(ui.notice, "RPN ONLY") && !ui.prompt, "029: XEQ shows RPN ONLY, no prompt");
        press(&a, "1 ENTER 2 ENTER");
        expect(c.hist_n == 2, "029: two lines in the history");
        press(&a, "BLUE \xe2\x86\x90");
        expect(c.alg && c.hist_n == 2 && !strcmp(c.ebuf, "2") && !c.alg_shown,
               "029: UNDO puts the line back before its ENTER; the history stays");
        /* (review) the complex display form is a setting in ALG; a key with no place on the line is
           RPN ONLY, the line kept */
        ab_do_arg(&c, AB_RTA, 0);
        expect(c.alg && c.cpolar && !strcmp(c.ebuf, "2"), "029: r-theta-a acts in ALG, the line kept");
        press(&a, "C 2 + 3 ENTER");
        ab_do_arg(&c, AB_TOPOL, 0);
        app_ui(&a, &ui);
        expect(!strcmp(c.ebuf, "2+3") && c.alg_shown && !c.msg, "029: ->POL after a result: the line and its result kept");
        /* (review) an ALG image loaded into a calc not from ab_init (all zero): no entry selected */
        {
            static ab_calc z;
            static uint8_t img[1 << 16];
            size_t n = ab_state_save(&c, img, sizeof img);
            memset(&z, 0, sizeof z);
            expect(n > 0 && ab_state_load(&z, img, n) == ABN_OK && z.alg && z.hist_at == -1,
                   "029: an ALG image into a zeroed calc: hist_at -1");
        }
    }

    /* unit 027: MODE, SETUP, SYS each one job; the buzzer's six uses; the clock; BEEP */
    {
        pwr_inputs ok = {false, false, 80, false};
        char k_[16];
        ab_init(&c);
        app_init(&a, &c, now);
        /* MODE: 33s 35s, F3 STU (unit 029; blank in 027), F4 RPN marked, F5 blank outside STU */
        press(&a, "BLUE ENTER");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "33s") && ui.soft[2] && !strcmp(ui.soft[2], "STU") && ui.soft[3] &&
                   !strcmp(ui.soft[3], "RPN") && ui.soft_on[3] && (!ui.soft[4] || !*ui.soft[4]),
               "027 MODE: 33s 35s, STU (029), RPN marked, blank");
        press(&a, "C");
        /* SETUP: OFF VOL BEEP> CLOCK>, the two submenus boxed */
        press(&a, "BLUE 3");
        app_ui(&a, &ui);
        expect(ui.soft[2] && !strcmp(ui.soft[2], "BEEP") && ui.soft_sub[2] && ui.soft[3] && !strcmp(ui.soft[3], "CLOCK") &&
                   ui.soft_sub[3],
               "027 SETUP: OFF VOL BEEP> CLOCK>");
        /* BEEP>: CLICK ERROR PRGM BATT ON >, all on; each toggles and shows it, the menu staying */
        press(&a, "k:2");
        app_ui(&a, &ui);
        expect(soft_of(&ui, "CLICK") == 0 && soft_of(&ui, "ERROR") == 1 && soft_of(&ui, "PRGM") == 2 &&
                   soft_of(&ui, "BATT") == 3 && soft_of(&ui, "ON") == 4 && ui.soft_sub[5],
               "027 BEEP>: CLICK ERROR PRGM BATT ON >");
        bool all_on = true;
        for (int i = 0; i < 5; i++) all_on = all_on && ui.soft_on[i];
        expect(all_on && a.set.uses == DEV_USE_ALL, "027: every use on by default");
        static const int USE1[5] = {DEV_USE_CLICK, DEV_USE_ERROR, DEV_USE_PRGM, DEV_USE_BATT, DEV_USE_ON};
        bool toggles = true;
        for (int i = 0; i < 5; i++) {
            snprintf(k_, sizeof k_, "k:%d", i);
            press(&a, k_);
            app_ui(&a, &ui);
            toggles = toggles && !(a.set.uses & USE1[i]) && !ui.soft_on[i] && a.menu >= 0;
            press(&a, k_);
            app_ui(&a, &ui);
            toggles = toggles && (a.set.uses & USE1[i]) && ui.soft_on[i];
        }
        expect(toggles, "027: each of CLICK ERROR PRGM BATT ON toggles, shows it, and the menu stays");
        press(&a, "k:5");                       /* the next page: DONE ALL */
        app_ui(&a, &ui);
        int kd = soft_of(&ui, "DONE"), ka = soft_of(&ui, "ALL");
        expect(kd >= 0 && ka >= 0 && ui.soft_on[kd], "027 BEEP> page 2: DONE (on) ALL");
        snprintf(k_, sizeof k_, "k:%d", kd);
        press(&a, k_);
        expect(!(a.set.uses & DEV_USE_DONE), "027: DONE toggles off");
        snprintf(k_, sizeof k_, "k:%d", ka);
        press(&a, k_);
        expect(a.set.uses == 0, "027: ALL with any on turns every use off");
        press(&a, k_);
        expect(a.set.uses == DEV_USE_ALL, "027: ALL with none on turns every use on");
        press(&a, "C");
        /* CLOCK>: SET and 12/24; never set: --:-- with no deadline of its own */
        app_ui(&a, &ui);
        expect(!strcmp(ui.clock, "--:--") && !ui.ampm, "027: the clock never set shows --:--");
        int64_t w;
        expect(!app_wall(&a, now, &w), "027: app_wall: unset");
        app_set_wall(&a, 1000, now);            /* an RTC that lost its cell boots near 1970 */
        app_tick(&a, &ok, now);
        app_ui(&a, &ui);
        expect(!app_wall(&a, now, &w) && !strcmp(ui.clock, "--:--"), "027: an RTC time before 2025 leaves the clock unset");
        app_saved(&a);
        app_tick(&a, &ok, now);
        uint32_t idle_wake = app_next_wake(&a, &ok, now);
        a.set_wall = fake_set_wall;
        press(&a, "BLUE 3 k:3");
        app_ui(&a, &ui);
        expect(soft_of(&ui, "SET") == 0 && soft_of(&ui, "12/24") == 1 && !ui.soft_on[1], "027 CLOCK>: SET, 12/24 (12)");
        press(&a, "k:0");
        app_ui(&a, &ui);
        expect(ui.prompt && strstr(ui.prompt, "SET") != NULL, "027 SET: a time prompt");
        press(&a, "1 3 0 5");
        app_ui(&a, &ui);
        expect(app_wall(&a, now, &w) && w % 86400 == 13 * 3600 + 5 * 60 && w >= APP_CLOCK_UNSET_S && rtc_set == w,
               "027 SET 13:05: the wall clock at 13:05:00 (2026-01-01), the RTC told");
        expect(!strcmp(ui.clock, "1:05") && ui.ampm == 'p', "027: 12-hour form 1:05 p");
        uint32_t t1 = now;
        app_saved(&a);
        app_tick(&a, &ok, t1);
        expect(app_next_wake(&a, &ok, t1) <= 60000u && app_next_wake(&a, &ok, t1) < idle_wake,
               "027: the minute's change is a deadline in app_next_wake");
        app_set_wall(&a, (int64_t)APP_CLOCK_KEYS_DATE + 13 * 3600 + 5 * 60 + 15, t1);
        app_saved(&a);                          /* no quiet save due, so the minute is the nearest */
        app_tick(&a, &ok, t1);                  /* the render app_set_wall asked for */
        expect(app_next_wake(&a, &ok, t1) == 45000u, "027: at 13:05:15 the next wake is in 45 s");
        app_tick(&a, &ok, t1 + 45000);
        app_ui(&a, &ui);
        expect(!strcmp(ui.clock, "1:06"), "027: at the minute the clock reads 1:06");
        app_set_wall(&a, (int64_t)APP_CLOCK_KEYS_DATE + 5 * 60, now);
        app_ui(&a, &ui);
        expect(!strcmp(ui.clock, "12:05") && ui.ampm == 'a', "027: 00:05 is 12:05 a");
        app_set_wall(&a, (int64_t)APP_CLOCK_KEYS_DATE + 12 * 3600 + 30 * 60, now);
        app_ui(&a, &ui);
        expect(!strcmp(ui.clock, "12:30") && ui.ampm == 'p', "027: 12:30 is 12:30 p");
        press(&a, "BLUE 3 k:3 k:1");
        app_set_wall(&a, (int64_t)APP_CLOCK_KEYS_DATE + 13 * 3600 + 5 * 60, now);
        app_ui(&a, &ui);
        expect(a.set.h24 && !strcmp(ui.clock, "13:05") && !ui.ampm, "027: 12/24 to 24-hour: 13:05");
        press(&a, "BLUE 3 k:3 k:1");
        expect(!a.set.h24, "027: and back to 12");
        /* SYS: BATT VER FLASH, each a view any key leaves */
        a.sysinfo = fake_sysinfo;
        press(&a, "BLUE C");
        app_ui(&a, &ui);
        expect(soft_of(&ui, "BATT") == 0 && soft_of(&ui, "VER") == 1 && soft_of(&ui, "FLASH") == 2, "027 SYS: BATT VER FLASH");
        press(&a, "k:0");
        app_ui(&a, &ui);
        bool batt = false, chg = false, volt = false;
        for (int i = 0; i < 4; i++)
            if (ui.view[i]) {
                batt = batt || strstr(ui.view[i], "87%");
                chg = chg || strstr(ui.view[i], "CHG");
                volt = volt || strstr(ui.view[i], "3.92");
            }
        expect(a.view == APP_VIEW_BATT && batt && chg && volt, "027 SYS BATT: 87%, charging, 3.92 V");
        double x0 = x_of(&c);
        press(&a, "7");
        app_ui(&a, &ui);
        expect(a.view == APP_VIEW_NONE && !ui.view[0] && x_of(&c) == x0 && !c.entry.active, "027: any key leaves the view, and only that");
        press(&a, "BLUE C k:1");
        app_ui(&a, &ui);
        bool ver = false, com = false;
        for (int i = 0; i < 4; i++)
            if (ui.view[i]) {
                ver = ver || strstr(ui.view[i], "0.27");
                com = com || strstr(ui.view[i], "abc1234");
            }
        expect(a.view == APP_VIEW_VER && ver && com, "027 SYS VER: version and commit");
        press(&a, "C BLUE C k:2");
        app_ui(&a, &ui);
        bool fl = false, heap = false;
        for (int i = 0; i < 4; i++)
            if (ui.view[i]) {
                fl = fl || strstr(ui.view[i], "98304");
                heap = heap || strstr(ui.view[i], "31744");
            }
        expect(a.view == APP_VIEW_FLASH && fl && heap, "027 SYS FLASH: free storage and the heap's largest block");
        press(&a, "C");

        /* the uses, each exactly when it should */
        ab_init(&c);
        app_init(&a, &c, now);
        a.tone = -1;
        press(&a, "5");
        expect(a.tone == a.set.vol && a.tone_use == DEV_USE_CLICK, "027 CLICK: a key ticks");
        a.set.uses &= (uint8_t)~DEV_USE_CLICK;
        a.tone = -1;
        press(&a, "6");
        expect(a.tone == -1, "027 CLICK off: no tick");
        press(&a, "ENTER 0 \xc3\xb7");
        expect(a.tone_use == DEV_USE_ERROR && a.tone == a.set.vol, "027 ERROR: the error tone");
        press(&a, "C");
        a.set.uses &= (uint8_t)~DEV_USE_ERROR;
        a.tone = -1;
        press(&a, "1 ENTER 0 \xc3\xb7");
        expect(a.tone == -1, "027 ERROR off: none");
        press(&a, "C");
        /* PRGM: a program's BEEP; BEEP typed from FLAGS (F4) in program entry */
        ab_init(&c);
        app_init(&a, &c, now);
        a.set.uses = DEV_USE_PRGM;
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:A BLUE 6");
        app_ui(&a, &ui);
        expect(ui.soft[3] && !strcmp(ui.soft[3], "BEEP"), "027: FLAGS F4 is BEEP");
        press(&a, "k:3 BLUE XEQ GOLD 9 k:0");
        char lt[32];
        ab_prog_line(&c, 1, NULL, lt, sizeof lt);
        expect(c.prog_len == 3 && !strcmp(lt, "BEEP"), "027: BEEP keyed into a program, listed BEEP");
        a.tone = -1;
        press(&a, "XEQ ltr:A");
        run_out(&a, &in);
        expect(a.tone_use == DEV_USE_PRGM && a.tone == a.set.vol && c.beeps == 1, "027 PRGM: the program's BEEP sounds");
        a.set.uses = 0;
        a.tone = -1;
        press(&a, "XEQ ltr:A");
        run_out(&a, &in);
        expect(a.tone == -1 && c.beeps == 2, "027 PRGM off: the BEEP is counted, not sounded");
        /* BATT: once when the charge first reads low, again only after it recovers */
        a.set.uses = DEV_USE_BATT;
        pwr_inputs low = {true, true, APP_BATT_LOW_SOC, false}, hi = {true, true, 50, false};
        a.tone = -1;
        app_tick(&a, &hi, now += 10);
        expect(a.tone == -1, "027 BATT: no tone at 50%");
        app_tick(&a, &low, now += 10);
        int t_low = a.tone_use;
        a.tone = -1;
        app_tick(&a, &low, now += 10);
        app_tick(&a, &low, now += 10);
        expect(t_low == DEV_USE_BATT && a.tone == -1, "027 BATT: one tone when it first reads low, not per tick");
        app_tick(&a, &hi, now += 10);
        app_tick(&a, &low, now += 10);
        expect(a.tone_use == DEV_USE_BATT && a.tone >= 0, "027 BATT: again after the charge recovered");
        /* abacus #4238: the device restores the latch after a wake from OFF: still low, no second tone */
        app_init(&a, &c, now);
        a.set.uses = DEV_USE_BATT;
        a.batt_low = true;
        a.tone = -1;
        app_tick(&a, &low, now += 10);
        expect(a.tone == -1 && a.batt_low, "027 BATT: the latch kept through OFF: no tone at the wake while still low");
        /* ON: a wake from OFF chirps; a tick (a light-sleep wake) does not */
        a.set.uses = DEV_USE_ON;
        a.tone = -1;
        app_tick(&a, &hi, now += 10);
        expect(a.tone == -1, "027 ON: no chirp on a light-sleep wake");
        app_powered_on(&a);
        expect(a.tone_use == DEV_USE_ON && a.tone == a.set.vol, "027 ON: the chirp at power-on");
        /* DONE: a run longer than 2 s, not 1.9 s; a sliced program and a one-call SOLVE */
        ab_init(&c);
        app_init(&a, &c, now);
        a.set.uses = DEV_USE_DONE;
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 0);
        ab_do(&c, AB_K1);
        ab_do(&c, AB_ADD);
        ab_do_arg(&c, AB_GTO, 0 * 1000 + 1);
        ab_do(&c, AB_PRGM);
        {   /* abacus #4238: a run the user stops (R/S after 3 s) sounds no DONE */
            a.tone = -1;
            press(&a, "XEQ ltr:A");
            uint32_t t0 = now;
            while (a.running && now - t0 < 3000) app_tick(&a, &ok, now += 50);
            press(&a, "R/S");
            run_out(&a, &in);
            app_tick(&a, &ok, now);
            expect(!a.running && a.tone == -1, "027 DONE: a 3 s run stopped with R/S: no tone (abacus #4238)");
            /* and the device's interrupt (its key task) counts the same */
            a.tone = -1;
            press(&a, "XEQ ltr:A");
            t0 = now;
            while (a.running && now - t0 < 3000) app_tick(&a, &ok, now += 50);
            app_interrupt(&a);
            run_out(&a, &in);
            app_tick(&a, &ok, now);
            expect(!a.running && a.tone == -1, "027 DONE: a 3 s run stopped by app_interrupt: no tone");
        }
        {   /* C in a PSE pause that ends a 3 s run: no tone either */
            ab_init(&c);
            app_init(&a, &c, now);
            a.set.uses = DEV_USE_DONE;
            ab_do(&c, AB_PRGM);
            ab_do_arg(&c, AB_LBL, 0);           /* A: 3000 STO C, LBL B, DSE C, GTO B, PSE, RTN */
            ab_do(&c, AB_K3);
            ab_do(&c, AB_K0);
            ab_do(&c, AB_K0);
            ab_do(&c, AB_K0);
            ab_do_arg(&c, AB_STO, 2);
            ab_do_arg(&c, AB_LBL, 1);
            ab_do_arg(&c, AB_DSE, 2);
            ab_do_arg(&c, AB_GTO, 1 * 1000 + 1);
            ab_do(&c, AB_PSE);
            ab_do(&c, AB_RTN);
            ab_do(&c, AB_PRGM);
            a.tone = -1;
            press(&a, "XEQ ltr:A");
            while (a.running) app_tick(&a, &ok, now += 100);
            bool paused = a.pause;
            press(&a, "C");
            app_tick(&a, &ok, now += 10);
            expect(paused && !a.pause && a.tone == -1, "027 DONE: C in the PSE pause after 3 s: no tone (abacus #4238)");
        }
        a.clock_ms = fake_clock;
        ab_init(&c);
        app_init(&a, &c, now);
        a.set.uses = DEV_USE_DONE;
        a.clock_ms = fake_clock;
        ab_eqn_add(&c, "X^2-2");
        fake_ms = now + 3000;                   /* the SOLVE "took" 3 s by the device's clock */
        a.tone = -1;
        press(&a, "1 STO ltr:X 2 GOLD 0 GOLD 7 ltr:X");
        expect(a.tone >= 0 && a.tone_use == DEV_USE_DONE, "027 DONE: a one-call SOLVE of 3 s");
        /* VOL 0 silences every use */
        ab_init(&c);
        app_init(&a, &c, now);
        a.set.vol = 0;
        a.tone = -1;
        press(&a, "5 ENTER 0 \xc3\xb7");
        app_powered_on(&a);
        expect(a.tone == -1, "027: VOL 0 silences every use");
        press(&a, "C");

        /* review: UNDO after a BEEP sounds nothing (the count is not state) */
        {
            static uint8_t u0[1 << 18], u1[1 << 18];
            ab_init(&c);
            app_init(&a, &c, now);
            app_undo_buffers(&a, u0, u1, sizeof u0);
            a.set.uses = DEV_USE_PRGM;
            press(&a, "BLUE 6 k:3");            /* FLAGS' BEEP from the keys */
            expect(a.tone_use == DEV_USE_PRGM && c.beeps == 1, "027: BEEP from the keys sounds");
            a.tone = -1;
            press(&a, "BLUE \xe2\x86\x90");     /* UNDO */
            app_tick(&a, &ok, now += 10);
            expect(a.tone == -1, "027 review: UNDO after a BEEP sounds nothing");
        }
        /* review: a sliced run that ends by itself after more than 2 s sounds DONE; one whose last
           slice held a long SOLVE is timed by the device's clock; an error's tone wins over DONE */
        {
            ab_init(&c);
            app_init(&a, &c, now);
            a.set.uses = DEV_USE_DONE | DEV_USE_ERROR;
            ab_do(&c, AB_PRGM);
            ab_do_arg(&c, AB_LBL, 0);           /* A: 3000 STO C, LBL B, DSE C, GTO B, RTN */
            ab_do(&c, AB_K3);
            ab_do(&c, AB_K0);
            ab_do(&c, AB_K0);
            ab_do(&c, AB_K0);
            ab_do_arg(&c, AB_STO, 2);
            ab_do_arg(&c, AB_LBL, 1);
            ab_do_arg(&c, AB_DSE, 2);
            ab_do_arg(&c, AB_GTO, 1 * 1000 + 1);
            ab_do(&c, AB_RTN);
            ab_do(&c, AB_PRGM);
            a.tone = -1;
            press(&a, "XEQ ltr:A");
            uint32_t t0 = now;
            while (a.running) app_tick(&a, &ok, now += 50);
            expect(now - t0 >= 2000 && a.tone >= 0 && a.tone_use == DEV_USE_DONE, "027 review: a sliced run ending by itself after 2 s: DONE");
            a.tone = -1;
            press(&a, "1 0 STO ltr:C XEQ ltr:A");
            run_out(&a, &in);
            app_tick(&a, &ok, now);
            expect(a.tone == -1, "027 review: the same run of a few slices: no DONE");
            /* a natural end at 1.9 s and at 2.0 s by the device's clock */
            a.clock_ms = fake_clock;
            for (int run = 0; run < 2; run++) {
                press(&a, "4 0 0 STO ltr:C XEQ ltr:A");
                fake_ms = now + (run == 0 ? 1900 : 2000);
                a.tone = -1;
                while (a.running) app_tick(&a, &ok, now += 1);
                if (run == 0) expect(a.tone == -1, "027 DONE: a run ending by itself at 1.9 s: no tone");
                else expect(a.tone >= 0 && a.tone_use == DEV_USE_DONE, "027 DONE: a run ending by itself at 2 s: the tone");
            }
            /* the last slice "takes" 5 s by the device's clock */
            press(&a, "4 0 0 STO ltr:C XEQ ltr:A");
            fake_ms = now + 5000;
            a.tone = -1;
            while (a.running) app_tick(&a, &ok, now += 1);
            expect(a.tone >= 0 && a.tone_use == DEV_USE_DONE, "027 review: a run timed by the device's clock at its end (5 s): DONE");
            a.clock_ms = NULL;
            /* a long run ending in an error: the error's tone */
            ab_init(&c);
            app_init(&a, &c, now);
            a.set.uses = DEV_USE_DONE | DEV_USE_ERROR;
            ab_do(&c, AB_PRGM);
            ab_do_arg(&c, AB_LBL, 0);
            ab_do(&c, AB_K3);
            ab_do(&c, AB_K0);
            ab_do(&c, AB_K0);
            ab_do(&c, AB_K0);
            ab_do_arg(&c, AB_STO, 2);
            ab_do_arg(&c, AB_LBL, 1);
            ab_do_arg(&c, AB_DSE, 2);
            ab_do_arg(&c, AB_GTO, 1 * 1000 + 1);
            ab_do(&c, AB_K0);
            ab_do(&c, AB_INV);                  /* 1/0: an error at the end */
            ab_do(&c, AB_PRGM);
            a.tone = -1;
            press(&a, "XEQ ltr:A");
            while (a.running) app_tick(&a, &ok, now += 50);
            expect(c.msg && a.tone >= 0 && a.tone_use == DEV_USE_ERROR, "027 review: a long run ending in an error sounds the error, not DONE");
            press(&a, "C");
        }
        /* review: the clock through 26 days of minute ticks (the ms clock's int32 half is 24.8 days) */
        {
            ab_init(&c);
            app_init(&a, &c, now);
            app_set_wall(&a, (int64_t)APP_CLOCK_KEYS_DATE, now);
            uint32_t t = now;
            bool mono = true;
            int64_t prev = 0, w2;
            for (int m = 0; m < 26 * 24 * 60; m++) {
                t += 60000u;                    /* a tick each minute (auto-off aside) */
                app_tick(&a, &ok, t);
                mono = mono && app_wall(&a, t, &w2) && w2 > prev;
                prev = w2;
            }
            expect(mono && w2 == (int64_t)APP_CLOCK_KEYS_DATE + 26 * 86400, "027 review: 26 days of minutes, the clock never going back");
            /* review: a minute passed between the last tick and app_next_wake is due at once */
            app_set_wall(&a, (int64_t)APP_CLOCK_KEYS_DATE + 12 * 3600, t);
            app_tick(&a, &ok, t);
            app_tick(&a, &ok, t + 59950);
            expect(app_next_wake(&a, &ok, t + 60150) == 0, "027 review: the minute already past is due now, not in 59.85 s");
        }
    }

    /* unit 026: an overflow is the largest number with the OVERFLOW notice, not an error (33s p.1-16,
       F-3); in a run the notice comes when it ends (p.13-9), and flag 5's stop holds the message */
    {
        static const ab_op OVF[] = {AB_LBL, AB_K1, AB_KE, AB_K6, AB_K0, AB_K0, AB_K0, AB_ENTER, AB_MUL, AB_K5, AB_RTN};
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "1 E 6 0 0 0 ENTER \xc3\x97");
        app_ui(&a, &ui);
        expect(ui.notice && !strcmp(ui.notice, "OVERFLOW") && !c.msg && ab_flag(&c, 6),
               "1E6000 squared from the keys: the OVERFLOW notice, no message, flag 6 set");
        press(&a, "C 1 E +/\xe2\x88\x92 6 0 0 0 ENTER \xc3\x97");
        app_ui(&a, &ui);
        expect(!ui.notice && !c.msg && x_of(&c) == 0, "an underflow: 0, no notice");
        /* a program that overflows, run in slices of 2 lines: the notice when the run ends */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do(&c, AB_PRGM);
        for (size_t i = 0; i < sizeof OVF / sizeof OVF[0]; i++) ab_do_arg(&c, OVF[i], 0);
        ab_do(&c, AB_PRGM);
        c.slice = 2;
        press(&a, "XEQ ltr:A");
        bool mid = a.running && !a.notice;
        run_out(&a, &in);
        app_ui(&a, &ui);
        expect(mid && !a.running && ui.notice && !strcmp(ui.notice, "OVERFLOW") && x_of(&c) == 5,
               "an overflow in a sliced run: no notice while it runs, OVERFLOW when it ends (33s p.13-9)");
        /* flag 5 set: the run stops at the line after with OVERFLOW held as a message, no notice */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do(&c, AB_PRGM);
        for (size_t i = 0; i < sizeof OVF / sizeof OVF[0]; i++) ab_do_arg(&c, OVF[i], 0);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_SF, 5);
        press(&a, "XEQ ltr:A");
        run_out(&a, &in);
        app_ui(&a, &ui);
        const char *m = ab_error_message(&c);
        expect(c.stop == AB_STOP_STOP && c.msg && m && !strcmp(m, "OVERFLOW") && !ui.notice,
               "flag 5: stopped with OVERFLOW held as a message, not a brief notice (33s p.F-3)");
    }

    /* unit 025: the 33s's i and (i) from the keys, labels stamped with their mode, MODE restamping */
    {
        char t[48];
        ab_init(&c);
        app_init(&a, &c, now);
        expect(!c.m35, "the default is 33s mode (decision 22)");
        press(&a, "5 STO k:39");
        expect(abn_cmp(&c.var[26].re, &c.x.re) == 0 && var_of(&c, 'I') == 0, "33s: STO then the (I) key stores in i, not I");
        press(&a, "1 STO k:39 7 STO k:43");
        expect(var_of(&c, 'A') == 7, "33s: the (J) key gives (i): i = 1 addresses A");
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "2 STO ltr:I 4 STO k:39");
        expect(c.ind_top == 2, "35s: the (I) key is still (I): I = 2 is unnamed register 2");
        /* a label is stamped with the mode it was entered in; the listing shows it */
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:A STO k:43 BLUE XEQ");
        ab_prog_line(&c, 0, NULL, t, sizeof t);
        expect(c.lmode[0] == 33 && !strcmp(t, "LBL A \xc2\xb7" "33"), "LBL A entered in 33s mode: LBL A \xc2\xb7" "33");
        ab_prog_line(&c, 1, NULL, t, sizeof t);
        expect(!strcmp(t, "STO (i)"), "under a 33s label the indirect lists as STO (i)");
        /* MODE on the LBL line restamps it (no line added), with a brief notice; the calculator's mode stays */
        int lines = c.prog_len;
        press(&a, "GOLD 9 k:4 GOLD 9 k:4");     /* BST twice (the PRGM menu's fifth): from RTN up to the LBL line */
        expect(c.pc == 0, "two BSTs bring the pointer to the LBL line");
        press(&a, "BLUE ENTER k:1");            /* MODE 35s */
        app_ui(&a, &ui);
        ab_prog_line(&c, 1, NULL, t, sizeof t);
        expect(c.lmode[0] == 35 && !c.m35 && c.prog_len == lines, "MODE 35s on LBL A: the label restamped, the calculator still 33s");
        expect(!strcmp(t, "STO (I)") && ui.notice && !strcmp(ui.notice, "(i) MEANS (I)"),
               "its indirect now lists STO (I), and the notice says so");
        a.notice = NULL;
        press(&a, "BLUE ENTER");
        app_ui(&a, &ui);
        expect(ui.soft_on[1] && !ui.soft_on[0], "on the LBL line MODE marks the label's mode (35s), not the calculator's");
        press(&a, "k:1");
        app_ui(&a, &ui);
        expect(c.lmode[0] == 35 && !ui.notice, "35s again on a 35s label: no notice, nothing changed");
        /* unit 029 (review): STU's stamp is the 35s's meaning: no notice from 35s to STU; 33s and back to
           STU say what the indirect now means */
        press(&a, "BLUE ENTER k:2");
        app_ui(&a, &ui);
        expect(c.lmode[0] == AB_MODEL_STU && !ui.notice, "029: 35s to STU on the LBL line: restamped, no notice");
        press(&a, "BLUE ENTER k:0");
        app_ui(&a, &ui);
        expect(c.lmode[0] == 33 && ui.notice && !strcmp(ui.notice, "(I) MEANS (i)"), "029: STU to 33s: (I) MEANS (i)");
        press(&a, "BLUE ENTER k:2");
        app_ui(&a, &ui);
        expect(c.lmode[0] == AB_MODEL_STU && ui.notice && !strcmp(ui.notice, "(i) MEANS (I)"),
               "029: 33s to STU: (i) MEANS (I)");
        press(&a, "GOLD 9 k:0");
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
