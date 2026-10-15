/* apps: programs that own the screen and the keys while they run, under APPS (unit 037). Each is a
 * struct of calls, linked in at build time and listed in apps.c's registry. While an app runs the
 * calculator's core is never called and its state image is untouched; C (key 42) always ends the app
 * and is never delivered to it. Each app keeps APP_SAVE_SIZE bytes of its own, stored apart from the
 * calculator's image (a MEMORY CLEAR leaves them).
 *
 * The calls, in order: load (the app's saved bytes, n 0 for none) -> start -> (key | tick)* -> save ->
 * stop. The first tick comes right after start. draw follows every tick, and a key whose call returns
 * true. Times are milliseconds since the app started, one clock for ticks and keys (arcade #4497).
 *
 * Version 1.1 (2026-10-07, Babal's merge): APP_CTX_MAX raised from 32 KB to 256 KB; nothing else changed.
 * Version 1.2 (2026-10-07, abacus #4734, arcade's docs/apps-h-v1.2.md): additive. The framework sends the panel only
 * the lines that changed (no call); an optional save_size at the end of app_def (up to APP_SAVE_MAX); app_seed;
 * app_tune; APP_KEY_* (app_keys.h, generated from the layout); APP_STACK_BYTES stated. An app written for 1.1
 * compiles and behaves unchanged. */
#ifndef APPS_H
#define APPS_H

#include <stdbool.h>
#include <stdint.h>

#include "app_keys.h"                /* v1.2: APP_KEY_* by name (generated from the layout) */
#include "ls027.h"

#define APP_SAVE_SIZE 64        /* each app's own save, apart from the calculator's image (a save_size of 0) */
#define APP_SAVE_MAX 4096       /* v1.2: the most an app's save_size may ask */
#define APP_TUNE_MAX 16         /* v1.2: the most notes app_tune plays (more are dropped) */
#define APP_STACK_BYTES 16384   /* v1.2: the stack an app's calls may use (the device's task gives more: the stand-in's
                                   calc task has 32 KB and Babal ran in its first 4 KB); every game is built with
                                   -Wframe-larger-than=1024 -Werror (the games list) */
#define APP_CTX_MAX (256 * 1024) /* the most state an app may declare (the framework gives it, zeroed). v1.1 (Babal's
                                    merge): 32 KB in v1; both devices hold it in PSRAM (Rev A's N16R8: 8 MB) */

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
    uint32_t save_size;         /* v1.2: the bytes of save the app keeps, 0 for APP_SAVE_SIZE; at most APP_SAVE_MAX
                                   (an app asking more isn't started). load gets up to that many, save's cap is it. A
                                   save kept before (a 1.1 app's 64) is given back as it was */
} app_def;

/* A tone for an app, at once, not blocking; the device's VOL and BEEP settings apply (silent when its
   uses are off). Provided by the framework (app.c); a host without a buzzer ignores it. */
void app_beep(uint16_t hz, uint16_t ms);

/* The running app's clock: milliseconds since its start, the clock of key and tick (abacus #4500). */
uint32_t app_now_ms(void);

/* v1.2: a seed for an app's own generator: the device's hardware RNG. On a host the framework's choice, which a test
   fixes with app_seed_set (host builds only) so a game's oracle can replay it. */
uint32_t app_seed(void);
#ifndef ESP_PLATFORM
void app_seed_set(uint32_t seed);
#endif

/* v1.2: a short tune, not blocking: n (hz, ms) pairs in one array, notes[2i] the pitch (0 a rest) and notes[2i+1] its
   length; at most APP_TUNE_MAX notes, copied. A new call replaces what is playing; the app's exit (C) stops it. The
   device's VOL and BEEP settings apply, as for app_beep (the one-note case). A host without a buzzer ignores it. */
void app_tune(const uint16_t *notes, uint8_t n);

extern const app_def *const app_registry[];     /* the built-in apps (apps.c) */
extern const int app_registry_n;
/* unit 072: the folder of each registered app, as the registry; gen_games.py's numbers (TOOLS 0, GAMES 1). APPS lists
   the tools, then a GAMES key; GAMES lists the games */
#define APP_FOLDER_TOOLS 0
#define APP_FOLDER_GAMES 1
extern const uint8_t app_registry_folder[];

#endif
