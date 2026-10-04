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

/* The radio (Rev A has none in use yet; the rule for when it does). Decision 36's buck-boost holds
   3.3 V across the whole cell, so the radio has no brown-out floor and needs no TX cap on USB (both
   came from the earlier linear regulator, abacus #3935). What remains is a reserve: no radio below
   about 3.3 V on the cell, so it keeps the charge for the state's save at power-off. */
#define PWR_RADIO_MIN_UV 3300000u

bool pwr_radio_allowed(uint32_t vbat_uv);

void pwr_init(pwr_state *p, uint32_t now_ms);
void pwr_activity(pwr_state *p, uint32_t now_ms);
pwr_action pwr_check(pwr_state *p, const pwr_inputs *in, uint32_t now_ms);
bool pwr_charging(const pwr_inputs *in);
bool pwr_external(const pwr_inputs *in);

#endif
