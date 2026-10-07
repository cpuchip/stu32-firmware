/* dev_common: what both boards' mains do the same way for unit 027 (header only, ESP-IDF): the wall
 * clock in the S3's RTC timer (gettimeofday: it runs through light and deep sleep and is lost only
 * with the cell), the console's TIME, and SYS's facts that come from the SDK. */
#ifndef DEV_COMMON_H
#define DEV_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#include "app.h"
#include "esp_heap_caps.h"
#include "version.h"

/* app.set_wall: the clock set from the keys, kept in the RTC */
static void dev_set_wall(void *ctx, int64_t secs)
{
    (void)ctx;
    struct timeval tv = {.tv_sec = (time_t)secs, .tv_usec = 0};
    settimeofday(&tv, NULL);
}

static int64_t dev_wall_now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec;
}

/* "hh:mm" or "hh:mm:ss" after TIME: the wall time on the clock's day (the date the app already has,
   or the keys' date when it is unset), false when it is no time */
static bool dev_parse_time(const app_state *a, uint32_t now, const char *s, int64_t *out)
{
    int h = -1, m = -1, sec = 0;
    if (sscanf(s, "%d:%d:%d", &h, &m, &sec) < 2 || h < 0 || h > 23 || m < 0 || m > 59 || sec < 0 || sec > 59)
        return false;
    int64_t day = APP_CLOCK_KEYS_DATE, w;
    if (app_wall(a, now, &w)) day = w - w % 86400;
    *out = day + h * 3600 + m * 60 + sec;
    return true;
}

/* SYS's facts the SDK knows; the board fills the gauge's and the charger's */
static void dev_sysinfo_common(app_sysinfo *o, uint32_t flash_free, uint32_t slot_size)
{
    o->slot_size = slot_size;                   /* MEM's bytes free: this less the image now (unit 041 C) */
    o->version = AB_FW_VERSION;
    o->commit = AB_CORE_COMMIT;
    o->built = AB_BUILT;
    o->flash_free = flash_free;
    o->heap_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    o->heap_largest = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
}

/* each use's sound: Hz and ms (ours; VOL's sample is use 0) */
static void dev_tone_of(int use, uint32_t base_hz, uint32_t *hz, uint32_t *ms)
{
    *hz = base_hz;
    *ms = use == DEV_USE_CLICK ? 4 : use == DEV_USE_PRGM ? 100 : use == DEV_USE_BATT ? 300 : use == DEV_USE_ON ? 30
          : use == DEV_USE_DONE ? 150 : 60;     /* the error tone and VOL's sample: 60 ms, as unit 022 */
    if (use == DEV_USE_BATT) *hz = base_hz / 2; /* lower, apart from the rest */
}

#endif
