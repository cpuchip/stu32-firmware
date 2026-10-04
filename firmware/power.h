/* power: when the calculator turns itself off (unit 018 scope 5). Portable C: the device feeds it
 * the time, its keys, the charger's pins and the gauge, and acts on what it returns (save the
 * state image, then deep sleep with the image held on the panel).
 *
 * - Idle: off after 10 minutes of inactivity (UG p.1-1). The guide does not define inactivity
 *   (manual note A6); ours: a key is activity, and so is a program, SOLVE or integral running in
 *   slices (the clock starts again when it stops). A halt at INPUT, VIEW or PSE is not.
 * - Low battery: off when the charge is below PWR_LOW_SOC % and no input source is present, so
 *   memory is saved while the cell still holds the board up (ours; the gauge's datasheet suggests
 *   shutting down at 3 or 5 %, p.8). A program running does not hold this off.
 * - The charger (BQ24072, SLUS810N 10.2.2.4): CHG_N low while charging, PGOOD_N low while a valid
 *   input source is present. */
#ifndef POWER_H
#define POWER_H

#include <stdbool.h>
#include <stdint.h>

#define PWR_IDLE_MS (10u * 60u * 1000u)
#define PWR_LOW_SOC 3

typedef enum { PWR_STAY = 0, PWR_OFF_IDLE, PWR_OFF_LOW } pwr_action;

typedef struct {
    bool chg_n, pgood_n;    /* the pins' levels */
    int soc;                /* percent from the gauge, -1 not read */
    bool busy;              /* a run in slices is in progress */
} pwr_inputs;

typedef struct {
    uint32_t last_ms;       /* the last activity, on a clock that may wrap */
} pwr_state;

/* The radio (Rev A has none in use yet; the rule for when it does, from the 3.3 V regulator's
   margins, abacus #3924/#3925, facts section 6): off below about V_BAT 3.5 V, where TX peaks brown
   the module out (3.2-3.35 V); allowed above, with its TX power capped while on USB, where
   sustained full-power TX reaches the LDO's 0.43 W rating. The cap's value is set by Rev A's
   bring-up soak (the LDO's case temperature under sustained TX), not here. */
#define PWR_RADIO_MIN_UV 3500000u

typedef struct {
    bool allowed;
    bool cap_tx;
} pwr_radio;

pwr_radio pwr_radio_rule(const pwr_inputs *in, uint32_t vbat_uv);

void pwr_init(pwr_state *p, uint32_t now_ms);
void pwr_activity(pwr_state *p, uint32_t now_ms);
pwr_action pwr_check(pwr_state *p, const pwr_inputs *in, uint32_t now_ms);
bool pwr_charging(const pwr_inputs *in);
bool pwr_external(const pwr_inputs *in);

#endif
