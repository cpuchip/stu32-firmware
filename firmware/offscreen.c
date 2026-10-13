/* offscreen: see offscreen.h. (068 (3): the stub, red first) */
#include "offscreen.h"

#include <string.h>

void off_params_default(off_params *p)
{
    memset(p, 0, sizeof *p);
}

off_plan_t off_plan(const off_params *p, uint32_t since_s, bool wall_set, int64_t wall_s, int soc, bool usb)
{
    (void)p, (void)since_s, (void)wall_set, (void)wall_s, (void)soc, (void)usb;
    off_plan_t r = {OFF_TIER_HIB, OFF_NEVER};
    return r;
}

void off_batt_init(off_batt *b)
{
    memset(b, 0, sizeof *b);
}

void off_batt_sample(off_batt *b, uint32_t now_ms, uint32_t vcell_uv)
{
    (void)b, (void)now_ms, (void)vcell_uv;
}

bool off_batt_cell(const off_batt *b)
{
    (void)b;
    return false;
}

int off_usb_state(off_batt *b, bool usb, bool chg_n, uint32_t now_ms)
{
    (void)b, (void)usb, (void)chg_n, (void)now_ms;
    return OFF_USB_NONE;
}

int off_ttf_min(int soc, int32_t crate, uint32_t since_plug_ms)
{
    (void)soc, (void)crate, (void)since_plug_ms;
    return -1;
}
