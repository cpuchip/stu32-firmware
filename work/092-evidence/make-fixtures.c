/* Unit 092's fixtures, written by the two-slot store as it was before 092 (store.c and store.h at b59caec), so the
 * ring is tested against what older firmware really left in the flash. Built once, against those two files:
 *
 *   cc ... -I<old store> make-fixtures.c <old store.c> $(CORE) $(ENGINE)   (see the record)
 *
 * tests/store-fixture-twoslot.bin: the state store's shape scaled to store_test's simulated flash (16 sectors of 4,096 B,
 *   two slots of 8): seq 1 (5,000 B) in slot 0, seq 2 (9,000 B) in slot 1, seq 3 (3,000 B) in slot 0 again, the newest.
 * tests/store-fixture-apps-v12.bin: the apps' partition (18 sectors, 72 KiB) as apps.h v1.1 left it, with 8 KiB slots:
 *   seq 1 (3,000 B) in slot 0, seq 2 (5,000 B) in the old second slot at 0x2000, the newest (v1.2's 36 KiB slots then
 *   put that one inside the first).
 * Byte i of seq's image is (seq × 31 + i) mod 251, store_sim's rule. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "store.h"

static uint8_t mem[18 * 4096];

static int e(void *c, uint32_t off, uint32_t len) { (void)c; memset(mem + off, 0xFF, len); return 0; }
static int w(void *c, uint32_t off, const void *src, uint32_t len)
{
    (void)c;
    for (uint32_t i = 0; i < len; i++) mem[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static int r(void *c, uint32_t off, void *dst, uint32_t len) { (void)c; memcpy(dst, mem + off, len); return 0; }

static void save(const ab_flash *f, uint32_t seq, uint32_t len)
{
    static uint8_t b[40000];
    for (uint32_t i = 0; i < len; i++) b[i] = (uint8_t)(((uint64_t)seq * 31 + i) % 251);
    if (ab_store_save(f, b, len) != 0) exit(1);
}

static void out(const char *path, size_t n)
{
    FILE *o = fopen(path, "wb");
    if (!o || fwrite(mem, 1, n, o) != n || fclose(o) != 0) exit(1);
}

int main(void)
{
    ab_flash st = {NULL, 4096, 8 * 4096, e, w, r};
    memset(mem, 0xFF, sizeof mem);
    save(&st, 1, 5000), save(&st, 2, 9000), save(&st, 3, 3000);
    out("tests/store-fixture-twoslot.bin", 16 * 4096);
    ab_flash ap = {NULL, 4096, 2 * 4096, e, w, r};     /* apps.h v1.1: 8 KiB slots */
    memset(mem, 0xFF, sizeof mem);
    save(&ap, 1, 3000), save(&ap, 2, 5000);
    out("tests/store-fixture-apps-v12.bin", 18 * 4096);
    return 0;
}
