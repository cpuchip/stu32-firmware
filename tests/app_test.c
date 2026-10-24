/* app_test: the device's behaviour from key presses (app.h), driven by legends the way a person
 * presses keys: "GOLD" "√x" is the gold shift then the square-root key. Results are read from
 * the core. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "app.h"
#include "games.h"
#include "keymap.h"
#include "const.h"
#include "state.h"
#include "eqn.h"
#include "version.h"
#include "fn.h"

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

/* unit 037: test apps that record what the framework calls (one shared recorder) */
static struct {
    int loads, starts, stops, saves, draws, ticks, keys, key, down, beeped;
    uint32_t key_ms, tick_ms, save_n;
    uint8_t saved[APP_SAVE_SIZE];
} rec;
/* 040b: a C arriving while a long computation runs (the device's key task calls app_interrupt from its own task) */
static app_state *busy_app;
static void busy_interrupt(void *ctx, int op) { (void)ctx; (void)op; app_interrupt(busy_app); }
typedef struct { int count; } tapp_ctx;
static void t_load(void *ctx, const uint8_t *s, uint32_t n) { rec.loads++; rec.save_n = n; if (n) ((tapp_ctx *)ctx)->count = s[0]; }
static void t_start(void *ctx) { (void)ctx; rec.starts++; }
static bool t_key(void *ctx, int key, bool down, uint32_t ms)
{
    rec.keys++, rec.key = key, rec.down = down, rec.key_ms = ms;
    if (down) ((tapp_ctx *)ctx)->count++;
    if (key == 38) app_beep(880, 50);           /* (the 2 key: a tone) */
    return key != 37;                           /* (the 1 key: no redraw) */
}
static uint32_t t_tick(void *ctx, uint32_t ms) { (void)ctx; rec.ticks++; rec.tick_ms = ms; return 50; }
static void t_draw(void *ctx, ls027_frame *f) { (void)ctx; rec.draws++; screen_clear(f); screen_fill(f, 0, 0, 10, 10, true); }
static void t_save(void *ctx, uint8_t *s, uint32_t cap) { rec.saves++; if (cap) s[0] = (uint8_t)((tapp_ctx *)ctx)->count; }
static void t_stop(void *ctx) { (void)ctx; rec.stops++; }
#define TAPP(n) {n, sizeof(tapp_ctx), t_load, t_start, t_key, t_tick, t_draw, t_save, t_stop, 0}
static const app_def tapps[7] = {TAPP("T1"), TAPP("T2"), TAPP("T3"), TAPP("T4"), TAPP("T5"), TAPP("T6"), TAPP("T7")};

/* apps.h v1.2 (abacus #4734): an app with a 300-byte save (item 2) that reads app_seed at start (3) and plays a tune
   on the 2 key (4); and one with no save_size (64) */
static struct { uint32_t seed, load_n; uint8_t loaded[400]; } rec2;
static const uint16_t V_TUNE[6] = {440, 100, 0, 50, 660, 100};
static uint16_t v_tune17[34];
static void v_load(void *ctx, const uint8_t *s, uint32_t n) { (void)ctx; rec2.load_n = n; if (n) memcpy(rec2.loaded, s, n < 400 ? n : 400); }
static void v_start(void *ctx) { (void)ctx; rec2.seed = app_seed(); }
static bool v_key(void *ctx, int key, bool down, uint32_t ms)
{
    (void)ctx, (void)ms;
    if (down && key == APP_KEY_2) app_tune(V_TUNE, 3);
    if (down && key == APP_KEY_3) app_tune(v_tune17, 17);   /* 17 notes: 16 played */
    if (down && key == APP_KEY_4) app_tune(NULL, 0);        /* nothing: stops */
    return false;
}
static uint32_t v_tick(void *ctx, uint32_t ms) { (void)ctx, (void)ms; return 0; }
static void v_draw(void *ctx, ls027_frame *f) { (void)ctx; screen_clear(f); }
static void v_save(void *ctx, uint8_t *s, uint32_t cap) { (void)ctx; for (uint32_t i = 0; i < cap; i++) s[i] = (uint8_t)(i * 7 + 1); }
static void v_stop(void *ctx) { (void)ctx; app_tune(V_TUNE, 3); }   /* (a tune started as it stops: cleared) */
static const app_def vapps[3] = {
    {"V300", 16, v_load, v_start, v_key, v_tick, v_draw, v_save, v_stop, 300},
    {"V64", 16, v_load, v_start, v_key, v_tick, v_draw, v_save, v_stop, 0},
    {"VBIG", 16, v_load, v_start, v_key, v_tick, v_draw, v_save, v_stop, APP_SAVE_MAX + 1},
};
static const app_def *const vlist[3] = {&vapps[0], &vapps[1], &vapps[2]};
static uint32_t fake_rng(void *ctx) { (void)ctx; return 0xC0FFEEu; }
static const app_def *const tlist[7] = {&tapps[0], &tapps[1], &tapps[2], &tapps[3], &tapps[4], &tapps[5], &tapps[6]};

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

static double y_of(const ab_calc *c)
{
    char t[64];
    double v = 0;
    if (abn_to_text(&c->y.re, t, sizeof t) > 0) sscanf(t, "%lf", &v);
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

/* VER's expected CASIM and GAMES come from the Makefile (unit 069); a compile without them (clang-check) gets values no
   build shows, so an app_test built that way fails rather than passes */
#ifndef VER_CASIM
#define VER_CASIM "(no VER_CASIM)"
#endif
#ifndef VER_GAMES
#define VER_GAMES "(no VER_GAMES)"
#endif

/* SYS's facts as the device fills its VER (unit 069): app_sysinfo_build, nothing else */
static void build_sysinfo(void *ctx, app_sysinfo *o)
{
    (void)ctx;
    memset(o, 0, sizeof *o);
    o->soc = -1;
    app_sysinfo_build(o);
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
    o->slot_size = 65536;       /* MEM's bytes free (unit 041 C) */
    o->heap_free = 66771;
    o->heap_largest = 31744;
}

/* the menu named name, -1 none (unit 041 C) */
static int menu_named_t(const char *name)
{
    for (int i = 0; km_menu_at(i); i++)
        if (!strcmp(km_menu_at(i)->name, name)) return i;
    return -1;
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
    expect(x_of(&c) == -64, "GOLD BLUE then the root key: blue, x^3 (041 C; decision 60 freed the duplicate ABS)");
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
        ab_do_arg(&c, AB_MODEL, 35);            /* GTO B004, a line number: 35s mode's (unit 041 A, rule 3.2) */
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 1);
        ab_do(&c, AB_K1); ab_do(&c, AB_K0); ab_do(&c, AB_K0); ab_do(&c, AB_K0);
        ab_do_arg(&c, AB_STO, 'N' - 'A');
        ab_do_arg(&c, AB_DSE, 'N' - 'A');
        ab_do_arg(&c, AB_GTO, 1 * 1000 + 4);
        ab_do(&c, AB_RTN);
        ab_do(&c, AB_PRGM);
        expect(c.prog_len == 6, "the DSE loop keyed");
        ab_do_arg(&c, AB_MODEL, 33);            /* back to 33s mode: XEQ's letter acts at once; label B runs as 35s */
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

    /* 068 (2), Michael's ruling (abacus #7567): on USB power (PGOOD_N low) the calculator stays up for the console. No
       auto-off; OFF is a soft off (the screen blank, keys dropped but C, which turns it on with ON's chirp); USB unplugged
       during a soft off is the deep sleep */
    {
        pwr_inputs usb = {true, false, 80, false}, cell = {true, true, 80, false};
        screen_ui u;
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "1");
        expect(app_tick(&a, &usb, now + 3 * PWR_IDLE_MS) != APP_OFF, "068: on USB, idle past auto-off: still on");
        ab_init(&c);                                /* (a fresh one: after a USB tick, the cell's is an unplug) */
        app_init(&a, &c, now);
        press(&a, "1");
        expect(app_tick(&a, &cell, now + 3 * PWR_IDLE_MS) == APP_OFF, "068: on the cell, idle: off, as before");
        ab_init(&c);                                /* unplugged after a long idle on USB: the clock restarts there */
        app_init(&a, &c, now);
        press(&a, "1");
        uint32_t tu = now + 3 * PWR_IDLE_MS;
        expect(app_tick(&a, &usb, tu - 1000) != APP_OFF, "068: on USB, long idle: on");
        expect(app_tick(&a, &cell, tu) != APP_OFF, "068: unplugged after a long idle: still on (abacus #7571)");
        expect(app_tick(&a, &cell, tu + PWR_IDLE_MS - 1) != APP_OFF, "068: a millisecond short of auto-off after the unplug: on");
        expect(app_tick(&a, &cell, tu + PWR_IDLE_MS) == APP_OFF, "068: auto-off after the unplug, counted from it");
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "1 ENTER 2");
        a.dirty = true;
        press(&a, "GOLD C");
        app_action r68 = app_tick(&a, &usb, now + 10);
        expect(r68 == APP_SAVE && a.soft_off, "068: OFF on USB is a soft off, not the deep sleep, and saves first, as OFF does");
        app_saved(&a);
        expect(app_tick(&a, &usb, now + 11) == APP_RENDER, "068: then draws the soft off");
        app_ui(&a, &u);
        expect(u.blank, "068: the soft off's screen is blank");
        press(&a, "5");
        expect(a.soft_off && x_of(&c) == 2, "068: in the soft off, a key other than C is dropped");
        a.tone = -1;
        press(&a, "C");
        app_ui(&a, &u);
        expect(!a.soft_off && !u.blank && a.tone_use == DEV_USE_ON, "068: C turns it on, the screen back, with ON's chirp");
        expect(x_of(&c) == 2, "068: and C's press turned it on only (X still 2)");
        press(&a, "GOLD C");
        expect(app_tick(&a, &usb, now + 20) != APP_OFF && a.soft_off, "068: OFF on USB again: a soft off");
        expect(app_tick(&a, &cell, now + 30) == APP_OFF, "068: USB unplugged during a soft off: the deep sleep");
    }

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
    press(&a, "BLUE +/\xe2\x88\x92 k:5 C");          /* CMPLX's sixth since 041 C (abacus #4878 (a)) */
    expect(!c.cpolar, "041 A2: 33s mode: r\xce\xb8" "a does nothing");
    ab_do_arg(&c, AB_MODEL, 35);                /* xiy and r\xce\xb8a are 35s mode's (unit 041 A2, decision 58) */
    press(&a, "BLUE +/\xe2\x88\x92 k:5");
    expect(c.cpolar, "r\xce\xb8" "a: complex numbers in polar form");
    press(&a, "C BLUE +/\xe2\x88\x92 k:4");
    expect(!c.cpolar, "xiy: complex numbers in rectangular form, from CMPLX's fifth");
    /* unit 041 C (B4): DISP's ENG→ and ←ENG: the view, DISP kept open to press again, no UNDO mark */
    press(&a, "C 1 2 . 3 4 6 E 4 GOLD 2 k:4");
    expect(c.eng_on && c.eng_e == 3 && c.eng_low == 1 && a.menu >= 0, "041 C: 12.346E4 then DISP's ENG\xe2\x86\x92: E3, the last digit at 10^1, DISP open");
    size_t undo_was = a.undo_len;
    uint8_t *undo_copy = malloc(undo_was ? undo_was : 1);
    if (undo_copy && undo_was) memcpy(undo_copy, a.undo[0], undo_was);
    press(&a, "k:4");
    expect(c.eng_on && c.eng_e == 0 && a.undo_len == undo_was && undo_copy &&
               (!undo_was || !memcmp(undo_copy, a.undo[0], undo_was)),
           "041 C: ENG\xe2\x86\x92 again: E0, no UNDO mark (the snapshot unchanged)");
    free(undo_copy);
    press(&a, "k:5 k:5");
    expect(c.eng_on && c.eng_e == 6, "041 C: \xe2\x86\x90" "ENG twice: E6");
    press(&a, "C");
    expect(c.eng_on && a.menu < 0, "041 C: C closes DISP, as any menu (the core sees no key): the view stays");
    press(&a, "C");
    expect(!c.eng_on, "041 C: the next key ends the view");
    /* fresh eyes: the view measures X's line as the screen draws it, grouping included (both machines group by
       default): ALL's 123,456,789,012,345.7 (21 cells; ungrouped, .67890 would fit), so the last place 10^-1 */
    ab_init(&c);
    app_init(&a, &c, now);
    ab_do_arg(&c, AB_SETALL, 0);                   /* ALL's line, as measured (a new calculator shows FIX 4, unit 067) */
    press(&a, "1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 . 6 7 8 9 0 1 ENTER GOLD 2 k:4");
    expect(a.set.group == 1 && c.eng_on && c.eng_low == -1 && c.eng_e == -3,
           "041 C: ENG\xe2\x86\x92 on a grouped ALL line: the place it shows, 10^-1");
    {   /* unit 041 D (fresh eyes): a 33s chain shown in STU keeps its ·33 mark with the 33s's names and colon; one in
           the 35s's spelling, which a 33s equation refuses (a message), shows none */
        ab_calc t;
        ab_init(&t);
        ab_do_arg(&t, AB_MODEL, AB_MODEL_STU);
        expect(!strcmp(ab_eqn_mark(&t, "2^3^2+CB(1)", 33), " \xc2\xb7" "33") &&
                   !strcmp(ab_eqn_mark(&t, "2^3^2+IDIV(7:2)", 33), " \xc2\xb7" "33") &&
                   !strcmp(ab_eqn_mark(&t, "2^3^2+IDIV(7,2)", 33), ""),
               "041 D: the chain mark reads a 33s equation by its own names and separator");
    }
    {   /* the image refuses an ENG view the key can't make (fresh eyes): in HEX, or past the key's bounds */
        static uint8_t img[1 << 16];
        ab_calc t;
        ab_init(&t);
        t.eng_on = true;
        t.eng_e = 3;
        t.eng_low = 0;
        size_t n = ab_state_save(&t, img, sizeof img);
        expect(n > 0 && ab_state_load(&t, img, n) == ABN_OK, "041 C: an ENG view in the image loads");
        t.base = 16;
        n = ab_state_save(&t, img, sizeof img);
        expect(n > 0 && ab_state_load(&t, img, n) == ABN_BAD_IMAGE, "041 C: an ENG view in HEX: refused");
        t.base = 10;
        t.eng_e = 30000 - 30000 % 3;
        n = ab_state_save(&t, img, sizeof img);
        expect(n > 0 && ab_state_load(&t, img, n) == ABN_BAD_IMAGE, "041 C: an ENG exponent past the key's bounds: refused");
    }
    press(&a, "GOLD 2 k:4");
    expect(!c.cpolar, "xiy: back to rectangular");
    ab_do_arg(&c, AB_MODEL, 33);
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
        app_ui(&a, &ui);
        expect(img_crc(&c) == before, "UNDO after +: the state before + back");
        expect(ui.notice && !strcmp(ui.notice, "UNDO"), "073: the first UNDO says UNDO");
        press(&a, "BLUE \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(img_crc(&c) == after && x_of(&c) == 5, "UNDO twice: + again (a redo)");
        expect(ui.notice && !strcmp(ui.notice, "REDO"), "073: the second says REDO");
        press(&a, "BLUE \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(ui.notice && !strcmp(ui.notice, "UNDO") && img_crc(&c) == before, "073: a third is UNDO again");
        press(&a, "BLUE \xe2\x86\x90 4 BLUE \xe2\x86\x90");    /* a REDO, then a new change: its UNDO says UNDO */
        app_ui(&a, &ui);
        expect(ui.notice && !strcmp(ui.notice, "UNDO"), "073: after a new change, UNDO says UNDO");

        /* a shift, a menu opening, a menu closed with C and VIEW are not undo points */
        press(&a, "7");
        before = img_crc(&c);
        press(&a, "STO ltr:A");
        after = img_crc(&c);
        press(&a, "GOLD GOLD BLUE GOLD \xe2\x86\x90 C GOLD STO ltr:A");
        press(&a, "BLUE \xe2\x86\x90");
        expect(img_crc(&c) == before && var_of(&c, 'A') == 0, "UNDO passes over shifts, a menu and VIEW to STO");

        /* UNDO after CLSTK (the CLEAR menu's second item; 35s mode's, unit 041 A6, decision 58) */
        ab_do_arg(&c, AB_MODEL, 35);
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
        /* PRGM's CLPGM asks the same (abacus #8495, #8497): in program entry and out of it */
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:A 2 BLUE XEQ GOLD 9 k:5");
        app_ui(&a, &ui);
        expect(c.prgm && ui.prompt && !strcmp(ui.prompt, "CLR PGMS?") && c.prog_len == 3,
               "PRGM's CLPGM in program entry: CLR PGMS?, nothing cleared yet");
        press(&a, "k:1");
        app_ui(&a, &ui);
        expect(c.prog_len == 3 && !ui.prompt, "N: the programs kept");
        press(&a, "GOLD 9 k:0 GOLD 9 k:5");
        app_ui(&a, &ui);
        expect(!c.prgm && ui.prompt && !strcmp(ui.prompt, "CLR PGMS?") && c.prog_len == 3,
               "PRGM's CLPGM out of program entry: CLR PGMS? too");
        press(&a, "k:0");
        app_ui(&a, &ui);
        expect(c.prog_len == 0 && !ui.prompt, "Y: the programs cleared");
        press(&a, "BLUE \xe2\x86\x90");
        expect(c.prog_len == 3, "UNDO after CLPGM: the programs back");
        /* and its Y clears in Equation mode and in ALG too (fresh eyes: it asked, then did nothing; unit 021: never a
           silent no-op once the device has asked) */
        press(&a, "GOLD 0 GOLD 9 k:5");
        app_ui(&a, &ui);
        expect(c.eqn_mode && ui.prompt && !strcmp(ui.prompt, "CLR PGMS?") && c.prog_len == 3,
               "Equation mode: PRGM's CLPGM asks CLR PGMS?");
        press(&a, "k:0");
        app_ui(&a, &ui);
        expect(c.prog_len == 0 && !ui.prompt && !c.msg && !ui.notice, "Equation mode: Y clears the programs");
        press(&a, "GOLD 0 GOLD 9 k:0 GOLD XEQ ltr:A 2 BLUE XEQ GOLD 9 k:0 BLUE ENTER k:2 BLUE ENTER k:4 GOLD 9 k:5");
        app_ui(&a, &ui);
        expect(c.alg && !c.eqn_mode && ui.prompt && !strcmp(ui.prompt, "CLR PGMS?") && c.prog_len == 3,
               "ALG: PRGM's CLPGM asks CLR PGMS?");
        press(&a, "k:0");
        app_ui(&a, &ui);
        expect(c.prog_len == 0 && !ui.prompt && !c.msg && !ui.notice, "ALG: Y clears the programs, no RPN ONLY");
        press(&a, "BLUE ENTER k:0");

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

    /* unit 041 A (decision 58): in 33s mode TABLE and GRAPH don't open, LOGIC and SUFFIX are dimmed and don't open,
       CMPLX opens (the 33s's prefix) with its items dimmed; in 35s mode none dimmed */
    {
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "BLUE 8");
        expect(a.menu < 0, "041 A14: 33s mode: TABLE doesn't open");
        press(&a, "BLUE 7");
        expect(a.menu < 0, "041 A14: 33s mode: GRAPH doesn't open");
        press(&a, "C BLUE x\xe2\x86\x94y");
        app_ui(&a, &ui);
        expect(ui.soft_dim[4] && ui.soft_dim[5] && !ui.soft_dim[0] && !ui.soft_dim[1],
               "041 A5: 33s mode: BASE's LOGIC and SUFX dimmed, its bases not");
        int base_menu = a.menu;
        press(&a, "k:4");
        expect(a.menu == base_menu, "041 A5: 33s mode: LOGIC doesn't open");
        press(&a, "C BLUE +/\xe2\x88\x92");
        app_ui(&a, &ui);
        expect(a.menu >= 0 && ui.soft_dim[0] && ui.soft_dim[1] && ui.soft_dim[2] && ui.soft_dim[3],
               "041 A1: 33s mode: CMPLX opens as the prefix, i theta ARG [ dimmed");
        press(&a, "C BLUE x\xe2\x86\x94y k:1 C BLUE +/\xe2\x88\x92");       /* HEX: no prefix (039), so CMPLX holds nothing */
        expect(a.menu < 0, "041 A1: 33s mode in HEX: CMPLX doesn't open (every item dimmed, no prefix)");
        press(&a, "C BLUE x\xe2\x86\x94y k:0 C");                         /* DEC again */
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "BLUE +/\xe2\x88\x92");
        app_ui(&a, &ui);
        expect(!ui.soft_dim[0] && !ui.soft_dim[3], "041 A1: 35s mode: CMPLX's items not dimmed");
        press(&a, "C BLUE 8");
        expect(a.menu >= 0, "041 A14: 35s mode: TABLE opens (its GO says STU ONLY)");
    }

    /* unit 041 C: x³ on blue √x (abacus decision 60: the duplicate ABS's slot), ∛x as EXP's fifth item; the 35s
       has neither, so in 35s mode blue √x does nothing and ∛x is dimmed with EXP kept open */
    {
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "3 BLUE \xe2\x88\x9ax");
        expect(x_of(&c) == 27, "041 C: 33s mode: blue root key is x^3");
        press(&a, "C 6 4 BLUE e\xcb\xa3");
        app_ui(&a, &ui);
        expect(ui.soft[4] && !strcmp(ui.soft[4], "\xe2\x88\x9bx") && !ui.soft_dim[4], "041 C: 33s mode: EXP's fifth is cube root, lit");
        press(&a, "k:4");
        expect(x_of(&c) == 4, "041 C: 33s mode: EXP cube root of 64");
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "C 3 +/\xe2\x88\x92 BLUE \xe2\x88\x9ax");
        expect(x_of(&c) == -3, "041 C: 35s mode: blue root key does nothing (not ABS, not x^3)");
        press(&a, "C 8 BLUE e\xcb\xa3");
        app_ui(&a, &ui);
        expect(ui.soft_dim[4] && !ui.soft_dim[0], "041 C: 35s mode: EXP's cube root dimmed, 2^x not");
        int exp_menu = a.menu;
        press(&a, "k:4");
        expect(x_of(&c) == 8 && a.menu == exp_menu, "041 C: 35s mode: the dimmed cube root does nothing, EXP stays");
        /* program entry follows the edited label's stamp (abacus #4850): in 33s mode, a 35s label's soft keys */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 0);
        ab_do_arg(&c, AB_MODEL, 35);            /* on the LBL line: restamps A only (025 rule 5) */
        press(&a, "BLUE e\xcb\xa3");
        app_ui(&a, &ui);
        expect(!c.m35 && ui.soft_dim[4], "041 C: 33s mode, editing a 35s label: EXP's cube root dimmed");
        press(&a, "C BLUE x\xe2\x86\x94y");
        app_ui(&a, &ui);
        expect(!ui.soft_dim[4] && !ui.soft_dim[5], "041 C: and BASE's LOGIC and SUFX lit");
    }

    /* unit 041 C (B10): in 33s mode HYP is also a prefix (33s p.4-6): HYP then SIN is SINH, HYP then GOLD SIN
       ASINH, in program entry and an equation's typing too; its soft keys stay the menu's; any other key closes it
       as a menu. 35s mode the same (35s p.4-6: abacus #8497, B10 corrected). STU, the menu only (the trig keys
       close it and act); in program entry the label's stamp decides, as ab_rec35 */
    {
        char t[24];
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "1 BLUE LN SIN");
        expect(fabs(x_of(&c) - sinh(1.0)) < 1e-12 && a.menu < 0, "041 C: 33s mode: HYP SIN is SINH, the prefix closed");
        press(&a, "C 1 BLUE LN COS");
        expect(fabs(x_of(&c) - cosh(1.0)) < 1e-12, "041 C: 33s mode: HYP COS is COSH");
        press(&a, "C 1 BLUE LN TAN");
        expect(fabs(x_of(&c) - tanh(1.0)) < 1e-12, "041 C: 33s mode: HYP TAN is TANH");
        press(&a, "C 2 BLUE LN GOLD SIN");
        expect(fabs(x_of(&c) - asinh(2.0)) < 1e-12, "041 C: 33s mode: HYP GOLD SIN is ASINH");
        press(&a, "C 2 BLUE LN GOLD COS");
        expect(fabs(x_of(&c) - acosh(2.0)) < 1e-12, "041 C: 33s mode: HYP GOLD COS is ACOSH");
        press(&a, "C . 5 BLUE LN GOLD TAN");
        expect(fabs(x_of(&c) - atanh(0.5)) < 1e-12, "041 C: 33s mode: HYP GOLD TAN is ATANH");
        press(&a, "C 2 BLUE LN k:0");
        expect(fabs(x_of(&c) - sinh(2.0)) < 1e-12, "041 C: 33s mode: the menu's SINH soft key still works");
        press(&a, "C 2 ENTER 3 BLUE LN +");
        expect(x_of(&c) == 5 && a.menu < 0, "041 C: 33s mode: another key closes HYP and acts");
        press(&a, "C");
        ab_do(&c, AB_PRGM);
        press(&a, "BLUE LN SIN");
        ab_prog_line(&c, c.pc, NULL, t, sizeof t);
        expect(!strcmp(t, "SINH"), "041 C: 33s mode: HYP SIN in program entry records SINH");
        ab_do(&c, AB_PRGM);
        press(&a, "C GOLD 0 BLUE LN SIN");
        expect(c.eqn_typing && !strncmp(c.ebuf, "SINH(", 5), "041 C: 33s mode: HYP SIN in an equation types SINH(");
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "1 BLUE LN SIN");
        expect(fabs(x_of(&c) - sinh(1.0)) < 1e-12 && a.menu < 0, "35s mode: HYP SIN is SINH, the prefix closed (35s p.4-6)");
        press(&a, "C 2 BLUE LN GOLD COS");
        expect(fabs(x_of(&c) - acosh(2.0)) < 1e-12, "35s mode: HYP GOLD COS is ACOSH");
        press(&a, "C 2 ENTER 3 BLUE LN +");
        expect(x_of(&c) == 5 && a.menu < 0, "35s mode: another key closes HYP and acts");
        press(&a, "C");
        ab_do(&c, AB_PRGM);
        press(&a, "GOLD XEQ ltr:A BLUE LN TAN");
        ab_prog_line(&c, c.pc, NULL, t, sizeof t);
        expect(!strcmp(t, "TANH"), "35s mode: HYP TAN in program entry records TANH");
        ab_do(&c, AB_PRGM);
        press(&a, "C GOLD 0 BLUE LN SIN");
        expect(c.eqn_typing && !strncmp(c.ebuf, "SINH(", 5), "35s mode: HYP SIN in an equation types SINH(");
        press(&a, "C C");                       /* the typing cancelled, then Equation mode left */
        expect(!c.eqn_mode, "35s mode: out of Equation mode");
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        press(&a, "C 1 BLUE LN COS");
        expect(fabs(x_of(&c) - cosh(1.0)) < 1e-12 && a.menu < 0, "073: STU: HYP then COS is COSH (the prefix, every mode)");
        ab_do(&c, AB_PRGM);
        press(&a, "BLUE LN SIN");
        ab_prog_line(&c, c.pc, NULL, t, sizeof t);
        expect(!strcmp(t, "SINH"), "073: STU: in program entry on a 35s label, HYP SIN records SINH");
        press(&a, "GOLD XEQ ltr:B BLUE LN SIN");
        ab_prog_line(&c, c.pc, NULL, t, sizeof t);
        expect(!strcmp(t, "SINH"), "073: STU: on a label stamped STU, HYP then SIN records SINH too");
        ab_do(&c, AB_PRGM);
        /* unit 073: while HYP waits, HYP_ is the prompt and HYP the band's annunciator, in each mode; a soft key acts as
           the menu's, and any key ends both */
        static const int MODEL[3] = {33, 35, AB_MODEL_STU};
        for (int m = 0; m < 3; m++) {
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, MODEL[m]);
            press(&a, "BLUE LN");
            app_ui(&a, &ui);
            static screen_page pg;
            screen_lines(&c, &ui, &pg);
            char w[80];
            snprintf(w, sizeof w, "073: mode %d: HYP armed: HYP_ the prompt, HYP the annunciator", MODEL[m]);
            expect(ui.prompt && !strcmp(ui.prompt, "HYP_") && ui.hyp && pg.status.hyp && strstr(pg.status.text, "HYP"), w);
            press(&a, "SIN");
            app_ui(&a, &ui);
            snprintf(w, sizeof w, "073: mode %d: after HYP SIN, no prompt and no annunciator", MODEL[m]);
            expect(!ui.hyp && (!ui.prompt || strcmp(ui.prompt, "HYP_")) && a.menu < 0, w);
        }
    }

    /* unit 041 C (B7): FN= on the keys (until now only the core's token): PRGM's fourth item, in SST's cell (SST is
       blue R/S); a letter chooses that label, (I) or (J) the label it names (35s p.15-1, p.14-22); then SOLVE from
       the keys solves that program */
    {
        int ki = -1, kj = -1;
        for (int k = 0; k < KB_KEYS; k++) {
            if (km_letter_of(k)->kind == KL_IND_I) ki = k;
            if (km_letter_of(k)->kind == KL_IND_J) kj = k;
        }
        char kk[48];
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 'Z' - 'A');       /* LBL Z: X^2 - 2 */
        ab_do_arg(&c, AB_RCL, 23);
        ab_do(&c, AB_SQ);
        ab_do(&c, AB_K2);
        ab_do(&c, AB_SUB);
        ab_do(&c, AB_RTN);
        ab_do(&c, AB_PRGM);
        press(&a, "GOLD 9");
        app_ui(&a, &ui);
        expect(ui.soft[3] && !strcmp(ui.soft[3], "FN="), "041 C: PRGM's fourth item is FN=");
        press(&a, "k:3");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "FN= _"), "041 C: FN= prompts for a label");
        press(&a, "ltr:Z");
        app_ui(&a, &ui);
        expect(c.fn == 'Z' - 'A' && !ui.prompt, "041 C: FN= Z chooses Z");
        press(&a, "1 STO ltr:X 9 GOLD 7 ltr:X");
        expect(!c.msg && fabs(x_of(&c) - sqrt(2.0)) < 1e-12, "041 C: then SOLVE X from the keys solves Z's X^2-2");
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "C 2 6 +/\xe2\x88\x92 STO ltr:I");     /* I = -26: label Z (p.14-22: -1 to -26 name A to Z; 013's O02) */
        c.fn = 0;
        snprintf(kk, sizeof kk, "GOLD 9 k:3 k:%d", ki);
        press(&a, kk);
        expect(c.fn == 'Z' - 'A', "041 C: 35s mode: FN= (I) chooses the label I names");
        ab_do_arg(&c, AB_MODEL, 33);
        c.fn = 0;                               /* 33s mode: the (I) key is i, the (J) key (i), the 33s's index */
        snprintf(kk, sizeof kk, "C 2 6 +/\xe2\x88\x92 STO k:%d GOLD 9 k:3 k:%d", ki, kj);
        press(&a, kk);
        expect(c.fn == 'Z' - 'A', "041 C: 33s mode: FN= (i) chooses the label i names");
    }

    /* unit 041 C (B5, abacus #4850): MEM on blue 1. Its menu VAR PGM, with bytes free on X's line: the image's room,
       the store's slot less the image now (ours); in 35s mode and STU also the indirect count (35s p.3-4). The VAR
       catalog of the nonzero variables (33s p.3-3): the soft keys up and down, ENTER copies the one shown to X, CLEAR
       zeroes it, C cancels. The PGM catalog of the labels with LN= their bytes (33s p.12-20): up and down, XEQ or R/S
       runs the one shown, PRGM opens it in program entry, CLEAR deletes it, C cancels */
    {
        char want[40];
        ab_init(&c);
        app_init(&a, &c, now);
        a.sysinfo = fake_sysinfo;
        press(&a, "BLUE 1");
        app_ui(&a, &ui);
        snprintf(want, sizeof want, "%lu B FREE", (unsigned long)(65536 - ab_state_save(&c, NULL, 0)));
        expect(ui.soft[0] && !strcmp(ui.soft[0], "VAR") && ui.soft[1] && !strcmp(ui.soft[1], "PGM") && ui.prompt &&
                   !strcmp(ui.prompt, want), "041 C: MEM: VAR, PGM and the image's room");
        press(&a, "C");
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "BLUE 1");
        app_ui(&a, &ui);
        snprintf(want, sizeof want, "%lu B FREE  0 IND", (unsigned long)(65536 - ab_state_save(&c, NULL, 0)));
        expect(ui.prompt && !strcmp(ui.prompt, want), "041 C: 35s mode: and the indirect variables in use");
        ab_do_arg(&c, AB_SETALL, 0);               /* VAR's lines in ALL (a new calculator shows FIX 4, unit 067) */
        press(&a, "C 3 STO ltr:C 4 STO ltr:D C BLUE 1 k:0");
        app_ui(&a, &ui);
        expect(a.cat == 1 && ui.view[3] && !strcmp(ui.view[3], "C=3"), "041 C: VAR: the first nonzero variable, C=3");
        press(&a, "k:1");
        app_ui(&a, &ui);
        expect(ui.view[3] && !strcmp(ui.view[3], "D=4"), "041 C: VAR: down, D=4");
        press(&a, "k:1");
        app_ui(&a, &ui);
        expect(ui.view[3] && !strcmp(ui.view[3], "C=3"), "041 C: VAR: down again wraps to C=3");
        press(&a, "k:0 ENTER");
        app_ui(&a, &ui);
        expect(x_of(&c) == 4 && a.cat == 1, "041 C: VAR: ENTER copies D to X, the catalog kept");
        press(&a, "GOLD \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(x_of(&c) == 4 && abn_is_zero(&c.var[3].re) && ui.view[3] && !strcmp(ui.view[3], "C=3"),
               "041 C: VAR: CLEAR zeroes D; C=3 shown");
        press(&a, "C");
        expect(a.cat == 0, "041 C: VAR: C cancels");
        ab_init(&c);
        app_init(&a, &c, now);
        a.sysinfo = fake_sysinfo;
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 0);               /* LBL A: CHS */
        ab_do(&c, AB_KCHS);
        ab_do(&c, AB_RTN);
        ab_do_arg(&c, AB_LBL, 1);               /* LBL B: x^2 */
        ab_do(&c, AB_SQ);
        ab_do(&c, AB_RTN);
        ab_do(&c, AB_PRGM);
        int lines = c.prog_len;
        press(&a, "BLUE 1 k:1");
        app_ui(&a, &ui);
        expect(a.cat == 2 && ui.view[2] && !strcmp(ui.view[2], "LBL A") && ui.view[3] && !strcmp(ui.view[3], "LN=18"),
               "041 C: PGM: LBL A, LN=18 (three lines of a u16 op and an i32 argument)");
        expect(ui.soft[0] && ui.soft[1] && ui.soft[2] && !strcmp(ui.soft[2], "XEQ") && ui.soft[3] &&
                   !strcmp(ui.soft[3], "PRGM"), "041 C: PGM: up, down, XEQ, PRGM on the soft keys");
        press(&a, "k:1");
        app_ui(&a, &ui);
        expect(ui.view[2] && !strcmp(ui.view[2], "LBL B"), "041 C: PGM: down, LBL B");
        press(&a, "C");                         /* the catalog cancelled; 3, then XEQ from B's catalog line */
        press(&a, "3 BLUE 1 k:1 k:1 XEQ");
        run_out(&a, &in);
        expect(x_of(&c) == 9 && a.cat == 0, "041 C: PGM: XEQ runs LBL B (3 squared), the catalog closed");
        press(&a, "BLUE 1 k:1 R/S");
        run_out(&a, &in);
        expect(x_of(&c) == -9, "041 C: PGM: R/S runs LBL A (CHS)");
        press(&a, "BLUE 1 k:1 k:1 k:3");
        expect(c.prgm && c.pc == c.labelpos[1], "041 C: PGM: PRGM opens LBL B in program entry");
        ab_do(&c, AB_PRGM);
        press(&a, "C BLUE 1 k:1 GOLD \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(c.labelpos[0] < 0 && c.prog_len == lines - 3 && ui.view[2] && !strcmp(ui.view[2], "LBL B"),
               "041 C: PGM: CLEAR deletes LBL A's program; LBL B shown");
        press(&a, "C");
        expect(a.cat == 0, "041 C: PGM: C cancels");
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "BLUE 1 k:0");
        expect(a.cat == 0 && a.notice, "041 C: VAR with no nonzero variable: a notice, no catalog");
        /* fresh eyes: back-arrow backs out of a catalog to MEM (33s p.12-20: "C to cancel the catalog or back-arrow to
           back out"), X untouched; in program entry the catalog's ENTER, XEQ and R/S record nothing (ours) */
        press(&a, "C 7 STO ltr:C 9 BLUE 1 k:0 \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(a.cat == 0 && a.menu == menu_named_t("MEM") && x_of(&c) == 9, "041 C: back-arrow: back to MEM, X kept");
        press(&a, "\xe2\x86\x90");
        expect(a.menu < 0 && x_of(&c) == 9, "041 C: back-arrow again: MEM closed, X kept (35s p.1-9)");
        press(&a, "C 7 ENTER GOLD 2 \xe2\x86\x90");
        expect(a.menu < 0 && x_of(&c) == 7, "35s p.1-9: back-arrow cancels any menu, acting on nothing (041 C, a sibling)");
        press(&a, "C");
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 1);
        ab_do(&c, AB_RTN);
        lines = c.prog_len;
        press(&a, "BLUE 1 k:0 ENTER C BLUE 1 k:1 XEQ R/S C");
        expect(c.prgm && c.prog_len == lines, "041 C: in program entry the catalogs record no line");
        ab_do(&c, AB_PRGM);
        /* the indirect count in 35s mode, nonzero; in 33s mode the VAR catalog shows the 33s's i */
        int ki = -1;
        for (int k = 0; k < KB_KEYS; k++)
            if (km_letter_of(k)->kind == KL_IND_I) ki = k;
        char kk[48];
        ab_init(&c);
        app_init(&a, &c, now);
        a.sysinfo = fake_sysinfo;
        ab_do_arg(&c, AB_MODEL, 35);
        snprintf(kk, sizeof kk, "5 STO ltr:I 7 STO k:%d BLUE 1", ki);
        press(&a, kk);
        app_ui(&a, &ui);
        snprintf(want, sizeof want, "%lu B FREE  6 IND", (unsigned long)(65536 - ab_state_save(&c, NULL, 0)));
        expect(c.ind_top == 5 && ui.prompt && !strcmp(ui.prompt, want), "041 C: 35s mode: registers 0 to 5 in use, 6 IND");
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_SETALL, 0);               /* VAR's lines in ALL (a new calculator shows FIX 4, unit 067) */
        snprintf(kk, sizeof kk, "6 STO k:%d BLUE 1 k:0", ki);
        press(&a, kk);
        app_ui(&a, &ui);
        expect(a.cat == 1 && ui.view[3] && !strcmp(ui.view[3], "i=6"), "041 C: 33s mode: the VAR catalog shows i");
    }
    /* fresh eyes: the XEQ prompt follows the edited label's stamp (abacus #4850): in 33s mode, editing a 35s label,
       XEQ A 0 0 3 records XEQ A003 (the 35s's form); in 35s mode, editing a 33s label, XEQ A records XEQ A001 at once */
    {
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 0);
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "XEQ ltr:A 0 0 3");
        expect(c.prog[c.pc].op == AB_XEQ && c.prog[c.pc].arg == 3, "041 C: 33s mode, a 35s label: XEQ A 0 0 3 is XEQ A003");
        {
            int ki2 = -1;
            char k2[24];
            for (int k = 0; k < KB_KEYS; k++)
                if (km_letter_of(k)->kind == KL_IND_I) ki2 = k;
            snprintf(k2, sizeof k2, "STO k:%d", ki2);
            press(&a, k2);
            expect(c.prog[c.pc].op == AB_STO && c.prog[c.pc].arg == AB_VAR_IND_I,
                   "041 C: 33s mode, a 35s label: the (I) key is the 35s's (I), not the 33s's i");
        }
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, 35);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 0);
        ab_do_arg(&c, AB_MODEL, 33);
        press(&a, "XEQ ltr:A");
        expect(c.prog[c.pc].op == AB_XEQ && c.prog[c.pc].arg == 1 && !a.prompt_text[0],
               "041 C: 35s mode, a 33s label: XEQ A records XEQ A001 at once (the 33s's form)");
    }

    /* apps.h v1.2 items 2, 3, 4 (abacus #4734) */
    {
        static uint8_t ctxbuf[APP_CTX_MAX];
        pwr_inputs pin = {true, true, 80, false};
        ab_init(&c);
        app_init(&a, &c, now);
        app_ctx_buffer(&a, ctxbuf, sizeof ctxbuf);
        app_apps(&a, vlist, 3);
        a.rng = fake_rng;
        /* (3) the seed: the device's RNG; a host sets it for a test's replay */
        memset(&rec2, 0, sizeof rec2);
        press(&a, "BLUE 0 k:0");
        expect(app_running(&a) && rec2.seed == 0xC0FFEEu, "v1.2 (3): app_seed is the device's RNG");
        app_seed_set(1234);
        expect(app_seed() == 1234, "v1.2 (3): app_seed_set fixes it on a host");
        /* (4) a tune: three notes in turn on the app's clock, a rest silent, then done */
        uint32_t t = now;
        a.tone = -1;
        app_key(&a, APP_KEY_2, t);
        expect(a.tone >= 0 && a.tone_use == DEV_USE_APP && a.tone_hz == 440 && a.tone_ms == 100,
               "v1.2 (4): app_tune: the first note at once");
        a.tone = -1;
        a.render = false;                       /* (a redraw due wakes at once: not the tune's doing) */
        uint32_t w = app_next_wake(&a, &pin, t);
        app_tick(&a, &pin, t + 100);
        expect(w <= 100 && a.tone < 0, "v1.2 (4): woken at the note's end; a rest sounds nothing");
        app_tick(&a, &pin, t + 150);
        expect(a.tone >= 0 && a.tone_hz == 660 && a.tone_ms == 100, "v1.2 (4): after the rest, the third note");
        a.tone = -1;
        app_tick(&a, &pin, t + 250);
        app_tick(&a, &pin, t + 400);
        a.render = false;
        expect(a.tone < 0 && app_next_wake(&a, &pin, t + 400) > 1000, "v1.2 (4): the tune over, no more wakes for it");
        app_key(&a, APP_KEY_2, t + 500);        /* a new tune replaces the one playing */
        a.tone = -1;
        app_key(&a, APP_KEY_2, t + 550);
        expect(a.tone >= 0 && a.tone_hz == 440, "v1.2 (4): a new call replaces what is playing (from its first note)");
        a.tone = -1;
        for (int i = 0; i < 17; i++) v_tune17[2 * i] = (uint16_t)(100 + i), v_tune17[2 * i + 1] = 10;
        app_key(&a, APP_KEY_3, t + 600);
        int notes = 0;
        uint16_t last = 0;
        for (uint32_t u = t + 600; u < t + 900; u += 10) {
            app_tick(&a, &pin, u);
            if (a.tone >= 0) notes++, last = a.tone_hz, a.tone = -1;
        }
        expect(notes == 16 && last == 115, "v1.2 (4): at most 16 notes, copied");
        app_key(&a, APP_KEY_2, t + 950);
        app_key(&a, APP_KEY_4, t + 960);
        expect(a.tune_n == 0, "v1.2 (4): app_tune(NULL, 0) stops what is playing");
        app_key(&a, APP_KEY_2, t + 1000);
        a.tone = -1;
        press(&a, "C");
        app_tick(&a, &pin, now + 200);
        app_tick(&a, &pin, now + 400);
        expect(!app_running(&a) && a.tone < 0 && a.tune_n == 0, "v1.2 (4): C (the exit) stops the tune, nothing left playing");
        /* (2) a 300-byte save: kept through an exit, given back whole; one with no save_size gets 64 */
        memset(&rec2, 0, sizeof rec2);
        press(&a, "BLUE 0 k:0");
        bool all = rec2.load_n == 300;
        for (int i = 0; i < 300 && all; i++) all = rec2.loaded[i] == (uint8_t)(i * 7 + 1);
        expect(all, "v1.2 (2): save_size 300: its save back, all 300 bytes");
        press(&a, "C");
        memset(&rec2, 0, sizeof rec2);
        press(&a, "BLUE 0 k:1 C BLUE 0 k:1");
        expect(rec2.load_n == APP_SAVE_SIZE, "v1.2 (2): no save_size: 64, as before");
        press(&a, "C");
        a.notice = NULL;
        press(&a, "BLUE 0 k:2");
        expect(!app_running(&a) && a.notice, "v1.2 (2): a save_size past APP_SAVE_MAX: not started");
        /* the saves' image round trip, and a version-1 image (8 x 64) loads unchanged */
        static uint8_t img[APP_SAVES_IMG_MAX];
        size_t n = app_saves_image(&a, img, sizeof img);
        app_state b;
        static uint8_t bsaves[APP_SAVES_IMG_MAX], brun[APP_SAVE_MAX];  /* b's own (fresh eyes: the host's shared ones
                                                   let a load that copied nothing pass) */
        app_init(&b, &c, now);
        app_save_buffers(&b, bsaves, sizeof bsaves, brun, sizeof brun);
        app_ctx_buffer(&b, ctxbuf, sizeof ctxbuf);
        app_apps(&b, vlist, 3);
        expect(n > 0 && app_saves_load(&b, img, n), "v1.2 (2): the saves' image loads back");
        memset(&rec2, 0, sizeof rec2);
        press(&b, "BLUE 0 k:0");
        expect(rec2.load_n == 300 && rec2.loaded[299] == (uint8_t)(299 * 7 + 1), "v1.2 (2): and gives the 300 bytes back");
        press(&b, "C");
        static uint8_t v1[6 + 8 * 69 + 4];
        memcpy(v1, "ABAP", 4);
        v1[4] = 1, v1[5] = 8;
        uint32_t h = 2166136261u;
        for (const char *q = "V64"; *q; q++) h = (h ^ (uint8_t)*q) * 16777619u;
        for (int k = 0; k < 4; k++) v1[6 + k] = (uint8_t)(h >> (8 * k));
        v1[10] = 1;
        for (int i = 0; i < 64; i++) v1[11 + i] = (uint8_t)(200 - i);
        uint32_t crc = ab_crc32(v1, sizeof v1 - 4);
        for (int k = 0; k < 4; k++) v1[sizeof v1 - 4 + k] = (uint8_t)(crc >> (8 * k));
        app_init(&b, &c, now);
        memset(bsaves, 0, sizeof bsaves);
        app_save_buffers(&b, bsaves, sizeof bsaves, brun, sizeof brun);
        app_ctx_buffer(&b, ctxbuf, sizeof ctxbuf);
        app_apps(&b, vlist, 3);
        expect(app_saves_load(&b, v1, sizeof v1), "v1.2 (2): a version-1 saves image loads");
        memset(&rec2, 0, sizeof rec2);
        press(&b, "BLUE 0 k:1");
        expect(rec2.load_n == 64 && rec2.loaded[0] == 200 && rec2.loaded[63] == 137, "v1.2 (2): its save unchanged");
        press(&b, "C");
        {   /* a v1 image too big for the buffer changes nothing; a zero-length record is refused (fresh eyes) */
            static uint8_t tiny[20], z[6 + 6 + 4];
            size_t had = b.saves_len;
            uint8_t *keep = b.saves;
            size_t keepcap = b.saves_cap;
            b.saves = tiny, b.saves_cap = sizeof tiny, b.saves_len = 0;
            bool refused = !app_saves_load(&b, v1, sizeof v1) && b.saves_len == 0;
            b.saves = keep, b.saves_cap = keepcap, b.saves_len = had;
            memcpy(z, "ABAP", 4);
            z[4] = 2, z[5] = 1;
            memset(z + 6, 0, 6);
            uint32_t zc = ab_crc32(z, sizeof z - 4);
            for (int k = 0; k < 4; k++) z[sizeof z - 4 + k] = (uint8_t)(zc >> (8 * k));
            expect(refused && !app_saves_load(&b, z, sizeof z) && b.saves_len == had,
                   "v1.2 (2): a v1 image that doesn't fit, or a zero-length record: refused, nothing changed");
            static uint8_t big[6 + 6 + APP_SAVE_MAX + 1 + 4];   /* one record of APP_SAVE_MAX + 1 bytes (abacus #4889) */
            memset(big, 0x5A, sizeof big);
            memcpy(big, "ABAP", 4);
            big[4] = 2, big[5] = 1;
            big[10] = (uint8_t)((APP_SAVE_MAX + 1) & 0xFF), big[11] = (uint8_t)((APP_SAVE_MAX + 1) >> 8);
            uint32_t bc = ab_crc32(big, sizeof big - 4);
            for (int k = 0; k < 4; k++) big[sizeof big - 4 + k] = (uint8_t)(bc >> (8 * k));
            expect(!app_saves_load(&b, big, sizeof big) && b.saves_len == had,
                   "v1.2 (2): a record past APP_SAVE_MAX: the image refused whole (a save is taken whole or not at all)");
        }
    }

    /* apps.h v1.2 item 5 (abacus #4734): APP_KEY_* name every key by the layout (app_keys.h, generated as keymap.c,
       gen_keymap.py --check failing on a stale one); each agrees with the keymap's reading of that key */
    {
        static const int DIG[10] = {APP_KEY_0, APP_KEY_1, APP_KEY_2, APP_KEY_3, APP_KEY_4, APP_KEY_5, APP_KEY_6,
                                    APP_KEY_7, APP_KEY_8, APP_KEY_9};
        bool ok = true;
        for (int d = 0; d < 10; d++) ok = ok && km_lookup(DIG[d], 0)->kind == KM_OP && km_lookup(DIG[d], 0)->op == AB_K0 + d;
        static const int SOFT[6] = {APP_KEY_SOFT1, APP_KEY_SOFT2, APP_KEY_SOFT3, APP_KEY_SOFT4, APP_KEY_SOFT5, APP_KEY_SOFT6};
        for (int i = 0; i < 6; i++) ok = ok && km_lookup(SOFT[i], 0)->kind == KM_SOFT && km_lookup(SOFT[i], 0)->arg == i;
        ok = ok && km_lookup(APP_KEY_ENTER, 0)->op == AB_ENTER && km_lookup(APP_KEY_DOT, 0)->op == AB_KPOINT &&
             km_lookup(APP_KEY_PLUS, 0)->op == AB_ADD && km_lookup(APP_KEY_MINUS, 0)->op == AB_SUB &&
             km_lookup(APP_KEY_TIMES, 0)->op == AB_MUL && km_lookup(APP_KEY_DIVIDE, 0)->op == AB_DIV &&
             km_lookup(APP_KEY_BACK, 0)->op == AB_KBKSP && km_lookup(APP_KEY_XEQ, 0)->op == AB_XEQ &&
             km_lookup(APP_KEY_RS, 0)->op == AB_RS && km_lookup(APP_KEY_EXIT, 0)->op == AB_CANCEL &&
             km_lookup(APP_KEY_GOLD, 0)->kind == KM_SHIFT && km_lookup(APP_KEY_BLUE, 0)->kind == KM_SHIFT &&
             km_lookup(APP_KEY_GOLD, 0)->arg == 1 && km_lookup(APP_KEY_BLUE, 0)->arg == 2;
        expect(ok && APP_KEY_EXIT == 42 && APP_KEY_4 == 31, "apps.h v1.2 (5): APP_KEY_* agree with the keymap");
    }

    /* unit 019: every menu on a key opens with its items on the soft keys (CONST shows its first
       five constants and "more"); in 35s mode, where every menu opens (unit 041 A, decision 58) */
    for (int k = 0; k < KB_KEYS; k++)
        for (int sh = 1; sh < 3; sh++) {
            const km_action *act = km_lookup(k, sh);
            if (act->kind != KM_MENU || act->arg < 0) continue;
            const km_menu *mn = km_menu_at(act->arg);
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, 35);
            tap(&a, shift_key(sh));
            tap(&a, k);
            app_ui(&a, &ui);
            bool ok = true;
            const char *tools[8];                   /* unit 072: APPS lists the tools, then GAMES (any game listed) */
            int nt = 0, ng = 0;
            for (int r = 0; r < app_registry_n; r++) {
                if (app_registry_folder[r] == APP_FOLDER_GAMES) ng++;
                else if (nt < 8) tools[nt++] = app_registry[r]->name;
            }
            if (ng && nt < 8) tools[nt++] = "GAMES";
            for (int i = 0; i < 6; i++) {
                const char *want = mn->paged == KM_PAGED_APPS ? (i < 5 ? (i < nt ? tools[i] : NULL) : "more")
                                   : mn->paged ? (i < 5 ? ab_const_symbol(i) : "more") : mn->item[i].legend;
                if (!want) {                            /* (APPS: fewer apps than five) */
                    ok = ok && (!ui.soft[i] || !*ui.soft[i]);
                    continue;
                }
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
        static const struct { const char *keys; double x; const char *what; int m35; } S[] = {   /* m35: 041 A */
            {"1 BLUE LN k:0", 1.1752011936438014, "HYP SINH 1", 0},
            {"1 \xce\xa3+ 3 \xce\xa3+ BLUE \xce\xa3+ k:0", 2, "STAT x-bar of 1 and 3", 0},
            {"2 ENTER 1 \xce\xa3+ 4 ENTER 2 \xce\xa3+ BLUE + k:3", 2, "L.R. m of (1,2) (2,4)", 0},
            {"2 ENTER 1 \xce\xa3+ 4 ENTER 2 \xce\xa3+ GOLD + k:0", 2, "SUMS n", 0},
            {"5 ENTER 2 BLUE \xc3\x97 k:0", 10, "PROB Cn,r 5 2", 0},
            {"1 ENTER 2 GOLD 6 k:2 1 0", 10, "x?y: a test from the keys leaves the stack", 0},
            {"1 0 BLUE RCL k:4", 25.4, "CONV 10 in to cm", 0},
            {"1 GOLD 1 k:2", 1.609344, "UNITS 1 mile to km", 1},
            {"1 BLUE SIN k:4", 57.295779513082321, "ANGLE 1 rad to degrees", 0},
            {"5 BLUE R\xe2\x86\x93 k:0", 0, "STACK CLSTK", 1},
            {"2 . 5 BLUE y\xcb\xa3 k:0", 2, "POW IP 2.5", 0},
            {"GOLD RCL k:0", 299792458, "CONST c", 0},
        };
        for (size_t i = 0; i < sizeof S / sizeof S[0]; i++) {
            ab_init(&c);
            app_init(&a, &c, now);
            if (S[i].m35) ab_do_arg(&c, AB_MODEL, 35);  /* hidden in 33s mode (unit 041 A, decision 58) */
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
        {   /* a keyboard test shows YES or NO (33s p.13-7, 35s p.14-7/14-8; abacus #8495, #8497) as a VIEW's display:
               C or backspace only clears it, any other key clears it and acts, in 35s mode too (no error, so no
               017 rule 4 swallow); in program entry the test is a line, and nothing shows */
            static screen_page pg;
            static uint8_t img[1 << 18];
            ab_init(&c);
            app_init(&a, &c, now);
            press(&a, "1 ENTER 2 GOLD 6 k:3");
            app_ui(&a, &ui);
            screen_lines(&c, &ui, &pg);
            expect(pg.x.kind == SCREEN_VIEW && !strcmp(pg.x.text, "YES") && x_of(&c) == 2,
                   "x>y? with y 1, x 2: YES on the X line, X still 2");
            press(&a, "GOLD 6 k:2");
            app_ui(&a, &ui);
            screen_lines(&c, &ui, &pg);
            expect(pg.x.kind == SCREEN_VIEW && !strcmp(pg.x.text, "NO"), "x<y?: NO");
            size_t n = ab_state_save(&c, img, sizeof img);
            {
                static ab_calc t;
                ab_init(&t);
                expect(n > 0 && n <= sizeof img && ab_state_load(&t, img, n) == ABN_OK && t.viewing &&
                       t.view_id == AB_VIEW_TEST && !t.yes, "the NO display is in the image, and loads");
            }
            press(&a, "C");
            app_ui(&a, &ui);
            screen_lines(&c, &ui, &pg);
            expect(pg.x.kind != SCREEN_VIEW && x_of(&c) == 2 && !c.entry.active, "C only clears YES / NO: X 2");
            press(&a, "GOLD 6 k:3 \xe2\x86\x90");
            app_ui(&a, &ui);
            screen_lines(&c, &ui, &pg);
            expect(pg.x.kind != SCREEN_VIEW && x_of(&c) == 2, "backspace only clears it too: X 2, not cleared");
            press(&a, "GOLD 6 k:3 +");
            app_ui(&a, &ui);
            screen_lines(&c, &ui, &pg);
            expect(pg.x.kind != SCREEN_VIEW && x_of(&c) == 3, "a test then +: the display goes and + adds (1 + 2)");
            ab_do_arg(&c, AB_MODEL, 35);
            press(&a, "0 BLUE 5 k:3");
            app_ui(&a, &ui);
            screen_lines(&c, &ui, &pg);
            expect(pg.x.kind == SCREEN_VIEW && !strcmp(pg.x.text, "NO"), "35s mode: x>0? of 0: NO");
            press(&a, "+");
            expect(x_of(&c) == 3 && !c.viewing, "35s mode: the next key acts (3 + 0), not swallowed");
            ab_do_arg(&c, AB_MODEL, 33);
            press(&a, "GOLD 9 k:0 GOLD 6 k:3");
            app_ui(&a, &ui);
            screen_lines(&c, &ui, &pg);
            expect(c.prgm && c.prog_len == 1 && pg.x.kind != SCREEN_VIEW && !c.viewing,
                   "in program entry the test is a line: no YES / NO");
            press(&a, "GOLD 9 k:0");
        }
        /* BASE, and BASE > LOGIC > AND (12 AND 10 is 8); LOGIC, the suffixes and CMPLX's i are 35s mode's (unit 041
           A, decision 58) */
        ab_do_arg(&c, AB_MODEL, 35);
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
    ab_do_arg(&c, AB_MODEL, 35);                /* the 35s's 41 (in 33s mode the 33s's 40: unit 041 B) */
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
        ab_do_arg(&t, AB_MODEL, 35);            /* the 41st is 35s mode's (unit 041 B) */
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
        static const char *const LIST[6] = {"\xe2\x96\xb2", "\xe2\x96\xbc", "EDIT", "NEW", NULL, "EXIT"};   /* 073 */
        static const char *const TYPE[6] = {"\xe2\x97\x80", "\xe2\x96\xb6", "( )", "=", "SPC", "REG"};   /* REG boxed (#3958) */
        bool ok;
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "GOLD 0");
        app_ui(&a, &ui);
        ok = c.eqn_mode;
        for (int i = 0; i < 6; i++) ok = ok && (LIST[i] ? ui.soft[i] && !strcmp(ui.soft[i], LIST[i]) : !ui.soft[i] || !ui.soft[i][0]);
        expect(ok, "EQN: the list's bar, \xe2\x96\xb2 \xe2\x96\xbc EDIT NEW - EXIT (073: EXIT back; ( ) on GOLD's F6)");
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
        ab_do_arg(&c, AB_MODEL, 35);            /* the built-ins are 35s mode's (unit 041 A4, decision 58) */
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

        /* C leaves with the list unchanged (EXIT's soft key, the same route, went for ( ): unit 046) */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_eqn_add(&c, "B+2");
        press(&a, "k:1 k:1");
        int count = c.eqn_count;
        press(&a, "C");
        app_ui(&a, &ui);
        expect(!c.eqn_mode && c.eqn_count == count && !ui.soft[0], "C: Equation mode off, the list as it was");
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
        ab_do_arg(&c, AB_SETALL, 0);               /* VIEW's text in ALL; unit 067's FIX 4 is checked below */
        press(&a, "5 STO ltr:A 7 GOLD STO ltr:A");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(pg.x.kind == SCREEN_VIEW && !strcmp(pg.x.text, "A=5") && !pg.x.label && x_of(&c) == 7,
               "VIEW A: A=5 on the X line, X still 7");
        {   /* unit 067: a new calculator shows FIX 4, on the X line and in VIEW */
            static ab_calc n;
            static app_state na;
            static screen_ui nui;
            static screen_page np;
            ab_init(&n);
            app_init(&na, &n, now);
            app_ui(&na, &nui);
            screen_lines(&n, &nui, &np);
            expect(!strcmp(np.x.text, "0.0000"), "067: a new calculator's X line is 0.0000 (FIX 4)");
            press(&na, "5 STO ltr:A 7 GOLD STO ltr:A");
            app_ui(&na, &nui);
            screen_lines(&n, &nui, &np);
            expect(np.x.kind == SCREEN_VIEW && !strcmp(np.x.text, "A=5.0000"), "067: and VIEW A shows A=5.0000");
        }
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
        ab_do_arg(&c, AB_MODEL, 35);            /* CMPLX's i is 35s mode's (unit 041 A1, decision 58) */
        press(&a, "C 3 BLUE +/\xe2\x88\x92 k:0 4 ENTER BLUE R\xe2\x86\x93 k:4");    /* 3 i 4 (CMPLX's i, blue +/-) */
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(ui.show_n == 2 && !strcmp(pg.t.text, "30000000000000000") && !strcmp(pg.z.text, "00000000000000000") &&
                   !strcmp(pg.y.text, "40000000000000000") && !strcmp(pg.x.text, "00000000000000000"),
               "030: a complex X: the real part on T and Z, the imaginary on Y and X");
        app_key_up(&a, 4, now);
    }

    /* unit 037: APPS (blue 0): the registry's KEYS; a test's seven apps paged; keys and key-ups to the app
       with the app's clock, C never; ticks at the app's rate; the core never called (the image the same);
       auto-off suspended; the app's save apart from the calculator's image, through MEMORY CLEAR */
    {
        static uint8_t ctxbuf[APP_CTX_MAX];
        static ls027_frame af;
        ab_init(&c);
        app_init(&a, &c, now);
        app_ctx_buffer(&a, ctxbuf, sizeof ctxbuf);
        press(&a, "BLUE 0");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "KEYS"), "037: APPS lists KEYS");
        press(&a, "k:0");
        expect(app_running(&a) && app_draw(&a, &af), "037: KEYS starts, its frame in place of the screen");
        press(&a, "C");
        expect(!app_running(&a), "037: C ends it");
        app_apps(&a, tlist, 7);
        press(&a, "BLUE 0");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "T1") && ui.soft[4] && !strcmp(ui.soft[4], "T5") && ui.soft[5] &&
                   !strcmp(ui.soft[5], "more"), "037: seven apps: five a page, then more");
        press(&a, "k:5");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "T6") && ui.soft[1] && !strcmp(ui.soft[1], "T7"), "037: the next page");
        uint32_t img0 = img_crc(&c);
        memset(&rec, 0, sizeof rec);
        press(&a, "k:1");                       /* T7 */
        expect(app_running(&a) && rec.loads == 1 && rec.save_n == 0 && rec.starts == 1 && rec.ticks == 1,
               "037: load (nothing saved yet), start, the first tick at once");
        uint32_t t = now;
        app_key(&a, 31, t + 120);               /* the 4 key, down then up */
        app_key_up(&a, 31, t + 180);
        expect(rec.keys == 2 && rec.key == 31 && !rec.down && rec.key_ms == 180, "037: keys and key-ups to the app, on its clock");
        app_key(&a, APP_KEY_GOLD, t + 190);     /* GOLD is an app's key like any other: the games jump and fire on it */
        expect(rec.keys == 3 && rec.key == APP_KEY_GOLD && rec.down && a.shift == 0, "037: GOLD to the app, no shift taken");
        app_key_up(&a, APP_KEY_GOLD, t + 195);
        expect(rec.keys == 4 && rec.key == APP_KEY_GOLD && !rec.down && a.shift == 0, "037: GOLD's release to the app");
        int draws = rec.draws;
        app_key(&a, 37, t + 200);               /* the 1 key: no redraw asked */
        expect(rec.draws == draws, "037: a key returning false is not drawn");
        app_key(&a, 38, t + 210);               /* the 2 key: a tone */
        expect(a.tone >= 0 && a.tone_use == DEV_USE_APP && a.tone_hz == 880 && a.tone_ms == 50,
               "037: app_beep sounds through the buzzer (its hz and ms, the APP use)");
        a.tone = -1;
        a.set.uses &= (uint8_t)~DEV_USE_APP;    /* (SETUP > BEEP > the second page's APP off) */
        app_key(&a, 38, t + 220);
        expect(a.tone < 0, "037: with the APP use off, app_beep is silent");
        a.set.uses |= DEV_USE_APP;
        expect(app_ticking(&a) && app_next_wake(&a, &(pwr_inputs){false, false, 80, false}, t + 210) <= 50,
               "037: ticks at the app's rate (50 ms)");
        a.set.off_min = 2;
        bool off = false;
        for (uint32_t ms = 260; ms < 260 + 3 * 60000; ms += 50)     /* past auto-off (2 min), short of twice it */
            off = off || app_tick(&a, &(pwr_inputs){false, false, 80, false}, t + ms) == APP_OFF;
        expect(!off && rec.ticks > 3000 && rec.tick_ms == 50, "037: auto-off suspended while an app runs; ticks at 50 ms");
        int keys = rec.keys;
        app_key(&a, 42, t + 300000);            /* C */
        app_key_up(&a, 42, t + 300010);
        expect(!app_running(&a) && rec.keys == keys && rec.saves == 1 && rec.stops == 1 && img_crc(&c) == img0,
               "037: C ends it, never delivered; saved, stopped; the calculator's image the same");
        /* the save: apart from the image, through a store image and back, and through MEMORY CLEAR */
        uint8_t sv[1024];
        size_t n = app_saves_image(&a, sv, sizeof sv);
        static app_state a3;
        app_init(&a3, &c, now);
        app_apps(&a3, tlist, 7);
        app_ctx_buffer(&a3, ctxbuf, sizeof ctxbuf);
        expect(n > 0 && app_saves_load(&a3, sv, n), "037: the apps' saves through a store image");
        ab_memory_clear(&c);
        memset(&rec, 0, sizeof rec);
        press(&a3, "BLUE 0 k:5 k:1");
        expect(app_running(&a3) && rec.save_n > 0 && rec.loads == 1, "037: T7's save comes back after MEMORY CLEAR");
        press(&a3, "C");
        a.set.off_min = 10;
        /* abacus #4639: the idle limit. After twice the device's auto-off time with no key the app is saved and
           stopped, and the calculator's auto-off resumes (the device turns off); a key short of it keeps it going */
        {
            static app_state a5;
            uint32_t t0 = now + 1000;
            app_init(&a5, &c, t0);
            app_apps(&a5, tlist, 7);
            app_ctx_buffer(&a5, ctxbuf, sizeof ctxbuf);
            a5.set.off_min = 2;
            memset(&rec, 0, sizeof rec);
            app_key(&a5, 36, t0);               /* BLUE 0 k:0: APPS, T1 */
            app_key(&a5, 43, t0 + 10);
            app_key(&a5, 0, t0 + 20);
            bool off5 = false, run_at_7 = false;
            const pwr_inputs pin5 = {true, true, 80, false};       /* on the cell: on USB there is no auto-off (068) */
            for (uint32_t ms = 30; ms < 7 * 60000; ms += 50) {
                if (ms == 3 * 60000 + 30) app_key(&a5, 37, t0 + ms);     /* a key at 3 min: the clock restarts */
                if (ms == 7 * 60000 - 20) run_at_7 = app_running(&a5);
                off5 = off5 || app_tick(&a5, &pin5, t0 + ms) == APP_OFF;
            }
            bool alive_at_7 = run_at_7 && !off5 && rec.saves == 0;
            uint32_t off_at = 0;
            for (uint32_t ms = 7 * 60000; ms < 12 * 60000 && !off5; ms += 50)
                if ((off5 = app_tick(&a5, &pin5, t0 + ms) == APP_OFF)) off_at = ms;
            /* the app stops at 7 min (the last key at 3 min, plus twice 2 min); the calculator's auto-off then resumes
               from there, so the device turns off 2 min later */
            expect(alive_at_7 && off5 && off_at >= 9 * 60000 - 100 && off_at <= 9 * 60000 + 100 && !app_running(&a5) &&
                       rec.saves == 1 && rec.stops == 1,
                   "idle limit: twice auto-off with no key saves and stops the app; auto-off resumes, off 2 min later");
        }
        /* abacus #4661: a host may turn the idle limit off (tally's phone never auto-offs; a paused game stays) */
        {
            static app_state a6;
            uint32_t t1 = now + 1000;
            app_init(&a6, &c, t1);
            app_apps(&a6, tlist, 7);
            app_ctx_buffer(&a6, ctxbuf, sizeof ctxbuf);
            a6.set.off_min = 2;
            bool dflt = a6.idle_limit;
            a6.idle_limit = false;
            app_key(&a6, 36, t1);
            app_key(&a6, 43, t1 + 10);
            app_key(&a6, 0, t1 + 20);
            const pwr_inputs pin6 = {false, false, 80, false};
            for (uint32_t ms = 30; ms < 10 * 60000; ms += 50) app_tick(&a6, &pin6, t1 + ms);
            expect(dflt && app_running(&a6), "idle limit off (a host's switch): the app still runs after 10 min, no key");
            app_key(&a6, 42, t1 + 10 * 60000);
        }
        /* apps.h v1.1 (Babal's merge): an app may declare up to 256 KB (Babal 164,656 bytes, its whole level and
           log); one byte more is refused, NO MEMORY */
        {
            static const app_def big[2] = {{"BIG", 164656, t_load, t_start, t_key, t_tick, t_draw, t_save, t_stop, 0},
                                           {"OVER", 256 * 1024 + 1, t_load, t_start, t_key, t_tick, t_draw, t_save, t_stop, 0}};
            static const app_def *const blist[2] = {&big[0], &big[1]};
            static app_state a4;
            app_init(&a4, &c, now);
            app_apps(&a4, blist, 2);
            app_ctx_buffer(&a4, ctxbuf, sizeof ctxbuf);
            press(&a4, "BLUE 0 k:0");
            bool bigok = app_running(&a4);
            press(&a4, "C BLUE 0 k:1");
            app_ui(&a4, &ui);
            expect(APP_CTX_MAX == 256 * 1024 && bigok && !app_running(&a4) && ui.notice && !strcmp(ui.notice, "NO MEMORY"),
                   "v1.1: a 164,656-byte app starts; one past 256 KB is refused");
            press(&a4, "C");
        }
    }

    /* unit 038: integer powers up to 64 either way are exact then rounded once, never the slow high-precision
       route (a cubic's graph took 10.8 s on the S3) */
    {
        ab_init(&c);
        unsigned long before = fn_pow_hp_calls;
        abn y3, want;
        abn_from_text(&c.x.re, "3");
        abn_from_text(&c.y.re, "1.234567890123456789012345678901234");
        c.x.kind = c.y.kind = AB_REAL;
        ab_do(&c, AB_POW);
        y3 = c.x.re;
        abn_from_text(&want, "1.881676372353657772546716040595284");   /* 1.2345...^3: mpmath at 80 digits, rounded to 34 */
        abn_from_text(&c.x.re, "-5");
        abn_from_text(&c.y.re, "7.654321098765432109876543210987654");
        ab_do(&c, AB_POW);
        expect(fn_pow_hp_calls == before && abn_cmp(&y3, &want) == 0, "038: x^3 and x^-5 of 34-digit bases: exact, then rounded once");
    }

    /* unit 036: GTO . from the keyboard (33s p.13-5, 35s p.14-6): . . to PRGM TOP; . label and the line's
       digits (three in 35s and STU mode, four in 33s mode) to that line; nothing recorded; C and backspace
       back out */
    {
        /* LBL A, 1, R/S, 2, RTN: XEQ A stops at the R/S */
        ab_init(&c);
        app_init(&a, &c, now);
        c.m35 = true;
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 0);
        ab_do(&c, AB_K1);
        ab_do(&c, AB_RS);
        ab_do(&c, AB_K2);
        ab_do(&c, AB_RTN);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_XEQ, 0);
        int lines = c.prog_len;
        expect(c.stop == AB_STOP_STOP && c.pc > 0, "036: a program stopped at its R/S");
        press(&a, "GOLD 4 .");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "GTO ._"), "036: . at the GTO prompt opens the address form");
        press(&a, ".");
        expect(c.pc == AB_PC_TOP && c.prog_len == lines && !a.prompt_text[0], "036: . . goes to PRGM TOP");
        press(&a, "GOLD 9 k:0 GOLD XEQ ltr:B GOLD 9 k:0");
        expect(c.prog[0].op == AB_LBL && c.prog[0].arg == 1, "036: a label keyed after GTO . . lands at the top");
        /* in program entry: moves the pointer, records nothing */
        press(&a, "GOLD 9 k:0 GOLD 4 . ltr:A 0 0 2");
        char t[32];
        ab_prog_addr(&c, c.pc, t);
        expect(c.prgm && c.prog_len == lines + 1 && !strcmp(t, "A002"), "036: GTO . A002 in program entry: the pointer there, no line");
        press(&a, "GOLD 4 . .");
        expect(c.prgm && c.pc == AB_PC_TOP && c.prog_len == lines + 1, "036: GTO . . in program entry: TOP, no line");
        press(&a, "GOLD 9 k:0");
        /* a missing line: NONEXISTENT */
        press(&a, "GOLD 4 . ltr:A 0 0 9");
        expect(c.msg && c.err_status == ABN_NONEXISTENT, "036: a missing line: NONEXISTENT");
        press(&a, "C");
        /* 33s mode: four digits */
        c.m35 = false;
        press(&a, "GOLD 4 . ltr:A 0 0 0");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "GTO .A000_"), "036: 33s mode: four digits (A0005)");
        press(&a, "3");
        ab_prog_addr(&c, c.pc, t);
        expect(!strcmp(t, "A003"), "036: 33s mode: GTO . A0003 goes to A003");
        c.m35 = true;
        /* backing out: backspace steps back a digit, the letter, the point; C leaves */
        press(&a, "GOLD 4 . ltr:A 0 \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "GTO .A___"), "036: backspace removes a digit");
        press(&a, "\xe2\x86\x90 \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "GTO _"), "036: backspace steps back past the letter and the point");
        int pc0 = c.pc;
        press(&a, ". C");
        app_ui(&a, &ui);
        expect(!ui.prompt && c.pc == pc0, "036: C backs out of the address form, nothing moved");
        /* in program entry, the edited label's mode counts the digits, as the letter form (abacus #8776, H52): a 33s
           label edited in 35s mode takes four, a 35s label in 33s mode three */
        c.lmode[0] = 33;
        press(&a, "GOLD 9 k:0");
        ab_do_arg(&c, AB_GTOPOS, 2);           /* A002: inside LBL A (a label's code times 1000, A is 0) */
        press(&a, "GOLD 4 . ltr:A 0 0 0");
        app_ui(&a, &ui);
        expect(c.prgm && ui.prompt && !strcmp(ui.prompt, "GTO .A000_"), "075: a 33s label in 35s mode: GTO . takes four digits");
        press(&a, "3");
        ab_prog_addr(&c, c.pc, t);
        expect(!strcmp(t, "A003"), "075: a 33s label in 35s mode: GTO . A0003 goes to A003");
        c.lmode[0] = 35;
        c.m35 = false;
        press(&a, "GOLD 4 . ltr:A 0 0 4");
        ab_prog_addr(&c, c.pc, t);
        app_ui(&a, &ui);
        expect(!ui.prompt && !strcmp(t, "A004"), "075: a 35s label in 33s mode: GTO . A004, three digits");
        c.m35 = true;
        press(&a, "GOLD 9 k:0");
    }

    /* unit 036b: in 35s and STU mode GTO and XEQ take a letter, then ENTER (its first line) or three digits
       (35s p.14-6, p.13-10); 33s mode acts on the letter (33s p.13-5); backspace steps back, C backs out */
    {
        /* LBL A, 1, R/S, 2, RTN: A001..A005; LBL B, 3, RTN: B001..B003 */
        ab_init(&c);
        app_init(&a, &c, now);
        c.m35 = true;
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 0);
        ab_do(&c, AB_K1);
        ab_do(&c, AB_RS);
        ab_do(&c, AB_K2);
        ab_do(&c, AB_RTN);
        ab_do_arg(&c, AB_LBL, 1);
        ab_do(&c, AB_K3);
        ab_do(&c, AB_RTN);
        ab_do(&c, AB_PRGM);
        int lines = c.prog_len, pc0 = c.pc;
        char t[32];
        press(&a, "XEQ ltr:A");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "XEQ A___") && c.pc == pc0 && c.stop == AB_STOP_NONE && !a.running,
               "036b: 35s XEQ A waits for ENTER or the line");
        press(&a, "ENTER");
        run_out(&a, &in);
        expect(!a.prompt_text[0] && c.stop == AB_STOP_STOP && x_of(&c) == 1, "036b: 35s XEQ A ENTER runs from A001");
        press(&a, "XEQ ltr:A 0 0");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "XEQ A00_") && c.stop == AB_STOP_STOP, "036b: the digits so far");
        press(&a, "4");
        run_out(&a, &in);
        expect(!a.prompt_text[0] && c.stop != AB_STOP_STOP && x_of(&c) == 2, "036b: 35s XEQ A 0 0 4 runs from A004 (past the R/S)");
        press(&a, "GOLD 4 ltr:A");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "GTO A___"), "036b: 35s GTO A waits");
        press(&a, "ENTER");
        ab_prog_addr(&c, c.pc, t);
        expect(!a.prompt_text[0] && !strcmp(t, "A001"), "036b: 35s GTO A ENTER: A001");
        press(&a, "GOLD 4 ltr:A 0 0 4");
        ab_prog_addr(&c, c.pc, t);
        expect(!a.prompt_text[0] && !strcmp(t, "A004") && c.prog_len == lines, "036b: 35s GTO A 0 0 4: A004");
        press(&a, "XEQ ltr:B ENTER");
        run_out(&a, &in);
        expect(x_of(&c) == 3 && c.stop != AB_STOP_STOP, "036b: XEQ B ENTER runs B (the label in the address)");
        press(&a, "GOLD 4 ltr:B 0 0 2");
        ab_prog_addr(&c, c.pc, t);
        expect(!strcmp(t, "B002"), "036b: GTO B 0 0 2: B002");
        pc0 = c.pc;
        press(&a, "XEQ ltr:A 0 ENTER");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "XEQ A0__") && c.pc == pc0 && !a.running, "036b: ENTER after a digit: nothing, the digits wait");
        press(&a, "C");
        press(&a, "GOLD 4 ltr:A 0 0 9");
        expect(c.msg && c.err_status == ABN_NONEXISTENT, "036b: a missing line: NONEXISTENT");
        press(&a, "C");
        pc0 = c.pc;
        press(&a, "XEQ ltr:A 0 0 0");
        expect(c.msg && c.err_status == ABN_NONEXISTENT && c.pc == pc0 && !a.running, "036b: XEQ A 0 0 0: no line 000, NONEXISTENT, nothing run");
        press(&a, "C GOLD 9 k:0 GOLD 4 ltr:A 0 0 0");
        expect(c.prog_len == lines && c.msg && c.err_status == ABN_NONEXISTENT, "036b: GTO A 0 0 0 in program entry: NONEXISTENT, no line");
        press(&a, "C GOLD 9 k:0");
        /* backspace steps back a digit, then the letter; C leaves, nothing moved */
        pc0 = c.pc;
        press(&a, "XEQ ltr:A 0 \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "XEQ A___"), "036b: backspace removes a digit");
        press(&a, "\xe2\x86\x90");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "XEQ _"), "036b: backspace steps back past the letter");
        press(&a, "ltr:A C");
        app_ui(&a, &ui);
        expect(!ui.prompt && c.pc == pc0 && c.stop == AB_STOP_NONE, "036b: C backs out, nothing run or moved");
        /* in program entry: the lines XEQ A001 and GTO A003 */
        press(&a, "GOLD 9 k:0 XEQ ltr:A ENTER GOLD 4 ltr:B 0 0 3 GOLD 9 k:0");
        expect(c.prog_len == lines + 2 && c.pc >= 1 && c.prog[c.pc - 1].op == AB_XEQ && c.prog[c.pc - 1].arg == 1 &&
                   c.prog[c.pc].op == AB_GTO && c.prog[c.pc].arg == 1003,
               "036b: in program entry, XEQ A ENTER and GTO B 0 0 3 are the lines XEQ A001 and GTO B003");
        /* STU behaves as the 35s */
        c.stu = true;
        press(&a, "XEQ ltr:A");
        expect(!strcmp(a.prompt_text, "XEQ A___") && c.stop == AB_STOP_NONE, "036b: STU XEQ A waits");
        press(&a, "ENTER");
        run_out(&a, &in);
        expect(c.stop == AB_STOP_STOP, "036b: STU XEQ A ENTER runs");
        press(&a, "C GOLD 4 ltr:A 0 0 2");
        ab_prog_addr(&c, c.pc, t);
        expect(!strcmp(t, "A002"), "036b: STU GTO A 0 0 2: A002");
        /* 33s mode: the letter acts at once */
        c.stu = false;
        c.m35 = false;
        press(&a, "C GOLD 4 ltr:A");
        ab_prog_addr(&c, c.pc, t);
        expect(!a.prompt_text[0] && !strcmp(t, "A001"), "036b: 33s GTO A acts on the letter");
        press(&a, "XEQ ltr:A");
        run_out(&a, &in);
        expect(!a.prompt_text[0] && c.stop == AB_STOP_STOP, "036b: 33s XEQ A acts on the letter");
    }

    /* unit 039: 33s mode's CMPLX prefix (33s p.9-2): CMPLX shows "CMPLX_" with its menu kept on the soft keys
       (x i y stays, 039 rule 5); an operator key does the operation on the stack pairs; a soft key its menu
       item; C closes it; any other key closes it and acts, as a menu does. 35s and STU keep the menu alone */
    {
        static const char *const CXNAME[AB_CXP_N] = {"CMPLX +", "CMPLX -", "CMPLX \xc3\x97", "CMPLX \xc3\xb7",
                                                     "CMPLXy\xcb\xa3", "CMPLX +/-", "CMPLX1/x", "CMPLXLN",
                                                     "CMPLXe\xcb\xa3", "CMPLXSIN", "CMPLXCOS", "CMPLXTAN"};
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "3 ENTER 2 BLUE +/\xe2\x88\x92");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "CMPLX_") && a.menu >= 0, "039: 33s CMPLX shows CMPLX_, its menu kept");
        press(&a, "SIN");
        app_ui(&a, &ui);
        expect(fabs(x_of(&c) - 9.154499146911430) < 1e-9 && fabs(y_of(&c) + 4.168906959966564) < 1e-9 && !ui.prompt && a.menu < 0,
               "039: 33s CMPLX SIN: sin(2 + i3) on the pair, the prefix gone");
        press(&a, "C 1 ENTER 2 BLUE +/\xe2\x88\x92 +/\xe2\x88\x92");
        expect(x_of(&c) == -2 && c.y.kind == AB_REAL && y_of(&c) == -1,
               "039: 33s CMPLX +/-: the pair negated (the prefix's +/- is the operator, not CHS)");
        press(&a, "BLUE +/\xe2\x88\x92 C");
        app_ui(&a, &ui);
        expect(!ui.prompt && a.menu < 0 && x_of(&c) == -2, "039: C closes the prefix, nothing done");
        press(&a, "BLUE +/\xe2\x88\x92 5");
        app_ui(&a, &ui);
        expect(!ui.prompt && a.menu < 0 && c.entry.active, "039: another key closes the prefix and acts (5 typed)");
        press(&a, "C 1 BLUE +/\xe2\x88\x92 k:0 2 ENTER");      /* was 039's "33s mode keeps the menu's i": decision 58 */
        expect(c.x.kind == AB_REAL && x_of(&c) == 12, "041 A1: 33s mode: the menu's i does nothing (1 i 2 is 12)");
        /* a shifted operator is not the operator: GOLD LN is LOG, the prefix closed (039 review) */
        press(&a, "C C 1 0 0 BLUE +/\xe2\x88\x92 GOLD LN");
        app_ui(&a, &ui);
        expect(!ui.prompt && a.menu < 0 && x_of(&c) == 2, "039: under the prefix GOLD LN is LOG (log 100 = 2)");
        /* HEX: no prefix (the operations are decimal only); e^x's key types B as ever (039 review) */
        press(&a, "C");
        c.base = 16;
        press(&a, "1 ENTER BLUE +/\xe2\x88\x92");
        app_ui(&a, &ui);
        bool hexnp = !ui.prompt || strcmp(ui.prompt, "CMPLX_");
        press(&a, "e\xcb\xa3");
        expect(hexnp && c.entry.active && !c.msg, "039: HEX: no CMPLX_, and e^x's key types its digit");
        c.base = 10;
        /* the operators by key: + - x / y^x 1/x LN e^x COS TAN, each a CMPLX line in program entry */
        press(&a, "GOLD 9 k:0");
        static const char *const KEY[AB_CXP_N] = {"+", "\xe2\x88\x92", "\xc3\x97", "\xc3\xb7", "y\xcb\xa3", "+/\xe2\x88\x92", "1/x",
                                                  "LN", "e\xcb\xa3", "SIN", "COS", "TAN"};
        int ok = c.prgm, base = c.prog_len;
        for (int k = 0; k < AB_CXP_N; k++) {
            char keys[48];
            snprintf(keys, sizeof keys, "BLUE +/\xe2\x88\x92 %s", KEY[k]);
            press(&a, keys);
            char line[40];
            ab_prog_line(&c, c.pc, NULL, line, sizeof line);
            if (c.prog_len != base + k + 1 || c.prog[c.pc].op != AB_CMPLX || c.prog[c.pc].arg != k || strcmp(line, CXNAME[k])) {
                ok = 0;
                fprintf(stderr, "039: key %s: line %d op %d arg %d '%s'\n", KEY[k], c.prog_len, c.prog[c.pc].op, c.prog[c.pc].arg, line);
            }
        }
        expect(ok, "039: in program entry each CMPLX operator is a line, named as the 33s's index (p.G-6/G-7)");
        press(&a, "GOLD 9 k:0");
        /* an equation being typed or Equation mode's list: no prefix; the operator key types as ever */
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "GOLD 0 RCL ltr:A BLUE +/\xe2\x88\x92");
        app_ui(&a, &ui);
        bool np = !ui.prompt || strcmp(ui.prompt, "CMPLX_");
        press(&a, "+ 2");
        expect(np && c.eqn_typing && !strcmp(c.ebuf, "A+2"), "039: typing an equation, CMPLX + types + (no prefix)");
        press(&a, "ENTER");
        ab_val x0 = c.x, y0 = c.y;
        press(&a, "BLUE +/\xe2\x88\x92");
        app_ui(&a, &ui);
        np = !ui.prompt || strcmp(ui.prompt, "CMPLX_");
        press(&a, "SIN");
        expect(np && c.eqn_mode && !memcmp(&c.x, &x0, sizeof x0) && !memcmp(&c.y, &y0, sizeof y0),
               "039: Equation mode's list: no prefix, the stack untouched");
        /* an equation typed into a program (Equation mode off, the typing on): the + types too */
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "GOLD 9 k:0 GOLD 0 RCL ltr:A BLUE +/\xe2\x88\x92 + 2");
        expect(c.prgm && !c.eqn_mode && c.eqn_typing && !strcmp(c.ebuf, "A+2"), "039: an equation typed into a program: CMPLX + types +");
        /* 35s and STU: CMPLX is its menu only; SIN closes it and is the real sine */
        for (int m = 0; m < 2; m++) {
            ab_init(&c);
            app_init(&a, &c, now);
            c.m35 = true;
            c.stu = m == 1;
            c.angle = AB_ANGLE_RAD;
            press(&a, "3 ENTER 2 BLUE +/\xe2\x88\x92");
            app_ui(&a, &ui);
            bool noprefix = !ui.prompt || strcmp(ui.prompt, "CMPLX_");
            press(&a, "SIN");
            expect(noprefix && fabs(x_of(&c) - 0.9092974268256817) < 1e-12 && c.y.kind == AB_REAL && y_of(&c) == 3,
                   m ? "039: STU: CMPLX is the menu; SIN is sin 2" : "039: 35s: CMPLX is the menu; SIN is sin 2");
        }
    }

    /* unit 040: the open graph's soft keys in three pages (▸), a shift's bank; the digits move the cursor; a zoom
       redraws; the free cursor, the mark and BOXZ's box, TANL's line drawn; the readout in the status band */
    {
        static ls027_frame fr;
        static app_graph gbuf;
        const pwr_inputs idle = {false, false, 80, false};
        ab_init(&c);
        app_init(&a, &c, now);
        c.stu = c.m35 = true;
        ab_eqn_add(&c, "X^2-30");
        ab_do(&c, AB_EQN);
        app_graph_buffer(&a, &gbuf);
        press(&a, "BLUE 7 k:5");
        for (int i = 0; i < 200 && a.graphing; i++) app_tick(&a, &idle, now);
        app_ui(&a, &ui);
        bool p0 = ui.soft[0] && !strcmp(ui.soft[0], "\xe2\x97\x80") && ui.soft[2] && !strcmp(ui.soft[2], "TRACE") &&
                  ui.soft[5] && !strcmp(ui.soft[5], "\xe2\x96\xb8");
        press(&a, "k:5");
        app_ui(&a, &ui);
        bool p1 = ui.soft[0] && !strcmp(ui.soft[0], "ZIN") && ui.soft[4] && !strcmp(ui.soft[4], "ZAUTO");
        press(&a, "GOLD");
        app_ui(&a, &ui);
        bool p1g = ui.soft[4] && !strcmp(ui.soft[4], "ZLAST");
        press(&a, "GOLD BLUE");
        app_ui(&a, &ui);
        bool p1b = ui.soft[1] && !strcmp(ui.soft[1], "ZDECI");
        press(&a, "BLUE k:5");
        app_ui(&a, &ui);
        bool p2 = ui.soft[0] && !strcmp(ui.soft[0], "ROOT") && ui.soft[4] && !strcmp(ui.soft[4], "F(X)");
        press(&a, "k:5");
        app_ui(&a, &ui);
        bool back = ui.soft[2] && !strcmp(ui.soft[2], "TRACE");
        expect(p0 && p1 && p1g && p1b && p2 && back, "040: the graph's pages: the bar, ZOOM (GOLD, BLUE banks), FCN, round again");
        /* the digits: free cursor 10 right (GOLD 6), 1 up (8) */
        press(&a, "k:2 GOLD 6 8");
        expect(!c.gr_trace && c.gr_cur == 210 && c.gr_row == AB_GRAPH_CROW - 1, "040: TRACE off; GOLD 6 ten right, 8 one up");
        /* MARK, then the cursor 20 right and 20 up: the box drawn between them, the cross at the cursor */
        press(&a, "k:4 GOLD 9 GOLD 9");
        app_ui(&a, &ui);
        screen_render(&c, &ui, &fr);
        int x0 = 210, x1 = 230, y0 = 21 + AB_GRAPH_CROW - 21, y1 = 21 + AB_GRAPH_CROW - 1, boxbad = 0;
        for (int x = x0; x <= x1; x++)
            boxbad += !(fr.px[y0][x / 8] >> (x % 8) & 1) + !(fr.px[y1][x / 8] >> (x % 8) & 1);
        for (int y = y0; y <= y1; y++)
            boxbad += !(fr.px[y][x0 / 8] >> (x0 % 8) & 1) + !(fr.px[y][x1 / 8] >> (x1 % 8) & 1);
        expect(c.gr_mark && c.gr_cur == 230 && boxbad == 0, "040: MARK and the moved cursor: BOXZ's box drawn in the frame");
        /* the readout: the free cursor's x and y, in the status band */
        {
            screen_graph_text gt;
            screen_graph_texts(&c, &ui, &gt);
            expect(gt.readout[0] == 'x' && strstr(gt.readout, " y="), "040: the free cursor's readout is x= y=");
        }
        /* BOXZ on ZOOM's page: the window is the box, the graph drawn again */
        abn bx;
        ab_graph_x(&c, 210, &bx);
        press(&a, "k:5 k:2");
        expect(abn_cmp(&c.gr_xmin, &bx) == 0 && !c.gr_mark && a.graphing, "040: BOXZ: the box's window, redrawing");
        for (int i = 0; i < 200 && a.graphing; i++) app_tick(&a, &idle, now);
        /* ZLAST back; the GOLD bank's key */
        press(&a, "GOLD k:4");
        abn m10 = abn_from_int(-10);
        expect(abn_cmp(&c.gr_xmin, &m10) == 0, "040: GOLD ZLAST: the window before BOXZ");
        for (int i = 0; i < 200 && a.graphing; i++) app_tick(&a, &idle, now);
        /* FCN's ROOT near x = 5.5: the readout names it; ENTER would take it */
        press(&a, "k:5 5 k:2 GOLD 6 GOLD 6 GOLD 6 GOLD 6 GOLD 6 GOLD 6 GOLD 6 GOLD 6 GOLD 6 GOLD 6 k:0");
        {
            screen_graph_text gt;
            app_ui(&a, &ui);
            screen_graph_texts(&c, &ui, &gt);
            expect(c.gr_res_kind == AB_F_ROOT + 1 && !strncmp(gt.readout, "ROOT: 5.477", 11), "040: FCN ROOT near 5.5: ROOT: 5.477... read out");
        }
        /* TANL (GOLD bank): its line drawn: a dark pixel on y = m x + b in each column where it is in the window */
        press(&a, "GOLD k:0");
        for (int i = 0; i < 200 && a.graphing; i++) app_tick(&a, &idle, now);
        app_ui(&a, &ui);
        screen_render(&c, &ui, &fr);
        int on = 0, lbad = 0;
        for (int i = 0; i < AB_GRAPH_W; i++)
            if (gbuf.s.tl[i] >= 0) {
                on++;
                lbad += !(fr.px[21 + gbuf.s.tl[i]][i / 8] >> (i % 8) & 1);
            }
        expect(c.gr_res_kind == AB_F_TANL + 1 && gbuf.s.tl_on && on > 20 && lbad == 0, "040: TANL's line drawn");
        /* (X,Y) turns the readout off; ZFACT cycles 2 4 8 10 with a notice */
        press(&a, "k:5 k:3 6");                 /* (a move clears TANL's result: the readout's own state shows) */
        {
            screen_graph_text gt;
            app_ui(&a, &ui);
            screen_graph_texts(&c, &ui, &gt);
            expect(a.gxy_off && !c.gr_res_kind && gt.readout[0] == '\0', "040: (X,Y) turns the readout off");
        }
        press(&a, "GOLD k:0");
        app_ui(&a, &ui);
        abn k8 = abn_from_int(8);
        expect(abn_cmp(&c.gr_hf, &k8) == 0 && ui.notice && !strcmp(ui.notice, "ZFACT 8"), "040: ZFACT from 4 to 8, noticed");
        /* the frame tie with the free cursor: the plot area is the model's rows and axes, but for the cross, the mark */
        press(&a, "GOLD k:0 GOLD k:0 GOLD k:0");     /* (10, 2, 4: back to 4) */
        press(&a, "5");
        app_ui(&a, &ui);
        screen_render(&c, &ui, &fr);
        abn lo, hi;
        ab_graph_window(&c, &lo, &hi);
        int xrow, ycol, wrong = 0;
        ab_graph_axes(&c, &lo, &hi, &xrow, &ycol);
        for (int col = 60; col <= 340; col++) {
            int rl, rh;
            bool any = ab_graph_column(&c, col, &lo, &hi, &rl, &rh);
            for (int r = 0; r < AB_GRAPH_H; r++) {
                bool dark = fr.px[21 + r][col / 8] >> (col % 8) & 1;
                if (col == c.gr_cur || (col >= c.gr_cur - 3 && col <= c.gr_cur + 3 && r == c.gr_row)) continue;
                bool want = (any && r >= rl && r <= rh) || r == xrow || col == ycol;
                wrong += dark != want;
            }
        }
        expect(c.gr_res_kind == 0 && wrong == 0, "040: the frame's plot area is the model's, the free cursor's cross apart");
        /* 040 review: the app's fit is the core's too: a fresh plot, then the free cursor's first readout samples nothing;
           and while a fitted plot is still being drawn the free cursor's readout samples nothing either */
        {
            press(&a, "C");
            press(&a, "BLUE 7 k:5 k:2");        /* GO, TRACE off at once: the plot under way */
            unsigned long n1 = ab_graph_sample_calls;
            app_ui(&a, &ui);
            bool during = a.graphing && ab_graph_sample_calls - n1 < 10;
            for (int i = 0; i < 200 && a.graphing; i++) app_tick(&a, &idle, now);
            n1 = ab_graph_sample_calls;
            app_ui(&a, &ui);
            expect(during && c.gr_auto && ab_graph_sample_calls - n1 < 10,
                   "040: the free cursor reads out with no samples, while the plot is drawn and after (the app's fit known)");
        }
        /* 040 review: with Y fitted, the free cursor's readout and a key's redraw take no samples (the window known) */
        {
            unsigned long n0 = ab_graph_sample_calls;
            press(&a, "6");
            app_ui(&a, &ui);
            app_ui(&a, &ui);
            expect(c.gr_auto && !c.gr_trace && ab_graph_sample_calls - n0 < 10, "040: the free cursor moves and reads out without refitting the window");
            n0 = ab_graph_sample_calls;
            press(&a, "k:5 k:0");               /* ZIN from the fitted window: the fit known, not sampled again */
            expect(ab_graph_sample_calls - n0 < 10, "040: ZIN from a fitted Y takes the known fit, not 400 samples");
            for (int i = 0; i < 200 && a.graphing; i++) app_tick(&a, &idle, now);
        }
        /* 040 review: a second TANL at a moved cursor, with no redraw between, draws its own line */
        {
            press(&a, "k:4");                   /* ZAUTO: the curve and its tangents in view */
            for (int i = 0; i < 200 && a.graphing; i++) app_tick(&a, &idle, now);
            press(&a, "k:5 GOLD k:0");
            app_ui(&a, &ui);                    /* (the first tangent's line drawn: cached) */
            for (int k = 0; k < 4; k++) ab_do_arg(&c, AB_GMOVE, 14);    /* moved 40 left and a second TANL with no redraw
                                                                           between (press redraws each key) */
            ab_do_arg(&c, AB_GFCN, AB_F_TANL);
            app_ui(&a, &ui);
            int bad = 0;
            for (int i = 0; i < AB_GRAPH_W; i++) {
                abn x, t, y;
                ab_graph_x(&c, i, &x);
                abn_mul(&t, &c.gr_res, &x);
                abn_add(&y, &t, &c.gr_res_b);
                int r = ab_graph_row(&y, &gbuf.s.ymin, &gbuf.s.ymax);
                int want = r >= 0 && r <= AB_GRAPH_H - 1 ? r : -1;
                bad += gbuf.s.tl[i] != want;
            }
            abn tx0, ty0;
            ab_val tv;
            ab_graph_sample(&c, c.gr_cur, &tx0, &tv);
            int trow = ab_graph_row(&tv.re, &gbuf.s.ymin, &gbuf.s.ymax);
            expect(c.gr_res_kind == AB_F_TANL + 1 && bad == 0 && gbuf.s.tl[c.gr_cur] == trow && trow >= 0,
                   "040: TANL's line is the latest tangent's, through the cursor's point");
            (void)ty0;
        }
        /* 040 review: a graph opened again (an image, UNDO) starts on the first page */
        {
            static uint8_t img[262144];
            size_t n = ab_state_save(&c, img, sizeof img);
            press(&a, "C");
            ab_state_load(&c, img, n);
            a.gr_live = false;                  /* (as UNDO leaves it) */
            app_tick(&a, &idle, now);
            app_ui(&a, &ui);
            expect(c.gr_on && a.gpage == 0, "040: a graph reopened starts on its first page");
        }
    }

    /* 040b (abacus #4641): C during a graph tool interrupts it, the window kept; and, the same way, C during a
       keyboard SOLVE of an equation (the devices delivered C only between a program's slices, so neither could be) */
    {
        static app_graph gb;
        ab_init(&c);
        app_init(&a, &c, now);
        c.stu = c.m35 = true;
        ab_eqn_add(&c, "X^2-30");
        ab_do(&c, AB_EQN);
        app_graph_buffer(&a, &gb);
        press(&a, "BLUE 7 k:5");
        for (int i = 0; i < 200 && a.graphing; i++) app_tick(&a, &(pwr_inputs){false, false, 80, false}, now);
        abn x0 = c.gr_xmin;
        busy_app = &a;
        a.busy = busy_interrupt;
        press(&a, "k:5 k:5 k:0");               /* FCN's ROOT, C pressed while it runs */
        bool cut = c.gr_on && c.gr_res_kind == 0 && c.msg && c.err_status == ABN_INTERRUPTED && !abn_cmp(&c.gr_xmin, &x0);
        a.busy = NULL;
        press(&a, "C k:0");                     /* the message cleared; ROOT again, nothing left over */
        expect(cut && c.gr_res_kind == AB_F_ROOT + 1 && !a.computing, "040b: C interrupts ROOT (INTERRUPTED, the window kept); the next ROOT runs whole");
        /* a keyboard SOLVE (of FN='s program, which solves at once, as 016's I03) */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_LBL, 5);               /* LBL F: X^2 - 2 */
        ab_do_arg(&c, AB_RCL, 23);
        ab_do(&c, AB_SQ);
        ab_do(&c, AB_K2);
        ab_do(&c, AB_SUB);
        ab_do(&c, AB_RTN);
        ab_do(&c, AB_PRGM);
        ab_do_arg(&c, AB_FN, 5);
        ab_do(&c, AB_K1);
        ab_do_arg(&c, AB_STO, 23);
        ab_do(&c, AB_K9);
        busy_app = &a;
        a.busy = busy_interrupt;
        press(&a, "GOLD 7 ltr:X");
        expect(c.msg && c.err_status == ABN_INTERRUPTED && !a.computing, "040b: C interrupts a keyboard SOLVE");
        a.busy = NULL;
        press(&a, "C GOLD 7 ltr:X");            /* nothing left over: the next SOLVE finds the root */
        expect(!c.msg && fabs(x_of(&c) - 1.4142135623730951) < 1e-12, "040b: the next SOLVE runs whole");
    }

    /* unit 033: GRAPH: blue 7's menu; the frame drawn from the model: in the curve's columns (clear of the
       labels at the edges) a pixel of the plot area is dark exactly where the model draws a row or an axis */
    {
        static ls027_frame fr;
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, 35);            /* GRAPH's menu doesn't open in 33s mode (unit 041 A14, decision 58) */
        press(&a, "BLUE 7");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "XMIN") && ui.soft[3] && !strcmp(ui.soft[3], "YMAX") && ui.soft[4] &&
                   !strcmp(ui.soft[4], "AUTO") && ui.soft[5] && !strcmp(ui.soft[5], "GO"),
               "033: blue 7 is GRAPH: XMIN XMAX YMIN YMAX AUTO GO");
        press(&a, "C");
        c.stu = c.m35 = true;
        ab_eqn_add(&c, "X^2-30");
        ab_do(&c, AB_EQN);
        static app_graph gbuf;
        app_graph_buffer(&a, &gbuf);
        press(&a, "BLUE 7 k:5");
        for (int i = 0; i < 200 && a.graphing; i++) app_tick(&a, &(pwr_inputs){false, false, 80, false}, now);
        app_ui(&a, &ui);
        screen_render(&c, &ui, &fr);
        abn lo, hi;
        ab_graph_window(&c, &lo, &hi);
        int xrow, ycol, drawn = 0, wrong = 0;
        ab_graph_axes(&c, &lo, &hi, &xrow, &ycol);
        for (int col = 60; col <= 340; col++) {
            int rl, rh;
            bool any = ab_graph_column(&c, col, &lo, &hi, &rl, &rh);
            drawn += any;
            for (int r = 0; r < AB_GRAPH_H; r++) {
                int y = 21 + r;
                bool dark = fr.px[y][col / 8] >> (col % 8) & 1;
                if (col == c.gr_cur) continue;  /* (the trace's marks: not the model's) */
                bool want = (any && r >= rl && r <= rh) || r == xrow || col == ycol;
                wrong += dark != want;
            }
        }
        expect(c.gr_on && drawn > 250 && wrong == 0, "033: the frame's plot area is the model's rows and axes");
        /* unit 033b: the graph's text, one source: each text drawn alone where draw_graph puts it is in the
           frame; the readout's part of the soft band is exactly that text; screen_lines carries the same */
        {
            static ls027_frame tf;
            static screen_page gp;
            screen_graph_text gt;
            screen_graph_texts(&c, &ui, &gt);
            screen_lines(&c, &ui, &gp);
            struct { const char *s; int x, y; } T[] = {
                {gt.xmin, 0, 21 + AB_GRAPH_H - 8}, {gt.xmax, 400 - 6 * screen_cells(gt.xmax), 21 + AB_GRAPH_H - 8},
                {gt.ymax, 0, 22}, {gt.ymin, 0, 21 + AB_GRAPH_H - 17},
                {gt.readout, SCREEN_READOUT_X, SCREEN_READOUT_Y}};      /* unit 040: the readout in the status band */
            int missing = 0, extra = 0;
            for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
                screen_clear(&tf);
                screen_text(&tf, T[i].x, T[i].y, 1, T[i].s, true);
                if (i == 4 && gt.pad[0])        /* unit 073: the digit pad's legend shares the band, right-aligned */
                    screen_text(&tf, SCREEN_PAD_RIGHT - 6 * screen_cells(gt.pad), SCREEN_READOUT_Y, 1, gt.pad, true);
                for (int y = 0; y < 240; y++)
                    for (int x = 0; x < 400; x++) {
                        bool want = tf.px[y][x / 8] >> (x % 8) & 1, got = fr.px[y][x / 8] >> (x % 8) & 1;
                        if (want && !got) missing++;
                        if (i == 4 && y >= SCREEN_READOUT_Y - 1 && y < SCREEN_READOUT_Y + 8 && x >= SCREEN_READOUT_X &&
                            x < 328 && got != want)
                            extra++;
                    }
            }
            char ro[100];
            screen_graph_readout(&c, &ui, ro, sizeof ro);
            expect(gt.readout[0] == 'x' && gt.ymax[0] && missing == 0 && extra == 0 && !strcmp(ro, gt.readout) &&
                       gp.graph_on && !strcmp(gp.graph.readout, gt.readout) && !strcmp(gp.graph.ymin, gt.ymin),
                   "033b: the graph's texts are what the frame shows (labels, readout), and screen_lines carries them");
            expect(gt.pad[0] == '8' && !strcmp(gp.graph.pad, gt.pad),
                   "073: the digit pad's legend in the band beside the readout, and screen_lines carries it");
            int r_end = SCREEN_READOUT_X + 6 * screen_cells(gt.readout), p_at = SCREEN_PAD_RIGHT - 6 * screen_cells(gt.pad);
            expect(r_end + 6 <= p_at && p_at + 6 * screen_cells(gt.pad) <= 330,
                   "073: the legend clear of the readout (a cell between) and of STU");
            /* the forms by the readout's length: long to 22 cells, then 5 dropped, then 5 and HOME's cell, to 33 */
            static const struct { int n, cells; } L[] = {{0, 19}, {22, 19}, {23, 9}, {32, 9}, {33, 8}};
            bool lf = true;
            for (size_t i = 0; i < sizeof L / sizeof L[0]; i++) {
                const char *p = screen_pad_legend(L[i].n);
                lf = lf && screen_cells(p) == L[i].cells &&
                     SCREEN_READOUT_X + 6 * L[i].n + 6 <= SCREEN_PAD_RIGHT - 6 * screen_cells(p);
            }
            expect(lf, "073: the pad legend's form by the readout: 19 cells to 22, 9 to 32, 8 at the longest (33)");
        }
        /* the trace through the keys: the bar's right moves it; ENTER takes its value and leaves */
        abn tx;
        ab_val ty;
        press(&a, "k:1");
        ab_graph_sample(&c, 201, &tx, &ty);
        expect(c.gr_cur == 201, "033: the bar's right moves the trace a column");
        press(&a, "ENTER");
        expect(!c.gr_on && !a.graphing && abn_cmp(&c.x.re, &ty.re) == 0, "033: ENTER takes the traced value into X");
        /* Y set: the columns draw as they're computed, a slice a tick (rule 8) */
        press(&a, "5 +/\xe2\x88\x92 BLUE 7 k:2 5 0 BLUE 7 k:3 BLUE 7 k:5");
        app_tick(&a, &(pwr_inputs){false, false, 80, false}, now);
        expect(c.gr_on && a.graphing && gbuf.s.ready && gbuf.s.done == 8, "033: Y set: drawing as computed (8 columns a tick)");
        /* C interrupts: the graph left, nothing computed further, the variable as it was */
        ab_val keep = c.var[23];
        press(&a, "C");
        app_tick(&a, &(pwr_inputs){false, false, 80, false}, now);
        expect(!c.gr_on && !a.graphing && gbuf.s.done == 8 && !memcmp(&keep, &c.var[23], sizeof keep),
               "033: C interrupts a plot, the variable kept");
        /* AUTO: the samples first, the window unknown until all are in */
        press(&a, "BLUE 7 k:4 BLUE 7 k:5");
        app_tick(&a, &(pwr_inputs){false, false, 80, false}, now);
        expect(c.gr_on && a.graphing && !gbuf.s.ready, "033: AUTO: samples first, under a progress bar");
        for (int i = 0; i < 200 && a.graphing; i++) app_tick(&a, &(pwr_inputs){false, false, 80, false}, now);
        expect(gbuf.s.ready && gbuf.s.done == AB_GRAPH_W, "033: AUTO: then the fit and the drawing");
        press(&a, "C");
        {   /* review F1: a graph open in a loaded image (a boot) is plotted afresh; so is one UNDO brings back */
            static ab_calc c2;
            static app_state a2;
            static app_graph g2;
            static uint8_t img[1 << 16];
            ab_init(&c);
            c.stu = c.m35 = true;
            ab_eqn_add(&c, "X^2");
            ab_do(&c, AB_EQN);
            ab_do_arg(&c, AB_GRAPH, 0);
            size_t n = ab_state_save(&c, img, sizeof img);
            ab_do(&c, AB_CANCEL);               /* (c's graph left: the checks below use c) */
            ab_init(&c2);
            expect(ab_state_load(&c2, img, n) == ABN_OK && c2.gr_on, "033 F1: an open graph in the image loads");
            app_init(&a2, &c2, now);
            app_graph_buffer(&a2, &g2);
            for (int i = 0; i < 200 && (a2.graphing || !g2.s.ready); i++)
                app_tick(&a2, &(pwr_inputs){false, false, 80, false}, now);
            expect(g2.s.ready && g2.s.done == AB_GRAPH_W, "033 F1: after a boot it is plotted afresh");
            {   /* and one UNDO brings back after C mid-plot */
                static uint8_t u0[1 << 18], u1[1 << 18];
                app_undo_buffers(&a2, u0, u1, sizeof u0);
                press(&a2, "C");
                press(&a2, "BLUE 7 k:5");       /* GO again (AUTO): an undo point before it */
                app_tick(&a2, &(pwr_inputs){false, false, 80, false}, now);
                press(&a2, "C");                /* left mid-plot */
                press(&a2, "BLUE \xe2\x86\x90");    /* UNDO: the graph open again */
                for (int i = 0; i < 200 && (a2.graphing || !g2.s.ready); i++)
                    app_tick(&a2, &(pwr_inputs){false, false, 80, false}, now);
                expect(c2.gr_on && g2.s.ready && g2.s.done == AB_GRAPH_W, "033 F1: a graph UNDO brings back is plotted afresh");
                app_undo_buffers(&a2, NULL, NULL, 0);
            }
            /* the form's note from the core's own test (review F4): S=SIN(X) plots SIN(X), noted S */
            ab_init(&c2);
            c2.stu = c2.m35 = true;
            ab_eqn_add(&c2, "S=SIN(X)");
            ab_do(&c2, AB_EQN);
            app_init(&a2, &c2, now);
            app_graph_buffer(&a2, &g2);
            ab_do_arg(&c2, AB_GRAPH, 0);
            app_tick(&a2, &(pwr_inputs){false, false, 80, false}, now);
            expect(!strcmp(g2.s.note, "S"), "033 F4: S=SIN(X) is noted S, not L-R");
            /* review F3: an erring ENTER's message shows in the readout's place */
            static ls027_frame f0, f1;
            ab_init(&c2);
            c2.stu = c2.m35 = true;
            ab_eqn_add(&c2, "SQRT(X)");
            ab_do(&c2, AB_EQN);
            app_init(&a2, &c2, now);
            app_graph_buffer(&a2, &g2);
            press(&a2, "BLUE 7 k:5");
            for (int i = 0; i < 200 && a2.graphing; i++) app_tick(&a2, &(pwr_inputs){false, false, 80, false}, now);
            press(&a2, "k:0");
            app_ui(&a2, &ui);
            screen_render(&c2, &ui, &f0);
            press(&a2, "ENTER");
            app_ui(&a2, &ui);
            screen_render(&c2, &ui, &f1);
            expect(c2.msg && c2.gr_on && memcmp(&f0, &f1, sizeof f0) != 0, "033 F3: an erring ENTER's message is shown");
        }
        {   /* the fit alone (answer 2): an overflowed sample left out; a constant +-1; none finite -10..10;
               huge values clamp to the largest instead of overflowing */
            abn yv[3] = {abn_largest(), abn_from_int(1), abn_from_int(2)}, lo2, hi2, want_lo, want_hi;
            bool okv[3] = {true, true, true};
            ab_graph_fit(yv, okv, 3, &lo2, &hi2);
            abn_from_text(&want_lo, "0.95");
            abn_from_text(&want_hi, "2.05");
            expect(abn_cmp(&lo2, &want_lo) == 0 && abn_cmp(&hi2, &want_hi) == 0, "033: an overflowed sample is left out of the fit");
            okv[1] = okv[2] = false;
            ab_graph_fit(yv, okv, 3, &lo2, &hi2);
            want_lo = abn_from_int(-10);
            want_hi = abn_from_int(10);
            expect(abn_cmp(&lo2, &want_lo) == 0 && abn_cmp(&hi2, &want_hi) == 0, "033: no finite sample: -10..10");
            abn_from_text(&yv[0], "-9E6144");
            abn_from_text(&yv[1], "9E6144");
            okv[0] = okv[1] = true;
            ab_graph_fit(yv, okv, 2, &lo2, &hi2);
            abn_from_text(&want_lo, "-9.5E6144");     /* the span clamps to the largest, 5% of it is 5E6143 */
            abn_from_text(&want_hi, "9.5E6144");
            expect(abn_cmp(&lo2, &want_lo) == 0 && abn_cmp(&hi2, &want_hi) == 0,
                   "033: a span past the range clamps to the largest (the window +-9.5E6144)");
        }
        c.stu = false;
        press(&a, "BLUE 7 k:5");
        app_ui(&a, &ui);
        expect(!c.gr_on && ui.notice && !strcmp(ui.notice, "STU ONLY: MODE, STU"), "033: 35s mode: STU ONLY briefly (073: where to go)");
    }

    /* unit 035: after a keyboard SOLVE the root is VIEWed (W=3.5000), no prompt on Y; the next key gives
       the ordinary stack; the integral as its labelled result; a linear solution's x= then y= */
    {
        static screen_page pg;
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_SETFIX, 4);
        ab_eqn_add(&c, "A=L\xc3\x97W");
        ab_do_arg(&c, AB_SOLVE, AB_VAR_A + 22);
        ab_do(&c, AB_K4); ab_do(&c, AB_K2); ab_do(&c, AB_RS);
        ab_do(&c, AB_K1); ab_do(&c, AB_K2); ab_do(&c, AB_RS);
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(pg.x.text, "W=3.5000") && pg.x.kind == SCREEN_VIEW && !strchr(pg.y.text, '?') && pg.y.label,
               "035: after SOLVE W: W=3.5000 viewed, no prompt on Y");
        press(&a, "ENTER");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(pg.x.kind != SCREEN_VIEW && pg.x.label && !strchr(pg.y.text, '?'), "035: the next key: the ordinary stack");
        ab_init(&c);
        ab_do_arg(&c, AB_MODEL, 35);            /* the built-ins are 35s mode's (unit 041 A4, decision 58) */
        ab_do_arg(&c, AB_SETFIX, 4);
        ab_do(&c, AB_EQN);
        ab_do(&c, AB_SST);                      /* 2*2 lin. solve */
        ab_do_arg(&c, AB_SOLVE, 0);
        static const ab_op K[] = {AB_K1, AB_K2, AB_K5, AB_K3, AB_K4};
        for (int i = 0; i < 5; i++) { ab_do(&c, K[i]); ab_do(&c, AB_RS); }
        ab_do(&c, AB_K1); ab_do(&c, AB_K1); ab_do(&c, AB_RS);
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(pg.x.text, "X=1.0000"), "035: the 2x2 solver ends showing X=1.0000 (35s p.7-7)");
        ab_do(&c, AB_SST);
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(pg.x.text, "Y=2.0000"), "035: down: Y=2.0000");
        /* (review F1) through the keys: the view's bar, and the shifted SST, step it; not the program */
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "\xe2\x96\xb2") && ui.soft[1] && !strcmp(ui.soft[1], "\xe2\x96\xbc"),
               "035: a linear solution's bar: up, down");
        int pc0 = c.pc;
        press(&a, "k:1");
        expect(c.viewing && c.view_id == AB_VIEW_LIN2 && c.pc == pc0, "035: the bar's down wraps to X=, the program untouched");
        press(&a, "BLUE R/S");
        expect(c.viewing && c.view_id == AB_VIEW_LIN2 - 1 && c.pc == pc0 && c.stop == AB_STOP_NONE,
               "035: the shifted SST steps to Y=, no program line run");
    }

    /* unit 043: SOLVE on a built-in solver, from the keys, goes straight to A's prompt (35s p.7-6), in 35s mode and STU;
       on an ordinary equation it asks for the unknown. The guide's example keyed as printed: x = 1, y = 2. The 3*3 case
       ours: rows (1 1 1 | 6), (1 2 3 | 14), (2 1 1 | 7), x y z = 1 2 3. */
    {
        static const int MODES[2] = {35, AB_MODEL_STU};
        for (int m = 0; m < 2; m++) {
            const char *nm = m ? "STU" : "35s";
            char what[96];
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, MODES[m]);
            press(&a, "GOLD 0 k:0 k:0");        /* EQN, then up twice from the top: 2*2 lin. solve */
            press(&a, "GOLD 7 1 R/S 2 R/S 5 R/S 3 R/S 4 R/S 1 1 R/S");
            snprintf(what, sizeof what, "043: %s: 2*2 lin. solve from the keys (35s p.7-6): x = 1, y = 2", nm);
            expect(var_of(&c, 'X') == 1 && var_of(&c, 'Y') == 2, what);
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, MODES[m]);
            press(&a, "GOLD 0 k:0");            /* up once from the top: 3*3 lin. solve */
            press(&a, "GOLD 7 1 R/S 1 R/S 1 R/S 6 R/S 1 R/S 2 R/S 3 R/S 1 4 R/S 2 R/S 1 R/S 1 R/S 7 R/S");
            snprintf(what, sizeof what, "043: %s: 3*3 lin. solve from the keys: x y z = 1 2 3", nm);
            expect(var_of(&c, 'X') == 1 && var_of(&c, 'Y') == 2 && var_of(&c, 'Z') == 3, what);
        }
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, 35);
        ab_eqn_add(&c, "A+1");                  /* an ordinary equation shown */
        press(&a, "GOLD 7");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "SOLVE _"), "043: SOLVE on an ordinary equation still asks for the unknown");
    }

    /* unit 043 rule 3 (abacus #4799, 3): XEQ while an equation is typed ends the entry, stores it and evaluates it,
       as the token path does (the 35s guide is silent: p.6-11 has ENTER only ending it, p.6-12 XEQ on one shown) */
    {
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "GOLD 0 2 + 3 XEQ");
        app_ui(&a, &ui);
        expect(x_of(&c) == 5 && !c.eqn_typing && c.eqn_cur >= 0 && !strcmp(c.eqn[c.eqn_cur], "2+3") && !ui.prompt,
               "043: XEQ while typing 2+3: stored and evaluated, X = 5, no XEQ prompt");
    }

    /* CAS 004 (casim's work/004 rule 4): CAS on EQN LIST's fifth key, STU only; D/DX, COLCT and SUBST prompt for the
       variable; the result after the original, shown; UNDO removes it; a refusal leaves everything as it was; in 33s
       and 35s modes no CAS. Expected texts: casim's host CLI at CASIM_PIN (STU, the calculator's DEG). */
    {
        static uint8_t u0[1 << 18], u1[1 << 18];
        ab_init(&c);
        app_init(&a, &c, now);
        app_undo_buffers(&a, u0, u1, sizeof u0);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c, "X^2+3\xc3\x97X");     /* (Equation mode on, it shown) */
        app_ui(&a, &ui);
        expect(ui.soft[4] && !strcmp(ui.soft[4], "CAS") && ui.soft_sub[4], "CAS 004: STU: EQN LIST's fifth key is CAS");
        press(&a, "k:4");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "D/DX") && ui.soft[1] && !strcmp(ui.soft[1], "SIMP") && ui.soft[2] &&
                   !strcmp(ui.soft[2], "EXPND") && ui.soft[3] && !strcmp(ui.soft[3], "COLCT") && ui.soft[4] &&
                   !strcmp(ui.soft[4], "SUBST"),
               "CAS 004: its menu: D/DX SIMP EXPND COLCT SUBST");
        press(&a, "k:0");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "D/DX _"), "CAS 004: D/DX prompts for the variable");
        int n = c.eqn_count;
        press(&a, "ltr:X");
        expect(c.eqn_count == n + 1 && c.eqn_cur > 0 && !strcmp(c.eqn[c.eqn_cur], "2\xc3\x97X+3") &&
                   !strcmp(c.eqn[c.eqn_cur - 1], "X^2+3\xc3\x97X"),
               "CAS 004: D/DX X: 2×X+3 after the original, shown, the original kept");
        press(&a, "BLUE \xe2\x86\x90");
        expect(c.eqn_count == n && !strcmp(c.eqn[c.eqn_cur], "X^2+3\xc3\x97X"), "CAS 004: UNDO removes it");
        press(&a, "k:4 k:3");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "COLCT _"), "CAS 004: COLCT prompts for the variable");
        press(&a, "C k:4 k:4");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "SUBST _"), "CAS 004: SUBST prompts for the variable");
        press(&a, "ltr:X");
        expect(c.eqn_typing && c.cas_sub == 'X' - 'A' + 1, "CAS 004: then the editor, for the replacement");
        press(&a, "RCL ltr:Y + 1 ENTER");
        expect(!c.eqn_typing && c.eqn_count == n + 1 && !strcmp(c.eqn[c.eqn_cur], "3\xc3\x97Y+(Y+1)^2+3"),
               "CAS 004: SUBST X by Y+1, typed: 3×Y+(Y+1)^2+3 after it");
        ab_eqn_add(&c, "IP(X)");
        n = c.eqn_count;
        ab_val x0 = c.x, y0 = c.y;
        int cur0 = c.eqn_cur;
        press(&a, "k:4 k:0 ltr:X");
        app_ui(&a, &ui);
        {
            static screen_page pg;
            screen_lines(&c, &ui, &pg);
            expect(c.eqn_count == n && c.eqn_cur == cur0 && !strcmp(c.eqn[c.eqn_cur], "IP(X)") &&
                       abn_cmp(&c.x.re, &x0.re) == 0 && abn_cmp(&c.y.re, &y0.re) == 0 &&
                       strstr(pg.x.text, "UNSUPPORTED IP") != NULL,
                   "CAS 004: a refusal: UNSUPPORTED IP, the list, the equation shown and the stack as they were");
        }
        for (int m = 0; m < 2; m++) {           /* 33s and 35s: no CAS */
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, m ? 35 : 33);
            ab_eqn_add(&c, "X^2");
            app_ui(&a, &ui);
            int menu0 = a.menu;
            press(&a, "k:4");
            expect(!ui.soft[4] && a.menu == menu0 && c.eqn_count == 3,
                   m ? "CAS 004: 35s mode: no CAS (blank, does nothing)" : "CAS 004: 33s mode: no CAS (blank, does nothing)");
        }
    }

    /* unit 041 Part B: CONST's pages in 33s mode are the 33s's 40 (eight pages: the eighth "more" wraps to the
       first); in 35s mode a ninth holds e */
    {
        const char *e = ab_const_symbol(40), *c0 = ab_const_symbol(0);
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "GOLD RCL k:5 k:5 k:5 k:5 k:5 k:5 k:5 k:5");
        app_ui(&a, &ui);
        expect(ui.soft[0] && c0 && !strcmp(ui.soft[0], c0), "041 B: 33s mode: CONST's eighth more wraps to c (40, no e)");
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "GOLD RCL k:5 k:5 k:5 k:5 k:5 k:5 k:5 k:5");
        app_ui(&a, &ui);
        expect(ui.soft[0] && e && !strcmp(ui.soft[0], e) && !ui.soft[1], "041 B: 35s mode: CONST's ninth page is e alone");
    }

    /* unit 073 (stu-discover, abacus #8714): an STU feature refused says where to go, never silence: blue GRAPH and blue
       TABLE in 33s mode (041 A14 had them silent), and the list's CAS key in 33s and 35s modes */
    {
        static const char *const KEYS[2] = {"BLUE 7", "BLUE 8"};
        for (int k = 0; k < 2; k++) {
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, 33);
            press(&a, KEYS[k]);
            app_ui(&a, &ui);
            char w[64];
            snprintf(w, sizeof w, "073: 33s mode: %s says STU ONLY: MODE, STU, no menu", KEYS[k]);
            expect(a.menu < 0 && ui.notice && !strcmp(ui.notice, "STU ONLY: MODE, STU"), w);
        }
        static const int M[2] = {33, 35};
        for (int m = 0; m < 2; m++) {
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, M[m]);
            ab_eqn_add(&c, "X^2");              /* (listed and shown: Equation mode) */
            press(&a, "k:4");
            app_ui(&a, &ui);
            char w[64];
            snprintf(w, sizeof w, "073: %ds mode: the list's CAS says STU ONLY: MODE, STU", M[m]);
            expect(ui.notice && !strcmp(ui.notice, "STU ONLY: MODE, STU") && a.menu < 0, w);
        }
        ab_init(&c);                            /* STU: the same keys open their menus */
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        press(&a, "BLUE 7");
        expect(a.menu >= 0 && !a.notice, "073: STU: blue GRAPH opens its menu, no notice");
    }

    /* unit 073, fresh eyes: a prompted key Equation mode refuses says so before its prompt (RCL still prompts, its
       letter types); HYP_ never covers an equation being typed (the annunciator says it); UNDO with nothing to undo
       says so; a refused key keeps UNDO's last change, so UNDO undoes what came before it, then redoes it */
    {
        static uint8_t v0[262144], v1[262144];
        ab_init(&c);
        app_init(&a, &c, now);
        ab_eqn_add(&c, "X^2");
        press(&a, "STO");
        app_ui(&a, &ui);
        expect(!a.prompt_text[0] && ui.notice && !strcmp(ui.notice, "NOT IN EQN"), "073: STO with the list shown: NOT IN EQN, no prompt");
        press(&a, "RCL");
        app_ui(&a, &ui);
        expect(a.prompt_text[0] && ui.prompt && !strncmp(ui.prompt, "RCL", 3), "073: RCL with the list shown still prompts (it types its letter)");
        press(&a, "C");
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "GOLD 0 2 + BLUE LN");
        app_ui(&a, &ui);
        expect(ui.hyp && (!ui.prompt || strcmp(ui.prompt, "HYP_")) && c.eqn_typing, "073: HYP while an equation is typed: the annunciator, no HYP_ over the line");
        ab_init(&c);
        app_init(&a, &c, now);
        app_undo_buffers(&a, v0, v1, sizeof v0);
        press(&a, "BLUE \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(ui.notice && !strcmp(ui.notice, "NOTHING TO UNDO"), "073: UNDO with nothing to undo says so");
        ab_do_arg(&c, AB_MODEL, 35);
        press(&a, "2 ENTER 3 + GOLD 0");      /* EQN: the last change */
        bool in = c.eqn_mode;
        press(&a, "R\xe2\x86\x93");              /* R↓ refused: NOT IN EQN, no change */
        press(&a, "BLUE \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(in && !c.eqn_mode && x_of(&c) == 5 && ui.notice && !strcmp(ui.notice, "UNDO"),
               "073: after a refused key, UNDO undoes the change before it (EQN), not the refusal");
        press(&a, "BLUE \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(c.eqn_mode && ui.notice && !strcmp(ui.notice, "REDO"), "073: and the next is its REDO");
    }

    /* unit 075 (abacus #8736, #8738): HELP (blue 9), then a key: its page, the key never run; HELP_ and the band's
       HELP while armed; a shift first, then the key (GOLD 7's); in an open menu a soft key's item; the closing key does
       nothing else; no lift, no UNDO mark, the image as it was */
    {
        static uint8_t h0[262144], h1[262144];
        ab_init(&c);
        app_init(&a, &c, now);
        app_undo_buffers(&a, h0, h1, sizeof h0);
        press(&a, "2 ENTER 3");
        uint32_t crc = img_crc(&c);
        size_t ul = a.undo_len;
        press(&a, "BLUE 9");
        app_ui(&a, &ui);
        expect(a.help && ui.prompt && !strcmp(ui.prompt, "HELP_") && ui.help, "075: HELP armed: HELP_ and the band's HELP");
        press(&a, "SIN");
        app_ui(&a, &ui);
        expect(a.view == APP_VIEW_HELP && ui.view[0] && strstr(ui.view[0], "SIN") && img_crc(&c) == crc &&
                   a.undo_len == ul && !a.help, "075: HELP SIN: SIN's page, nothing run (the image, UNDO as they were)");
        press(&a, "5");
        expect(a.view == APP_VIEW_NONE && img_crc(&c) == crc, "075: a key closes the page and does nothing else");
        press(&a, "BLUE 9 GOLD 7");
        app_ui(&a, &ui);
        expect(a.view == APP_VIEW_HELP && ui.view[0] && !strncmp(ui.view[0], "GOLD 7: ", 8), "075: HELP GOLD 7: GOLD 7's page");
        press(&a, "C");
        expect(a.view == APP_VIEW_NONE && img_crc(&c) == crc, "075: C closes it, nothing else");
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c, "X^2");
        press(&a, "k:4 k:5");                   /* CAS, its second page */
        int menu = a.menu;
        press(&a, "BLUE 9");
        expect(a.help && a.menu == menu, "075: HELP over a menu keeps it open");
        press(&a, "k:4");
        app_ui(&a, &ui);
        expect(a.view == APP_VIEW_HELP && ui.view[0] && !strncmp(ui.view[0], "F5 in CAS2: \xce\xa3", 15) && !c.cas_def,
               "075: then F5: CAS2's \xce\xa3 explained, not started");
        /* fresh eyes: the keys app_key takes first are explained as they act; MEM's catalog kept */
        static uint8_t g0[APP_CTX_MAX];
        ab_init(&c);
        app_init(&a, &c, now);
        app_ctx_buffer(&a, g0, sizeof g0);
        press(&a, "5 STO ltr:A BLUE 1 k:0");     /* MEM, VAR: the catalog */
        int cat = a.cat, bar = a.menu;
        press(&a, "BLUE 9 k:0");
        app_ui(&a, &ui);
        expect(cat && a.view == APP_VIEW_HELP && a.cat == cat && a.menu == bar && ui.view[1],
               "075: HELP in MEM's catalog explains its bar's key, the catalog kept");
        press(&a, "5");
        expect(a.cat == cat && a.menu == bar, "075: and the page closed, the catalog as it was");
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "BLUE LN BLUE 9 SIN");
        app_ui(&a, &ui);
        expect(ui.view[0] && !strncmp(ui.view[0], "HYP SIN", 7), "075: HYP open, HELP SIN: SINH's page (the prefix)");
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "BLUE +/\xe2\x88\x92 BLUE 9 \xc3\xb7");
        app_ui(&a, &ui);
        expect(ui.view[1] && !strncmp(ui.view[1], "33s CMPLX", 9), "075: 33s CMPLX open, HELP \xc3\xb7: the pair operation");
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c, "X^2");
        ab_do(&c, AB_GRAPH);
        press(&a, "BLUE 9 GOLD 7");
        app_ui(&a, &ui);
        expect(c.gr_on && ui.view[1] && !strncmp(ui.view[1], "Moves the graph's cursor", 24),
               "075: a graph open, HELP GOLD 7: the cursor's ten pixels, not SOLVE");
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "BLUE 0 BLUE 9 k:1");
        app_ui(&a, &ui);
        expect(ui.view[0] && !strcmp(ui.view[0], "F2 in APPS: GAMES") && ui.view[1] && !strncmp(ui.view[1], "Games:", 6),
               "075: APPS's GAMES key has its page");
        press(&a, "C BLUE 0 k:1 BLUE 9 k:5");
        app_ui(&a, &ui);
        expect(ui.view[1] && !strcmp(ui.view[1], "Shows the next five."), "075: GAMES's more has its page");
        press(&a, "C C C");                     /* the page, GAMES back to APPS, APPS closed */
        press(&a, "BLUE 9 k:2");
        app_ui(&a, &ui);
        expect(ui.view[0] && !strcmp(ui.view[0], "F3: -"), "075: a soft key with nothing over it is named by its place");
    }

    /* unit 072 (abacus #8570, #8600, #8679): APPS lists KEYS, then GAMES; GAMES pages the folder's games five at a time
       with more, in the registry's order; C or back-arrow returns to APPS; a game starts from GAMES */
    {
        const char *g[16];
        int ng = 0;
        for (int r = 0; r < app_registry_n && ng < 16; r++)
            if (app_registry_folder[r] == APP_FOLDER_GAMES) g[ng++] = app_registry[r]->name;
        static uint8_t gctx[APP_CTX_MAX];         /* (a game's state: the device's buffer) */
        ab_init(&c);
        app_init(&a, &c, now);
        app_ctx_buffer(&a, gctx, sizeof gctx);
        press(&a, "BLUE 0");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "KEYS") && ui.soft[1] && !strcmp(ui.soft[1], "GAMES") &&
                   (!ui.soft[2] || !*ui.soft[2]) && ui.soft[5] && !strcmp(ui.soft[5], "more"),
               "072: APPS: KEYS, GAMES, nothing more listed");
        press(&a, "k:1");
        app_ui(&a, &ui);
        bool p1 = ng > 5;
        for (int i = 0; i < 5 && i < ng; i++) p1 = p1 && ui.soft[i] && !strcmp(ui.soft[i], g[i]);
        expect(p1 && ui.soft[5] && !strcmp(ui.soft[5], "more"), "072: GAMES's first page: five games, then more");
        press(&a, "k:5");
        app_ui(&a, &ui);
        bool p2 = true;
        for (int i = 0; i < 5; i++)
            p2 = p2 && (5 + i < ng ? ui.soft[i] && !strcmp(ui.soft[i], g[5 + i]) : !ui.soft[i] || !*ui.soft[i]);
        expect(p2 && ng == 9 && !strcmp(g[5], "SALVO") && !strcmp(g[ng - 1], "CATCH"),
               "078: GAMES's second page: SALVO, 3720, MOLES, CATCH, last in the registry");
        press(&a, "k:5");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], g[0]), "072: more on the last page wraps to the first");
        press(&a, "C");
        app_ui(&a, &ui);
        expect(ui.soft[1] && !strcmp(ui.soft[1], "GAMES") && a.menu >= 0, "072: C in GAMES returns to APPS");
        press(&a, "C");
        expect(a.menu < 0, "072: C in APPS closes it, as before");
        press(&a, "BLUE 0 k:1 \xe2\x86\x90");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "KEYS") && a.menu >= 0, "072: back-arrow in GAMES returns to APPS");
        press(&a, "k:1 k:0");
        expect(a.app && !strcmp(a.app->name, g[0]), "072: a game starts from GAMES");
        /* a list with no folders (a test's, app_apps): every app in APPS, no GAMES key */
        static const app_def *const one[] = {&tapps[0], &tapps[1]};
        ab_init(&c);
        app_init(&a, &c, now);
        app_apps(&a, one, 2);
        press(&a, "BLUE 0");
        app_ui(&a, &ui);
        expect(ui.soft[1] && !strcmp(ui.soft[1], one[1]->name) && (!ui.soft[2] || !*ui.soft[2]),
               "072: without folders every app is APPS's, no GAMES key");
    }

    /* the games list (soroban's gate): each game's app_def name is the name its manifest gives (what APPS shows) */
    {
        static const char *const names[] = {"KEYS" GAMES_NAMES};
        bool same = app_registry_n == (int)(sizeof names / sizeof names[0]);
        for (int i = 0; same && i < app_registry_n; i++) same = !strcmp(app_registry[i]->name, names[i]);
        expect(same, "games list: every game's APPS name is its manifest's");
    }

    /* unit 043 rule 3: letter prompts the token path skips, driven on the real keys to the vectors' results: the
       integral's (GOLD 8, then the letter), TABLE's VAR, and ISG (BLUE 4) */
    {
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, 35);
        ab_eqn_add(&c, "X^2");                  /* listed and shown; C leaves Equation mode */
        press(&a, "C 0 ENTER 3 GOLD 0 GOLD 8 ltr:X");
        double v = x_of(&c);
        expect(v > 9 - 1e-9 && v < 9 + 1e-9, "043: the integral from the keys: X^2 from 0 to 3 is 9");
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c, "Y^2");
        ab_do(&c, AB_EQN);                      /* out of Equation mode, Y^2 last shown */
        press(&a, "3 STO ltr:Y BLUE 8 k:0 ltr:Y");
        abn tx, nine = abn_from_int(9);
        ab_val ty;
        press(&a, "BLUE 8 k:3");                /* GO */
        expect(c.tbl_var == 'Y' - 'A' && ab_table_row(&c, 0, &tx, &ty) == ABN_OK && abn_cmp(&ty.re, &nine) == 0,
               "043: TABLE's VAR from the keys: Y, then GO: row 0 is 3^2 = 9");
        ab_init(&c);
        app_init(&a, &c, now);
        press(&a, "1 . 0 0 3 STO ltr:N BLUE 4 ltr:N");
        v = var_of(&c, 'N');
        expect(v > 2.003 - 1e-12 && v < 2.003 + 1e-12, "043: ISG from the keys: N 1.003 becomes 2.003");
    }

    /* unit 032: TABLE: blue 8's menu (VAR START STEP GO); the table on the four lines, x left and the
       value right, row k on X; the bar's up and down; ENTER takes the row; the status band says TABLE;
       STU ONLY briefly in 35s mode */
    {
        static screen_page pg;
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_SETALL, 0);               /* the rows in ALL, as checked (a new calculator shows FIX 4, unit 067) */
        ab_do_arg(&c, AB_MODEL, 35);            /* TABLE's menu doesn't open in 33s mode (unit 041 A14, decision 58) */
        press(&a, "BLUE 8");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "VAR") && ui.soft[1] && !strcmp(ui.soft[1], "START") && ui.soft[2] &&
                   !strcmp(ui.soft[2], "STEP") && ui.soft[3] && !strcmp(ui.soft[3], "GO"),
               "032: blue 8 is TABLE: VAR START STEP GO");
        press(&a, "C");
        ab_eqn_add(&c, "X^2");
        ab_do(&c, AB_EQN);                      /* out of Equation mode, X^2 last shown */
        press(&a, "3 STO ltr:X");
        c.stu = c.m35 = true;                   /* STU, which is 35s mode (MODE's keys tested elsewhere; 041 A14) */
        press(&a, "BLUE 8 k:3");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(c.tbl_on && !strncmp(pg.x.text, "3 ", 2) && strlen(pg.x.text) == 33 && pg.x.text[32] == '9' &&
                   !strncmp(pg.t.text, "0 ", 2) && pg.t.text[32] == '0',
               "032: the table: row 0 (3 left, 9 right, 33 cells) on X, row -3 (0, 0) on T (display ALL)");
        expect(strstr(pg.status.text, "TABLE") != NULL, "032: the status band says TABLE");
        expect(ui.soft[0] && !strcmp(ui.soft[0], "\xe2\x96\xb2") && ui.soft[1] && !strcmp(ui.soft[1], "\xe2\x96\xbc"),
               "032: the table's bar: up, down");
        press(&a, "k:1");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(c.tbl_k == 1 && !strncmp(pg.x.text, "4 ", 2) && !strcmp(pg.x.text + 31, "16"), "032: down: row 1 (4, 16) on X");
        press(&a, "ENTER");
        expect(!c.tbl_on && x_of(&c) == 16, "032: ENTER takes the row's value into X");
        c.stu = false;
        c.m35 = true;
        press(&a, "BLUE 8 k:3");
        app_ui(&a, &ui);
        expect(!c.tbl_on && ui.notice && !strcmp(ui.notice, "STU ONLY: MODE, STU") && !c.msg, "032: 35s mode: STU ONLY briefly (073: where to go)");
        /* review F2: an ENTER that overflows and then errs leaves no OVERFLOW notice behind for C */
        ab_init(&c);
        app_init(&a, &c, now);
        c.stu = true;
        ab_eqn_add(&c, "X\xc3\x97" "1E6000+1\xc3\xb7(X-1E6000)");
        ab_do(&c, AB_EQN);
        press(&a, "1 E 6 0 0 0 STO ltr:X BLUE 8 k:3 ENTER C C");
        app_ui(&a, &ui);
        expect(!c.tbl_on && !ui.notice, "032: C after an erring ENTER leaves no OVERFLOW notice");
        /* review F4: an image with an open table and STU off, in program entry, a number typed, or no room
           above its row is refused */
        {
            static ab_calc z;
            static uint8_t img[1 << 16];
            ab_init(&c);
            c.stu = c.m35 = true;               /* (STU is a 35s: the loader holds it so) */
            ab_eqn_add(&c, "X^2");
            ab_do(&c, AB_EQN);
            ab_do_arg(&c, AB_TABLE, 0);
            size_t n = ab_state_save(&c, img, sizeof img);
            expect(c.tbl_on && ab_state_load(&z, img, n) == ABN_OK, "032: an open table's image loads");
            c.stu = false;
            n = ab_state_save(&c, img, sizeof img);
            expect(ab_state_load(&z, img, n) == ABN_BAD_IMAGE, "032: an open table with STU off is refused");
            c.stu = true;
            c.prgm = true;
            n = ab_state_save(&c, img, sizeof img);
            expect(ab_state_load(&z, img, n) == ABN_BAD_IMAGE, "032: an open table in program entry is refused");
            c.prgm = false;
            c.entry.active = true;
            c.entry.len = 1;
            c.entry.text[0] = '5';
            n = ab_state_save(&c, img, sizeof img);
            expect(ab_state_load(&z, img, n) == ABN_BAD_IMAGE, "032: an open table with a number typed is refused");
            memset(&c.entry, 0, sizeof c.entry);
            c.tbl_k = INT32_MIN;
            n = ab_state_save(&c, img, sizeof img);
            expect(ab_state_load(&z, img, n) == ABN_BAD_IMAGE, "032: an open table at INT32_MIN is refused");
        }
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
        expect(ui.view[3] && !strcmp(ui.view[3], "CASIM ? GAMES ?"), "069 SYS VER: CASIM ? GAMES ? when sysinfo knows neither");
        /* unit 069: VER from app_sysinfo_build, line for line; CASIM and GAMES against the Makefile's VER_CASIM and VER_GAMES
           (CASIM_PIN cut to 7; the shell's sha256sum over the build's list, "none" for no game), not the generator's */
        press(&a, "C");
        a.sysinfo = build_sysinfo;
        press(&a, "BLUE C k:1");
        app_ui(&a, &ui);
        static const char *const VER_LINES[4] = {"VER " AB_FW_VERSION, "CORE " AB_CORE_COMMIT, "BUILT " AB_BUILT,
                                                 "CASIM " VER_CASIM " GAMES " VER_GAMES};
        bool lines = a.view == APP_VIEW_VER;
        for (int i = 0; i < 4; i++)
            if (!ui.view[i] || strcmp(ui.view[i], VER_LINES[i])) {
                lines = false;
                printf("  069 VER line %d: \"%s\", want \"%s\"\n", i, ui.view[i] ? ui.view[i] : "(none)", VER_LINES[i]);
            }
        expect(lines, "069 SYS VER: VER, CORE, BUILT, then CASIM " VER_CASIM " GAMES " VER_GAMES);
        a.sysinfo = fake_sysinfo;
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
        ab_do_arg(&c, AB_GTO, 0 * 1000);      /* GTO A: the label form (A001), as 33s mode keys it (041 A) */
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
            ab_do_arg(&c, AB_GTO, 1 * 1000);      /* GTO B: the label form (B001), as 33s mode keys it (041 A) */
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
            ab_do_arg(&c, AB_GTO, 1 * 1000);      /* GTO B: the label form (B001), as 33s mode keys it (041 A) */
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
            ab_do_arg(&c, AB_GTO, 1 * 1000);      /* GTO B: the label form (B001), as 33s mode keys it (041 A) */
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

    /* unit 044 rule 5 (abacus #4893): a plot's pixels take the engine's path, the trace's readout is correctly
       rounded, and the two agree to the pixel at every column; an integral's integrand takes the engine's path and
       the keys are correctly rounded again after it */
    {
        static const char *const F[] = {"SIN(X)", "LN(X)", "ASINH(X)", "ATAN(X)", "TANH(X)"};
        int disagree = 0, differ = 0, notcr = 0, samples = 0;
        unsigned long zpix = 0, zread = 0;
        for (int i = 0; i < 5; i++) {
            ab_init(&c);
            ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
            ab_do(&c, AB_RAD);
            ab_eqn_add(&c, F[i]);
            ab_do(&c, AB_EQN);
            c.gr_xmin = abn_from_int(1), c.gr_xmax = abn_from_int(3);
            ab_do(&c, AB_GRAPH);
            abn lo, hi;
            ab_graph_window(&c, &lo, &hi);
            c.gr_trace = true;
            for (int k = 0; k < AB_GRAPH_W; k++) {
                abn x, xc, y, want = abn_zero();
                ab_val v;
                unsigned long z0 = fn_ziv_calls;
                if (ab_graph_sample(&c, k, &x, &v) != ABN_OK) continue;
                zpix += fn_ziv_calls - z0;
                samples++;
                c.gr_cur = k;
                z0 = fn_ziv_calls;
                abn_status cst = ab_graph_cursor(&c, &xc, &y);
                zread += fn_ziv_calls - z0;
                if (cst != ABN_OK) {
                    disagree++;
                    continue;
                }
                disagree += ab_graph_row(&v.re, &lo, &hi) != ab_graph_row(&y, &lo, &hi) || abn_cmp(&x, &xc) != 0;
                differ += abn_cmp(&v.re, &y) != 0;
                switch (i) {
                case 0: fn_sin(&want, &x, FN_RAD); break;
                case 1: fn_ln(&want, &x); break;
                case 2: fn_asinh(&want, &x); break;
                case 3: fn_atan(&want, &x, FN_RAD); break;
                default: fn_tanh(&want, &x); break;
                }
                notcr += abn_cmp(&y, &want) != 0;
            }
        }
        expect(samples == 5 * AB_GRAPH_W && disagree == 0 && notcr == 0,
               "044: every column's pixel and the trace's readout there agree; the readout correctly rounded");
        expect(differ > 0 && zpix == 0 && zread > 0,
               "044: the plot's samples are the engine's (no correctly rounded computation; some differ from the readout)");
        ab_init(&c);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_do_arg(&c, AB_SETFIX, 4);
        ab_eqn_add(&c, "LN(X)");
        ab_do(&c, AB_CANCEL);
        ab_do(&c, AB_K1); ab_do(&c, AB_ENTER); ab_do(&c, AB_K2);
        ab_do(&c, AB_EQN);
        unsigned long zi = fn_ziv_calls;
        abn_status ist = ab_do_arg(&c, AB_INTEG, AB_VAR_A + 23);
        zi = fn_ziv_calls - zi;
        bool was = fn_set_fast(false);
        abn r30, x30 = abn_from_int(30);
        fn_sin(&r30, &x30, FN_RAD);
        char t30[48];
        abn_to_text(&r30, t30, sizeof t30);
        expect(ist == ABN_OK && zi == 0, "044: an integral's integrand takes the engine's path (no correctly rounded computation)");
        expect(!was && !strcmp(t30, "-9880316240928617899877489072944582E-34"),
               "044: after an integral the switch is off: RAD 30 SIN is ...4582 again");
        /* the app's plot with TRACE: some 50 frames while it draws, the readout computed once for its column; a cursor
           moved, once more, correctly rounded at the new column */
        static app_graph g44;
        const pwr_inputs idle = {false, false, 80, false};
        ab_init(&c);
        app_init(&a, &c, now);
        c.stu = c.m35 = true;
        ab_do(&c, AB_RAD);
        ab_eqn_add(&c, "SIN(X)");
        ab_do(&c, AB_EQN);
        app_graph_buffer(&a, &g44);
        unsigned long za = fn_ziv_calls;
        press(&a, "BLUE 7 k:5");
        int frames = 0;
        for (int i = 0; i < 200 && a.graphing; i++, frames++) {
            app_tick(&a, &idle, now);
            app_ui(&a, &ui);
            app_ui(&a, &ui);
        }
        za = fn_ziv_calls - za;
        abn want0;
        fn_sin(&want0, &g44.s.cx, FN_RAD);
        bool at0 = c.gr_trace && abn_cmp(&g44.s.cy.re, &want0) == 0;
        int col0 = c.gr_cur;
        press(&a, "6");
        app_ui(&a, &ui);
        abn want1;
        fn_sin(&want1, &g44.s.cx, FN_RAD);
        expect(frames > 20 && za <= 2 && at0 && c.gr_cur != col0 && abn_cmp(&g44.s.cy.re, &want1) == 0 &&
                   abn_cmp(&g44.s.cy.re, &want0) != 0,
               "044: a plot's frames compute the trace's readout once; a moved cursor's readout is its new column's");
    }

    /* unit 073: the list's sixth key is EXIT (unit 020's): Equation mode left, the list kept, in each mode */
    {
        static const int MODEL[3] = {AB_MODEL_STU, 35, 33};
        int ok = 0;
        for (int m = 0; m < 3; m++) {
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, MODEL[m]);
            ab_eqn_add(&c, "A+B");
            int n0 = c.eqn_count;
            press(&a, "k:5");
            ok += !c.eqn_mode && c.eqn_count == n0 && !c.eqn_typing;
        }
        expect(ok == 3, "073: EXIT on the list's F6 leaves Equation mode, the list kept (STU, 35s, 33s)");
    }

    /* unit 046 (abacus #4996): EQN LIST's sixth soft key is "( )", in EXIT's place: from EQN LIST TOP and from an
       equation shown, in each mode, it begins a new equation with the pair, the cursor between (35s p.6-6), and the
       rest types as ever; ENTER stores it. GOLD's bank holds REGX..REGT as a first character, hidden and dimmed in
       33s mode (041 A11). C from the list still stores and leaves (EXIT's route: nothing lost). */
    {
        static const int MODEL[3] = {AB_MODEL_STU, 35, 33};
        int paren = 0, reg = 0, regdim = 0, cexit = 0;
        for (int m = 0; m < 3; m++) {
            for (int from = 0; from < 2; from++) {
                ab_init(&c);
                app_init(&a, &c, now);
                ab_do_arg(&c, AB_MODEL, MODEL[m]);
                ab_eqn_add(&c, "A+B");
                if (!c.eqn_mode) press(&a, "GOLD 0");
                if (from == 0) press(&a, "k:3");               /* NEW: EQN LIST TOP */
                press(&a, "GOLD k:5");                          /* 073: ( ) on GOLD's sixth key */
                bool typing = c.eqn_typing && !strcmp(c.ebuf, "()");
                press(&a, "2 + 1 k:1 ENTER");
                paren += typing && !c.eqn_typing && c.eqn_mode && !strcmp(c.eqn[c.eqn_cur], "(2+1)");
            }
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, MODEL[m]);
            ab_eqn_add(&c, "A+B");
            if (!c.eqn_mode) press(&a, "GOLD 0");
            screen_ui u46;
            tap(&a, shift_key(1));
            app_ui(&a, &u46);
            bool shown = u46.soft[0] && !strcmp(u46.soft[0], "REGX") && u46.soft[3] && !strcmp(u46.soft[3], "REGT");
            press(&a, "k:0");
            if (MODEL[m] == 33) {
                regdim += shown && u46.soft_dim[0] && !c.eqn_typing;
                reg++;
            } else {
                reg += shown && !u46.soft_dim[0] && c.eqn_typing && !strcmp(c.ebuf, "REGX");
                regdim++;
            }
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, MODEL[m]);
            ab_eqn_add(&c, "A+B");
            if (!c.eqn_mode) press(&a, "GOLD 0");
            int n0 = c.eqn_count;
            press(&a, "C");
            cexit += !c.eqn_mode && c.eqn_count == n0;
        }
        expect(paren == 6, "046, 073: ( ) on EQN LIST GOLD's sixth key begins an equation with the pair, from TOP and from "
                           "an equation shown, in STU, 35s and 33s modes; ENTER stores (2+1)");
        expect(reg == 3 && regdim == 3, "046: GOLD's bank in EQN LIST: REGX..REGT begin an equation (STU, 35s); in 33s "
                                        "mode dimmed and doing nothing");
        expect(cexit == 3, "046: C from EQN LIST still leaves Equation mode, the list kept");
    }

    /* unit 047: an equation nested past the parser's bound (153 units: 20 groups are 160) is refused at ENTER with
       NESTED TOO DEEP, still typed, nothing stored: in STU and 35s modes, and in program entry. An older image holding
       one loads, and using it answers NESTED TOO DEEP (quarantined: never parsed unbounded). */
    {
        static const int MODEL[2] = {AB_MODEL_STU, 35};
        for (int m = 0; m < 2; m++) {
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, MODEL[m]);
            ab_eqn_add(&c, "A+B");
            if (!c.eqn_mode) press(&a, "GOLD 0");
            int n0 = c.eqn_count;
            press(&a, "GOLD k:5");                  /* ( ) begins the equation (046; GOLD's F6 since 073) */
            for (int i = 0; i < 19; i++) press(&a, "k:2");
            press(&a, "RCL ltr:X ENTER");
            const char *msg = ab_error_message(&c);
            char what[96];
            snprintf(what, sizeof what, "047: model %d: 20 groups typed, ENTER: NESTED TOO DEEP, still typed, nothing stored",
                     MODEL[m]);
            expect(msg && !strcmp(msg, "NESTED TOO DEEP") && c.eqn_typing && c.eqn_count == n0, what);
        }
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        press(&a, "GOLD 9 k:0");                    /* program entry */
        int l0 = c.prog_len;
        press(&a, "GOLD 0 k:2");
        for (int i = 0; i < 19; i++) press(&a, "k:2");
        press(&a, "RCL ltr:X ENTER");
        const char *pm = ab_error_message(&c);
        expect(pm && !strcmp(pm, "NESTED TOO DEEP") && c.prog_len == l0,
               "047: program entry: a 20-group line refused at ENTER with NESTED TOO DEEP, no line added");
        /* an image from before 047 holding an over-deep equation (written into the list directly, as it was stored) */
        static uint8_t img[1 << 16];
        static ab_calc c2;
        ab_init(&c);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c, "X+1");
        char deep[200] = "";
        for (int i = 0; i < 30; i++) strcat(deep, "(");
        strcat(deep, "X");
        for (int i = 0; i < 30; i++) strcat(deep, ")");
        snprintf(c.eqn[c.eqn_cur], AB_EQN_LEN, "%s", deep);
        size_t n = ab_state_save(&c, img, sizeof img);
        ab_init(&c2);
        abn_status lst = n ? ab_state_load(&c2, img, n) : ABN_BAD_IMAGE;
        expect(lst == ABN_OK && !strcmp(c2.eqn[c2.eqn_cur], deep), "047: an older image's over-deep equation loads, kept as text");
        if (!c2.eqn_mode) ab_do(&c2, AB_EQN);       /* (the image keeps Equation mode) */
        abn_status xst = ab_do_arg(&c2, AB_XEQ, AB_TGT_EQN);  /* XEQ: X? first, then R/S evaluates */
        for (int i = 0; i < 4 && xst == ABN_OK && c2.stop == AB_STOP_INPUT; i++) xst = ab_do(&c2, AB_RS);
        expect(xst == ABN_TOO_DEEP && c2.err_status == ABN_TOO_DEEP,
               "047: using it answers NESTED TOO DEEP (quarantined: its parse bounded)");
        /* fresh eyes: it can be looked at and left as it was (an unchanged edit stores without the check); CAS
           answers NESTED TOO DEEP in each stamp, nothing stored (casim never walks it); SUBST's over-deep replacement
           answers NESTED TOO DEEP, not SYNTAX */
        ab_init(&c2);
        ab_state_load(&c2, img, n);
        if (!c2.eqn_mode) ab_do(&c2, AB_EQN);
        ab_do(&c2, AB_KRIGHT);                      /* the editor open on it */
        bool opened = c2.eqn_typing && c2.eqn_edit == c2.eqn_cur;
        abn_status cst = ab_do(&c2, AB_CANCEL);
        expect(opened && cst == ABN_OK && !c2.eqn_typing && !strcmp(c2.eqn[c2.eqn_cur], deep),
               "047: viewing the over-deep equation and leaving it unchanged stores it as it was");
        static const int STAMP[3] = {AB_MODEL_STU, 35, 33};
        for (int k = 0; k < 3; k++)
            for (int op = 0; op < 2; op++) {
                ab_init(&c2);
                ab_do_arg(&c2, AB_MODEL, AB_MODEL_STU);
                ab_eqn_add(&c2, "X+1");
                snprintf(c2.eqn[c2.eqn_cur], AB_EQN_LEN, "%s", deep);
                c2.eqn_model[c2.eqn_cur] = (uint8_t)STAMP[k];
                if (!c2.eqn_mode) ab_do(&c2, AB_EQN);
                int n2 = c2.eqn_count;
                abn_status ks = op ? ab_do_arg(&c2, AB_CASD, 'X' - 'A') : ab_do(&c2, AB_CASSIMP);
                char what[96];
                snprintf(what, sizeof what, "047: CAS %s of an over-deep equation stamped %d: NESTED TOO DEEP, nothing stored",
                         op ? "D/DX" : "SIMP", STAMP[k]);
                expect(ks == ABN_TOO_DEEP && c2.eqn_count == n2, what);
            }
        ab_init(&c2);
        ab_do_arg(&c2, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c2, "X+1");
        if (!c2.eqn_mode) ab_do(&c2, AB_EQN);
        ab_do_arg(&c2, AB_CASSUB, 'X' - 'A');
        char g20[64] = "";
        for (int i = 0; i < 20; i++) strcat(g20, "(");
        strcat(g20, "Y");
        for (int i = 0; i < 20; i++) strcat(g20, ")");
        snprintf(c2.ebuf, sizeof c2.ebuf, "%s", g20);
        abn_status ss = ab_do(&c2, AB_ENTER);
        expect(ss == ABN_TOO_DEEP && c2.eqn_typing, "047: SUBST's 20-group replacement: NESTED TOO DEEP, still typed");
    }

    /* unit 048 (abacus #5267): CAS's sixth key opens its second page, SOLVE ∫DX FACTR, each prompting for the variable;
       SOLVE's report (the CAS REPORT bar, rows k-3..k with the cursor's on X), ENTER copying a root through the core
       and showing X; ∫'s +C beside the answer on the screen, never in its text; FACTOR's MAY FACTOR MORE above its
       answer; neither the report nor a note in the image; the quarantine (047) before casim on each key */
    {
        static screen_page pg;
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c, "X^2-5\xc3\x97X+6=0");
        press(&a, "k:4");
        app_ui(&a, &ui);
        expect(ui.soft[5] && !strcmp(ui.soft[5], "\xe2\x96\xb8"), "048: CAS's sixth key is \xe2\x96\xb8");
        press(&a, "k:5");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "SOLVE") && ui.soft[1] && !strcmp(ui.soft[1], "\xe2\x88\xab" "DX") &&
                   ui.soft[2] && !strcmp(ui.soft[2], "FACTR") && ui.soft[3] && !strcmp(ui.soft[3], "\xe2\x88\xab" "AB") &&
                   ui.soft[4] && !strcmp(ui.soft[4], "\xce\xa3") && ui.soft[5] && !strcmp(ui.soft[5], "\xe2\x96\xb8"),
               "048, 056, 070: CAS2: SOLVE \xe2\x88\xab" "DX FACTR \xe2\x88\xab" "AB \xce\xa3, its sixth key back");
        press(&a, "k:0");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "SOLVE _"), "048: SOLVE prompts for the variable");
        int n = c.eqn_count;
        press(&a, "ltr:X");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(ab_cas_report(&c) && c.eqn_count == n && ui.soft[0] && !strcmp(ui.soft[0], "\xe2\x96\xb2") &&
                   strstr(pg.x.text, "SOLVED") && !pg.y.text[0],
               "048: SOLVE X: the report, the status row on X, the CAS REPORT bar, nothing stored");
        expect(pg.view, "048: the report's page says it is a view (tally #5455)");
        press(&a, "k:1");                       /* down */
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(pg.x.text, "X=2") && strstr(pg.y.text, "SOLVED"), "048: down: X=2 on X, the status above");
        press(&a, "k:1 ENTER");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        abn three = abn_from_int(3);
        expect(!ab_cas_report(&c) && !c.eqn_mode && abn_cmp(&c.x.re, &three) == 0 && c.eqn_count == n &&
                   strstr(pg.x.text, "3"),
               "048: ENTER on X=3: 3 in X, shown, the report gone, nothing stored");
        expect(!pg.view, "048: the stack's page is not a view");
        /* the image keeps neither the report nor a note */
        static uint8_t img[1 << 16];
        static ab_calc c3;
        ab_eqn_add(&c, "X^2-1");
        ab_do_arg(&c, AB_CASSOLVE, 'X' - 'A');
        size_t len = ab_state_save(&c, img, sizeof img);
        ab_init(&c3);
        expect(ab_cas_report(&c) && len && ab_state_load(&c3, img, len) == ABN_OK && !ab_cas_report(&c3),
               "048: the report is not in the image");
        /* unit 056: ∫AB through the keys: the variable, the lower bound, ENTER, the upper, ENTER; the exact answer
           inserted after the integrand, no note beside it */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_do(&c, AB_RAD);
        ab_eqn_add(&c, "1\xc3\xb7X");
        press(&a, "k:4 k:5 k:3");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "\xe2\x88\xab" "AB _"), "056: \xe2\x88\xab" "AB prompts for the variable");
        n = c.eqn_count;
        press(&a, "ltr:X 1 ENTER 3 ENTER");
        expect(c.eqn_count == n + 1 && c.eqn_cur >= 0 && !strcmp(c.eqn[c.eqn_cur], "LN(3)") && !ab_cas_note(&c) &&
                   !c.eqn_typing && !c.cas_def,
               "056: \xe2\x88\xab" "AB X of 1\xc3\xb7X from 1 to 3 through the keys: LN(3) inserted, no note, the step closed");
        /* unit 070: Σ through the keys (CAS2's fifth key): the variable, "ΣK FROM", ENTER, "ΣK TO"; while the upper
           bound is typed the bar's fourth key is ∞ (= elsewhere) and types it; X^K from 0 to ∞ inserts its answer,
           shown, then the condition after it, labeled WHERE */
        {
            char lb[24];
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
            ab_do(&c, AB_RAD);
            ab_eqn_add(&c, "X^K");
            press(&a, "k:4 k:5 k:4");
            app_ui(&a, &ui);
            expect(ui.prompt && !strcmp(ui.prompt, "\xce\xa3 _"), "070: \xce\xa3 prompts for the variable");
            press(&a, "ltr:K");
            app_ui(&a, &ui);
            expect(ab_cas_prompt_label(&c, lb, sizeof lb) && !strcmp(lb, "\xce\xa3K FROM") && ui.soft[3] &&
                       !strcmp(ui.soft[3], "="), "070: \xce\xa3K FROM on Y; the bar's fourth key = there");
            press(&a, "k:3");
            expect(c.ebuf[0] == '=', "070: in the lower bound the fourth key types = as ever (no \xe2\x88\x9e)");
            press(&a, "\xe2\x86\x90 0 ENTER");
            app_ui(&a, &ui);
            expect(ab_cas_prompt_label(&c, lb, sizeof lb) && !strcmp(lb, "\xce\xa3K TO") && ui.soft[3] &&
                       !strcmp(ui.soft[3], "\xe2\x88\x9e"), "070: \xce\xa3K TO on Y; the bar's fourth key \xe2\x88\x9e");
            press(&a, "k:3");
            expect(!strcmp(c.ebuf, "\xe2\x88\x9e"), "070: the fourth key types \xe2\x88\x9e in the upper bound");
            n = c.eqn_count;
            press(&a, "ENTER");
            expect(c.eqn_count == n + 2 && !strcmp(c.eqn[c.eqn_cur], "-1\xc3\xb7(X-1)") && !ab_cas_note(&c) &&
                       c.eqn_note[c.eqn_cur + 1] == AB_NOTE_WHERE && !strcmp(c.eqn[c.eqn_cur + 1], "ABS(X)<1") &&
                       !c.eqn_typing && !c.cas_def,
                   "070: \xce\xa3 X^K from 0 to \xe2\x88\x9e: the answer shown, ABS(X)<1 after it labeled WHERE, the step closed");
            app_ui(&a, &ui);
            expect(ui.soft[3] && !strcmp(ui.soft[3], "NEW"), "070: the step closed: the list's bar, NEW in the \xe2\x88\x9e's place");
            /* ∫AB's upper bound keeps the plain bar */
            ab_init(&c);
            app_init(&a, &c, now);
            ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
            ab_eqn_add(&c, "X");
            press(&a, "k:4 k:5 k:3 ltr:X 0 ENTER");
            app_ui(&a, &ui);
            expect(c.cas_def && c.cas_def_step == 2 && ui.soft[3] && !strcmp(ui.soft[3], "="),
                   "070: \xe2\x88\xab" "AB's upper bound keeps = on the bar");
        }
        /* unit 057: CAS2's sixth key goes on to CAS3 (QUOT, GCD, CANCL); QUOT through the keys: the variable, g, ENTER:
           q and r inserted, q shown, labeled QUOTIENT */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_do(&c, AB_RAD);
        ab_eqn_add(&c, "X^2-1");
        press(&a, "k:4 k:5 k:5");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "QUOT") && ui.soft[1] && !strcmp(ui.soft[1], "GCD") && ui.soft[2] &&
                   !strcmp(ui.soft[2], "CANCL") && ui.soft[5] && !strcmp(ui.soft[5], "\xe2\x96\xb8"),
               "057: CAS2's sixth key: CAS3, QUOT GCD CANCL, its sixth key back");
        press(&a, "k:5");
        app_ui(&a, &ui);
        expect(ui.soft[0] && !strcmp(ui.soft[0], "D/DX"), "057: CAS3's sixth key back to CAS");
        press(&a, "k:5 k:5 k:0");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "QUOT _"), "057: QUOT prompts for the variable (CAS, CAS2, CAS3 again)");
        n = c.eqn_count;
        press(&a, "ltr:X 2 ENTER");
        expect(c.eqn_count == n + 2 && ab_cas_note(&c) && !strcmp(ab_cas_note(&c), "QUOTIENT") && !c.eqn_typing &&
                   !c.cas_pop && c.eqn_note[c.eqn_cur + 1] == AB_NOTE_REM,
               "057: QUOT X by 2 through the keys: q and r inserted, q shown as QUOTIENT, r after it as REMAINDER");
        /* ∫: +C beside the answer on the screen, never in its text; since 057 (#5632) it stays with its answer, in the
           image and through the list's moves */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_do(&c, AB_RAD);
        ab_eqn_add(&c, "3\xc3\x97X^2+2\xc3\x97X+1");
        press(&a, "k:4 k:5 k:1");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "\xe2\x88\xab" "DX _"), "048: \xe2\x88\xab" "DX prompts for the variable");
        press(&a, "ltr:X");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(c.eqn[c.eqn_cur], "X+X^2+X^3") && strstr(pg.x.text, "X+X^2+X^3") && strstr(pg.x.text, "+C") &&
                   !strchr(c.eqn[c.eqn_cur], 'C'),
               "048: \xe2\x88\xab" "DX X: the answer shown with +C beside it, the text without it");
        len = ab_state_save(&c, img, sizeof img);
        ab_init(&c3);
        expect(len && ab_state_load(&c3, img, len) == ABN_OK && ab_cas_note(&c3) && !strcmp(ab_cas_note(&c3), "+C") &&
                   !strcmp(c3.eqn[c3.eqn_cur], c.eqn[c.eqn_cur]),
               "048, 057: after a reload the answer still has +C (the label is in the image)");
        ab_do(&c, AB_BST);                      /* the list up and back down */
        ab_do(&c, AB_SST);
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(strstr(pg.x.text, "+C") != NULL, "048, 057: up and back down: +C still beside its answer");
        /* FACTOR's unproven factor: MAY FACTOR MORE above the answer */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c, "69120\xc3\x97X^12-883584\xc3\x97X^11+3675072\xc3\x97X^10-4777824\xc3\x97X^9-2178000\xc3\x97X^8+"
                       "7710864\xc3\x97X^7-2213952\xc3\x97X^6-2071296\xc3\x97X^5+775920\xc3\x97X^4+114000\xc3\x97X^3-"
                       "36000\xc3\x97X^2-2");
        press(&a, "k:4 k:5 k:2");
        app_ui(&a, &ui);
        expect(ui.prompt && !strcmp(ui.prompt, "FACTR _"), "048: FACTR prompts for the variable");
        n = c.eqn_count;
        press(&a, "ltr:X");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(c.eqn_count == n + 1 && !strcmp(pg.y.text, "MAY FACTOR MORE") && !c.msg,
               "048: FACTR's PARTIAL: inserted, MAY FACTOR MORE above it, not an error");
        /* fresh eyes: the note only over its answer: a prompt on X (SOLVE's, waiting for the variable) has none */
        press(&a, "k:4 k:5 k:0");
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(ab_cas_note(&c) && strstr(pg.x.text, "SOLVE") && strcmp(pg.y.text, "MAY FACTOR MORE") != 0,
               "048: SOLVE's prompt on X: no MAY FACTOR MORE over it");
        press(&a, "C");
        /* another key leaves the report and acts; a long report shows four rows, the cursor's on X */
        ab_init(&c);
        app_init(&a, &c, now);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c, "(X-1)\xc3\x97(X-2)\xc3\x97(X-3)\xc3\x97(X-4)");
        ab_do_arg(&c, AB_CASSOLVE, 'X' - 'A');
        for (int i = 0; i < 4; i++) ab_do(&c, AB_SST);
        app_ui(&a, &ui);
        screen_lines(&c, &ui, &pg);
        expect(!strcmp(pg.x.text, "X=4") && !strcmp(pg.t.text, "X=1"), "048: five rows: X=1 on T down to X=4 on X");
        ab_do(&c, AB_EQN);
        expect(!ab_cas_report(&c) && !c.eqn_mode, "048: EQN leaves the report and acts (Equation mode off)");
        /* the quarantine before casim, each key */
        static const ab_op OPS[3] = {AB_CASSOLVE, AB_CASINT, AB_CASFACT};
        for (int k = 0; k < 3; k++) {
            ab_init(&c);
            ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
            ab_eqn_add(&c, "X+1");
            char deep[200] = "";
            for (int i = 0; i < 30; i++) strcat(deep, "(");
            strcat(deep, "X");
            for (int i = 0; i < 30; i++) strcat(deep, ")");
            snprintf(c.eqn[c.eqn_cur], AB_EQN_LEN, "%s", deep);
            int n2 = c.eqn_count;
            abn_status ks = ab_do_arg(&c, OPS[k], 'X' - 'A');
            expect(ks == ABN_TOO_DEEP && c.eqn_count == n2 && !ab_cas_report(&c),
                   k == 0 ? "048: SOLVE of an over-deep equation: NESTED TOO DEEP" : k == 1 ? "048: \xe2\x88\xab of it: NESTED TOO DEEP"
                                                                         : "048: FACTOR of it: NESTED TOO DEEP");
        }
    }

    /* unit 076 (decision 88, abacus #9803): CAS3's fourth key is CPLX, SOLVE's complex roots: off at MEMORY CLEAR,
       marked when on; the image keeps it (a fresh calculator loads it on); UNDO takes the toggle back, as a setting */
    {
        static uint8_t u0[262144], u1[262144], im[65536];
        static ab_calc c2;
        ab_init(&c);
        app_init(&a, &c, now);
        app_undo_buffers(&a, u0, u1, sizeof u0);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_eqn_add(&c, "X^2+X+1");
        press(&a, "k:4 k:5 k:5");
        app_ui(&a, &ui);
        expect(ui.soft[3] && !strcmp(ui.soft[3], "CPLX") && !ui.soft_on[3] && !c.cas_cplx,
               "076: CAS3's fourth key is CPLX, off at MEMORY CLEAR, unmarked");
        press(&a, "k:3");
        app_ui(&a, &ui);
        expect(c.cas_cplx, "076: CPLX on");
        expect(ui.soft[3] && !strcmp(ui.soft[3], "CPLX") && ui.soft_on[3],
               "076: CAS3 stays open, CPLX's key marked at once (as the buzzer's uses; fresh eyes)");
        size_t n = ab_state_save(&c, im, sizeof im);
        ab_init(&c2);
        expect(n > 0 && n <= sizeof im && ab_state_load(&c2, im, n) == ABN_OK && c2.cas_cplx,
               "076: the image keeps CPLX: a fresh calculator loads it on");
        press(&a, "C BLUE \xe2\x86\x90");           /* the menu closed, then UNDO */
        expect(!c.cas_cplx, "076: UNDO takes the toggle back, as a setting");
        n = ab_state_save(&c, im, sizeof im);
        ab_init(&c2);
        c2.cas_cplx = true;
        expect(ab_state_load(&c2, im, n) == ABN_OK && !c2.cas_cplx, "076: an image with CPLX off loads it off");
    }
    printf("app_test: %d/%d checks\n", checks - bad, checks);
    return bad != 0;
}
