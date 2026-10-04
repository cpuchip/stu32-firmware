/* store_test: the two-slot flash store under a power cut at every moment (unit 018 scope 6).
 *
 *   build/store_test
 *
 * A simulated NOR flash: erase sets a sector to 0xFF; a write can only clear bits (new = old &
 * data, as NOR flash does); a cut after k bytes of a write keeps the first k bytes written and
 * fails everything after; a cut during an erase leaves the sector it was on as garbage (NOR
 * flash makes no promise about an interrupted erase). For a series of saves of different images,
 * the power is cut at every byte offset of every write and at every sector of every erase; after
 * each cut the device "reboots" and loads: the image must be either the last fully saved one or
 * the one being saved, never neither, and the next save must work. N/N is printed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "settings.h"
#include "state.h"
#include "store.h"

#define SECTOR 4096u
#define SLOT (8u * SECTOR)

typedef struct {
    uint8_t mem[2 * SLOT];
    long budget;            /* bytes (or sectors) still allowed before the cut; -1: no cut */
    bool dead;
    unsigned seed;
} sim;

static int sim_erase(void *ctx, uint32_t off, uint32_t len)
{
    sim *s = ctx;
    for (uint32_t a = off; a < off + len; a += SECTOR) {
        if (s->dead) return -1;
        if (s->budget == 0) {
            for (uint32_t i = 0; i < SECTOR; i++) s->mem[a + i] = (uint8_t)(s->seed = s->seed * 1103515245u + 12345u) >> 3;
            s->dead = true;
            return -1;
        }
        if (s->budget > 0) s->budget--;
        memset(s->mem + a, 0xFF, SECTOR);
    }
    return 0;
}

static int sim_write(void *ctx, uint32_t off, const void *src, uint32_t len)
{
    sim *s = ctx;
    const uint8_t *p = src;
    for (uint32_t i = 0; i < len; i++) {
        if (s->dead) return -1;
        if (s->budget == 0) {
            s->dead = true;
            return -1;
        }
        if (s->budget > 0) s->budget--;
        s->mem[off + i] &= p[i];
    }
    return 0;
}

static int sim_read(void *ctx, uint32_t off, void *dst, uint32_t len)
{
    sim *s = ctx;
    memcpy(dst, s->mem + off, len);
    return 0;
}

static uint8_t *image_of(int k, uint32_t *len)
{
    /* a state that differs from save to save: k in X and k keys of program */
    static ab_calc c;
    ab_init(&c);
    ab_do(&c, AB_K0 + (ab_op)(k % 10));
    ab_do(&c, AB_ENTER);
    ab_do(&c, AB_PRGM);
    for (int i = 0; i < k * 5; i++) ab_do(&c, AB_SQRT);
    ab_do(&c, AB_PRGM);
    *len = (uint32_t)ab_state_save(&c, NULL, 0);
    uint8_t *b = malloc(*len);
    ab_state_save(&c, b, *len);
    return b;
}

static bool same(const uint8_t *a, uint32_t na, const uint8_t *b, uint32_t nb)
{
    return na == nb && memcmp(a, b, na) == 0;
}

/* unit 022: the device's settings saved with the image, through the store and back */
static int settings_checks(void)
{
    static ab_calc c, c2;
    static uint8_t img[SLOT], got[SLOT];
    static sim s2;
    ab_flash f2 = {&s2, SECTOR, SLOT, sim_erase, sim_write, sim_read};
    dev_settings set, back, def;
    int bad = 0, checks = 0;
#define CHECK(ok, what) do { checks++; if (!(ok)) { bad++; fprintf(stderr, "store_test: %s\n", what); } } while (0)
    memset(s2.mem, 0xFF, sizeof s2.mem);
    s2.budget = -1;
    dev_settings_default(&def);
    CHECK(def.radix == '.' && def.group == 0 && def.off_min == 10 && def.vol == 4 && def.beep,
          "the defaults: radix '.', no grouping, 10 minutes, the loudest level, BEEP on");
    ab_init(&c);
    ab_do(&c, AB_K7);
    ab_do(&c, AB_ENTER);
    set = (dev_settings){',', ' ', 30, 2, false};
    size_t n = dev_image_save(&c, &set, img, sizeof img);
    CHECK(n > ab_state_save(&c, NULL, 0), "the record follows the core's image");
    CHECK(n > 0 && n <= sizeof img && ab_store_save(&f2, img, (uint32_t)n) == 0, "saved through the store");
    uint32_t m = ab_store_load(&f2, got, sizeof got);
    ab_init(&c2);
    back = def;
    CHECK(m == n && dev_image_load(&c2, &back, got, m) == ABN_OK, "loaded back");
    CHECK(back.radix == ',' && back.group == ' ' && back.off_min == 30 && back.vol == 2 && !back.beep,
          "non-default settings load back");
    CHECK(abn_cmp(&c2.x.re, &c.x.re) == 0, "and the state with them");
    /* an image from before unit 022 (the core's alone) loads the defaults */
    n = ab_state_save(&c, img, sizeof img);
    back = set;
    CHECK(dev_image_load(&c2, &back, img, n) == ABN_OK && !memcmp(&back, &def, sizeof back),
          "a pre-022 image loads, with the default settings");
    /* a damaged record gives the defaults; the state still loads */
    n = dev_image_save(&c, &set, img, sizeof img);
    img[n - 8] ^= 0x20;                         /* grouping ' ' to 0: still in range, so only the CRC tells */
    back = set;
    CHECK(dev_image_load(&c2, &back, img, n) == ABN_OK && !memcmp(&back, &def, sizeof back),
          "a damaged record: the defaults, the state loaded");
    /* a refused core image: BAD_IMAGE and the defaults */
    n = dev_image_save(&c, &set, img, sizeof img);
    img[5] ^= 0x7F;
    back = set;
    CHECK(dev_image_load(&c2, &back, img, n) == ABN_BAD_IMAGE && !memcmp(&back, &def, sizeof back),
          "a refused core image: BAD_IMAGE, the defaults");
#undef CHECK
    printf("store_test: settings %d/%d checks\n", checks - bad, checks);
    return bad != 0;
}

int main(void)
{
    static sim s;
    ab_flash f = {&s, SECTOR, SLOT, sim_erase, sim_write, sim_read};
    static uint8_t buf[SLOT];
    long cases = 0, good = 0;
    memset(s.mem, 0xFF, sizeof s.mem);
    s.budget = -1;
    uint32_t prev_len = 0;
    uint8_t *prev = NULL;
    for (int k = 1; k <= 6; k++) {
        uint32_t len;
        uint8_t *img = image_of(k, &len);
        /* how many steps this save takes: one per erased sector and per byte written */
        long steps = (long)(SLOT / SECTOR) + (long)len + AB_STORE_HEADER;
        static uint8_t snapshot[2 * SLOT];
        memcpy(snapshot, s.mem, sizeof snapshot);
        for (long cut = 0; cut <= steps; cut++) {
            memcpy(s.mem, snapshot, sizeof s.mem);
            s.dead = false;
            s.budget = cut;
            s.seed = (unsigned)(cut * 2654435761u + (unsigned)k);
            int r = ab_store_save(&f, img, len);
            s.budget = -1;              /* the reboot: power back */
            s.dead = false;
            uint32_t n = ab_store_load(&f, buf, sizeof buf);
            cases++;
            bool ok;
            if (cut == steps) ok = r == 0 && same(buf, n, img, len);          /* no cut: the new one */
            else ok = same(buf, n, img, len) || (prev && same(buf, n, prev, prev_len)) || (!prev && n == 0);
            /* and the calculator must load whatever was found, and the next save must work */
            if (ok && n) {
                static ab_calc c;
                ab_init(&c);
                ok = ab_state_load(&c, buf, n) == ABN_OK;
            }
            if (ok) {
                static uint8_t keep[2 * SLOT];
                memcpy(keep, s.mem, sizeof keep);
                ok = ab_store_save(&f, img, len) == 0 && same(buf, ab_store_load(&f, buf, sizeof buf), img, len);
                memcpy(s.mem, keep, sizeof s.mem);
            }
            if (ok) good++;
            else if (cases - good <= 10) fprintf(stderr, "store_test: save %d, cut after %ld steps: no good image (%u bytes found)\n", k, cut, (unsigned)n);
        }
        /* the save completes for the next round */
        memcpy(s.mem, snapshot, sizeof s.mem);
        if (ab_store_save(&f, img, len) != 0) {
            fprintf(stderr, "store_test: save %d failed with no cut\n", k);
            return 1;
        }
        free(prev);
        prev = img;
        prev_len = len;
    }
    free(prev);
    printf("store_test: power cut at every moment of 6 saves: %ld/%ld leave a good image\n", good, cases);
    return (good == cases ? 0 : 1) | settings_checks();
}
