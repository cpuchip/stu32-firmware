/* tb: a THROWAWAY prototype (abacus #8391, branch proto-textbook; never for main). casim's linear answer text laid out
 * as a textbook would set it, into an LS027 frame in the firmware's own 5 x 7 face: a stacked fraction with a bar, a
 * raised exponent one scale smaller, a radical with an overbar, ABS as bars, parentheses sized to what they hold.
 * Entry stays linear; this is only the display of a result.
 *
 * One pool of nodes per answer, no allocation. Parse, then lay out (sizes into the nodes), then draw. */
#ifndef TB_H
#define TB_H

#include <stdint.h>
#include "ls027.h"

#define TB_MAX_NODES 320
#define TB_MAX_CONDS 4
#define TB_MAX_LINES 8

enum tb_kind { TB_NUM, TB_SYM, TB_ADD, TB_SUB, TB_MUL, TB_DIV, TB_POW, TB_NEG, TB_FN, TB_REL, TB_LEAD, TB_ROWS };

typedef struct {
    uint8_t kind, scale, op;    /* op: TB_REL's relation, TB_LEAD's sign ('+' or '-') */
    int16_t a, b;               /* children; -1 for none (TB_ROWS: a the first line, b the next ROWS) */
    uint16_t src, len;          /* a NUM, SYM or FN's name: its bytes in the source */
    int16_t w, h, axis;         /* the box, once laid out: axis is the row of the fraction bar / the minus */
} tb_node;

typedef struct {
    uint8_t scale;              /* the answer's scale: 2 or 3 (1 for a shrunk line) */
    uint8_t implicit_mul;       /* 1: 3X, X ATAN(X), 2·3; 0: every × drawn */
    uint8_t exp_as_e;           /* 1: EXP(u) drawn e^u */
} tb_style;

typedef struct {
    const char *src;
    tb_node n[TB_MAX_NODES];
    int count, root, conds[TB_MAX_CONDS], ncond;
    tb_style st;
    int err;                    /* nonzero: the text didn't parse, or the pool ran out */
} tb_tree;

/* Parse "answer; cond; cond" (the conditions are relations such as X≠1). Returns 0, or -1 (t->err says why). */
int tb_parse(tb_tree *t, const char *s, const tb_style *st);
/* Lay out node n at scale s; returns its width. The answer at the style's scale is tb_layout(t, t->root, st.scale). */
int tb_layout(tb_tree *t, int n, int s);
/* Draw node n (laid out) with its top-left at x, top. */
void tb_draw(tb_tree *t, int n, ls027_frame *f, int x, int top);
/* Break the answer at its top-level + and − into lines no wider than maxw, laid out at scale s: the root becomes a
 * TB_ROWS chain, each line right-aligned when drawn. Returns the number of lines, or 0 when one term alone is wider. */
int tb_wrap(tb_tree *t, int maxw, int s);
/* The conditions as one row (", " between them) at scale s; returns its width. */
int tb_cond_layout(tb_tree *t, int s, int *h);
void tb_cond_draw(tb_tree *t, ls027_frame *f, int x, int top, int s);
/* A code point in the face at scale s (left-top x, top); returns the advance (6 s). */
int tb_glyph(ls027_frame *f, uint32_t cp, int x, int top, int s);
int tb_text(ls027_frame *f, const char *u, int x, int top, int s);   /* UTF-8; returns the x after */
int tb_text_w(const char *u, int s);

#endif
