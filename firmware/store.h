/* store: unit 016's state image kept in flash, safe against a power cut (unit 018, scope 6), in a ring over the whole
 * partition since unit 092 (work/092-store-wear.md). Portable C over a small flash interface, so the same code runs on
 * the host against a simulated flash (tests/store_test.c cuts the power at every byte, tests/store_sim.c is the hidden
 * set's tool) and on the device against an ESP-IDF partition.
 *
 * Each image: a 32-byte header (magic "ABST", sequence number, payload length, CRC-32 of the payload, CRC-32 of the
 * header's first 16 bytes, padding) at a sector's start, and the image after it, running on past the partition's end
 * at its start. A save starts at the sector after the newest good image's last: erase the sectors it takes, write the
 * image, write the header last. An image is at most half the partition, so the new one never reaches the newest, and a
 * cut at any moment leaves the newest that finished. A load reads every sector-start header and takes the good image
 * with the highest sequence number (an older firmware's two-slot images are found the same way); with none, the device
 * does MEMORY CLEAR. The partition is 2 × slot_size (slot_size: the old layout's slot, still each image's most). */
#ifndef STORE_H
#define STORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Unit 092: where the newest image is (its header's sector, sequence number, length and CRC), kept in RAM after a load or
   a save, so a save needn't scan every sector's header again (lean 3; on the stand-in's flash a scan is about 12 ms). A
   save reads the remembered header back before it trusts it, so a partition erased or written behind the store's back
   (the console's WIPE) costs one scan, never a wrong place. */
typedef struct {
    uint32_t seq, len, crc, at;
    bool valid;
} ab_store_ring;

typedef struct {
    void *ctx;
    uint32_t sector;        /* erase unit, bytes (4096 on the S3's flash) */
    uint32_t slot_size;     /* bytes per slot, a multiple of sector; two slots follow each other */
    /* each returns 0 on success; offsets are within the partition */
    int (*erase)(void *ctx, uint32_t off, uint32_t len);
    int (*write)(void *ctx, uint32_t off, const void *src, uint32_t len);
    int (*read)(void *ctx, uint32_t off, void *dst, uint32_t len);
    ab_store_ring *ring;    /* unit 092: the partition's remembered newest (one per partition, shared by every ab_flash over
                               it: SAVEBENCH's timing wrapper copies it); NULL: every save scans */
} ab_flash;

#define AB_STORE_HEADER 32

/* Writes image (len bytes) after the newest good image, at most slot_size - AB_STORE_HEADER. 0 on success. */
int ab_store_save(const ab_flash *f, const uint8_t *image, uint32_t len);

/* apps.h v1.2's migration (the old 8 KB second slot inside today's first) saved that image again into the two-slot
   layout. Since unit 092 the load's scan finds it where it is, so this does nothing and returns 0; kept for its
   callers. */
int ab_store_migrate(const ab_flash *f, uint32_t old_slot_size, uint8_t *buf, uint32_t cap);

/* The newest good image's sequence number, 0 if there is none (unit 092: store_sim's images are a function of it). */
uint32_t ab_store_seq(const ab_flash *f);

/* Reads the newest good image into buf (cap bytes): its length, or 0 if there is none (or it does not fit). An image is
   good when its header and payload CRCs match. */
uint32_t ab_store_load(const ab_flash *f, uint8_t *buf, uint32_t cap);

#endif
