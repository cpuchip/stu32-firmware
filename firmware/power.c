/* power: the off policy (see power.h). */
#include "power.h"

void pwr_init(pwr_state *p, uint32_t now_ms)
{
    p->last_ms = now_ms;
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
    return (uint32_t)(now_ms - p->last_ms) >= PWR_IDLE_MS ? PWR_OFF_IDLE : PWR_STAY;   /* wraps */
}

bool pwr_charging(const pwr_inputs *in)
{
    return !in->chg_n;
}

bool pwr_external(const pwr_inputs *in)
{
    return !in->pgood_n;
}

/* red: decision 36's stub, replaced in the green commit */
bool pwr_radio_allowed(uint32_t vbat_uv)
{
    (void)vbat_uv;
    return false;
}
