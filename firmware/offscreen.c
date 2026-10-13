/* offscreen: see offscreen.h. */
#include "offscreen.h"

#include <string.h>

#define DAY_S 86400u

void off_params_default(off_params *p)
{
    memset(p, 0, sizeof *p);
    p->clock_s = DAY_S;
    p->hib_days = 5;
    p->date_soc = 25;
    p->hib_soc = 15;
    p->clock = false;                       /* opt-in until P0 measures its cost (#7585) */
    p->date = true;
    p->batt = true;
}

static uint64_t up_to(int64_t wall, uint32_t unit)  /* the seconds to the next multiple of unit (a whole unit on one) */
{
    int64_t r = wall % (int64_t)unit;
    if (r < 0) r += unit;
    return unit - (uint64_t)r;
}

off_plan_t off_plan(const off_params *p, uint32_t since_s, bool wall_set, int64_t wall_s, int soc, bool usb)
{
    if (usb) since_s = 0, soc = -1;         /* the chip is up on USB: no tier runs down, no cutoff */
    uint64_t since = since_s, hib = p->hib_days ? (uint64_t)p->hib_days * DAY_S : UINT64_MAX;
    off_plan_t r = {OFF_TIER_HIB, OFF_NEVER};
    if ((soc >= 0 && soc <= p->hib_soc) || since >= hib) return r;
    uint64_t wake = OFF_REFRESH_S;
    if (p->clock && wall_set && since < p->clock_s && !(soc >= 0 && soc <= p->date_soc)) {
        r.tier = OFF_TIER_CLOCK;
        if (up_to(wall_s, 60) < wake) wake = up_to(wall_s, 60);
        if (p->clock_s - since < wake) wake = p->clock_s - since;
    } else {
        r.tier = OFF_TIER_DATE;
        if (p->date && wall_set && up_to(wall_s, DAY_S) < wake) wake = up_to(wall_s, DAY_S);
    }
    if (hib - since < wake) wake = hib - since;
    r.wake_s = (uint32_t)wake;
    return r;
}

void off_batt_init(off_batt *b)
{
    memset(b, 0, sizeof *b);
}

void off_batt_sample(off_batt *b, uint32_t now_ms, uint32_t vcell_uv)
{
    b->t_ms[b->at] = now_ms;
    b->uv[b->at] = vcell_uv;
    b->at = (uint8_t)((b->at + 1) % OFF_CELL_RING);
    if (b->n < OFF_CELL_RING) b->n++;
}

/* From the newest sample back: every one in range and within the spread, until OFF_CELL_N of them span OFF_CELL_MS. */
bool off_batt_cell(const off_batt *b)
{
    uint32_t lo = UINT32_MAX, hi = 0, newest = b->t_ms[(b->at + OFF_CELL_RING - 1) % OFF_CELL_RING];
    for (int k = 0; k < b->n; k++) {
        int i = (b->at + OFF_CELL_RING - 1 - k) % OFF_CELL_RING;
        uint32_t v = b->uv[i];
        if (v < OFF_CELL_MIN_UV || v > OFF_CELL_MAX_UV) return false;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
        if (hi - lo > OFF_CELL_SPREAD_UV) return false;
        if (k + 1 >= OFF_CELL_N && newest - b->t_ms[i] >= OFF_CELL_MS) return true;
    }
    return false;
}

int off_usb_state(off_batt *b, bool usb, bool chg_n, uint32_t now_ms)
{
    if (usb != b->usb) {                    /* a session begins or ends */
        b->usb = usb;
        b->chg_seen = false;
        if (usb) b->plug_ms = now_ms;
    }
    if (!usb) return OFF_USB_NONE;
    if (!off_batt_cell(b)) return OFF_USB_NOCELL;   /* (CHG_N then counts for nothing: detection toggles it) */
    if (!chg_n) {
        b->chg_seen = true;
        return OFF_USB_CHARGING;
    }
    return b->chg_seen ? OFF_USB_CHARGED : OFF_USB_IDLE;
}

int off_ttf_min(int soc, int32_t crate, uint32_t since_plug_ms)
{
    if (soc < 0) return -1;
    if (soc >= 100) return 0;
    if (crate <= 0 || since_plug_ms < OFF_TTF_SETTLE_MS) return -1;
    uint64_t num = (uint64_t)(100 - soc) * 60000u, c = (uint64_t)crate;    /* minutes = num / c */
    if (num > (uint64_t)OFF_TTF_MAX_MIN * c) return -1;
    uint64_t m = (2 * num + 5 * c) / (10 * c) * 5;                          /* to the nearest 5, a half up */
    return m ? (int)m : 5;                  /* not full: never 0 */
}
