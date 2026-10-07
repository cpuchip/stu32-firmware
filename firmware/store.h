/* store: unit 016's state image kept in flash, two slots, safe against a power cut (unit 018,
 * scope 6). Portable C over a small flash interface, so the same code runs on the host against
 * a simulated flash (tests/store_test.c cuts the power at every byte) and on the device against
 * an ESP-IDF partition.
 *
 * Each slot: a 32-byte header (magic "ABST", sequence number, payload length, CRC-32 of the
 * payload, CRC-32 of the header's first 16 bytes, padding) and the image after it. A save goes
 * to the slot that does not hold the newest good image: erase it, write the image, write the
 * header last. A cut at any moment leaves at most that slot bad; the other is never touched, so
 * one good image always remains (the newest that finished, or the one before). A load takes the
 * good slot with the higher sequence number; with none, the device does MEMORY CLEAR. */
#ifndef STORE_H
#define STORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    void *ctx;
    uint32_t sector;        /* erase unit, bytes (4096 on the S3's flash) */
    uint32_t slot_size;     /* bytes per slot, a multiple of sector; two slots follow each other */
    /* each returns 0 on success; offsets are within the partition */
    int (*erase)(void *ctx, uint32_t off, uint32_t len);
    int (*write)(void *ctx, uint32_t off, const void *src, uint32_t len);
    int (*read)(void *ctx, uint32_t off, void *dst, uint32_t len);
} ab_flash;

#define AB_STORE_HEADER 32

/* Writes image (len bytes) to the slot the newest good image is not in. 0 on success. */
int ab_store_save(const ab_flash *f, const uint8_t *image, uint32_t len);

/* After a partition's slots grew in place (apps.h v1.2: the apps' saves, 8 KB to 36 KB slots), the old second slot
   (at old_slot_size) lies inside the new first. A good image there newer than every good slot now is saved again into
   the new layout (buf, cap: room to read it). 1 if one moved, 0 if none, -1 on a failure. Call before loading. */
int ab_store_migrate(const ab_flash *f, uint32_t old_slot_size, uint8_t *buf, uint32_t cap);

/* Reads the newest good image into buf (cap bytes): its length, or 0 if no slot holds one (or
   it does not fit). A slot is good when its header and payload CRCs match. */
uint32_t ab_store_load(const ab_flash *f, uint8_t *buf, uint32_t cap);

#endif
