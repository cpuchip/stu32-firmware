/* apps: the registry of built-in apps (unit 037), and KEYS, the framework's own app: each key's
 * number and legend as it goes down and up, and a count of presses kept in its save. */
#include <stdio.h>
#include <string.h>

#include "apps.h"
#include "games.h" /* generated from games.list by scripts/gen_games.py: GAMES_DECLARE, GAMES_ENTRIES */
#include "keymap.h"
#include "screen.h"

typedef struct {
    int key, presses;
    bool down, any;
} keys_ctx;

static void keys_load(void *ctx, const uint8_t *save, uint32_t n)
{
    keys_ctx *k = ctx;
    if (n >= 4) k->presses = (int)(save[0] | save[1] << 8 | save[2] << 16 | (uint32_t)save[3] << 24);
}

static void keys_start(void *ctx)
{
    (void)ctx;
}

static bool keys_key(void *ctx, int key, bool down, uint32_t ms)
{
    keys_ctx *k = ctx;
    (void)ms;
    k->key = key;
    k->down = down;
    k->any = true;
    if (down) k->presses++;
    return true;
}

static uint32_t keys_tick(void *ctx, uint32_t ms)
{
    (void)ctx;
    (void)ms;
    return 0;                                   /* KEYS needs no ticks */
}

static void keys_draw(void *ctx, ls027_frame *f)
{
    keys_ctx *k = ctx;
    char t[64];
    screen_clear(f);
    screen_text(f, 4, 4, 3, "KEYS", true);
    screen_text(f, 4, 220, 2, "C ENDS", true);
    if (!k->any) {
        screen_text(f, 4, 80, 2, "PRESS A KEY", true);
    } else {
        const km_action *a = km_lookup(k->key, 0);
        snprintf(t, sizeof t, "KEY %d %s", k->key, a && a->legend[0] ? a->legend : "-");
        screen_text(f, 4, 80, 3, t, true);
        screen_text(f, 4, 120, 3, k->down ? "DOWN" : "UP", true);
    }
    snprintf(t, sizeof t, "PRESSES %d", k->presses);
    screen_text(f, 4, 160, 2, t, true);
}

static void keys_save(void *ctx, uint8_t *save, uint32_t cap)
{
    keys_ctx *k = ctx;
    if (cap < 4) return;
    for (int i = 0; i < 4; i++) save[i] = (uint8_t)((uint32_t)k->presses >> (8 * i));
}

static void keys_stop(void *ctx)
{
    (void)ctx;
}

static const app_def keys_app = {"KEYS", sizeof(keys_ctx), keys_load, keys_start, keys_key, keys_tick,
                                 keys_draw, keys_save, keys_stop, 0};

GAMES_DECLARE
const app_def *const app_registry[] = {&keys_app GAMES_ENTRIES};
const int app_registry_n = sizeof app_registry / sizeof app_registry[0];
