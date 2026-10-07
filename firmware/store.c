/* store: two flash slots for the state image (see store.h). */
#include <string.h>

#include "state.h"
#include "store.h"

typedef struct {
    uint32_t seq, len, crc;
    bool good;
} slot_info;

static void put32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* A slot's header, and with its CRCs right, its payload's CRC checked by reading it back in
   pieces (no buffer of the image's size needed). */
static slot_info examine(const ab_flash *f, int s)
{
    slot_info si = {0, 0, 0, false};
    uint8_t h[AB_STORE_HEADER], buf[256];
    uint32_t base = (uint32_t)s * f->slot_size;
    if (f->read(f->ctx, base, h, sizeof h) != 0) return si;
    if (memcmp(h, "ABST", 4) != 0 || ab_crc32(h, 16) != get32(h + 16)) return si;
    si.seq = get32(h + 4);
    si.len = get32(h + 8);
    si.crc = get32(h + 12);
    if (si.len == 0 || si.len > f->slot_size - AB_STORE_HEADER) return si;
    {
        /* the payload's CRC, a piece at a time (CRC-32 continued across pieces) */
        uint32_t c = 0xFFFFFFFFu;
        for (uint32_t done = 0; done < si.len;) {
            uint32_t n = si.len - done < sizeof buf ? si.len - done : (uint32_t)sizeof buf;
            if (f->read(f->ctx, base + AB_STORE_HEADER + done, buf, n) != 0) return si;
            for (uint32_t i = 0; i < n; i++) {
                c ^= buf[i];
                for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
            }
            done += n;
        }
        si.good = ~c == si.crc;
    }
    return si;
}

int ab_store_save(const ab_flash *f, const uint8_t *image, uint32_t len)
{
    slot_info a = examine(f, 0), b = examine(f, 1);
    int target;
    uint32_t seq, base;
    uint8_t h[AB_STORE_HEADER];
    if (len == 0 || len > f->slot_size - AB_STORE_HEADER) return -1;
    /* the slot without the newest good image; with neither good, slot 0 */
    if (a.good && b.good) target = a.seq > b.seq ? 1 : 0;
    else if (a.good) target = 1;
    else target = 0;
    seq = (a.good && a.seq >= (b.good ? b.seq : 0) ? a.seq : b.good ? b.seq : 0) + 1;
    base = (uint32_t)target * f->slot_size;
    /* only the sectors the header and image take (apps.h v1.2, fresh eyes: a 36 KB slot erased whole for an
       80-byte save); the rest of the slot is never read: the header's length and CRCs decide */
    uint32_t need = (AB_STORE_HEADER + len + f->sector - 1) / f->sector * f->sector;
    if (need > f->slot_size) need = f->slot_size;
    if (f->erase(f->ctx, base, need) != 0) return -1;
    if (f->write(f->ctx, base + AB_STORE_HEADER, image, len) != 0) return -1;
    memset(h, 0xFF, sizeof h);
    memcpy(h, "ABST", 4);
    put32(h + 4, seq);
    put32(h + 8, len);
    put32(h + 12, ab_crc32(image, len));
    put32(h + 16, ab_crc32(h, 16));
    return f->write(f->ctx, base, h, sizeof h);      /* the header last: it commits the save */
}

int ab_store_migrate(const ab_flash *f, uint32_t old_slot_size, uint8_t *buf, uint32_t cap)
{
    ab_flash old = *f;
    old.slot_size = old_slot_size;
    if (old_slot_size == 0 || old_slot_size >= f->slot_size) return 0;
    slot_info ob = examine(&old, 1), a = examine(f, 0), b = examine(f, 1);
    uint32_t newest = a.good ? a.seq : 0;
    if (b.good && b.seq > newest) newest = b.seq;
    if (!ob.good || ((a.good || b.good) && ob.seq <= newest) || ob.len > cap) return 0;
    if (f->read(f->ctx, old_slot_size + AB_STORE_HEADER, buf, ob.len) != 0 || ab_crc32(buf, ob.len) != ob.crc) return -1;
    return ab_store_save(f, buf, ob.len) == 0 ? 1 : -1;
}

uint32_t ab_store_load(const ab_flash *f, uint8_t *buf, uint32_t cap)
{
    slot_info a = examine(f, 0), b = examine(f, 1);
    int s;
    slot_info *pick;
    if (!a.good && !b.good) return 0;
    s = a.good && (!b.good || a.seq > b.seq) ? 0 : 1;
    pick = s == 0 ? &a : &b;
    if (pick->len > cap) return 0;
    if (f->read(f->ctx, (uint32_t)s * f->slot_size + AB_STORE_HEADER, buf, pick->len) != 0) return 0;
    if (ab_crc32(buf, pick->len) != pick->crc) return 0;   /* changed since examined: refuse */
    return pick->len;
}
