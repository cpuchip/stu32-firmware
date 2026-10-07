/* apps: programs that own the screen and the keys while they run, under APPS (unit 037). Each is a
 * struct of calls, linked in at build time and listed in apps.c's registry. While an app runs the
 * calculator's core is never called and its state image is untouched; C (key 42) always ends the app
 * and is never delivered to it. Each app keeps APP_SAVE_SIZE bytes of its own, stored apart from the
 * calculator's image (a MEMORY CLEAR leaves them).
 *
 * The calls, in order: load (the app's saved bytes, n 0 for none) -> start -> (key | tick)* -> save ->
 * stop. The first tick comes right after start. draw follows every tick, and a key whose call returns
 * true. Times are milliseconds since the app started, one clock for ticks and keys (arcade #4497). */
#ifndef APPS_H
#define APPS_H

#include <stdbool.h>
#include <stdint.h>

#include "ls027.h"

#define APP_SAVE_SIZE 64        /* each app's own save, apart from the calculator's image */
#define APP_CTX_MAX (32 * 1024) /* the most state an app may declare (the framework gives it, zeroed) */

typedef struct {
    const char *name;           /* APPS' label: at most 5 cells */
    uint32_t ctx_size;          /* bytes of state the framework allocates, zeroed, per run (<= APP_CTX_MAX) */
    void (*load)(void *ctx, const uint8_t *save, uint32_t n);   /* before start: the saved bytes, n 0 none */
    void (*start)(void *ctx);
    bool (*key)(void *ctx, int key, bool down, uint32_t ms);   /* keys.h numbering (row*6+col), down on
                                                   press, at ms on the app's clock; true: draw now */
    uint32_t (*tick)(void *ctx, uint32_t ms);   /* ms since the last tick; returns ms to the next (0: none) */
    void (*draw)(void *ctx, ls027_frame *f);    /* the whole 400x240 frame, after any key or tick */
    void (*save)(void *ctx, uint8_t *save, uint32_t cap);       /* what to keep (cap APP_SAVE_SIZE) */
    void (*stop)(void *ctx);
} app_def;

/* A tone for an app, at once, not blocking; the device's VOL and BEEP settings apply (silent when its
   uses are off). Provided by the framework (app.c); a host without a buzzer ignores it. */
void app_beep(uint16_t hz, uint16_t ms);

/* The running app's clock: milliseconds since its start, the clock of key and tick (abacus #4500). */
uint32_t app_now_ms(void);

extern const app_def *const app_registry[];     /* the built-in apps (apps.c) */
extern const int app_registry_n;

#endif
