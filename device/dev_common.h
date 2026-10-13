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
#include "calc.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "screen.h"
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
static __attribute__((unused)) void dev_tone_of(int use, uint32_t base_hz, uint32_t *hz, uint32_t *ms)
{
    *hz = base_hz;
    *ms = use == DEV_USE_CLICK ? 4 : use == DEV_USE_PRGM ? 100 : use == DEV_USE_BATT ? 300 : use == DEV_USE_ON ? 30
          : use == DEV_USE_DONE ? 150 : 60;     /* the error tone and VOL's sample: 60 ms, as unit 022 */
    if (use == DEV_USE_BATT) *hz = base_hz / 2; /* lower, apart from the rest */
}

/* The console's X (unit 053: one format on both boards, so the stand-in's scripts run on Rev A unchanged): the state
   line (X, the stop, the program, the app's shift and prompt, the message, the internal heap free, the calc task's stack
   high-water mark), the status band's clock and SYS's first line, the app running, the equation shown with 048's note
   and report, and SHOW. A board adds its own lines after it. noinline: called on the calc task, its locals inlined into
   calc_task would sit under every deep path (unit 052). */
static __attribute__((noinline, unused)) void dev_dump(const ab_calc *calc, app_state *app)
{
    char x[96], stop[32];
    ab_display d = {{AB_ALL, 0, 40, '.', false}, false, 4095, 0, 10, false, 0};
    int ind;
    if (ab_format_display(&calc->x.re, &d, x, sizeof x, &ind) < 0) snprintf(x, sizeof x, "?");
    ab_stop_text(calc, stop);
    const char *m = ab_error_message(calc);
    printf("state: X=%s stop=%s lines=%d prgm=%d running=%d shift=%d prompt=%s msg=%s heap=%u stack-free=%u\n", x, stop,
           calc->prog_len, calc->prgm, app->running, app->shift, app->prompt_op >= 0 ? app->prompt_text : "-", m ? m : "-",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)uxTaskGetStackHighWaterMark(NULL));
    screen_ui ui;                               /* (on the stack while X runs, not 176 B of internal RAM for good) */
    app_ui(app, &ui);
    printf("ui: clock=%s%c view=%s soft1=%s off=%d\n", ui.clock, ui.ampm ? ui.ampm : ' ', ui.view[0] ? ui.view[0] : "-",
           ui.soft[0] && ui.soft[0][0] ? ui.soft[0] : "-", app->soft_off);   /* off=1: the soft off on USB (068) */
    printf("app: %s\n", app_running(app) ? app->app->name : "-");          /* unit 037 */
    printf("eqn: %s | note=%s | rep=%s\n", calc->eqn_mode && calc->eqn_cur >= 0 ? calc->eqn[calc->eqn_cur] : "-",
           ab_cas_note(calc) ? ab_cas_note(calc) : "-", ab_cas_report(calc) ? ab_cas_report(calc) : "-");   /* 048 */
    printf("ui: show=%d %s%s%s\n", ui.show_n, ui.show_n ? ui.show[0] : "-", ui.show_n > 1 ? "/" : "",
           ui.show_n > 1 ? ui.show[1] : "");                             /* unit 030: SHOW held */
}

#endif
