/* offscreen: what the off screen shows and when the chip wakes to change it (068 (3); Michael's rulings off-screen
 * and charging-screen, abacus #7585, #7587, #7588). Pure: no IDF, no clock of its own; the device passes the times.
 *
 * Off on the cell, three tiers by the time since OFF (on the RTC's timer, so they run with the wall clock unset):
 *   OFF_TIER_CLOCK  the minute clock, the date and the battery: a wake at each minute;
 *   OFF_TIER_DATE   the date and the battery: a wake at midnight;
 *   OFF_TIER_HIB    hibernate: the panel cleared and powered down, no timer, C alone wakes it.
 * The clock tier lasts clock_s, the date tier until hib_days; a charge at or below date_soc caps it at the date tier,
 * at or below hib_soc hibernates (both "to give room to let a person use the calculator if they forget", #7588).
 * The panel's image wants a rewrite within Sharp's two hours (p.4), so a tier that shows anything wakes at least every
 * OFF_REFRESH_S. On USB the chip stays up (the soft off, 068 (2)): the clock tier, whatever the time or charge.
 *
 * The battery on USB (abacus #7587): a cell counts as present when the last OFF_CELL_N or more VCELL samples, over at
 * least OFF_CELL_MS, stay within OFF_CELL_SPREAD_UV and inside 3.0-4.25 V (with no cell the gauge still answers while
 * the charger's detection cycles VBAT, board 0005 on 10-12). Then CHG_N low is "charging" with the %, CHG_N high the %
 * with no word, and "charged" only after CHG_N went low then high in this USB session. No cell: "on USB", no %. */
#ifndef OFFSCREEN_H
#define OFFSCREEN_H

#include <stdbool.h>
#include <stdint.h>

#define OFF_NEVER UINT32_MAX            /* no timer wake */
#define OFF_REFRESH_S 6000u             /* 100 minutes: the panel's rewrite, under Sharp's two hours */

enum { OFF_TIER_CLOCK = 0, OFF_TIER_DATE, OFF_TIER_HIB };

/* The tiers' parameters (SYS's, kept in the settings record). Michael's defaults, #7588. */
typedef struct {
    uint32_t clock_s;       /* the minute clock's span after OFF: 86,400 (one day) */
    uint16_t hib_days;      /* hibernate this many days after OFF; 0 never: 5 until P0 measures it */
    uint8_t date_soc;       /* the date tier at most, at or below this charge: 25 */
    uint8_t hib_soc;        /* hibernate at or below: 15 */
    bool clock, date, batt; /* "off disp?": what the screen shows (068 (3) (1)) */
} off_params;

void off_params_default(off_params *p);

/* The tier and the seconds to the next timer wake. since_s: the time since OFF; wall_s the wall clock (secs since
   1970, the local time held as UTC, as app.h's), wall_set false when it was never set; soc the charge (-1 not read);
   usb: on USB power. With the clock unset there is no clock or date to show: the tier is the date tier at most and the
   wake is the refresh. A tier with nothing to show (every off disp? item off) wakes only for the refresh. */
typedef struct {
    int tier;
    uint32_t wake_s;        /* OFF_NEVER: none */
} off_plan_t;

off_plan_t off_plan(const off_params *p, uint32_t since_s, bool wall_set, int64_t wall_s, int soc, bool usb);

/* ---- the battery on USB ---- */

#define OFF_CELL_N 8
#define OFF_CELL_MS 2000u
#define OFF_CELL_SPREAD_UV 30000u
#define OFF_CELL_MIN_UV 3000000u
#define OFF_CELL_MAX_UV 4250000u
#define OFF_CELL_RING 32               /* 2 s of samples 66 ms apart */

enum { OFF_USB_NONE = 0,        /* not on USB */
       OFF_USB_NOCELL,          /* "on USB", no % */
       OFF_USB_CHARGING,        /* "charging" and the % */
       OFF_USB_IDLE,            /* the % with no word */
       OFF_USB_CHARGED };       /* "charged" and the % */

typedef struct {
    uint32_t t_ms[OFF_CELL_RING], uv[OFF_CELL_RING];
    uint8_t n, at;          /* samples held, the next slot */
    bool usb;               /* the session's USB, at the last off_usb_state */
    bool chg_seen;          /* CHG_N low seen in this USB session */
    uint32_t plug_ms;       /* when this USB session began */
} off_batt;

void off_batt_init(off_batt *b);
/* A VCELL sample (the gauge's microvolts) at now_ms. */
void off_batt_sample(off_batt *b, uint32_t now_ms, uint32_t vcell_uv);
bool off_batt_cell(const off_batt *b);
/* The state shown, from USB and CHG_N (active low) now; a change of USB starts or ends the session. */
int off_usb_state(off_batt *b, bool usb, bool chg_n, uint32_t now_ms);

/* ---- time to full ---- */

#define OFF_TTF_SETTLE_MS 120000u       /* CRATE settles after a plug-in: "--" until then (to fit to the board's log) */
#define OFF_TTF_MAX_MIN (24u * 60u)     /* longer: "--" */

/* Minutes to a full charge, rounded to 5, from the charge and CRATE (0.001 %/hr, gauge.h); -1 for "--": CRATE not
   positive, not settled since the plug-in, or longer than OFF_TTF_MAX_MIN. 0 at 100 %. */
int off_ttf_min(int soc, int32_t crate, uint32_t since_plug_ms);

#endif
