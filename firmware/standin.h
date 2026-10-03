/* standin: the 4.3" S3 board standing in for Rev A (unit 018 device acceptance): its 800 x 480
 * screen shows the LS027's 400 x 240 frame and a drawn keyboard that its touch panel presses.
 * Portable C: what is drawn and where a touch lands; the board code (device/standin/) only moves
 * the lines to the RGB panel and reads the touch controller.
 *
 * The arrangement (abacus #3860, #3861):
 * - left half: the frame at x1, 400 x 240, rows 120..359; a tap on it shows the x2 view;
 * - right half: the 43 keys, 8 rows of 60 px (the layout's rows, stu32-v0.json), each row's keys
 *   sharing its 400 px by width units (ENTER is two of row 3's six). On each key: its face legend,
 *   the gold legend above at its left and the blue at its right, the faceplate letter (decision 25,
 *   keymap.h's letter layer) small at its lower right. While a prompt waits the letter layer is
 *   drawn instead: each key shows its letter or role large;
 * - the x2 view: the frame at x2 over the whole screen; any tap returns.
 * The keyboard is a 4-bit indexed image (si_draw_keys); si_line composes one 800-pixel line of
 * RGB565 from it and the frame, with tables only, so it can run in the RGB driver's interrupt. */
#ifndef STANDIN_H
#define STANDIN_H

#include <stdbool.h>
#include <stdint.h>

#include "ls027.h"

#define SI_W 800
#define SI_H 480
#define SI_KEYS_X 400                           /* the keyboard's left edge */
#define SI_ROW_H 60
#define SI_FRAME_Y 120                          /* the frame's top row at x1 */
#define SI_IMG_W 400                            /* the keyboard image: 4 bits a pixel */
#define SI_IMG_BYTES (SI_IMG_W / 2 * SI_H)

enum {                                          /* the palette (si_line's colours) */
    SI_BG = 0, SI_KEY, SI_KEY_DOWN, SI_FACE, SI_GOLD, SI_BLUE, SI_LETTER, SI_EDGE, SI_LCD_DARK, SI_LCD_LIGHT,
    SI_COLOURS
};

typedef struct {
    int shift;                                  /* 0, 1 gold, 2 blue: the shift held (its key lit) */
    bool letters;                               /* a prompt waits: the letter layer */
    int down;                                   /* the key held, -1 none */
} si_keys_state;

typedef struct {
    const ls027_frame *frame;                   /* the frame to show */
    const uint8_t *keys;                        /* the keyboard image, SI_IMG_BYTES */
    bool x2;                                    /* the x2 view */
} si_view;

/* Where key k is (x, y, w, h on the screen); false for a number that is not a key. */
bool si_key_rect(int key, int *x, int *y, int *w, int *h);
/* The key under a touch at (x, y), -1 for none (the frame, or between keys). */
int si_key_at(int x, int y);
/* Whether a touch at (x, y) is on the frame (in the x1 view). */
bool si_on_frame(int x, int y);

void si_draw_keys(uint8_t *img, const si_keys_state *s);
void si_line(const si_view *v, int y, uint16_t *out);
extern const uint16_t SI_PALETTE[SI_COLOURS];
void si_init(void);                             /* builds si_line's tables (once, before it runs) */

#endif
