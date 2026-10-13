/* power_test: the fuel gauge driver against a model of the part, the power policy, and the boot
 * levels of Rev A's outputs (unit 018 scope 5). The model is the datasheet's register behaviour
 * (gauge.h): 16-bit words high byte first, an 8-bit write ignored, a NACK at any other address. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gauge.h"
#include "piezo.h"
#include "pins.h"
#include "power.h"

static int bad, checks;

static void expect(int ok, const char *what)
{
    checks++;
    if (!ok && bad++ < 40) printf("power_test: %s\n", what);
}

typedef struct {
    uint16_t reg[256];
    int writes, reads;
    bool dead;              /* no part on the bus */
} part;

static bool xfer(void *ctx, uint8_t addr, const uint8_t *w, size_t wn, uint8_t *r, size_t rn)
{
    part *p = ctx;
    if (p->dead || addr != GAUGE_ADDR || wn < 1) return false;
    if (wn == 3) {                              /* a word: the register, high byte, low byte */
        p->reg[w[0]] = (uint16_t)(w[1] << 8 | w[2]);
        p->writes++;
    }                                           /* wn == 2: an 8-bit write, no effect (p.10) */
    if (rn) {
        uint16_t v = p->reg[w[0]];
        for (size_t i = 0; i < rn; i++) r[i] = (uint8_t)(i % 2 == 0 ? v >> 8 : v);
        p->reads++;
    }
    return true;
}

int main(void)
{
    static part m;
    gauge_bus bus = {&m, xfer};
    uint16_t v;

    /* the scales (pp.10-13) */
    expect(gauge_microvolts(0xB400) == 3600000, "VCELL 0xB400 is 3.6 V (46080 x 78.125 uV)");
    expect(gauge_microvolts(0) == 0, "VCELL 0");
    expect(gauge_microvolts(0xFFFF) == 5119921, "VCELL full scale, 5.119921875 V, truncated");
    expect(gauge_microvolts(16) == 1250, "16 bits are the 1.25 mV resolution");
    expect(gauge_percent(0x3200) == 50, "SOC 0x3200 is 50 %");
    expect(gauge_percent(0x327F) == 50 && gauge_percent(0x3280) == 51, "SOC rounded to the nearest percent");
    expect(gauge_percent(0x0000) == 0 && gauge_percent(0x007F) == 0 && gauge_percent(0x0080) == 1, "SOC near empty");
    expect(gauge_percent(0x6400) == 100 && gauge_percent(0x6480) == 100 && gauge_percent(0xFFFF) == 100,
           "SOC past 100 % (the ROM model can read high) shown as 100");
    expect(gauge_crate(1) == 208 && gauge_crate(0xFFFF) == -208 && gauge_crate(0x8000) == -32768 * 208,
           "CRATE: 0.208 %/hr a bit, signed");

    /* the words on the bus */
    m.reg[GAUGE_VCELL] = 0xB4C3;
    expect(gauge_read(&bus, GAUGE_VCELL, &v) && v == 0xB4C3, "a register read high byte first");
    expect(gauge_write(&bus, GAUGE_CONFIG, 0x971C) && m.reg[GAUGE_CONFIG] == 0x971C, "a register written as a word");
    m.dead = true;
    expect(!gauge_read(&bus, GAUGE_VCELL, &v) && !gauge_write(&bus, GAUGE_CONFIG, 1), "no part: the calls fail");
    m.dead = false;

    /* boot: the version, and RI cleared with the rest of STATUS kept */
    m.reg[GAUGE_VERSION] = 0x0012;
    m.reg[GAUGE_STATUS] = GAUGE_STATUS_RI | 0x4000 | 0x00AB;   /* RI, EnVR, low byte don't care */
    v = 0;
    expect(gauge_init(&bus, &v) && v == 0x0012, "the part found by its version");
    expect(m.reg[GAUGE_STATUS] == (0x4000 | 0x00AB), "RI cleared, the other bits written back");
    m.writes = 0;
    expect(gauge_init(&bus, &v) && m.writes == 0, "RI already clear: nothing written");
    m.reg[GAUGE_VERSION] = 0x0000;
    expect(!gauge_init(&bus, &v), "a version not 0x001_: not our part");
    m.reg[GAUGE_VERSION] = 0x0021;
    expect(!gauge_init(&bus, &v), "0x0021: not our part");
    m.reg[GAUGE_VERSION] = 0x0011;
    m.dead = true;
    expect(!gauge_init(&bus, &v), "no part: init fails");
    m.dead = false;

    /* a sample */
    gauge_reading g;
    m.reg[GAUGE_VCELL] = 0xB400;
    m.reg[GAUGE_SOC] = 0x4B80;
    m.reg[GAUGE_CRATE] = 0xFFF6;
    m.reg[GAUGE_STATUS] = GAUGE_STATUS_RI;
    expect(gauge_sample(&bus, &g) && g.microvolts == 3600000 && g.percent == 76 && g.crate == -2080 && g.was_reset,
           "a sample: 3.6 V, 76 %, -2.08 %/hr, reset seen");
    m.reg[GAUGE_STATUS] = 0;
    expect(gauge_sample(&bus, &g) && !g.was_reset, "no reset since cleared");
    m.dead = true;
    expect(!gauge_sample(&bus, &g), "no part: no sample");
    m.dead = false;

    /* the policy: 10 minutes idle (p.1-1) */
    pwr_state p;
    pwr_inputs in = {true, true, 50, false};    /* not charging, no input, half charged, idle */
    pwr_init(&p, 1000);
    expect(pwr_check(&p, &in, 1000 + PWR_IDLE_MS - 1) == PWR_STAY, "a millisecond short of 10 minutes: on");
    expect(pwr_check(&p, &in, 1000 + PWR_IDLE_MS) == PWR_OFF_IDLE, "10 minutes idle: off");
    pwr_activity(&p, 500000);
    expect(pwr_check(&p, &in, 500000 + PWR_IDLE_MS - 1) == PWR_STAY, "a key starts the 10 minutes again");
    expect(pwr_check(&p, &in, 500000 + PWR_IDLE_MS) == PWR_OFF_IDLE, "and they run out");
    pwr_init(&p, 0);
    in.busy = true;
    expect(pwr_check(&p, &in, 700000) == PWR_STAY, "a run in progress: on past 10 minutes");
    in.busy = false;
    expect(pwr_check(&p, &in, 700000 + PWR_IDLE_MS - 1) == PWR_STAY, "the run's end starts the 10 minutes");
    expect(pwr_check(&p, &in, 700000 + PWR_IDLE_MS) == PWR_OFF_IDLE, "then off");
    pwr_init(&p, 0xFFFFFF00u);
    expect(pwr_check(&p, &in, 0xFFFFFF00u + PWR_IDLE_MS - 1) == PWR_STAY, "across the clock's wrap: on");
    expect(pwr_check(&p, &in, 0xFFFFFF00u + PWR_IDLE_MS) == PWR_OFF_IDLE, "across the clock's wrap: off");
    /* unit 023: pwr_next, the ms until pwr_check would act with these inputs */
    pwr_init(&p, 1000);
    expect(pwr_next(&p, &in, 1000) == PWR_IDLE_MS, "pwr_next: the whole 10 minutes at the start");
    expect(pwr_next(&p, &in, 1000 + PWR_IDLE_MS - 1) == 1, "pwr_next: 1 ms before the auto-off");
    expect(pwr_next(&p, &in, 1000 + PWR_IDLE_MS) == 0 && pwr_next(&p, &in, 1000 + PWR_IDLE_MS + 5000) == 0,
           "pwr_next: 0 once it is due");
    p.idle_ms = 2 * 60000u;
    expect(pwr_next(&p, &in, 61000) == 60000, "pwr_next: follows the OFF setting (2 minutes, 1 gone)");
    pwr_init(&p, 0xFFFFFF00u);
    expect(pwr_next(&p, &in, 0xFFFFFF00u + 100) == PWR_IDLE_MS - 100, "pwr_next: across the clock's wrap");
    {
        pwr_inputs low = {true, true, PWR_LOW_SOC - 1, false}, low_usb = {true, false, PWR_LOW_SOC - 1, false};
        pwr_init(&p, 0);
        expect(pwr_next(&p, &low, 10) == 0, "pwr_next: a low battery with no input is due now");
        expect(pwr_next(&p, &low_usb, 10) == PWR_IDLE_MS - 10, "pwr_next: a low battery on USB waits for the auto-off");
    }
    {   /* the contract, at random: pwr_check before the deadline stays on, at it turns off */
        unsigned seed = 12345;
        bool ok = true;
        for (int i = 0; i < 2000; i++) {
            seed = seed * 1103515245u + 12345u;
            uint32_t start = seed;
            seed = seed * 1103515245u + 12345u;
            uint32_t now = start + (seed >> 8) % (PWR_IDLE_MS + 50000u);
            seed = seed * 1103515245u + 12345u;
            pwr_inputs ri = {true, (seed >> 4) & 1, (int)((seed >> 8) % 100), false};
            pwr_init(&p, start);
            uint32_t w = pwr_next(&p, &ri, now);
            if (w > 0) {
                pwr_state q = p;
                seed = seed * 1103515245u + 12345u;
                ok = ok && pwr_check(&q, &ri, now + (seed >> 8) % w) == PWR_STAY;
            }
            pwr_state q = p;
            ok = ok && w != UINT32_MAX && pwr_check(&q, &ri, now + w) != PWR_STAY;
        }
        expect(ok, "pwr_next's contract over 2000 random states");
    }
    /* low battery */
    pwr_init(&p, 0);
    in.soc = PWR_LOW_SOC - 1;
    expect(pwr_check(&p, &in, 10) == PWR_OFF_LOW, "below 3 % on the cell: off");
    in.busy = true;
    expect(pwr_check(&p, &in, 10) == PWR_OFF_LOW, "a run does not hold a low cell on");
    in.busy = false;
    in.soc = PWR_LOW_SOC;
    expect(pwr_check(&p, &in, 10) == PWR_STAY, "3 %: on");
    in.soc = 0;
    in.pgood_n = false;
    expect(pwr_check(&p, &in, 10) == PWR_STAY, "an input source present: a low cell stays on");
    in.pgood_n = true;
    in.soc = -1;
    expect(pwr_check(&p, &in, 10) == PWR_STAY, "the gauge not read: no low-battery off");
    /* the charger's pins */
    in.chg_n = false;
    in.pgood_n = false;
    expect(pwr_charging(&in) && pwr_external(&in), "CHG_N and PGOOD_N low: charging, on input");
    in.chg_n = true;
    expect(!pwr_charging(&in) && pwr_external(&in), "CHG_N high on input: charged (or disabled)");
    in.pgood_n = true;
    expect(!pwr_charging(&in) && !pwr_external(&in), "both high: on the cell");

    /* boot levels: the boost off (never before the display wants it), the panel's inputs low until
       its 5 V is up (Sharp 6-2; facts risk 4), the charger in USB500 (BQ Table 7-2: EN2 0, EN1 1) */
    int seen[64] = {0}, lv[64];
    for (int i = 0; i < PIN_BOOT_COUNT; i++) {
        int pin = PIN_BOOT[i].pin;
        expect(pin >= 0 && pin < 64 && !seen[pin], "a boot pin out of range or listed twice");
        if (pin >= 0 && pin < 64) {
            seen[pin] = 1;
            lv[pin] = PIN_BOOT[i].level;
        }
    }
    expect(PIN_BOOT_COUNT > 0 && PIN_BOOT[0].pin == PIN_BOOST_EN && !PIN_BOOT[0].level, "BOOST_EN low, first");
    expect(seen[PIN_EN1] && lv[PIN_EN1] == 1 && seen[PIN_EN2] && lv[PIN_EN2] == 0, "EN1 high, EN2 low: USB500");
    expect(seen[PIN_SCS] && !lv[PIN_SCS] && seen[PIN_DISP] && !lv[PIN_DISP] && seen[PIN_EXTCOMIN] &&
               !lv[PIN_EXTCOMIN] && seen[PIN_SCLK] && !lv[PIN_SCLK] && seen[PIN_SI] && !lv[PIN_SI],
           "the panel's inputs low");
    expect(!seen[PIN_CHG_N] && !seen[PIN_PGOOD_N] && !seen[PIN_SDA] && !seen[PIN_SCL], "inputs and the bus not driven");
    {
        static const int rows[] = PIN_ROWS, cols[] = PIN_COLS;
        for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) expect(!seen[rows[i]], "a row driven at boot");
        for (size_t i = 0; i < sizeof cols / sizeof cols[0]; i++) expect(!seen[cols[i]], "a column driven at boot");
    }

    /* Rev A pass 6 (decision 34): C on IO0 wakes the chip alone (EXT1 ANY_LOW on an RTC GPIO); IO0
       and the piezo's IO46 (a strapping pin that must read 0 at reset) are not driven at boot */
    {
        uint64_t mask = pin_wake_mask();
        expect(mask == (1ull << PIN_ON) && PIN_ON == 0, "the wake mask: IO0 alone");
        expect((mask >> 22) == 0, "the wake pins are RTC GPIOs (IO0-IO21, facts 2.5)");
        expect(PIN_PIEZO == 46, "the piezo on IO46");
        for (int i = 0; i < PIN_BOOT_COUNT; i++) {
            expect(PIN_BOOT[i].pin != PIN_ON, "IO0 (C) not driven at boot: its pull-up holds it");
            expect(PIN_BOOT[i].pin != PIN_PIEZO, "IO46 untouched until after boot (a strapping pin)");
        }
    }
    /* the radio (decision 36: a buck-boost holds 3.3 V across the cell, so the old regulator's
       brown-out floor and USB TX cap are gone): only a reserve floor remains, so the cell keeps the
       charge for the save at power-off */
    {
        expect(pwr_radio_allowed(3300000), "3.3 V: the radio allowed");
        expect(!pwr_radio_allowed(3299999), "below 3.3 V: no radio (the reserve for the save)");
        expect(pwr_radio_allowed(4200000), "a full cell: allowed");
        expect(!pwr_radio_allowed(0), "the cell not read (0): no radio");
    }

    /* the piezo's volume (abacus 2b8e52d): the fundamental scales as sin(pi * duty), so the levels'
       duties give off, about -16, -10, -3 and 0 dB */
    {
        static const double want_db[PIEZO_LEVELS] = {0, -16.1, -10.2, -3.0, 0.0};
        double prev = -1e9;
        expect(piezo_duty(0, 10) == 0, "level 0: off");
        expect(PIEZO_HZ == 4000, "the tones at the piezo's rated 4 kHz (PKMCS0909E4000; abacus #4073)");
        for (int lv = 1; lv < PIEZO_LEVELS; lv++) {
            uint32_t d = piezo_duty(lv, 10);
            double duty = d / 1024.0, db = 20 * log10(sin(3.14159265358979323846 * duty));   /* pi */
            char m[96];
            snprintf(m, sizeof m, "level %d: %.1f dB (duty %u/1024), want %.1f", lv, db, (unsigned)d, want_db[lv]);
            expect(d > 0 && d <= 512 && fabs(db - want_db[lv]) < 0.5, m);
            expect(db > prev, "each level louder than the one below");
            prev = db;
        }
        expect(piezo_duty(PIEZO_LEVELS - 1, 10) == 512, "the loudest: a square wave, half of 10 bits");
        expect(piezo_duty(PIEZO_LEVELS - 1, 13) == 4096, "the duty follows the resolution");
        expect(piezo_duty(-1, 10) == 0 && piezo_duty(PIEZO_LEVELS, 10) == 0, "a level out of range: off");
    }

    /* OFF waits for C's release (abacus #7561): high for 30 ms is OK, low for 3 s is STUCK, nothing earlier */
    {
        pwr_release_state r;
        uint32_t t0 = 1000;
        pwr_release_init(&r, t0);
        int ok_at = -1;
        for (uint32_t t = 0; t <= 400 && ok_at < 0; t += 5) {   /* C held 300 ms, then released */
            pwr_release s = pwr_release_step(&r, t >= 300, t0 + t);
            if (s == PWR_RELEASE_OK) ok_at = (int)t;
            else expect(s == PWR_RELEASE_WAIT, "release: WAIT while held or not yet steady");
        }
        expect(ok_at == 330, "release: held 300 ms, OK after 30 ms high (at 330)");

        pwr_release_init(&r, t0);                   /* a bounce: high 10 ms twice, then steady */
        static const int lv[] = {1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        ok_at = -1;
        for (int i = 0; i < (int)(sizeof lv / sizeof lv[0]) && ok_at < 0; i++)
            if (pwr_release_step(&r, lv[i], t0 + 5u * (uint32_t)i) == PWR_RELEASE_OK) ok_at = 5 * i;
        expect(ok_at == 65, "release: a bounce restarts the 30 ms (steady high from 35, OK at 65)");

        pwr_release_init(&r, t0);                   /* already released: OK after the debounce */
        expect(pwr_release_step(&r, 1, t0) == PWR_RELEASE_WAIT, "release: high at once, still WAIT");
        expect(pwr_release_step(&r, 1, t0 + 29) == PWR_RELEASE_WAIT, "release: 29 ms high, WAIT");
        expect(pwr_release_step(&r, 1, t0 + 30) == PWR_RELEASE_OK, "release: 30 ms high, OK");

        pwr_release_init(&r, t0);                   /* stuck: low throughout */
        expect(pwr_release_step(&r, 0, t0 + 2999) == PWR_RELEASE_WAIT, "release: low 2,999 ms, WAIT");
        expect(pwr_release_step(&r, 0, t0 + 3000) == PWR_RELEASE_STUCK, "release: low 3 s, STUCK");

        uint32_t tw = 0xFFFFFF00u;                  /* across the clock's wrap */
        pwr_release_init(&r, tw);
        expect(pwr_release_step(&r, 0, tw + 2000) == PWR_RELEASE_WAIT, "release: across the wrap, 2 s low, WAIT");
        expect(pwr_release_step(&r, 1, tw + 2100) == PWR_RELEASE_WAIT, "release: across the wrap, high, WAIT");
        expect(pwr_release_step(&r, 1, tw + 2130) == PWR_RELEASE_OK, "release: across the wrap, 30 ms high, OK");
    }

    printf("power_test: %d/%d checks\n", checks - bad, checks);
    return bad != 0;
}
