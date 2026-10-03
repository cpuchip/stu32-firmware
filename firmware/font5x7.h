/* font5x7: our own 5 x 7 face (see font5x7.c). */
#ifndef FONT5X7_H
#define FONT5X7_H

#include <stdint.h>

typedef struct {
    uint32_t cp;            /* the code point */
    uint8_t rows[7];        /* bit 4 the left column */
} font_glyph;

extern const font_glyph FONT5X7[];
extern const int FONT5X7_COUNT;

#endif
