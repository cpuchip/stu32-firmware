/* Unit 089 (work/worksheet-1-rows.md): the worksheet's rows. Each operation that makes a value on STU's stack (or an
 * ALG line evaluated) appends a row: the op that made it, the 1-3 rows it used (the cause links), what was typed, and
 * its exact value. A value carries its row's serial (ab_val.row), so the stack's labels are each level's row.
 *
 * A record, as kept in RAM and as a save writes it (little-endian):
 *   u16 its own length | u32 serial | u16 op | i32 arg | u8 flags (bits 0 ALG, 1 more read, 2 an op's second value,
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
       AB_ROWK_RUN };   /* runs program lines or a solver: one row for X when it stops */
/* Test hook (tests/rows_test.c): a NUM row for v, its serial stamped in v; 0 if it can't be written */
uint32_t ab_rows_test_stamp(ab_calc *c, ab_val *v);
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
/* Every value in the state that holds a row (X, Y, Z, T, LASTx, ANS, the 27 letters): 33 at most */
int ab_rows_held(const ab_calc *c, uint32_t out[33]);
/* The first byte of the newest rows within budget bytes (a save keeps those, and the held rows before them) */
uint32_t ab_rows_cut(const ab_calc *c, uint32_t budget);
uint32_t ab_rows_visible_len(const ab_calc *c);    /* bytes of the rows not hidden */
/* After a load: the rows put back, and each held serial that no row has set to 0 */
void ab_rows_loaded(ab_calc *c);
void ab_rows_clear(ab_calc *c);                     /* the page emptied (MEMORY CLEAR does it by memset) */
void ab_rows_set_next(ab_calc *c, uint32_t next);   /* tests: the next serial (the wrap, a link far back) */

#endif
