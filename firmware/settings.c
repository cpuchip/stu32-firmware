/* settings: the device's settings and their record after the state image (see settings.h). */
#include "settings.h"

#include <string.h>

#include "piezo.h"
#include "state.h"

#define REC_LEN 17u             /* "ABDS", version, 0, five fields, CRC */

void dev_settings_default(dev_settings *s)
{
    s->radix = '.';
    s->group = 1;               /* 1,000: both machines group by default (UG p.1-23, p.B-3; #3965) */
    s->off_min = 10;
    s->vol = PIEZO_LEVELS - 1;
    s->uses = DEV_USE_ALL;
    s->h24 = false;
}

static void put32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

size_t dev_image_save(const ab_calc *c, const dev_settings *s, uint8_t *buf, size_t cap)
{
    size_t n = ab_state_save(c, buf, cap);
    if (n == 0) return 0;
    if (n + REC_LEN <= cap) {
        uint8_t *r = buf + n;
        memcpy(r, "ABDS", 4);
        r[4] = DEV_SETTINGS_VERSION;
        r[5] = r[6] = r[7] = 0;
        r[8] = (uint8_t)s->radix;
        r[9] = (uint8_t)s->group;
        r[10] = s->off_min;
        r[11] = s->vol;
        r[12] = s->uses != 0;
        put32(r + 13, ab_crc32(r, 13));
    }
    return n + REC_LEN;
}

/* A record's fields when it is whole, of this version and in range. */
static bool read_record(dev_settings *s, const uint8_t *r, size_t n)
{
    if (n < REC_LEN || memcmp(r, "ABDS", 4) != 0 || (r[4] | r[5] << 8) != DEV_SETTINGS_VERSION ||
        r[6] || r[7] || get32(r + 13) != ab_crc32(r, 13))
        return false;
    dev_settings t = {(char)r[8], (char)r[9], r[10], r[11], (uint8_t)(r[12] ? DEV_USE_ALL : 0), false};
    if ((t.radix != '.' && t.radix != ',') || (t.group != 0 && t.group != 1 && t.group != ' ') ||
        (t.off_min != 2 && t.off_min != 5 && t.off_min != 10 && t.off_min != 30) || t.vol >= PIEZO_LEVELS ||
        r[12] > 1)
        return false;
    *s = t;
    return true;
}

abn_status dev_image_load(ab_calc *c, dev_settings *s, const uint8_t *buf, size_t n)
{
    /* the core's image says its length at bytes 8-11 (state.h's header) */
    size_t core = n >= 12 && memcmp(buf, "ABSI", 4) == 0 ? get32(buf + 8) : n;
    dev_settings_default(s);
    if (core > n) core = n;                     /* a short image: the core refuses it */
    abn_status st = ab_state_load(c, buf, core);
    if (st == ABN_OK) read_record(s, buf + core, n - core);
    return st;
}
