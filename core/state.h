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
 * Left out, with why: the device's settings (budget, slice, and a test's pending interrupt count:
 * the device sets them, they survive a load); the interrupt flag and run nesting (nothing runs
 * between calls); the arena, the small
 * pools and the integral's work arrays (scratch inside one call); the label table, the literal and
 * equation-line stores' slot numbers (rebuilt from the lines). */
#ifndef STATE_H
#define STATE_H

#include <stddef.h>
#include <stdint.h>

#include "calc.h"

#define AB_STATE_VERSION 13     /* 13 (unit 041 C, B4): ENG→ / ←ENG's view; 12 (CAS 004): SUBST's replacement typed (its variable); 11 (unit 041): no new field; an older image's 33s-stamped equations that fail
                                   the 33s parse and pass the 35s one load stamped 35 (abacus #4799 (a)); */
                                /* 2 (unit 017): the 33s/35s mode, the message, entries'
                                   insertion points; 3 (unit 018): what the message says; 4: a
                                   keyboard VIEW's display (abacus #3982); 5 (unit 025): i, each
                                   label's mode, the message's mode; 6 (unit 029): STU, ALG, ANS (the
                                   history is RAM: not in the image); 7 (unit 031): each equation's
                                   model; 8 (unit 032): TABLE's settings and the open table; 9 (unit 033): GRAPH's window and the open graph; 10 (unit 040): the plot screen's cursor, zooms and tool; an older image gives what it
                                   lacks its default (its labels stamped with its own mode) */

/* Writes the image into buf when it fits in cap; returns its length either way (call with cap 0
   to size the buffer). 0 if the calculator is in the middle of a run (it never is between calls). */
size_t ab_state_save(const ab_calc *c, uint8_t *buf, size_t cap);
/* Unit 041 C (B5): the bytes label l's program takes in the image (its LBL line and the lines after it to the next
   label), MEM's LN= (33s p.12-20; our bytes, not the 33s's); 0 if there is no such label. ab_state_save(c, NULL, 0)
   is the whole image's size. */
size_t ab_state_label_bytes(const ab_calc *c, int l);

/* Replaces the state with the image's, keeping the device's settings. ABN_BAD_IMAGE, nothing
   changed, for a wrong checksum, a truncated image, a version from the future or any field out
   of range: every byte is checked before anything is written.

   The device: at first boot (no image) and after a refused load, call ab_memory_clear and show
   MEMORY CLEAR (UG p.B-3), not ab_init. ab_init keeps unit 002's ALL display so the earlier
   vectors keep their meaning; MEMORY CLEAR's default is FIX 4 (p.B-4). Ruled by abacus, #3734. */
abn_status ab_state_load(ab_calc *c, const uint8_t *buf, size_t len);

/* Whether a line (op, arg) is one an image may hold (unit 018). Program entry records only these,
   so the core never writes an image it would refuse; AB_LIT and AB_EQLINE (pool lines) are false. */
bool ab_line_ok(ab_op op, int32_t arg);

/* CRC-32 of n bytes (for tests: a golden image's identity). */
uint32_t ab_crc32(const uint8_t *p, size_t n);

#endif
