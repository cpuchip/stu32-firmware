/* state: the calculator's state as a byte image, for Continuous Memory (UG p.1-1; unit 016).
 *
 * The image is versioned and independent of the compiler's layout and of the number engine's
 * encoding: little-endian, tagged sections, numbers as (sign, exponent, coefficient), program
 * lines by frozen operation codes with their literals and equation texts inline, everything
 * stored by content (only the lines, equations and registers in use). A later firmware reads an
 * older image, anything the image lacks taking its MEMORY CLEAR default. CRC-32 (IEEE 802.3,
 * reflected, polynomial 0xEDB88320) over everything before it closes the image.
 *
 *   "ABSI" version:u16 0:u16 length:u32 { tag:u16 size:u32 payload } crc:u32
 *
 * Left out, with why: the device's settings (budget, slice: the device sets them, they survive a
 * load); the interrupt flag and run nesting (nothing runs between calls); the arena, the small
 * pools and the integral's work arrays (scratch inside one call); the label table, the literal and
 * equation-line stores' slot numbers (rebuilt from the lines). */
#ifndef STATE_H
#define STATE_H

#include <stddef.h>
#include <stdint.h>

#include "calc.h"

#define AB_STATE_VERSION 2      /* 2 (unit 017): the 33s/35s mode, the message, entries'
                                   insertion points; a version-1 image gives them their defaults */

/* Writes the image into buf when it fits in cap; returns its length either way (call with cap 0
   to size the buffer). 0 if the calculator is in the middle of a run (it never is between calls). */
size_t ab_state_save(const ab_calc *c, uint8_t *buf, size_t cap);

/* Replaces the state with the image's, keeping the device's settings. ABN_BAD_IMAGE, nothing
   changed, for a wrong checksum, a truncated image, a version from the future or any field out
   of range: every byte is checked before anything is written.

   The device: at first boot (no image) and after a refused load, call ab_memory_clear and show
   MEMORY CLEAR (UG p.B-3), not ab_init. ab_init keeps unit 002's ALL display so the earlier
   vectors keep their meaning; MEMORY CLEAR's default is FIX 4 (p.B-4). Ruled by abacus, #3734. */
abn_status ab_state_load(ab_calc *c, const uint8_t *buf, size_t len);

/* CRC-32 of n bytes (for tests: a golden image's identity). */
uint32_t ab_crc32(const uint8_t *p, size_t n);

#endif
