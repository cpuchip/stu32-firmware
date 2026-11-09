/* rows_test: unit 089's own checks of the worksheet's rows (work/worksheet-1-rows.md; abacus #13989 a-e, #13991).
 *
 *   build/rows_test
 *
 * 1. The census (d): every op on STU's RPN stack (CMPLX with each of its 12 operations), on four bases, with the 27
 *    letters set to distinct non-zero values and every level, letter, LASTx and ANS holding a row of its own. Each
 *    level and letter is changed by 100 in turn; a level is read when the change moves a value the op made. The
 *    table's levels read (rows.c's ab_rows_op) must be the levels the census finds read on some base, and each row
 *    made must link exactly the rows of the levels the table reads (R1's order, R2 once each).
 * 2. The invariant (d): after any op, a level whose value differs from every value before it (the levels, LASTx, ANS,
 *    the letters) carries a new row, unless the table says the op moves or clears, or a number is still being typed.
 * 3. The format: the record's layout; a save's budget, its held rows past it and its dropped count; a record longer
 *    than this build knows, skipped; the image's version (a rowless state as before, 23 with rows); the serial's end.
 * Exit status 0 when everything passes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "rows.h"
#include "state.h"
#include "opnames.h"

static int checks, fails;

static void expect(bool ok, const char *what)
{
    checks++;
    if (!ok) {
        fails++;
        printf("FAIL: %s\n", what);
    }
}

static abn num(const char *t)
{
    abn a;
    abn_from_text(&a, t);
    return a;
}

static ab_val real(const char *t)
{
    ab_val v;
    memset(&v, 0, sizeof v);
    v.kind = AB_REAL;
    v.re = num(t);
    v.im = v.p3 = abn_zero();
    return v;
}

static bool same(const ab_calc *a, const ab_val *p, const ab_calc *b, const ab_val *q)
{
    if (p->kind != q->kind) return false;
    if (p->kind == AB_EXPR || p->kind == AB_NAME) {
        const char *x = ab_expr_text(a, p), *y = ab_expr_text(b, q);
        return x && y && !strcmp(x, y);
    }
    return p->len == q->len && !memcmp(&p->re, &q->re, 16) && !memcmp(&p->im, &q->im, 16) && !memcmp(&p->p3, &q->p3, 16);
}

static ab_val *lv(ab_calc *c, int i) { return i == 0 ? &c->x : i == 1 ? &c->y : i == 2 ? &c->z : &c->t; }

/* the four bases, X Y Z T: no two values equal, none 0, none whole, in [-3, 6] for the functions' domains */
static const char *BASES[4][4] = {{"0.37", "1.93", "-2.61", "5.27"},
                                  {"-0.41", "2.73", "1.17", "-1.89"},
                                  {"0.83", "-1.37", "3.31", "0.59"},
                                  {"2.17", "0.29", "-0.73", "4.43"}};

static ab_calc c0, c1, pre;

/* base b, with level pl (0-3), the letter pv (0-26), LASTx (pl 4) or ANS (pl 5) changed by 100; -1 none */
static void setup(ab_calc *c, int b, int pl, int pv)
{
    ab_init(c);
    ab_do_arg(c, AB_MODEL, AB_MODEL_STU);
    c->x = real("1.5"), c->y = real("2.5");
    ab_do(c, AB_SIGMAPLUS);                     /* statistics to read: two points */
    c->x = real("3.25"), c->y = real("-0.5");
    ab_do(c, AB_SIGMAPLUS);
    ab_rows_clear(c);
    c->rows_next = 1;
    for (int i = 0; i < 4; i++) *lv(c, i) = real(BASES[b][i]);
    for (int i = 0; i < 27; i++) {
        char t[16];
        snprintf(t, sizeof t, "%d.%02d", 1 + i / 5, 13 + 3 * i);
        c->var[i] = real(t);
    }
    c->lastx = real("0.77");
    c->ans = real("1.61");
    if (pl >= 0 || pv >= 0) {
        ab_val *v = pv >= 0 ? &c->var[pv] : pl == 4 ? &c->lastx : pl == 5 ? &c->ans : lv(c, pl);
        abn h = num("100"), s;
        abn_add(&s, &v->re, &h);
        v->re = s;
    }
    for (int i = 0; i < 4; i++) ab_rows_test_stamp(c, lv(c, i));
    for (int i = 0; i < 27; i++) ab_rows_test_stamp(c, &c->var[i]);
    ab_rows_test_stamp(c, &c->lastx);
    ab_rows_test_stamp(c, &c->ans);
    c->lift = true;
}

static int arg_of(ab_op op)
{
    switch (op) {
    case AB_STO: case AB_RCL: case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV: case AB_RCLADD:
    case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV: case AB_XVAR: case AB_VIEW: case AB_CLVARX:
        return 0;                               /* the letter A */
    case AB_SETFIX: case AB_SETSCI: case AB_SETENG: return 4;
    default: return 0;
    }
}

/* ops that leave the stack's mode (program entry, the list, the table, the graph, another model, MEMORY CLEAR's kind):
   nothing to census, and the invariant has nothing to read */
static bool skipped(ab_op op)
{
    return op == AB_MODEL || op == AB_PRGM || op == AB_EQN || op == AB_TABLE || op == AB_GRAPH || op == AB_KEQ;
}

static int fails_census, fails_inv, ops_op, made_rows;

static void census(void)
{
    char what[256];
    for (int op = 0; op <= AB_OP_LAST; op++) {
        if (skipped((ab_op)op)) continue;
        setup(&pre, 0, -1, -1);
        if (ab_hidden(&pre, op)) continue;      /* a key STU hides (CUBE, CBRT: 041 C) does nothing there */
        int nargs = op == AB_CMPLX ? AB_CXP_N : 1;
        for (int a = 0; a < nargs; a++) {
            int arg = op == AB_CMPLX ? a : arg_of((ab_op)op);
            ab_rowop o = ab_rows_op((ab_op)op, arg);
            unsigned found[2] = {0, 0};         /* the levels read, per part, over the bases (bit 4: the source) */
            bool any_made = false;
            for (int b = 0; b < 4; b++) {
                setup(&pre, b, -1, -1);
                memcpy(&c0, &pre, sizeof c0);
                uint32_t top = c0.row_top;
                abn_status st = ab_do_arg(&c0, (ab_op)op, arg);
                bool typing = c0.entry.active || c0.cpart || c0.vlen;
                /* the invariant */
                for (int i = 0; i < 4 && !typing; i++) {
                    ab_val *v = lv(&c0, i);
                    bool old = false;
                    for (int j = 0; j < 4 && !old; j++) old = same(&c0, v, &pre, lv(&pre, j));
                    for (int j = 0; j < 27 && !old; j++) old = same(&c0, v, &pre, &pre.var[j]);
                    old = old || same(&c0, v, &pre, &pre.lastx) || same(&c0, v, &pre, &pre.ans);
                    if (old || o.kind == AB_ROWK_MOVE || o.kind == AB_ROWK_CLEAR || o.kind == AB_ROWK_RUN) continue;
                    if (v->row <= top) {
                        fails_inv++;
                        snprintf(what, sizeof what, "invariant: %s (arg %d) on base %d gives %c a new value with no "
                                 "new row (status %d)", AB_OPNAMES[op], arg, b, "XYZT"[i], (int)st);
                        expect(false, what);
                    }
                }
                if (o.kind != AB_ROWK_OP || (st != ABN_OK && st != ABN_CLAMPED)) continue;
                any_made = true;
                /* the census: each part's levels read */
                for (int part = 0; part < 2; part++) {
                    if (!(o.makes >> part & 1)) continue;
                    for (int L = 0; L < 6; L++) {
                        if (L >= 4 && !((L == 4 && o.src == AB_ROWSRC_LASTX) || (L == 5 && o.src == AB_ROWSRC_ANS)))
                            continue;
                        setup(&c1, b, L, -1);
                        ab_do_arg(&c1, (ab_op)op, arg);
                        if (!same(&c1, lv(&c1, part), &c0, lv(&c0, part))) found[part] |= L < 4 ? 1u << L : 16u;
                    }
                    if (o.src == AB_ROWSRC_VAR) {
                        setup(&c1, b, -1, arg);
                        ab_do_arg(&c1, (ab_op)op, arg);
                        if (!same(&c1, lv(&c1, part), &c0, lv(&c0, part))) found[part] |= 16u;
                    }
                    /* the row made links the table's levels' rows, deepest first, once each, three at most */
                    uint32_t want[6];
                    int nw = 0;
                    unsigned reads = op == AB_CMPLX ? ab_rows_cmplx_reads(arg, part) : o.reads;
                    for (int l = 3; l >= 0; l--)
                        if (reads >> l & 1) want[nw++] = lv(&pre, l)->row;
                    if (o.src == AB_ROWSRC_VAR) want[nw++] = pre.var[arg].row;
                    if (o.src == AB_ROWSRC_LASTX) want[nw++] = pre.lastx.row;
                    if (o.src == AB_ROWSRC_ANS) want[nw++] = pre.ans.row;
                    ab_row r;
                    bool ok = ab_rows_find(&c0, lv(&c0, part)->row, &r) && lv(&c0, part)->row > top &&
                              r.nlink == (nw > 3 ? 3 : nw) && (((r.flags & AB_ROWF_MORE) != 0) == (nw > 3));
                    for (int l = 0; ok && l < r.nlink; l++) ok = r.link[l] == want[l];
                    made_rows += ok;
                    if (!ok) {
                        fails_census++;
                        snprintf(what, sizeof what, "census: %s (arg %d) part %d on base %d: its row's links aren't the "
                                 "table's levels' rows", AB_OPNAMES[op], arg, part, b);
                        expect(false, what);
                    }
                }
            }
            if (!any_made) continue;
            ops_op++;
            for (int part = 0; part < 2; part++) {
                if (!(o.makes >> part & 1)) continue;
                unsigned table = (op == AB_CMPLX ? ab_rows_cmplx_reads(arg, part) : o.reads) |
                                 (o.src != AB_ROWSRC_NONE ? 16u : 0u);
                checks++;
                if (found[part] != table) {
                    fails++, fails_census++;
                    printf("FAIL: census: %s (arg %d) part %d: the table reads %c%c%c%c%s, the census found %c%c%c%c%s\n",
                           AB_OPNAMES[op], arg, part, table & 1 ? 'X' : '.', table & 2 ? 'Y' : '.', table & 4 ? 'Z' : '.',
                           table & 8 ? 'T' : '.', table & 16 ? "+src" : "", found[part] & 1 ? 'X' : '.',
                           found[part] & 2 ? 'Y' : '.', found[part] & 4 ? 'Z' : '.', found[part] & 8 ? 'T' : '.',
                           found[part] & 16 ? "+src" : "");
                }
            }
        }
    }
    printf("rows_test: census and invariant: %d ops make rows, %d rows' links as the table's; %d census and %d "
           "invariant failures\n", ops_op, made_rows, fails_census, fails_inv);
}

/* ---- the format ---- */

static uint8_t img[1 << 18];

static void format(void)
{
    static ab_calc c, d;
    char what[200];
    size_t n;

    /* a rowless state's image is as before: the 35s's (no row), STU with no row made */
    ab_init(&c);
    ab_do(&c, AB_K5);
    ab_do(&c, AB_ENTER);
    n = ab_state_save(&c, img, sizeof img);
    expect(n > 4 && img[4] == 20 && c.x.row == 0 && ab_rows_count(&c) == 0, "a 35s state makes no row, its image version 20");
    ab_init(&c);                                /* unit 090: four levels the 35s made, none bare, then STU: as before */
    for (int i = 1; i <= 4; i++) {
        ab_do(&c, (ab_op)(AB_K0 + i));
        if (i < 4) ab_do(&c, AB_ENTER);
    }
    ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
    n = ab_state_save(&c, img, sizeof img);
    expect(n > 4 && img[4] == 25 && !c.x.bare && !c.t.bare,
           "STU with no row made and no level bare: version 25 (unit 096: an STU image always holds T_ROUNDED; 20 before)");
    ab_init(&c);
    ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
    n = ab_state_save(&c, img, sizeof img);
    expect(n > 4 && img[4] == 25 && c.y.bare && c.t.bare, "unit 090: a fresh STU's levels are bare: version 25 (T_BARE; 096's T_ROUNDED)");
    ab_do(&c, AB_K5);
    ab_do(&c, AB_ENTER);
    n = ab_state_save(&c, img, sizeof img);
    expect(n > 4 && img[4] == 25 && c.x.row == 1 && c.y.row == 1 && !c.y.bare && c.z.bare,
           "a row: X and ENTER's copy hold row 1, Z and T still bare (version 25, 096's; 24 before)");
    {                                           /* and with no level bare, a row alone is version 23, as 089's */
        static ab_calc d;
        ab_init(&d);
        ab_do_arg(&d, AB_MODEL, AB_MODEL_STU);
        ab_do(&d, AB_K5);
        for (int i = 0; i < 3; i++) ab_do(&d, AB_ENTER);
        n = ab_state_save(&d, img, sizeof img);
        expect(n > 4 && img[4] == 25 && !d.t.bare, "no level bare, a row held: version 25 in STU (096's T_ROUNDED; 23 before)");
        ab_init(&d);                            /* (096: a 35s image is as before: the mark is STU's) */
        ab_do(&d, AB_K5);
        for (int i = 0; i < 3; i++) ab_do(&d, AB_ENTER);
        n = ab_state_save(&d, img, sizeof img);
        expect(n > 4 && img[4] < 25, "unit 096: a 35s image holds no T_ROUNDED: below version 25");
    }

    /* the record's layout: 5 ENTER 3 + makes NUM 5, NUM 3, ADD:1,2 */
    ab_do(&c, AB_K3);
    ab_do(&c, AB_ADD);
    ab_row r;
    expect(ab_rows_count(&c) == 3 && ab_rows_get(&c, 1, &r) && r.serial == 3 && r.op == AB_ADD && r.nlink == 2 &&
               r.link[0] == 1 && r.link[1] == 2 && r.tlen == 0 && r.vlen == 22,
           "ADD's record: serial 3, links 1 then 2, no text, a real's 22 B");
    expect(ab_rows_get(&c, 2, &r) && r.op == AB_ROW_OP_NUM && r.tlen == 1 && r.text[0] == '3' && r.nlink == 0,
           "the typed 3's record: NUM, its keys as its text");
    expect(c.rows_len == (13 + 1 + 1 + 22) * 2 + (13 + 8 + 1 + 22), "the page's bytes: 37 + 37 + 44");

    /* the save: the page round trip, and UNDO's image without it */
    n = ab_state_save_rows(&c, img, sizeof img);
    ab_init(&d);
    expect(ab_state_load(&d, img, n) == ABN_OK && ab_rows_count(&d) == 3 && d.x.row == 3 && d.rows_next == 4,
           "the device's save loads back with its page, X's row and the next serial");
    size_t u = ab_state_save(&c, img, sizeof img);
    ab_init(&d);
    expect(ab_state_load(&d, img, u) == ABN_OK && ab_rows_count(&d) == 0 && d.x.row == 0,
           "UNDO's image holds no page: a fresh calculator gets no rows and X none");
    expect(u + (13 + 1 + 1 + 22) * 2 + (13 + 8 + 1 + 22) + 6 + 12 == n,
           "the page costs the save its records and its section's 18 B, and UNDO's image none of it");

    /* the budget, its held row and the dropped count: 7 STO A, then 150 × (1 +): 301 rows, 12,187 B */
    ab_init(&c);
    ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
    ab_do(&c, AB_K7);
    ab_do_arg(&c, AB_STO, 0);
    for (int i = 0; i < 150; i++) ab_do(&c, AB_K1), ab_do(&c, AB_ADD);
    expect(ab_rows_count(&c) == 301 && c.rows_len == 37 + 150 * (37 + 44), "301 rows in 12,187 B");
    n = ab_state_save_rows(&c, img, sizeof img);
    ab_init(&d);
    expect(ab_state_load(&d, img, n) == ABN_OK, "the budget's save loads");
    expect(ab_rows_count(&d) == 203 && d.rows_dropped == 98 && d.var[0].row == 1 && ab_rows_find(&d, 1, &r) &&
               ab_rows_get(&d, 202, &r) && r.serial == 100 && d.x.row == 301,
           "kept: the newest 202 within 8,192 B (rows 100 to 301, 8,181 B) and A's row 1 past it; 98 dropped");
    snprintf(what, sizeof what, "the save's page section: %zu B", n);
    expect(n < 8192 + 37 + 2048, what);

    /* a record longer than this build knows (a later unit's field after the value) is read, its tail skipped */
    {
        static uint8_t rec[64];
        ab_init(&c);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_do(&c, AB_K5);
        ab_do(&c, AB_ENTER);
        memcpy(rec, c.rows, c.rows_len);
        size_t len = c.rows_len;
        rec[len] = 0xAB, rec[len + 1] = 0xCD;       /* two bytes a later build appends */
        rec[0] = (uint8_t)(len + 2);
        bool parsed = len > 0 && ab_rows_parse(rec, len + 2, &r);
        expect(parsed && r.serial == 1 && r.vlen == 22 + 2, "a longer record parses");
        ab_val v;
        char t[AB_EQN_LEN];
        expect(parsed && !ab_val_decode(r.val, r.vlen, &v, t, sizeof t) && ab_val_decode(r.val, 22, &v, t, sizeof t),
               "its value is read by its own length: the tail is past it");
    }

    /* the serial's end (abacus #13990 a): 0xFFFFFFFE is the last; the next row starts the page again from 1 */
    ab_init(&c);
    ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
    ab_do(&c, AB_K5);
    ab_do(&c, AB_ENTER);
    ab_rows_set_next(&c, 0xFFFFFFFEu);
    ab_do(&c, AB_K3);
    ab_do(&c, AB_ADD);
    expect(ab_rows_count(&c) == 1 && ab_rows_get(&c, 1, &r) && r.serial == 1 && r.op == AB_ADD && r.nlink == 0 &&
               c.rows_dropped == 2 && c.x.row == 1 && c.y.row == 0,
           "the wrap: 3 took 0xFFFFFFFE, then + restarted the page: one row, serial 1, no links, 2 dropped");
    /* links far back are serials: across 65,535 and 65,536 */
    for (uint32_t gap = 65535; gap <= 65536; gap++) {
        ab_init(&c);
        ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
        ab_do(&c, AB_K5);
        ab_do_arg(&c, AB_STO, 0);
        ab_rows_set_next(&c, 1 + gap);
        ab_do_arg(&c, AB_RCL, 0);
        snprintf(what, sizeof what, "RCL A %lu rows after A's STO links row 1", (unsigned long)gap);
        expect(ab_rows_get(&c, 1, &r) && r.serial == 1 + gap && r.nlink == 1 && r.link[0] == 1, what);
        n = ab_state_save_rows(&c, img, sizeof img);
        ab_init(&d);
        expect(ab_state_load(&d, img, n) == ABN_OK && ab_rows_get(&d, 1, &r) && r.link[0] == 1 && d.x.row == 1 + gap,
               "and saved and loaded, the link and X's row as they were");
    }

    /* a page past the RAM's 64 KB drops its oldest, and counts them */
    ab_init(&c);
    ab_do_arg(&c, AB_MODEL, AB_MODEL_STU);
    int batches = 0;
    uint32_t smallest = AB_ROWS_RAM;
    for (int i = 0; i < 1000; i++) {
        uint32_t d = c.rows_dropped, len = c.rows_len;
        ab_do(&c, AB_K1);
        if (c.rows_dropped != d) batches++, smallest = len - c.rows_len < smallest ? len - c.rows_len + 37 : smallest;
        d = c.rows_dropped, len = c.rows_len;
        ab_do(&c, AB_ADD);
        if (c.rows_dropped != d) batches++, smallest = len - c.rows_len < smallest ? len - c.rows_len + 44 : smallest;
    }
    snprintf(what, sizeof what, "2,000 rows past 64 KB: %d kept in %lu B, %lu dropped in %d batches, the least %lu B",
             ab_rows_count(&c), (unsigned long)c.rows_len, (unsigned long)c.rows_dropped, batches, (unsigned long)smallest);
    expect(c.rows_len <= AB_ROWS_RAM && (uint32_t)ab_rows_count(&c) + c.rows_dropped == 2000 && c.rows_dropped > 0, what);
    expect(batches >= 1 && batches <= 8 && smallest >= AB_ROWS_DROP - 64,
           "the page full drops 8 KB at once (a few batches, not one move a row: the stand-in's 5,551 us a key)");
    printf("rows_test: %s\n", what);
}

int main(void)
{
    census();
    format();
    printf("rows_test: %d/%d checks\n", checks - fails, checks);
    return fails ? 1 : 0;
}
