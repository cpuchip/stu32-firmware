/* screen: layout v0 (unit 018 scope 7), the calculator's state drawn into an LS027 frame.
 *
 * Portable C, no platform calls: the device sends the frame with ls027_encode; the host test draws
 * the same frames into image files. The face is ours (font5x7.txt), scaled by whole numbers.
 *
 * The layout (ours, v0; 400 x 240):
 *   rows 0..17   the status band: the shift held, RAD or GRAD, HEX OCT or BIN, PRGM, EQN, the
 *                fraction indicator (up or down triangle), flags 0..4 that are set, the mode (33 or
 *                35, unit 017), the battery (a plus over it while charging)
 *   rows 19..20  a rule
 *   T, Z, Y      scale 2, right aligned, their letter at the left
 *   X            scale 4 when it fits 15 cells, else 3, else 2 (a window around the cursor)
 *   rows 206..   the six soft keys' labels, each in a box over its key
 * What the X line shows, first that applies: the op running (SOLVING ...), the error message,
 * the number being typed (with its cursor), in program entry the pointer's address, in Equation
 * mode the equation being typed or shown, else X. A prompt (INPUT A?) or a VIEW (A=) takes the
 * Y line. */
#ifndef SCREEN_H
#define SCREEN_H

#include <stdbool.h>
#include <stdint.h>
#include "calc.h"
#include "font5x7.h"
#include "ls027.h"

#define SCREEN_SOFT 6

/* What the device knows that the core does not. */
typedef struct {
    int shift;              /* 0 none, 1 gold, 2 blue (the layout's two shift keys) */
    int battery;            /* charge in percent, 0..100; -1 not read yet */
    bool charging;
    int busy;               /* 0, or the op running in slices: AB_SOLVE, AB_INTEG, AB_XEQ, AB_RS */
    char radix;             /* '.' or ',': the device's setting, as the core leaves it */
    bool separator;         /* thousands separator */
    const char *soft[SCREEN_SOFT];  /* the soft keys' labels (UTF-8); NULL or "" for none */
} screen_ui;

void screen_ui_default(screen_ui *ui);

/* The whole frame for c and ui. */
void screen_render(const ab_calc *c, const screen_ui *ui, ls027_frame *f);

/* Drawing, clipped to the frame. A set pixel is dark. */
void screen_clear(ls027_frame *f);
void screen_fill(ls027_frame *f, int x, int y, int w, int h, bool dark);
/* UTF-8 text with its top left at (x, y), each cell 6 * scale wide; returns the cells drawn. A
   code point the face lacks is drawn as a hollow box. */
int screen_text(ls027_frame *f, int x, int y, int scale, const char *s, bool dark);

const font_glyph *screen_glyph(uint32_t cp);    /* NULL when the face has none */
extern long screen_missed;                      /* hollow boxes drawn so far (a test reads it) */
int screen_cells(const char *s);                /* code points in UTF-8 text */
int screen_missing(const char *s);              /* of those, the ones the face lacks */

#endif
