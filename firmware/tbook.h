/* tbook: unit 080, the textbook display (work/080-textbook-display.md; abacus #10848, #10907). An STU equation's
 * linear text (casim's answer, or a typed equation with its × written in, unit 031 rule 7) laid out as a textbook
 * sets it, in the face's own cells: a stacked fraction, a raised exponent one scale smaller, a radical with an
 * overbar, ABS as bars, EXP(u) as e^u, brackets sized to what they hold, and × dropped where STU's parser reads the
 * product back (031 rules 5, D, E, G; a number before E keeps it). The text is never changed: the tree is built from
 * it at each draw, into one pool, and drawn; whatever it can't draw (it doesn't parse, it nests past TB_DEPTH, the
 * pool is full, a term is wider than the line) is drawn as the text itself, wrapped at its operators.
 *
 * Sums and products are lists walked in a loop, so the recursion follows brackets, fractions, powers, radicals and
 * functions only: no corpus answer lays out past 12 levels but casim's own stress nests (33 to 44; tbook_test), and the
 * S3 measured about 254 B a level of the prototype's (the blueprint's probe). Proto-textbook 8f98ea5 was the start; this is a rewrite, not a copy. */
#ifndef TBOOK_H
#define TBOOK_H

#include <stdbool.h>
#include <stdint.h>
#include "ls027.h"

#ifndef TB_NODES
#define TB_NODES 200            /* 166 the most an answer of 191 B needs, wrapped (tbook_test's census) */
#endif
#ifndef TB_DEPTH
#define TB_DEPTH 16             /* layout levels; past them the text (no corpus answer past 12 but casim's
                                   stress nests, 33 to 44) */
#endif
#ifdef TB_MEASURE
extern int tb_depth_seen;       /* the deepest layout level reached since it was last set to 0 */
#endif
#define TB_CONDS 4

enum tb_kind { TB_NUM, TB_SYM, TB_SUM, TB_PROD, TB_ITEM, TB_DIV, TB_POW, TB_NEG, TB_FN, TB_REL, TB_ROWS };

typedef struct {
    uint8_t kind, scale, op;    /* op: an ITEM's sign in a SUM ('+', '-', 0 the first); a REL's relation */
    int16_t a, b;               /* children; -1 none. SUM/PROD: a the first ITEM. ITEM: a its term, b the next ITEM.
                                   ROWS: a a line (a SUM), b the next ROWS */
    uint16_t src, len;          /* a NUM, SYM or FN's name: its bytes in the text */
    int16_t w, h, axis;         /* the box once laid out; axis the row of a fraction bar or a minus */
} tb_node;

typedef struct {
    const char *src;
    tb_node n[TB_NODES];
    int count, root, conds[TB_CONDS], ncond;
    int err;                    /* nonzero: 1 it doesn't parse, 2 the pool is full, 3 it nests past TB_DEPTH */
} tb_tree;

/* The look the glass check decides (abacus's card (b)): a binary minus beside a fraction on the bars' row or 1 px
   above it, and two letters side by side with a 1 px gap or touching. Zero is the blueprint's proposal for each, so the
   struct is PSRAM's zeroed BSS on the devices (internal RAM +0). */
typedef struct {
    uint8_t minus_up;           /* 0: on the bars' row; 1: one pixel above it */
    uint8_t letter_touch;       /* 0: a 1 px gap between any two factors drawn side by side (3X, XY, (X+1)(X-1), X(Y+1));
                                   1: touching. Michael's card named two letters (abacus #10992) */
} tb_look;
extern tb_look tb_looks;

extern tb_tree tb_screen;        /* the screen's, rebuilt at each draw (PSRAM on the devices) */

/* Parse "answer; cond; cond" into t. 0, or -1 with t->err why. The text must outlive t. */
int tb_parse(tb_tree *t, const char *s);
/* Lay the answer out by the card's rule (stu-textbook-size): scale 3 whole, else 2 whole, else wrapped at its
   top-level + and − at 2, no line wider than maxw nor taller than maxh (a line can't scroll inside itself). Returns the
   scale, or 0 when it can't (t->err 3, or a term or a line too big). After it, t->n[t->root] is the answer's box (a
   ROWS chain when wrapped, whose lines together may be taller than maxh: they scroll). */
int tb_fit(tb_tree *t, int maxw, int maxh);
/* The lines of the laid-out answer (1 unwrapped), and line i's height, for a scroll. */
int tb_lines(const tb_tree *t);
int tb_line_h(const tb_tree *t, int i);
/* Draw lines first.. of the laid-out answer, its top-left at (x, top), no lower than bottom; returns the next line
   not drawn (tb_lines when all were). */
int tb_draw(tb_tree *t, ls027_frame *f, int x, int top, int bottom, int first);
/* The conditions' row ("; " between them), laid out at scale s: its width and height, then drawn. */
int tb_cond_fit(tb_tree *t, int s, int *h);
void tb_cond_draw(tb_tree *t, ls027_frame *f, int x, int top);

/* What is drawn as text (tutor #10917: × only where drawn, e^u, |u|, √u, a fraction a÷b), or with readback the text
   STU's parser reads back as the same value (EXP, ABS, SQRT, × where a drawn form can't be typed beside its
   neighbour): the readback oracle's. Returns the length, or -1 when it doesn't fit size. */
int tb_text(const tb_tree *t, bool readback, char *out, int size);

/* The fallback: the text's lines of at most cells cells, broken after an operator where one is in reach. Returns the
   line count; starts[i] and lens[i] each line's bytes. */
int tb_text_lines(const char *s, int cells, int starts[], int lens[], int max);

#endif
