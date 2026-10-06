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
 * What the X line shows, first that applies: the op running (SOLVING ...), a prompt waiting for
 * its argument (STO _), the error message,
 * the number being typed (with its cursor), in program entry the pointer's address and line, in Equation
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
    const char *prompt;     /* an op waiting for its argument ("STO _", app.h), NULL none */
    const char *notice;     /* a brief notice in the X line (a base result clamped: OVERFLOW, unit 024),
                               NULL none */
    char radix;             /* '.' or ',': the device's setting, as the core leaves it */
    char separator;         /* digit grouping, as ab_fmt's: 0 none, 1 the guide's mark, ' ' a space */
    const char *soft[SCREEN_SOFT];  /* the soft keys' labels (UTF-8); NULL or "" for none */
    bool soft_on[SCREEN_SOFT];      /* a label marked as the setting in effect (unit 019: MODE) */
    bool soft_sub[SCREEN_SOFT];     /* a label that opens a submenu: its box marked (abacus #3958) */
    char clock[8];                  /* the status band's clock ("12:59", "--:--"); "" none (unit 027) */
    char ampm;                      /* 'a' or 'p' after a 12-hour clock, 0 none */
    const char *view[4];            /* a view in place of the stack (SYS, unit 027): its lines, NULL none */
} screen_ui;

void screen_ui_default(screen_ui *ui);

/* The screen as text (abacus #3971): each line's text and kind, from the rules screen_render draws
 * by (it draws from these), so a host that draws its own lines (the app, the 10" panel) shows what
 * the device shows. A line is whole: fitting it to a width (screen_render's scale and its ellipsis
 * window around the cursor) is the host's. */
#define SCREEN_TEXT_MAX 1024

typedef enum {
    SCREEN_VALUE = 0,       /* a register's value */
    SCREEN_ENTRY,           /* the number being typed, with its cursor */
    SCREEN_EQN,             /* an equation typed (with its cursor) or shown, or EQN LIST TOP */
    SCREEN_PROGRAM,         /* program entry: the pointer's address and line, or PRGM TOP */
    SCREEN_MESSAGE,         /* the op running (RUNNING...), or the error message */
    SCREEN_PROMPT,          /* an op waiting for its argument (X); INPUT's A? or VIEW's A= (Y) */
    SCREEN_VIEW             /* a keyboard VIEW's Variable=Value in X (UG p.3-4), until the next key */
} screen_kind;

typedef struct {
    char text[SCREEN_TEXT_MAX];     /* UTF-8; a cursor is the '_' in it */
    int cursor;                     /* the cursor's cell (screen_cells counting), -1 none */
    screen_kind kind;
    bool left;                      /* drawn from the left, else right aligned */
    const char *label;              /* the letter at its left ("T", "Z", "Y", "X"), NULL none */
} screen_line;

typedef struct {
    int shift;                      /* 0, 1 GOLD, 2 BLUE */
    const char *angle;              /* "RAD", "GRAD", NULL in degrees */
    const char *base;               /* "HEX", "OCT", "BIN", NULL in decimal */
    bool prgm, eqn;
    int frac;                       /* the fraction indicator: 1 up, -1 down, 0 none */
    char flags[6];                  /* the flags 0-4 that are set, as digits */
    const char *model;              /* "33" or "35" */
    const char *clock;              /* the clock as the band shows it, NULL none (unit 027) */
    int battery;                    /* percent, -1 not read */
    bool charging;
    char text[64];                  /* the annunciators in order, space-separated */
} screen_status;

typedef struct {
    screen_status status;
    screen_line t, z, y, x;
    const char *soft[SCREEN_SOFT];  /* as screen_ui's */
    bool soft_on[SCREEN_SOFT], soft_sub[SCREEN_SOFT];
} screen_page;

void screen_lines(const ab_calc *c, const screen_ui *ui, screen_page *out);

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
int screen_cells(const char *s);                /* cells in UTF-8 text: a combining mark takes none */
int screen_missing(const char *s);              /* of those, the ones the face lacks */
/* For drawing elsewhere (the stand-in's keys): text as cells (a combining mark folded into its
   letter), and a cell as 8 rows of 5 bits, bit 4 the left column: row 0 is the mark's own row
   above the glyph, rows 1..7 the glyph (a hollow box when the face lacks it). */
int screen_decode(const char *s, uint32_t *cells, int max);
void screen_cell_bitmap(uint32_t cell, uint8_t rows[8]);

#endif
