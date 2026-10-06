/* settings: the device's settings and their record after the state image (see settings.h). */
#include "settings.h"

#include <string.h>

#include "piezo.h"
#include "state.h"

#define REC_LEN 18u             /* "ABDS", version, 0, six fields, CRC (version 2, unit 027) */
#define REC1_LEN 17u            /* version 1: five fields, BEEP one switch (unit 022) */

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
        r[12] = s->uses;
        r[13] = s->h24;
        put32(r + 14, ab_crc32(r, 14));
    }
    return n + REC_LEN;
}

/* A record's fields when it is whole, of version 1 or 2 and in range. Version 1's BEEP becomes
 * every use on or none, with the 12-hour clock (unit 027 rule 3). */
static bool read_record(dev_settings *s, const uint8_t *r, size_t n)
{
    if (n < REC1_LEN || memcmp(r, "ABDS", 4) != 0 || r[6] || r[7]) return false;
    unsigned v = r[4] | r[5] << 8;
    size_t len = v == 1 ? REC1_LEN : REC_LEN;
    if ((v != 1 && v != DEV_SETTINGS_VERSION) || n < len || get32(r + len - 4) != ab_crc32(r, len - 4)) return false;
    dev_settings t = {(char)r[8], (char)r[9], r[10], r[11], 0, false};
    if (v == 1) {
        if (r[12] > 1) return false;
        t.uses = r[12] ? DEV_USE_ALL : 0;
    } else {
        if ((r[12] & ~DEV_USE_ALL) || r[13] > 1) return false;
        t.uses = r[12];
        t.h24 = r[13] != 0;
    }
    if ((t.radix != '.' && t.radix != ',') || (t.group != 0 && t.group != 1 && t.group != ' ') ||
        (t.off_min != 2 && t.off_min != 5 && t.off_min != 10 && t.off_min != 30) || t.vol >= PIEZO_LEVELS)
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
