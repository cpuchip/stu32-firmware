/* settings: the device's settings and their record after the state image (see settings.h). */
#include "settings.h"

#include "piezo.h"
#include "state.h"

void dev_settings_default(dev_settings *s)
{
    s->radix = '.';
    s->group = 0;
    s->off_min = 10;
    s->vol = PIEZO_LEVELS - 1;
    s->beep = true;
}

size_t dev_image_save(const ab_calc *c, const dev_settings *s, uint8_t *buf, size_t cap)
{
    (void)s;
    return ab_state_save(c, buf, cap);
}

abn_status dev_image_load(ab_calc *c, dev_settings *s, const uint8_t *buf, size_t n)
{
    dev_settings_default(s);
    return ab_state_load(c, buf, n);
}
