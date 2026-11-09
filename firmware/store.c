/* store: the state image in a ring over the whole partition (unit 092; see store.h). */
#include <string.h>

#include "state.h"
#include "store.h"

typedef struct {
    uint32_t seq, len, crc, at;     /* at: the sector its header starts */
    bool good;
} img_info;

static void put32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t sectors_of(const ab_flash *f)
{
    return 2 * f->slot_size / f->sector;        /* the whole partition: both of the old layout's slots */
}

static uint32_t span(const ab_flash *f, uint32_t len)
{
    return (AB_STORE_HEADER + len + f->sector - 1) / f->sector;    /* the sectors a header and its image take */
}

/* A read or a write at byte off of the image whose header is at sector at, running on past the partition's end at
   its start: one piece, or two */
static int ring_io(const ab_flash *f, bool wr, uint32_t at, uint32_t off, void *p, uint32_t len)
{
    uint32_t size = sectors_of(f) * f->sector, pos = (at * f->sector + off) % size;
    uint32_t first = len < size - pos ? len : size - pos;
    int r = wr ? f->write(f->ctx, pos, p, first) : f->read(f->ctx, pos, p, first);
    if (r != 0 || first == len) return r;
    return wr ? f->write(f->ctx, 0, (uint8_t *)p + first, len - first) : f->read(f->ctx, 0, (uint8_t *)p + first, len - first);
}

/* The header at sector i, its own CRC right and its length one a save could have written */
static bool header_at(const ab_flash *f, uint32_t i, img_info *hi)
{
    uint8_t h[AB_STORE_HEADER];
    if (f->read(f->ctx, i * f->sector, h, sizeof h) != 0) return false;
    if (memcmp(h, "ABST", 4) != 0 || ab_crc32(h, 16) != get32(h + 16)) return false;
    hi->seq = get32(h + 4);
    hi->len = get32(h + 8);
    hi->crc = get32(h + 12);
    hi->at = i;
    hi->good = false;
    return hi->len != 0 && hi->len <= f->slot_size - AB_STORE_HEADER;
}

/* Its payload's CRC, read back a piece at a time (CRC-32 continued across pieces; no buffer of the image's size) */
static bool payload_good(const ab_flash *f, const img_info *hi)
{
    uint8_t buf[256];
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t done = 0; done < hi->len;) {
        uint32_t n = hi->len - done < sizeof buf ? hi->len - done : (uint32_t)sizeof buf;
        if (ring_io(f, false, hi->at, AB_STORE_HEADER + done, buf, n) != 0) return false;
        for (uint32_t i = 0; i < n; i++) {
            c ^= buf[i];
            for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
        done += n;
    }
    return ~c == hi->crc;
}

/* The newest good image: every sector-start header read, the highest (seq, sector) first; the first whose payload's
   CRC is right. An older firmware's two-slot image starts at a slot's start, a sector start, so it is found the same
   way, as is apps.h v1.2's old second slot (inside today's first). A pass a header whose payload fails, which only a
   cut or a worn bit leaves, costs another pass of the headers. */
static img_info newest(const ab_flash *f)
{
    img_info best = {0, 0, 0, 0, false}, h;
    uint32_t n = sectors_of(f);
    uint64_t below = UINT64_MAX;                /* (seq, sector) bound: a failed candidate and everything above it */
    for (;;) {
        bool any = false;
        uint64_t key = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (!header_at(f, i, &h)) continue;
            uint64_t k = (uint64_t)h.seq << 32 | h.at;
            if (k < below && (!any || k > key)) best = h, key = k, any = true;
        }
        if (!any) {
            best.good = false;
            return best;
        }
        if (payload_good(f, &best)) {
            best.good = true;
            return best;
        }
        below = key;
    }
}

int ab_store_save(const ab_flash *f, const uint8_t *image, uint32_t len)
{
    uint8_t h[AB_STORE_HEADER];
    uint32_t n = sectors_of(f);
    /* half the partition at most: then the new image's sectors, which start after the newest's, end before the
       newest's start, whatever the ring's position (both at most n/2 sectors) */
    if (len == 0 || len > f->slot_size - AB_STORE_HEADER) return -1;
    img_info nw = newest(f);
    uint32_t at = nw.good ? (nw.at + span(f, nw.len)) % n : 0, k = span(f, len);
    uint32_t seq = (nw.good ? nw.seq : 0) + 1;
    /* only the sectors the header and image take, one range or two (past the end, from the start) */
    uint32_t k1 = k < n - at ? k : n - at;
    if (f->erase(f->ctx, at * f->sector, k1 * f->sector) != 0) return -1;
    if (k1 < k && f->erase(f->ctx, 0, (k - k1) * f->sector) != 0) return -1;
    if (ring_io(f, true, at, AB_STORE_HEADER, (void *)image, len) != 0) return -1;
    memset(h, 0xFF, sizeof h);
    memcpy(h, "ABST", 4);
    put32(h + 4, seq);
    put32(h + 8, len);
    put32(h + 12, ab_crc32(image, len));
    put32(h + 16, ab_crc32(h, 16));
    return f->write(f->ctx, at * f->sector, h, sizeof h);      /* the header last: it commits the save */
}

int ab_store_migrate(const ab_flash *f, uint32_t old_slot_size, uint8_t *buf, uint32_t cap)
{
    (void)f, (void)old_slot_size, (void)buf, (void)cap;
    return 0;                                   /* unit 092: the scan finds the old second slot's image itself */
}

uint32_t ab_store_seq(const ab_flash *f)
{
    img_info nw = newest(f);
    return nw.good ? nw.seq : 0;
}

uint32_t ab_store_load(const ab_flash *f, uint8_t *buf, uint32_t cap)
{
    img_info nw = newest(f);
    if (!nw.good || nw.len > cap) return 0;
    if (ring_io(f, false, nw.at, AB_STORE_HEADER, buf, nw.len) != 0) return 0;
    if (ab_crc32(buf, nw.len) != nw.crc) return 0;      /* changed since examined: refuse */
    return nw.len;
}
