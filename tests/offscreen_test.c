/* offscreen_test: the off screen's tiers and wakes, the battery on USB and the time to full (068 (3)). The
 * expectations come from the rulings (offscreen.h: Michael's off-screen and charging-screen, abacus #7585, #7587,
 * #7588), worked by hand; the times from 2026-10-13 14:30:20 (1791901820, the local time held as UTC). */
#include <stdio.h>
#include <string.h>

#include "offscreen.h"

static int bad, checks;

static void expect(int ok, const char *what)
{
    checks++;
    if (!ok && bad++ < 40) printf("offscreen_test: %s\n", what);
}

#define DAY 86400u
#define W 1791901820LL                  /* 2026-10-13 14:30:20 */
#define MIDNIGHT 1791849600LL           /* 2026-10-13 00:00:00 */
#define AT(h, m, s) (MIDNIGHT + (h) * 3600LL + (m) * 60LL + (s))

static void plan_is(const off_params *p, uint32_t since, bool set, int64_t wall, int soc, bool usb, int tier,
                    uint32_t wake, const char *what)
{
    off_plan_t r = off_plan(p, since, set, wall, soc, usb);
    char m[160];
    snprintf(m, sizeof m, "%s: tier %d wake %lu, want %d %lu", what, r.tier, (unsigned long)r.wake_s, tier,
             (unsigned long)wake);
    expect(r.tier == tier && r.wake_s == wake, m);
}

static void test_defaults(void)
{
    off_params p;
    memset(&p, 0x5a, sizeof p);
    off_params_default(&p);
    expect(p.clock_s == DAY, "the minute clock's span is a day");
    expect(p.hib_days == 5, "hibernate after 5 days (#7588 (6), until P0)");
    expect(p.date_soc == 25 && p.hib_soc == 15, "the low-cell cutoffs 25 and 15 (#7588 (5))");
    expect(!p.clock && p.date && p.batt, "the clock opt-in until P0 measures it; the date and the battery on");
}

static void test_tiers(void)
{
    off_params p;
    off_params_default(&p);
    p.clock = true;
    plan_is(&p, 0, true, W, 80, false, OFF_TIER_CLOCK, 40, "just off: the next minute");
    plan_is(&p, 0, true, AT(14, 31, 0), 80, false, OFF_TIER_CLOCK, 60, "on the minute: a whole minute");
    plan_is(&p, DAY - 10, true, W, 80, false, OFF_TIER_CLOCK, 10, "the date tier's start comes before the minute");
    plan_is(&p, DAY - 41, true, W, 80, false, OFF_TIER_CLOCK, 40, "the minute comes before the tier's start");
    plan_is(&p, DAY, true, W, 80, false, OFF_TIER_DATE, OFF_REFRESH_S, "a day off: the date, the refresh first");
    plan_is(&p, DAY, true, AT(23, 0, 0), 80, false, OFF_TIER_DATE, 3600, "the date tier wakes at midnight");
    plan_is(&p, DAY, true, AT(0, 0, 0), 80, false, OFF_TIER_DATE, OFF_REFRESH_S, "at midnight: the next is a day on");
    plan_is(&p, 5 * DAY - 100, true, AT(23, 59, 30), 80, false, OFF_TIER_DATE, 30, "midnight before hibernate");
    plan_is(&p, 5 * DAY - 20, true, AT(12, 0, 0), 80, false, OFF_TIER_DATE, 20, "hibernate before the refresh");
    plan_is(&p, 5 * DAY, true, W, 80, false, OFF_TIER_HIB, OFF_NEVER, "five days: hibernate, no timer");
    plan_is(&p, 40 * DAY, true, W, 80, false, OFF_TIER_HIB, OFF_NEVER, "still hibernating");
    p.hib_days = 0;
    plan_is(&p, 40 * DAY, true, W, 80, false, OFF_TIER_DATE, OFF_REFRESH_S, "hib_days 0: never hibernates");
    p.hib_days = 3;
    plan_is(&p, 3 * DAY, true, W, 80, false, OFF_TIER_HIB, OFF_NEVER, "hib_days 3");
    plan_is(&p, 3 * DAY - 1, true, W, 80, false, OFF_TIER_DATE, 1, "a second before");
    p.hib_days = 5;
    p.clock_s = 3600;
    plan_is(&p, 3600, true, W, 80, false, OFF_TIER_DATE, OFF_REFRESH_S, "clock_s is a parameter");
}

static void test_low_cell(void)
{
    off_params p;
    off_params_default(&p);
    p.clock = true;
    plan_is(&p, 0, true, W, 26, false, OFF_TIER_CLOCK, 40, "26 %: the clock");
    plan_is(&p, 0, true, W, 25, false, OFF_TIER_DATE, OFF_REFRESH_S, "25 %: the date at most");
    plan_is(&p, 0, true, W, 16, false, OFF_TIER_DATE, OFF_REFRESH_S, "16 %: the date");
    plan_is(&p, 0, true, W, 15, false, OFF_TIER_HIB, OFF_NEVER, "15 %: hibernate");
    plan_is(&p, 0, true, W, 0, false, OFF_TIER_HIB, OFF_NEVER, "0 %: hibernate");
    plan_is(&p, 0, true, W, -1, false, OFF_TIER_CLOCK, 40, "the charge not read: no cutoff");
    p.date_soc = 40;
    p.hib_soc = 30;
    plan_is(&p, 0, true, W, 35, false, OFF_TIER_DATE, OFF_REFRESH_S, "the cutoffs are parameters (date)");
    plan_is(&p, 0, true, W, 30, false, OFF_TIER_HIB, OFF_NEVER, "the cutoffs are parameters (hibernate)");
}

static void test_usb_and_unset(void)
{
    off_params p;
    off_params_default(&p);
    p.clock = true;
    plan_is(&p, 10 * DAY, true, W, 10, true, OFF_TIER_CLOCK, 40, "on USB: the clock, whatever the time or charge");
    plan_is(&p, 0, false, 0, 80, false, OFF_TIER_DATE, OFF_REFRESH_S, "the clock unset: no clock, the refresh");
    plan_is(&p, DAY, false, 0, 80, false, OFF_TIER_DATE, OFF_REFRESH_S, "the clock unset, a day on");
    plan_is(&p, 5 * DAY - 7, false, 0, 80, false, OFF_TIER_DATE, 7, "the clock unset: the tiers still run");
    plan_is(&p, 5 * DAY, false, 0, 80, false, OFF_TIER_HIB, OFF_NEVER, "the clock unset: hibernate on time");
    p.clock = false;
    plan_is(&p, 0, true, W, 80, false, OFF_TIER_DATE, OFF_REFRESH_S, "the clock off: the date tier from the start");
    plan_is(&p, 0, true, AT(23, 30, 0), 80, false, OFF_TIER_DATE, 1800, "the clock off: midnight for the date");
    p.date = false;
    plan_is(&p, 0, true, AT(23, 30, 0), 80, false, OFF_TIER_DATE, OFF_REFRESH_S, "no date shown: no midnight wake");
}

static void samples(off_batt *b, uint32_t *t, uint32_t step, int n, uint32_t uv, int32_t dv)
{
    for (int i = 0; i < n; i++, *t += step) off_batt_sample(b, *t, (uint32_t)((int32_t)uv + dv * i));
}

static void test_cell(void)
{
    off_batt b;
    uint32_t t = 1000;
    off_batt_init(&b);
    expect(!off_batt_cell(&b), "no samples: no cell");
    samples(&b, &t, 250, 8, 4000000, 0);
    expect(!off_batt_cell(&b), "8 samples over 1.75 s: not yet");
    samples(&b, &t, 250, 1, 4000000, 0);
    expect(off_batt_cell(&b), "9 samples over 2.0 s, steady: a cell");
    samples(&b, &t, 250, 1, 4100000, 0);
    expect(!off_batt_cell(&b), "a 100 mV jump: not a cell");
    samples(&b, &t, 250, 7, 4100000, 0);
    expect(!off_batt_cell(&b), "the jump's 4.0 V side still in the window");
    samples(&b, &t, 250, 1, 4100000, 0);
    expect(off_batt_cell(&b), "2 s steady again: a cell");

    off_batt_init(&b);
    t = 0;
    samples(&b, &t, 250, 9, 4000000, 0);
    off_batt_sample(&b, t, 4030000);
    t += 250;
    expect(off_batt_cell(&b), "a spread of exactly 30 mV: a cell");
    off_batt_sample(&b, t, 4030001);
    t += 250;
    expect(!off_batt_cell(&b), "30 mV and 1 uV: not");

    off_batt_init(&b);
    t = 0;
    samples(&b, &t, 250, 9, 4250000, 0);
    expect(off_batt_cell(&b), "4.25 V: inside");
    off_batt_init(&b);
    samples(&b, &t, 250, 9, 4250001, 0);
    expect(!off_batt_cell(&b), "above 4.25 V: not");
    off_batt_init(&b);
    samples(&b, &t, 250, 9, 3000000, 0);
    expect(off_batt_cell(&b), "3.0 V: inside");
    off_batt_init(&b);
    samples(&b, &t, 250, 9, 2999999, 0);
    expect(!off_batt_cell(&b), "below 3.0 V: not");

    off_batt_init(&b);
    t = 0;
    samples(&b, &t, 250, 400, 3700000, 1000);
    expect(off_batt_cell(&b), "a charge rising 400 mV over 100 s: a cell (2 s windows)");
    off_batt_init(&b);
    for (int i = 0; i < 40; i++, t += 250) off_batt_sample(&b, t, i % 2 ? 4200000 : 3500000);
    expect(!off_batt_cell(&b), "the charger's detection cycling: not a cell");
    off_batt_init(&b);
    t = 0;
    samples(&b, &t, 100, 20, 3900000, 0);
    expect(!off_batt_cell(&b), "100 ms samples: 20 span 1.9 s, not yet");
    samples(&b, &t, 100, 1, 3900000, 0);
    expect(off_batt_cell(&b), "100 ms samples: 21 span 2.0 s");
    off_batt_init(&b);
    t = 0;
    samples(&b, &t, 1000, 7, 3900000, 0);
    expect(!off_batt_cell(&b), "1 s samples: 7 are too few though they span 6 s");
    samples(&b, &t, 1000, 1, 3900000, 0);
    expect(off_batt_cell(&b), "1 s samples: 8");
}

static void test_usb_state(void)
{
    off_batt b;
    uint32_t t = 0;
    off_batt_init(&b);
    expect(off_usb_state(&b, false, true, t) == OFF_USB_NONE, "on the cell: none");
    expect(off_usb_state(&b, true, false, t) == OFF_USB_NOCELL, "USB, no evidence of a cell: on USB, though CHG_N low");
    for (int i = 0; i < 40; i++, t += 250) {        /* no cell: VBAT cycles and CHG_N with it */
        off_batt_sample(&b, t, i % 2 ? 4200000 : 3500000);
        off_usb_state(&b, true, i % 2, t);
    }
    samples(&b, &t, 250, 9, 4180000, 0);            /* then a cell's steady voltage, CHG_N high */
    expect(off_usb_state(&b, true, true, t) == OFF_USB_IDLE,
           "CHG_N's low while no cell was seen does not make it charged");

    off_batt_init(&b);
    t = 0;
    samples(&b, &t, 250, 9, 3900000, 0);
    expect(off_usb_state(&b, false, true, t) == OFF_USB_NONE, "a cell, no USB: none");
    expect(off_usb_state(&b, true, false, t) == OFF_USB_CHARGING, "a cell, USB, CHG_N low: charging");
    expect(off_usb_state(&b, true, true, t) == OFF_USB_CHARGED, "then CHG_N high: charged");
    expect(off_usb_state(&b, true, true, t) == OFF_USB_CHARGED, "and it stays charged");
    expect(off_usb_state(&b, false, true, t) == OFF_USB_NONE, "unplugged");
    expect(off_usb_state(&b, true, true, t) == OFF_USB_IDLE, "plugged in again, CHG_N high: the % with no word");
    expect(off_usb_state(&b, true, false, t) == OFF_USB_CHARGING, "a top-up starts: charging");
    expect(off_usb_state(&b, true, true, t) == OFF_USB_CHARGED, "and ends: charged");
    expect(b.plug_ms == t, "the session's start is kept for the time to full");
}

static void test_ttf(void)
{
    expect(off_ttf_min(50, 20000, OFF_TTF_SETTLE_MS) == 150, "50 % at 20 %/hr: 150 min");
    expect(off_ttf_min(50, 7000, OFF_TTF_SETTLE_MS) == 430, "50 % at 7 %/hr: 428.6 min, to 430");
    expect(off_ttf_min(28, 60000, OFF_TTF_SETTLE_MS) == 70, "72 min: 70");
    expect(off_ttf_min(27, 60000, OFF_TTF_SETTLE_MS) == 75, "73 min: 75");
    expect(off_ttf_min(99, 30000, OFF_TTF_SETTLE_MS) == 5, "2 min, not full: 5, never 0");
    expect(off_ttf_min(100, 0, OFF_TTF_SETTLE_MS) == 0, "100 %: 0");
    expect(off_ttf_min(50, 0, OFF_TTF_SETTLE_MS) == -1, "CRATE 0: --");
    expect(off_ttf_min(50, -5000, OFF_TTF_SETTLE_MS) == -1, "discharging: --");
    expect(off_ttf_min(50, 20000, OFF_TTF_SETTLE_MS - 1) == -1, "not settled: --");
    expect(off_ttf_min(4, 4000, OFF_TTF_SETTLE_MS) == 1440, "24 h: shown");
    expect(off_ttf_min(0, 4000, OFF_TTF_SETTLE_MS) == -1, "25 h: --");
    expect(off_ttf_min(50, 1, OFF_TTF_SETTLE_MS) == -1, "a tiny CRATE: -- (no overflow)");
    expect(off_ttf_min(-1, 20000, OFF_TTF_SETTLE_MS) == -1, "the charge not read: --");
}

int main(void)
{
    test_defaults();
    test_tiers();
    test_low_cell();
    test_usb_and_unset();
    test_cell();
    test_usb_state();
    test_ttf();
    printf("offscreen_test: %d/%d\n", checks - bad, checks);
    return bad != 0;
}
