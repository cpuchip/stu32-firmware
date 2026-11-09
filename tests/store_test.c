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
    if (k % 2) {                                /* unit 082: every other save a version-21 image, Y an expression */
        ab_val v;
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        if (ab_expr_new(&c, k % 4 == 1 ? "X^2+1" : "(A+B)\xc3\xb7(A-B)", &v) == ABN_OK) c.y = v;
    }
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
    ab_flash f2 = {&s2, SECTOR, SLOT, sim_erase, sim_write, sim_read, NULL};
    dev_settings set, back, def;
    int bad = 0, checks = 0;
#define CHECK(ok, what) do { checks++; if (!(ok)) { bad++; fprintf(stderr, "store_test: %s\n", what); } } while (0)
    memset(s2.mem, 0xFF, sizeof s2.mem);
    s2.budget = -1;
    dev_settings_default(&def);
    CHECK(def.radix == '.' && def.group == 1 && def.off_min == 10 && def.vol == 4 && def.uses == DEV_USE_ALL && !def.h24,
          "the defaults: radix '.', grouping 1,000 (abacus #3965: UG p.1-23, p.B-3), 10 minutes, the loudest level, BEEP on");
    ab_init(&c);
    ab_do(&c, AB_K7);
    ab_do(&c, AB_ENTER);
    set = (dev_settings){',', ' ', 30, 2, 0, false};
    size_t n = dev_image_save(&c, &set, img, sizeof img);
    CHECK(n > ab_state_save(&c, NULL, 0), "the record follows the core's image");
    CHECK(n > 0 && n <= sizeof img && ab_store_save(&f2, img, (uint32_t)n) == 0, "saved through the store");
    uint32_t m = ab_store_load(&f2, got, sizeof got);
    ab_init(&c2);
    back = def;
    CHECK(m == n && dev_image_load(&c2, &back, got, m) == ABN_OK, "loaded back");
    CHECK(back.radix == ',' && back.group == ' ' && back.off_min == 30 && back.vol == 2 && back.uses == 0,
          "non-default settings load back");
    CHECK(abn_cmp(&c2.x.re, &c.x.re) == 0, "and the state with them");
    /* an image from before unit 022 (the core's alone) loads the defaults */
    n = ab_state_save(&c, img, sizeof img);
    back = set;
    CHECK(dev_image_load(&c2, &back, img, n) == ABN_OK && !memcmp(&back, &def, sizeof back),
          "a pre-022 image loads, with the default settings");
    /* a damaged record gives the defaults; the state still loads */
    n = dev_image_save(&c, &set, img, sizeof img);
    img[ab_state_save(&c, NULL, 0) + 9] ^= 0x20;    /* grouping ' ' to 0: still in range, so only the CRC tells */
    back = set;
    CHECK(dev_image_load(&c2, &back, img, n) == ABN_OK && !memcmp(&back, &def, sizeof back),
          "a damaged record: the defaults, the state loaded");
    /* a refused core image: BAD_IMAGE and the defaults */
    n = dev_image_save(&c, &set, img, sizeof img);
    img[5] ^= 0x7F;
    back = set;
    CHECK(dev_image_load(&c2, &back, img, n) == ABN_BAD_IMAGE && !memcmp(&back, &def, sizeof back),
          "a refused core image: BAD_IMAGE, the defaults");
    /* unit 027: the uses mask and the 12/24 setting survive; a version-1 record (BEEP one switch) loads
       with every use on if BEEP was on, none if off, and the 12-hour clock */
    set = (dev_settings){',', ' ', 30, 2, DEV_USE_CLICK | DEV_USE_DONE, true};
    n = dev_image_save(&c, &set, img, sizeof img);
    back = def;
    CHECK(dev_image_load(&c2, &back, img, n) == ABN_OK && back.uses == (DEV_USE_CLICK | DEV_USE_DONE) && back.h24,
          "unit 027: two uses on and the 24-hour clock load back");
    {   /* unit 082: the record after a version-21 core image (an expression in X), and both back */
        ab_val v;
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        CHECK(ab_expr_new(&c, "3\xc3\x97X^2-4", &v) == ABN_OK, "082: an expression made");
        c.x = v;
        n = dev_image_save(&c, &set, img, sizeof img);
        back = def;
        CHECK(img[4] == 24 && dev_image_load(&c2, &back, img, n) == ABN_OK && back.h24 && c2.x.kind == AB_EXPR &&
                  !strcmp(ab_expr_text(&c2, &c2.x), "3\xc3\x97X^2-4") && c2.z.bare && c2.t.bare && !c2.y.bare,
              "082: an expression's image (version 24 since unit 090: Z and T bare) with the device's record: both load "
              "back, X's expression whole, the bare marks too");
    }
    for (int on = 0; on <= 1; on++) {
        size_t core = ab_state_save(&c, img, sizeof img);
        uint8_t *r = img + core;
        memcpy(r, "ABDS", 4);
        r[4] = 1;
        r[5] = r[6] = r[7] = 0;
        r[8] = '.';
        r[9] = 1;
        r[10] = 10;
        r[11] = 3;
        r[12] = (uint8_t)on;
        uint32_t crc = ab_crc32(r, 13);
        for (int i = 0; i < 4; i++) r[13 + i] = (uint8_t)(crc >> (8 * i));
        back = set;
        CHECK(dev_image_load(&c2, &back, img, core + 17) == ABN_OK && back.uses == (on ? DEV_USE_ALL : 0) &&
                  !back.h24 && back.vol == 3,
              on ? "a version-1 record with BEEP on: every use on, 12-hour" : "a version-1 record with BEEP off: none");
    }
#undef CHECK
    printf("store_test: settings %d/%d checks\n", checks - bad, checks);
    return bad != 0;
}

/* apps.h v1.2: abapps grew from 8 KB slots to bigger ones in place; the newest image in the old second slot (inside
   the new first) must not be lost (fresh eyes: it was). Unit 092: the ring's load finds it at its sector without a
   migration (before 092 the load gave the older one, the loss, until migrate saved it again), and migrate does
   nothing */
static int migrate_checks(void)
{
    static sim s;
    static uint8_t buf[SLOT];
    int bad = 0;
    memset(s.mem, 0xFF, sizeof s.mem);
    s.budget = -1;
    ab_flash oldf = {&s, SECTOR, 2 * SECTOR, sim_erase, sim_write, sim_read, NULL};
    ab_flash newf = {&s, SECTOR, SLOT, sim_erase, sim_write, sim_read, NULL};
    uint32_t l1, l2, l3;
    uint8_t *i1 = image_of(1, &l1), *i2 = image_of(2, &l2), *i3 = image_of(3, &l3);
    if (l1 > 2 * SECTOR - AB_STORE_HEADER || l2 > 2 * SECTOR - AB_STORE_HEADER) bad++;
    ab_store_save(&oldf, i1, l1);
    ab_store_save(&oldf, i2, l2);               /* the newest, in the old second slot */
    uint32_t n = ab_store_load(&newf, buf, sizeof buf);
    if (!same(buf, n, i2, l2)) bad++;           /* without a migration: the newest (092's scan; before, the loss) */
    if (ab_store_migrate(&newf, 2 * SECTOR, buf, sizeof buf) != 0) bad++;   /* (092: nothing to do) */
    n = ab_store_load(&newf, buf, sizeof buf);
    if (!same(buf, n, i2, l2)) bad++;           /* and still the newest */
    if (ab_store_migrate(&newf, 2 * SECTOR, buf, sizeof buf) != 0) bad++;   /* once only */
    ab_store_save(&newf, i3, l3);
    n = ab_store_load(&newf, buf, sizeof buf);
    if (!same(buf, n, i3, l3) || ab_store_migrate(&newf, 2 * SECTOR, buf, sizeof buf) != 0) bad++;
    memset(s.mem, 0xFF, sizeof s.mem);          /* one old save only: in the first slot, nothing to move */
    ab_store_save(&oldf, i1, l1);
    if (ab_store_migrate(&newf, 2 * SECTOR, buf, sizeof buf) != 0 || !same(buf, ab_store_load(&newf, buf, sizeof buf), i1, l1)) bad++;
    free(i1), free(i2), free(i3);
    printf("store_test: migration after the slots grew: %s\n", bad ? "FAIL" : "the newest image kept");
    return bad != 0;
}

/* ---- unit 092: the ring ---- */

static uint8_t apps_mem[18 * 4096];         /* the apps' partition (abapps, 72 KiB), no cuts */
static int apps_erase(void *c, uint32_t off, uint32_t len) { (void)c; memset(apps_mem + off, 0xFF, len); return 0; }
static int apps_write(void *c, uint32_t off, const void *src, uint32_t len)
{
    (void)c;
    for (uint32_t i = 0; i < len; i++) apps_mem[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static int apps_read(void *c, uint32_t off, void *dst, uint32_t len) { (void)c; memcpy(dst, apps_mem + off, len); return 0; }

static uint8_t *content(uint32_t seq, uint32_t n)
{
    uint8_t *b = malloc(n);
    for (uint32_t i = 0; i < n; i++) b[i] = (uint8_t)(((uint64_t)seq * 31 + i) % 251);
    return b;
}

static bool load_content(const ab_flash *f, uint32_t seq, uint32_t n)
{
    static uint8_t got[64 * 4096];
    uint8_t *c = content(seq, n);
    bool ok = ab_store_load(f, got, sizeof got) == n && ab_store_seq(f) == seq && memcmp(got, c, n) == 0;
    free(c);
    return ok;
}

static bool read_fixture(const char *path, uint8_t *mem, size_t n)
{
    FILE *in = fopen(path, "rb");
    bool ok = in && fread(mem, 1, n, in) == n && fgetc(in) == EOF;
    if (in) fclose(in);
    return ok;
}

/* The two-slot store's partitions (written by store.c at b59caec, work/092-evidence/make-fixtures.c) load under the
   ring: the state's shape (seq 3 in slot 0 the newest, seq 2 in slot 1), a ring save from it cut at every moment, the
   apps' v1.1 layout (the newest in the old 8 KiB second slot, inside v1.2's first) */
static int fixture_checks(void)
{
    static sim s;
    static uint8_t snapshot[2 * SLOT];
    int bad = 0;
    long cases = 0, good = 0;
    ab_flash f = {&s, SECTOR, SLOT, sim_erase, sim_write, sim_read, NULL};
    s.budget = -1, s.dead = false;
    if (!read_fixture("tests/store-fixture-twoslot.bin", s.mem, sizeof s.mem) || !load_content(&f, 3, 3000)) bad++;
    uint8_t *c4 = content(4, 6000);
    long steps = (long)((AB_STORE_HEADER + 6000 + SECTOR - 1) / SECTOR) + 6000 + AB_STORE_HEADER;
    memcpy(snapshot, s.mem, sizeof snapshot);
    for (long cut = 0; cut <= steps; cut++) {
        memcpy(s.mem, snapshot, sizeof s.mem);
        s.dead = false, s.budget = cut, s.seed = (unsigned)cut * 2654435761u;
        ab_store_save(&f, c4, 6000);
        s.budget = -1, s.dead = false;
        cases++;
        good += cut == steps ? load_content(&f, 4, 6000) : load_content(&f, 3, 3000) || load_content(&f, 4, 6000);
    }
    if (good != cases) bad++;
    /* the new one went after seq 3's sector; the next ones run on over slot 1's seq 2, and the newest always wins */
    for (uint32_t k = 5; k < 12; k++) {
        uint8_t *c = content(k, 7000 + 900 * k);
        if (ab_store_save(&f, c, 7000 + 900 * k) != 0 || !load_content(&f, k, 7000 + 900 * k)) bad++;
        free(c);
    }
    free(c4);
    /* the apps' partition, 18 sectors, v1.1's 8 KiB slots: seq 2 in the old second slot (sectors 2-3) loads without a
       migration, which does nothing; the next save goes after it, at sector 4 */
    ab_flash ap = {NULL, SECTOR, 9 * SECTOR, apps_erase, apps_write, apps_read, NULL};
    static uint8_t buf[9 * SECTOR];
    if (!read_fixture("tests/store-fixture-apps-v12.bin", apps_mem, sizeof apps_mem) || !load_content(&ap, 2, 5000) ||
        ab_store_migrate(&ap, 0x2000, buf, sizeof buf) != 0 || !load_content(&ap, 2, 5000))
        bad++;
    uint8_t *c3 = content(3, 30000);
    if (ab_store_save(&ap, c3, 30000) != 0 || !load_content(&ap, 3, 30000) || memcmp(apps_mem + 4 * SECTOR, "ABST", 4) != 0)
        bad++;
    free(c3);
    /* a good header over a payload that isn't whole (a worn bit, or sectors a later save took): skipped, the one
       before loads; and an image past half the partition refused (it could reach the newest) */
    apps_mem[4 * SECTOR + AB_STORE_HEADER + 100] ^= 0x01;
    if (!load_content(&ap, 2, 5000)) bad++;
    apps_mem[4 * SECTOR + AB_STORE_HEADER + 100] ^= 0x01;
    if (!load_content(&ap, 3, 30000)) bad++;
    uint8_t *big = content(9, 9 * SECTOR - AB_STORE_HEADER + 1);
    if (ab_store_save(&ap, big, 9 * SECTOR - AB_STORE_HEADER + 1) != -1 || !load_content(&ap, 3, 30000)) bad++;
    if (ab_store_save(&ap, big, 9 * SECTOR - AB_STORE_HEADER) != 0) bad++;    /* (exactly half: taken) */
    free(big);
    printf("store_test: the two-slot fixtures under the ring: %ld/%ld cuts leave the newest that finished; %s\n", good,
           cases, bad ? "FAIL" : "the state's seq 3 and the apps' v1.1 seq 2 loaded, then every newer one");
    return bad != 0;
}

/* The erases a sector takes over 10,000 saves on the state's partition (128 sectors of 4,096 B), for the three states
   of 091 (b)'s table: 1,237 B (1 sector), 9,138 B (3), 27,683 B (7). Each save erases the k sectors after the last
   one's, so every sector's count is 10,000 × k / 128 rounded down or up (the blueprint's table: k/128 of the saves,
   where the two-slot store put half of them on each slot's first sector) */
static uint8_t wear_mem[128 * 4096];
static uint32_t wear_n[128];
static int wear_erase(void *c, uint32_t off, uint32_t len)
{
    (void)c;
    memset(wear_mem + off, 0xFF, len);
    for (uint32_t a = off; a < off + len; a += 4096) wear_n[a / 4096]++;
    return 0;
}
static int wear_write(void *c, uint32_t off, const void *src, uint32_t len)
{
    (void)c;
    for (uint32_t i = 0; i < len; i++) wear_mem[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static long wear_read_bytes;
static int wear_read(void *c, uint32_t off, void *dst, uint32_t len)
{
    (void)c;
    memcpy(dst, wear_mem + off, len);
    wear_read_bytes += len;
    return 0;
}

static int wear_checks(void)
{
    static const uint32_t SIZES[3] = {1237, 9138, 27683}, K[3] = {1, 3, 7};
    ab_flash f = {NULL, 4096, 64 * 4096, wear_erase, wear_write, wear_read, NULL};
    int bad = 0;
    for (int z = 0; z < 3; z++) {
        memset(wear_mem, 0xFF, sizeof wear_mem);
        memset(wear_n, 0, sizeof wear_n);
        uint8_t *c = content(1, SIZES[z]);
        for (int i = 0; i < 10000; i++)
            if (ab_store_save(&f, c, SIZES[z]) != 0) bad++;
        free(c);
        uint32_t lo = 10000 * K[z] / 128, hi = (10000 * K[z] + 127) / 128, mn = UINT32_MAX, mx = 0;
        for (int i = 0; i < 128; i++) mn = wear_n[i] < mn ? wear_n[i] : mn, mx = wear_n[i] > mx ? wear_n[i] : mx;
        if (mn < lo || mx > hi || ab_store_seq(&f) != 10000) bad++;
        printf("store_test: 10,000 saves of %u B (%u sectors): every sector erased %u to %u times (%u to %u allowed)\n",
               (unsigned)SIZES[z], (unsigned)K[z], (unsigned)mn, (unsigned)mx, (unsigned)lo, (unsigned)hi);
    }
    return bad != 0;
}

/* The remembered newest (lean 3): after a load, a save reads one header (32 bytes) where a scan reads 128; a
   partition erased behind the store's back (the console's WIPE) costs a scan, never a wrong place; a copy of the
   ab_flash sharing its ring (SAVEBENCH's timing wrapper) keeps it right for both */
static int ring_checks(void)
{
    static ab_store_ring r;
    static uint8_t got[64 * 4096];
    ab_flash f = {NULL, 4096, 64 * 4096, wear_erase, wear_write, wear_read, &r};
    int bad = 0;
    long scan, one;
    memset(wear_mem, 0xFF, sizeof wear_mem);
    memset(&r, 0, sizeof r);
    uint8_t *c = content(1, 1237);
    if (ab_store_save(&f, c, 1237) != 0 || ab_store_save(&f, c, 1237) != 0) bad++;
    r.valid = false;                            /* (a boot: RAM empty) */
    wear_read_bytes = 0;
    if (ab_store_save(&f, c, 1237) != 0) bad++;
    scan = wear_read_bytes;
    r.valid = false;                            /* (a boot again, then its load: the load remembers) */
    if (ab_store_load(&f, got, sizeof got) != 1237) bad++;
    wear_read_bytes = 0;
    if (ab_store_save(&f, c, 1237) != 0) bad++;
    one = wear_read_bytes;
    if (scan < 128 * AB_STORE_HEADER || one != AB_STORE_HEADER || ab_store_seq(&f) != 4) bad++;
    memset(wear_mem, 0xFF, sizeof wear_mem);    /* WIPE behind its back */
    if (ab_store_save(&f, c, 1237) != 0 || ab_store_seq(&f) != 1 || memcmp(wear_mem, "ABST", 4) != 0) bad++;
    ab_flash g = f;                             /* SAVEBENCH's wrapper: the same ring */
    for (int i = 0; i < 5; i++) bad += ab_store_save(&g, c, 1237) != 0;
    if (ab_store_save(&f, c, 1237) != 0 || ab_store_seq(&f) != 7 || memcmp(wear_mem + 6 * 4096, "ABST", 4) != 0 ||
        ab_store_load(&f, got, sizeof got) != 1237)
        bad++;
    free(c);
    printf("store_test: the remembered newest: a save after a load reads %ld bytes (a scan %ld); WIPE and a shared ring "
           "%s\n", one, scan, bad ? "FAIL" : "kept right");
    return bad != 0;
}

int main(void)
{
    static sim s;
    ab_flash f = {&s, SECTOR, SLOT, sim_erase, sim_write, sim_read, NULL};
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
        long steps = (long)((AB_STORE_HEADER + len + SECTOR - 1) / SECTOR) + (long)len + AB_STORE_HEADER;  /* (v1.2:
                                                   only the sectors the save takes are erased) */
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
    return (good == cases ? 0 : 1) | settings_checks() | migrate_checks() | fixture_checks() | wear_checks() | ring_checks();
}
