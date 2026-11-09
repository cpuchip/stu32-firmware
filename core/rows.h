/* Unit 089 (work/worksheet-1-rows.md): the worksheet's rows. Each operation that makes a value on STU's stack (or an
 * ALG line evaluated) appends a row: the op that made it, the 1-3 rows it used (the cause links), what was typed, and
 * its exact value. A value carries its row's serial (ab_val.row), so the stack's labels are each level's row.
 *
 * A record, as kept in RAM and as a save writes it (little-endian):
 *   u16 its own length | u32 serial | u16 op | i32 arg | u8 flags (bits 0 ALG, 1 more read, 2 an op's second value,
 *   3 rounded (096, RAM only),
 *   4-5 the links' count) | u32 each link's serial | u8 the text's length, the text | the value as the image writes one
 * The length goes first, so a later unit's fields after the value are skipped by this build. Serials run from 1 to
 * 0xFFFFFFFE and are never reused; at the last, the page starts again (abacus #13990 a). */
#ifndef AB_ROWS_H
#define AB_ROWS_H

#include <stddef.h>
#include <stdint.h>
#include "calc.h"

#define AB_ROW_OP_NUM 0xFFFFu       /* a typed number (R3) */
#define AB_ROW_OP_ALG 0xFFFEu       /* an ALG line */
#define AB_ROWF_ALG 0x01u
#define AB_ROWF_MORE 0x02u          /* it read more than AB_ROW_LINKS rows: the first three are linked */
#define AB_ROWF_PART2 0x04u         /* an op's second value (→POL's θ, CMPLX's imaginary part) */
#define AB_ROWF_ROUNDED 0x08u       /* unit 096: its value is rounded; in RAM only (an image carries it in T_ROUNDED) */
#define AB_ROW_SERIAL_LAST 0xFFFFFFFEu

/* A row, read from its record (pointers into the page) */
typedef struct {
    uint32_t serial;
    unsigned op;
    int32_t arg;
    unsigned flags;
    int nlink;
    uint32_t link[AB_ROW_LINKS];
    const char *text;
    size_t tlen;
    const uint8_t *val;             /* the value's bytes: ab_val_decode (state.h) reads them */
    size_t vlen;
} ab_row;

/* What an op does to the rows (the table the census checks, tests/rows_test.c): the levels it makes a value in
   (bit 0 X, 1 Y), the levels it reads (bit 0 X, 1 Y, 2 Z, 3 T), a register it reads after them, and its class */
enum { AB_ROWSRC_NONE, AB_ROWSRC_VAR, AB_ROWSRC_LASTX, AB_ROWSRC_ANS };
enum { AB_ROWK_NONE,    /* makes no value on the stack (a setting, a menu, a test): the invariant holds it to that */
       AB_ROWK_MOVE,    /* moves values, which keep their rows (ENTER, R↓, R↑, x↔y, x<>A, STO, →EQN) */
       AB_ROWK_CLEAR,   /* clears: the value is 0 with no row (CLx, CLSTK, CLEAR ALL, C; abacus #13988 lean 3) */
       AB_ROWK_OP,      /* makes a row for each level in makes */
       AB_ROWK_RUN,     /* runs program lines or a solver: one row for X when it stops */
       AB_ROWK_LETTER }; /* unit 090: makes a value in the letter arg (STO+ − × ÷): a row the letter holds, linked to the
                           letter's old row, then X's (abacus #14041 5: A is the left operand) */
/* Test hook (tests/rows_test.c): a NUM row for v, its serial stamped in v; 0 if it can't be written */
uint32_t ab_rows_test_stamp(ab_calc *c, ab_val *v);
/* Bench hook (the stand-in's ROWBENCH): a NUM row for v with the text and links given, the largest a row can be */
uint32_t ab_rows_test_append(ab_calc *c, ab_val *v, const char *text, size_t tlen, const uint32_t *link, int nlink);
typedef struct {
    unsigned makes, reads;
    int src;
    int kind;
} ab_rowop;
ab_rowop ab_rows_op(ab_op op, int arg);
/* CMPLX's levels read by each part (abacus #13991): part 0 the real, 1 the imaginary */
unsigned ab_rows_cmplx_reads(int arg, int part);

/* The core's calls (calc.c, eqn.c) */
enum { AB_ROUTE_NONE, AB_ROUTE_DO, AB_ROUTE_ALG, AB_ROUTE_EQN, AB_ROUTE_REPORT };
void ab_rows_key_begin(ab_calc *c, ab_op op, int arg);
void ab_rows_key_end(ab_calc *c, ab_op op, int arg, abn_status st);
void ab_rows_continue_end(ab_calc *c, abn_status st);
void ab_rows_num(ab_calc *c);                       /* a typed number's entry ended, its value in X (R3) */
void ab_rows_read(ab_calc *c, const ab_val *src);   /* an ALG line read src (eqn.c's take_val) */
uint32_t ab_rows_alg(ab_calc *c, const char *text, const ab_val *v);   /* an ALG line's row: its serial, or 0 */

/* Reading the page: the rows UNDO hid (serial past row_top) are not counted */
int ab_rows_count(const ab_calc *c);
bool ab_rows_get(const ab_calc *c, int k, ab_row *out);     /* k = 1 the newest */
bool ab_rows_find(const ab_calc *c, uint32_t serial, ab_row *out);
bool ab_rows_parse(const uint8_t *p, size_t n, ab_row *out); /* one record */
/* Unit 096: the mark on a row's value. After a load: the row of a serial marked (false when no row has it), or every
   row (an image older than the mark: untracked, so rounded) */
bool ab_rows_mark(ab_calc *c, uint32_t serial);
void ab_rows_mark_all(ab_calc *c);
/* Every value in the state that holds a row (X, Y, Z, T, LASTx, ANS, the 27 letters): 33 at most */
int ab_rows_held(const ab_calc *c, uint32_t out[33]);
/* The first byte of the newest rows within budget bytes (a save keeps those, and the held rows before them) */
uint32_t ab_rows_cut(const ab_calc *c, uint32_t budget);
uint32_t ab_rows_visible_len(const ab_calc *c);    /* bytes of the rows not hidden */
/* After a load: the rows put back, and each held serial that no row has set to 0 */
void ab_rows_loaded(ab_calc *c);
void ab_rows_clear(ab_calc *c);                     /* the page emptied (MEMORY CLEAR does it by memset) */
void ab_rows_set_next(ab_calc *c, uint32_t next);   /* tests: the next serial (the wrap, a link far back) */
/* CLEAR ALL (unit 090, abacus #14047): the page emptied as MEMORY CLEAR empties it, the dropped count too; the serials go
   on, never given again */
void ab_rows_clear_all(ab_calc *c);

/* Unit 090: ALG's history is the page's ALG rows (029 rule 5: every one the page holds) */
uint32_t ab_rows_alg_prev(const ab_calc *c, uint32_t before);  /* the newest ALG row older than before (0: of all), 0 none */
uint32_t ab_rows_alg_next(const ab_calc *c, uint32_t after);   /* the oldest ALG row newer than after, 0 none */
int ab_rows_alg_count(const ab_calc *c);
bool ab_rows_text(const ab_calc *c, uint32_t serial, char *out, size_t size);  /* a row's text, "" for none */
/* A row's value decoded with no slot taken (an expression's text in text, its len -1), its serial in row */
bool ab_rows_peek(const ab_calc *c, uint32_t serial, ab_val *out, char *text, size_t tcap);
/* A row's value to use, its serial in row: an expression takes a slot of the store (ABN_EXPR_FULL when it can't) */
abn_status ab_rows_value(ab_calc *c, uint32_t serial, ab_val *out);

/* Unit 090 (B′): the bare mark (calc.h). The clears set it where they write their 0 (calc.c); these clear it from every
   level an op makes, by the table, in every mode: a key at the stack (after ab_rows_key_end), a program line as it runs */
void ab_rows_bare_key(ab_calc *c, ab_op op, int arg, abn_status st);
void ab_rows_bare_line(ab_calc *c, ab_op op, int arg);

/* Unit 093 (work/worksheet-3-select.md): the selection's walk over STU's home. The levels drawn (B′: the bare ones at the
   top hidden, X always), as 090's screen draws them; the page's rows are those no drawn level holds */
int ab_rows_levels(const ab_calc *c);
uint32_t ab_rows_page_prev(const ab_calc *c, uint32_t before);    /* the newest page row older than before (0: of all) */
uint32_t ab_rows_page_next(const ab_calc *c, uint32_t after);     /* the oldest page row newer than after, 0 none */
/* COPY's row: X's value (just placed, the stack lifted) linked to the row src, X holding it */
void ab_rows_copy(ab_calc *c, uint32_t src);
/* The screen's window over the page, as drawn now: its top and bottom rows' serials (0 0 with no page line). The screen
   registers it (app_init, the host's runners); the selection asks it whether a move leaves the window */
typedef void (*ab_rows_window_fn)(const ab_calc *c, uint32_t *top, uint32_t *bottom);
void ab_rows_set_window(ab_rows_window_fn fn);
bool ab_rows_window(const ab_calc *c, uint32_t *top, uint32_t *bottom);    /* false: none registered */

#endif
