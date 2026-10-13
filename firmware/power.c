/* power: the off policy (see power.h). */
#include "power.h"

void pwr_init(pwr_state *p, uint32_t now_ms)
{
    p->last_ms = now_ms;
    p->idle_ms = PWR_IDLE_MS;
}

void pwr_activity(pwr_state *p, uint32_t now_ms)
{
    p->last_ms = now_ms;
}

pwr_action pwr_check(pwr_state *p, const pwr_inputs *in, uint32_t now_ms)
{
    if (in->soc >= 0 && in->soc < PWR_LOW_SOC && !pwr_external(in)) return PWR_OFF_LOW;
    if (in->busy) {                             /* a run is activity: the clock starts at its end */
        p->last_ms = now_ms;
        return PWR_STAY;
    }
    if (pwr_external(in)) return PWR_STAY;     /* on USB power it stays up for the console (068, Michael) */
    return (uint32_t)(now_ms - p->last_ms) >= p->idle_ms ? PWR_OFF_IDLE : PWR_STAY;   /* wraps */
}

uint32_t pwr_next(const pwr_state *p, const pwr_inputs *in, uint32_t now_ms)
{
    if (in->soc >= 0 && in->soc < PWR_LOW_SOC && !pwr_external(in)) return 0;   /* as pwr_check */
    if (pwr_external(in)) return UINT32_MAX;   /* on USB pwr_check never turns it off (068): no deadline */
    uint32_t gone = now_ms - p->last_ms;                                           /* wraps */
    return gone >= p->idle_ms ? 0 : p->idle_ms - gone;
}

bool pwr_charging(const pwr_inputs *in)
{
    return !in->chg_n;
}

bool pwr_external(const pwr_inputs *in)
{
    return !in->pgood_n;
}

bool pwr_radio_allowed(uint32_t vbat_uv)
{
    return vbat_uv >= PWR_RADIO_MIN_UV;         /* 0 (the gauge not read) is below it */
}

void pwr_release_init(pwr_release_state *r, uint32_t now_ms)
{
    r->start_ms = now_ms;
    r->high_ms = now_ms;
    r->high = false;
}

pwr_release pwr_release_step(pwr_release_state *r, bool on_high, uint32_t now_ms)
{
    if (on_high) {
        if (!r->high) r->high_ms = now_ms;          /* the high starts now (a bounce starts it again) */
        r->high = true;
        if (now_ms - r->high_ms >= PWR_RELEASE_DEBOUNCE_MS) return PWR_RELEASE_OK;
    } else {
        r->high = false;
        if (now_ms - r->start_ms >= PWR_RELEASE_WAIT_MS) return PWR_RELEASE_STUCK;
    }
    return PWR_RELEASE_WAIT;
}
