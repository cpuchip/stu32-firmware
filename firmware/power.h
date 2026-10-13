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
 * - On USB power (PGOOD_N low) there is no auto-off: Michael, 068, "while plugged in we should keep the s3 in a state
 *   for it to respond to you". A low battery still turns it off.
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
    uint32_t idle_ms;       /* the auto-off time: PWR_IDLE_MS from pwr_init, SETUP's OFF (unit 022) */
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
/* The ms until pwr_check would turn the calculator off with these inputs (0: it would now; UINT32_MAX: never), for
   the device's sleep (unit 023). Not busy: a run's own deadlines are the app's. */
uint32_t pwr_next(const pwr_state *p, const pwr_inputs *in, uint32_t now_ms);
/* OFF waits for C's release before deep sleep (Rev A board 0005, abacus #7561). C is IO0, the only wake pin, and it
   wakes on low; OFF is taken on C's key down, so sleeping at once woke the chip while C was still held. The device
   steps this with IO0's level until it says OK (high for PWR_RELEASE_DEBOUNCE_MS: arm C's wake) or STUCK (still low
   after PWR_RELEASE_WAIT_MS: sleep without C's wake and look again soon, so a key held down in a bag can't keep the
   chip awake, or wake it at once). */
#define PWR_RELEASE_DEBOUNCE_MS 30u
#define PWR_RELEASE_WAIT_MS 3000u
typedef enum { PWR_RELEASE_WAIT = 0, PWR_RELEASE_OK, PWR_RELEASE_STUCK } pwr_release;
typedef struct {
    uint32_t start_ms;      /* when the wait began */
    uint32_t high_ms;       /* when IO0 last went high */
    bool high;              /* IO0 high at the last step */
} pwr_release_state;
void pwr_release_init(pwr_release_state *r, uint32_t now_ms);
pwr_release pwr_release_step(pwr_release_state *r, bool on_high, uint32_t now_ms);
bool pwr_charging(const pwr_inputs *in);
bool pwr_external(const pwr_inputs *in);

#endif
