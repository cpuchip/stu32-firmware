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

    printf("power_test: %d/%d checks\n", checks - bad, checks);
    return bad != 0;
}
