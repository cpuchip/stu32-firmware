/* ls027: the Sharp LS027B7DH01A memory LCD's serial format (unit 018 scope 3; docs/hw facts 1.4,
 * from Sharp section 6-5, pp. 15-18, and 6-6, p. 20). Portable C: it builds the bytes; the device
 * sends them over SPI mode 0, least significant bit first, at most 2 MHz, with SCS high around
 * the whole transfer.
 *
 * On the wire, first bit first: M0 (1: update), M1 (VCOM; EXTMODE is tied high on Rev A, so M1 is
 * sent 0 and EXTCOMIN inverts COM), M2 (all clear), 5 dummy bits; then for each line its address
 * AG0..AG7 (the line number from 1, AG0 first), its 400 pixels D1..D400, and 8 dummy bits; and 8
 * more dummy bits after the last line (16 in all, p. 16). With an LSB-first SPI that is the byte
 * stream [mode] then [address, 50 pixel bytes, 0x00] per line, then [0x00]. */
#ifndef LS027_H_INCLUDED
#define LS027_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LS027_W 400
#define LS027_H 240
#define LS027_ROW_BYTES (LS027_W / 8)

/* A frame: 240 rows of 50 bytes, pixel x of row y in bit (x % 8) of byte x / 8 (the order the
   LSB-first wire sends them: D1 first); a set bit is a dark pixel. */
typedef struct {
    uint8_t px[LS027_H][LS027_ROW_BYTES];
} ls027_frame;

/* Whether a sent 1 shows white (Sharp does not say in the text the facts doc extracted; to be
   settled on the first board: docs/hw facts 1.4 "UNVERIFIED"). The encoder inverts accordingly. */
extern bool ls027_one_is_white;

/* The update for the rows marked in dirty (NULL: every row) into out; returns the byte count, or
   the count it needs when cap is too small (at most 2 + 240 * 52). */
size_t ls027_encode(const ls027_frame *f, const bool *dirty, uint8_t *out, size_t cap);

/* apps.h v1.2 item 1 (abacus #4734): the rows of next that differ from shown, the frame last sent (NULL: none
   sent, every row), marked in dirty; returns how many. The device keeps shown (PSRAM on Rev A) and sends only those
   rows with ls027_encode. */
int ls027_dirty(const ls027_frame *shown, const ls027_frame *next, bool dirty[LS027_H]);

/* The other two commands: display mode (keep the image; 2 bytes) and all clear (2 bytes). */
size_t ls027_display_mode(uint8_t *out);
size_t ls027_all_clear(uint8_t *out);

#endif
