/* store_sim (unit 092, work/092-store-wear.md's interface for the hidden set): the store (firmware/store.c) over a
 * simulated NOR flash, driven by a script.
 *
 *   build/store_sim FILE
 *
 * Lines are a command and its arguments, separated by blanks; `#` starts a comment. Prints a FAIL line for each
 * expectation that fails, then a total; exits 0 when every expectation holds.
 *
 *   PART n s              a fresh partition of n sectors of s bytes (n even: the store's partition is two slot_size
 *                         halves), every byte 0xFF, and a store over it
 *   OLD slot bytes seq    an image written the way the two-slot store (before 092) wrote one: at slot 0 or 1 of a
 *                         two-slot layout over the partition (slot size n×s/2), header and payload
 *   SAVE bytes            a save through the store of an image of that many bytes; byte i is (seq × 31 + i) mod 251,
 *                         seq the image's own (the newest good image's + 1)
 *   CUT bytes budget      the same save with the power cut after budget flash operations (a byte written or a sector
 *                         erased each counts one; a cut during an erase leaves that sector garbage); then a reboot
 *   CUTALL bytes          the save from the same starting partition with the cut at every budget from 0 to the save's
 *                         own count; after each, a load must give the newest image that finished: this one if its
 *                         header is whole in the flash, else the one before. Prints the cuts and how many held, and
 *                         leaves the uncut save (its erases counted once)
 *   EXPECT LOAD seq|NONE  a load gives the image of that seq, its content whole, or none
 *   EXPECT ERASES sector lo hi    that sector's erase count since PART, from lo to hi
 *   EXPECT MAXERASES hi   no sector's count above hi
 *   REBOOT                the store's RAM state dropped (the store keeps none: every save scans)
 *
 * CUTALL replays the uncut save's operations a prefix at a time: the store reads only before its first erase, so a
 * save cut after b operations leaves exactly the first b of them applied (and, at an erase, that sector garbage). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "store.h"

typedef struct { uint32_t off, len; uint8_t kind, val; } op;    /* kind 0 an erase of one sector, 1 a byte written */

static uint8_t *mem, *snap;
static uint32_t *erases, *snap_erases;
static uint32_t nsec, ssz;
static long budget = -1;            /* operations still allowed; -1 no cut */
static bool dead;
static unsigned seed = 12345;
static op *log_ops;
static size_t nlog, caplog;
static bool logging;
static uint32_t lens[1 << 16];      /* each seq's image length, as the script made it (0 unknown) */
static int fails, checks, line_no;

static void logop(uint8_t kind, uint32_t off, uint8_t val)
{
    if (!logging) return;
    if (nlog == caplog) {
        caplog = caplog ? 2 * caplog : 4096;
        log_ops = realloc(log_ops, caplog * sizeof *log_ops);
        if (!log_ops) exit(2);
    }
    log_ops[nlog++] = (op){off, 0, kind, val};
}

static void garbage(uint32_t a)
{
    for (uint32_t i = 0; i < ssz; i++) mem[a + i] = (uint8_t)((seed = seed * 1103515245u + 12345u) >> 3);
}

static int sim_erase(void *ctx, uint32_t off, uint32_t len)
{
    (void)ctx;
    for (uint32_t a = off; a < off + len; a += ssz) {
        if (dead) return -1;
        if (budget == 0) {
            garbage(a);
            dead = true;
            return -1;
        }
        if (budget > 0) budget--;
        logop(0, a, 0);
        memset(mem + a, 0xFF, ssz);
        erases[a / ssz]++;
    }
    return 0;
}

static int sim_write(void *ctx, uint32_t off, const void *src, uint32_t len)
{
    (void)ctx;
    const uint8_t *p = src;
    for (uint32_t i = 0; i < len; i++) {
        if (dead) return -1;
        if (budget == 0) {
            dead = true;
            return -1;
        }
        if (budget > 0) budget--;
        logop(1, off + i, p[i]);
        mem[off + i] &= p[i];               /* NOR: a write only clears bits */
    }
    return 0;
}

static int sim_read(void *ctx, uint32_t off, void *dst, uint32_t len)
{
    (void)ctx;
    memcpy(dst, mem + off, len);
    return 0;
}

static ab_flash fl;

static void fail(const char *fmt, ...)
{
    va_list ap;
    fails++;
    printf("FAIL line %d: ", line_no);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

static uint8_t *content(uint32_t seq, uint32_t n)
{
    uint8_t *b = malloc(n ? n : 1);
    if (!b) exit(2);
    for (uint32_t i = 0; i < n; i++) b[i] = (uint8_t)(((uint64_t)seq * 31 + i) % 251);
    return b;
}

/* the load's image: seq (0 none) by its content, checked whole against the length the script gave that seq */
static bool load_is(uint32_t want, char *why, size_t wsz)
{
    uint32_t cap = fl.slot_size;
    uint8_t *b = malloc(cap);
    if (!b) exit(2);
    uint32_t n = ab_store_load(&fl, b, cap), seq = ab_store_seq(&fl);
    bool ok;
    if (want == 0) {
        ok = n == 0;
        snprintf(why, wsz, "loaded %u bytes (seq %u)", (unsigned)n, (unsigned)seq);
    } else {
        uint8_t *c = content(want, lens[want & 0xFFFF]);
        ok = n != 0 && seq == want && n == lens[want & 0xFFFF] && memcmp(b, c, n) == 0;
        snprintf(why, wsz, "loaded %u bytes, seq %u (want %u bytes, seq %u)%s", (unsigned)n, (unsigned)seq,
                 (unsigned)lens[want & 0xFFFF], (unsigned)want, n && seq == want && n == lens[want & 0xFFFF] &&
                 memcmp(b, c, n) ? ", content differs" : "");
        free(c);
    }
    free(b);
    return ok;
}

static void part(uint32_t n, uint32_t s)
{
    free(mem), free(snap), free(erases), free(snap_erases);
    nsec = n, ssz = s;
    mem = malloc((size_t)n * s), snap = malloc((size_t)n * s);
    erases = calloc(n, sizeof *erases), snap_erases = calloc(n, sizeof *snap_erases);
    if (!mem || !snap || !erases || !snap_erases) exit(2);
    memset(mem, 0xFF, (size_t)n * s);
    memset(lens, 0, sizeof lens);
    fl = (ab_flash){NULL, s, n * s / 2, sim_erase, sim_write, sim_read};
    budget = -1, dead = false;
}

static void put32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

/* the two-slot store's save (store.c before 092) into a slot, as it wrote: the sectors it takes erased from the
   slot's start, the payload after the header's 32 bytes, the header last */
static void old_save(int slot, uint32_t len, uint32_t seq)
{
    uint32_t base = (uint32_t)slot * fl.slot_size, need = (AB_STORE_HEADER + len + ssz - 1) / ssz * ssz;
    uint8_t h[AB_STORE_HEADER], *c = content(seq, len);
    if (need > fl.slot_size) need = fl.slot_size;
    sim_erase(NULL, base, need);
    sim_write(NULL, base + AB_STORE_HEADER, c, len);
    memset(h, 0xFF, sizeof h);
    memcpy(h, "ABST", 4);
    put32(h + 4, seq);
    put32(h + 8, len);
    put32(h + 12, ab_crc32(c, len));
    put32(h + 16, ab_crc32(h, 16));
    sim_write(NULL, base, h, sizeof h);
    lens[seq & 0xFFFF] = len;
    free(c);
}

/* a save of len bytes as the store makes it; its seq is the newest good image's + 1 */
static int save(uint32_t len, uint32_t *seq_out)
{
    uint32_t seq = ab_store_seq(&fl) + 1;
    uint8_t *c = content(seq, len);
    int r = ab_store_save(&fl, c, len);
    free(c);
    *seq_out = seq;
    return r;
}

static void apply(const op *o)
{
    if (o->kind == 0) memset(mem + o->off, 0xFF, ssz);
    else mem[o->off] &= o->val;
}

static void cutall(uint32_t len)
{
    size_t bytes = (size_t)nsec * ssz;
    uint32_t prev = ab_store_seq(&fl), seq;
    memcpy(snap, mem, bytes);
    memcpy(snap_erases, erases, nsec * sizeof *erases);
    nlog = 0, logging = true;
    int r = save(len, &seq);                     /* the uncut save, its operations logged */
    logging = false;
    if (r != 0) {
        checks++;
        fail("CUTALL %u: the save itself failed (%d)", (unsigned)len, r);
        return;
    }
    lens[seq & 0xFFFF] = len;
    uint8_t *after = malloc(bytes), hdr[AB_STORE_HEADER];
    uint32_t *after_er = malloc(nsec * sizeof *erases), at = 0;
    if (!after || !after_er) exit(2);
    memcpy(after, mem, bytes);
    memcpy(after_er, erases, nsec * sizeof *erases);
    for (uint32_t i = 0; i < nsec; i++)        /* the new image's header: where the uncut save put it */
        if (memcmp(after + (size_t)i * ssz, "ABST", 4) == 0 && memcmp(after + (size_t)i * ssz, snap + (size_t)i * ssz,
                                                                         AB_STORE_HEADER) != 0)
            at = i;
    memcpy(hdr, after + (size_t)at * ssz, AB_STORE_HEADER);
    memcpy(mem, snap, bytes);
    long held = 0;
    char why[160];
    for (size_t b = 0; b <= nlog; b++) {         /* the cut after b operations */
        uint8_t *sector_keep = NULL;
        if (b < nlog && log_ops[b].kind == 0) { /* cut at an erase: that sector garbage, for this load only */
            sector_keep = malloc(ssz);
            if (!sector_keep) exit(2);
            memcpy(sector_keep, mem + log_ops[b].off, ssz);
            garbage(log_ops[b].off);
        }
        bool whole = memcmp(mem + (size_t)at * ssz, hdr, AB_STORE_HEADER) == 0;
        bool ok = load_is(whole ? seq : prev, why, sizeof why);
        if (ok) held++;
        else if ((long)b - held < 10)            /* (the first few, as detail under the CUTALL's FAIL) */
            printf("  CUTALL %u: cut after %ld of %ld operations: %s\n", (unsigned)len, (long)b, (long)nlog, why);
        if (sector_keep) memcpy(mem + log_ops[b].off, sector_keep, ssz), free(sector_keep);
        if (b < nlog) apply(&log_ops[b]);
    }
    checks++;
    printf("CUTALL %u: %ld cuts, %ld held\n", (unsigned)len, (long)nlog + 1, held);
    if (held != (long)nlog + 1) fail("CUTALL %u: %ld of %ld cuts held", (unsigned)len, held, (long)nlog + 1);
    if (memcmp(mem, after, bytes) != 0) {
        fail("CUTALL %u: the replayed operations don't give the uncut save's flash", (unsigned)len);
    }
    memcpy(erases, after_er, nsec * sizeof *erases);
    free(after), free(after_er);
}

int main(int argc, char **argv)
{
    FILE *in = argc > 1 ? fopen(argv[1], "r") : NULL;
    char line[512];
    if (!in) {
        fprintf(stderr, "usage: store_sim FILE\n");
        return 2;
    }
    part(2, 4096);
    while (fgets(line, sizeof line, in)) {
        line_no++;
        char *h = strchr(line, '#');
        if (h) *h = '\0';
        char cmd[32], a[32] = "", why[160];
        long x = 0, y = 0, z = 0;
        int k = sscanf(line, "%31s %31s %ld %ld", cmd, a, &y, &z);
        if (k < 1) continue;
        x = strtol(a, NULL, 10);
        if (!strcmp(cmd, "PART") && k >= 3) {
            if (x < 2 || x % 2 || y < 64) {
                checks++;
                fail("PART %ld %ld: n must be even and at least 2, s at least 64", x, y);
                continue;
            }
            part((uint32_t)x, (uint32_t)y);
        } else if (!strcmp(cmd, "OLD") && k >= 4) {
            old_save((int)x, (uint32_t)y, (uint32_t)z);
        } else if (!strcmp(cmd, "SAVE") && k >= 2) {
            uint32_t seq;
            checks++;
            if (save((uint32_t)x, &seq) != 0) fail("SAVE %ld: refused", x);
            else lens[seq & 0xFFFF] = (uint32_t)x;
        } else if (!strcmp(cmd, "CUT") && k >= 3) {
            uint32_t seq = ab_store_seq(&fl) + 1;
            lens[seq & 0xFFFF] = (uint32_t)x;
            uint8_t *c = content(seq, (uint32_t)x);
            budget = y, dead = false;
            ab_store_save(&fl, c, (uint32_t)x);
            budget = -1, dead = false;           /* the reboot: power back */
            free(c);
        } else if (!strcmp(cmd, "CUTALL") && k >= 2) {
            cutall((uint32_t)x);
        } else if (!strcmp(cmd, "EXPECT") && !strcmp(a, "LOAD")) {
            char w[32];
            checks++;
            if (sscanf(line, "%*s %*s %31s", w) != 1) continue;
            uint32_t want = !strcmp(w, "NONE") ? 0 : (uint32_t)strtoul(w, NULL, 10);
            if (!load_is(want, why, sizeof why)) fail("EXPECT LOAD %s: %s", w, why);
        } else if (!strcmp(cmd, "EXPECT") && !strcmp(a, "ERASES")) {
            long s, lo, hi;
            checks++;
            if (sscanf(line, "%*s %*s %ld %ld %ld", &s, &lo, &hi) != 3 || s < 0 || s >= (long)nsec) {
                fail("EXPECT ERASES: bad arguments");
                continue;
            }
            if ((long)erases[s] < lo || (long)erases[s] > hi)
                fail("EXPECT ERASES %ld %ld %ld: %u", s, lo, hi, (unsigned)erases[s]);
        } else if (!strcmp(cmd, "EXPECT") && !strcmp(a, "MAXERASES")) {
            long hi;
            uint32_t m = 0, at = 0;
            checks++;
            sscanf(line, "%*s %*s %ld", &hi);
            for (uint32_t i = 0; i < nsec; i++)
                if (erases[i] > m) m = erases[i], at = i;
            if ((long)m > hi) fail("EXPECT MAXERASES %ld: sector %u has %u", hi, (unsigned)at, (unsigned)m);
        } else if (!strcmp(cmd, "REBOOT")) {
            /* nothing kept in RAM to drop */
        } else {
            checks++;
            line[strcspn(line, "\n")] = '\0';
            fail("not a command: %s", line);
        }
    }
    fclose(in);
    printf("%s: %d/%d expectations\n", argv[1], checks - fails, checks);
    return fails != 0;
}
