/* base: integers in base modes and the logic functions (unit 008).
 *
 * Numbers in HEX, OCT and BIN are AB_WORD_BITS-bit two's complement integers (UG p.11-6/11-7).
 * The word size is this one constant (ours, unit 008 rule 7: 36 as the guide's calculator, kept
 * settable later).
 */
#ifndef BASE_H
#define BASE_H

#include <stdbool.h>
#include <stdint.h>

#include "abn.h"

#define AB_WORD_BITS 36

/* The integer part of x (toward zero) as a word value. A value beyond the word's range is
   replaced by the nearest end of it and *clamped is set (UG p.11-7: OVERFLOW, TOO BIG). */
abn_status base_to_word(const abn *x, int64_t *v, bool *clamped);
abn base_from_word(int64_t v);
/* Clamps an exact integer result into the word's range; sets *clamped when it had to. */
int64_t base_clamp(int64_t v, bool *clamped);
int64_t base_min(void);
int64_t base_max(void);

/* The two's complement pattern of the integer part of x in base 16, 8 or 2, without leading
   zeros, with its suffix h, o or b (UG p.11-2, p.11-6). Returns -1 when x is beyond the range. */
int base_format(const abn *x, int base, char *out, int size);

/* Parses digits ('0'-'9', 'A'-'F') in the given base as a non-negative integer; false if a digit
   is not valid in the base or the value exceeds the largest positive word. */
bool base_parse(const char *digits, int base, int64_t *v);

typedef enum { BL_AND, BL_OR, BL_XOR, BL_NAND, BL_NOR } base_logic;
int64_t base_logic2(base_logic op, int64_t y, int64_t x);
int64_t base_not(int64_t x);

#endif
