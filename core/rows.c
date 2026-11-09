/* Unit 089: the worksheet's rows (rows.h; work/worksheet-1-rows.md, rulings abacus #13989, #13991). */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "rows.h"
#include "state.h"

/* The row field sits in what was padding after kind, and unit 090's bare mark in the padding after len: an ab_val is no
   larger for them (on both boards) */
typedef struct {
    ab_kind kind;
    abn re, im, p3;
    int len;
} val_without_row;
_Static_assert(sizeof(ab_val) == sizeof(val_without_row), "ab_val grew with its row field or its bare mark");

#define REC_HEAD 13u                /* length 2, serial 4, op 2, arg 4, flags 1 */

static uint32_t g32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static unsigned g16(const uint8_t *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }

static void p16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }

static void p32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

/* ---- the table (checked by the census and the invariant, tests/rows_test.c) ---- */

ab_rowop ab_rows_op(ab_op op, int arg)
{
    ab_rowop o = {0, 0, AB_ROWSRC_NONE, AB_ROWK_NONE};
    (void)arg;
    switch (op) {
    /* one value from X */
    case AB_KCHS: case AB_EXP: case AB_LN: case AB_TENX: case AB_LOG: case AB_SQ: case AB_SQRT: case AB_INV:
    case AB_SIN: case AB_COS: case AB_TAN: case AB_ASIN: case AB_ACOS: case AB_ATAN: case AB_SINH: case AB_COSH:
    case AB_TANH: case AB_ASINH: case AB_ACOSH: case AB_ATANH: case AB_HMS: case AB_HMSINV: case AB_TODEG:
    case AB_TORAD: case AB_TOKG: case AB_TOLB: case AB_TOC: case AB_TOF: case AB_TOCM: case AB_TOIN: case AB_TOL:
    case AB_TOGAL: case AB_TOKM: case AB_TOMILE: case AB_FACT: case AB_IP: case AB_FP: case AB_INTG: case AB_RND:
    case AB_ABS: case AB_SGN: case AB_NOT: case AB_ARG: case AB_XHAT: case AB_YHAT: case AB_EXPM1: case AB_LN1P:
    case AB_TWOX: case AB_LOG2: case AB_CUBE: case AB_CBRT: case AB_CASSIMP: case AB_CASEXP:
        o.makes = 1, o.reads = 1, o.kind = AB_ROWK_OP;
        break;
    /* one value from Y and X */
    case AB_ADD: case AB_SUB: case AB_MUL: case AB_DIV: case AB_IDIV: case AB_RMDR: case AB_POW: case AB_XROOT:
    case AB_PCT: case AB_PCTCHG: case AB_NCR: case AB_NPR: case AB_AND: case AB_XOR: case AB_OR: case AB_NAND:
    case AB_NOR:
        o.makes = 1, o.reads = 3, o.kind = AB_ROWK_OP;
        break;
    /* two values from Y and X */
    case AB_TOPOL: case AB_TOREC:
        o.makes = 3, o.reads = 3, o.kind = AB_ROWK_OP;
        break;
    case AB_CMPLX:                              /* two values, each part its own levels (ab_rows_cmplx_reads) */
        o.makes = 3, o.reads = 15, o.kind = AB_ROWK_OP;
        break;
    /* one value from no level: a constant, a register, a statistic */
    case AB_PI: case AB_CONST: case AB_RAND: case AB_RCLC: case AB_NSUM: case AB_SUMX: case AB_SUMY: case AB_SUMX2:
    case AB_SUMY2: case AB_SUMXY: case AB_XBAR: case AB_YBAR: case AB_XWBAR: case AB_SX: case AB_SY: case AB_PSX:
    case AB_PSY: case AB_CORR: case AB_SLOPE: case AB_YINT: case AB_SIGMAPLUS: case AB_SIGMAMINUS:
        o.makes = 1, o.kind = AB_ROWK_OP;
        break;
    /* a recall: a row linked to the value's (R4) */
    case AB_RCL:
        o.makes = 1, o.src = AB_ROWSRC_VAR, o.kind = AB_ROWK_OP;
        break;
    case AB_RCLADD: case AB_RCLSUB: case AB_RCLMUL: case AB_RCLDIV:
        o.makes = 1, o.reads = 1, o.src = AB_ROWSRC_VAR, o.kind = AB_ROWK_OP;
        break;
    case AB_LASTX:
        o.makes = 1, o.src = AB_ROWSRC_LASTX, o.kind = AB_ROWK_OP;
        break;
    /* (ANS on RPN's stack does nothing, 029: ALG's line reads it, and its row links ANS's through take_val) */
    /* program lines and solvers from the keyboard: one row when they stop */
    case AB_XEQ: case AB_RS: case AB_SST: case AB_SOLVE: case AB_INTEG:
        o.makes = 1, o.kind = AB_ROWK_RUN;
        break;
    case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:      /* (unit 090: was a move) */
        o.src = AB_ROWSRC_VAR, o.kind = AB_ROWK_LETTER;
        break;
    case AB_ENTER: case AB_RDN: case AB_RUP: case AB_XY: case AB_XVAR: case AB_STO: case AB_EQSTORE:
        o.kind = AB_ROWK_MOVE;
        break;
    case AB_CLX: case AB_CLSTK: case AB_CLALL: case AB_CANCEL: case AB_KBKSP: case AB_CLVARS: case AB_CLVARX:
        o.kind = AB_ROWK_CLEAR;
        break;
    default:
        break;
    }
    return o;
}

/* CMPLX on z1 = Z + iT and z2 = X + iY (33s p.9-3), from its census (work/worksheet-evidence/census-cmplx.txt) */
unsigned ab_rows_cmplx_reads(int arg, int part)
{
    switch (arg) {
    case AB_CXP_ADD: case AB_CXP_SUB: return part ? 2u | 8u : 1u | 4u;     /* Y and T; X and Z */
    case AB_CXP_MUL: case AB_CXP_DIV: case AB_CXP_POW: return 15;
    case AB_CXP_CHS: return part ? 2u : 1u;
    default: return 3;                                                    /* the one-number ones: X and Y */
    }
}

/* ---- the page ---- */

bool ab_rows_parse(const uint8_t *p, size_t n, ab_row *out)
{
    ab_row r;
    size_t len, at, nl;
    if (n < REC_HEAD + 1) return false;
    len = g16(p);
    if (len > n || len < REC_HEAD + 1) return false;
    memset(&r, 0, sizeof r);
    r.serial = g32(p + 2);
    r.op = g16(p + 6);
    r.arg = (int32_t)g32(p + 8);
    r.flags = p[12];
    nl = (r.flags >> 4) & 3u;
    if (r.serial == 0 || r.serial > AB_ROW_SERIAL_LAST || nl > AB_ROW_LINKS || (r.flags & 0xC8u)) return false;
    at = REC_HEAD;
    if (at + 4 * nl + 1 > len) return false;
    r.nlink = (int)nl;
    for (size_t i = 0; i < nl; i++, at += 4) {
        r.link[i] = g32(p + at);
        if (r.link[i] == 0 || r.link[i] >= r.serial) return false;      /* a link is to an older row */
    }
    r.tlen = p[at++];
    if (at + r.tlen >= len) return false;                               /* the value has at least 3 bytes */
    r.text = (const char *)(p + at);
    at += r.tlen;
    r.val = p + at;
    r.vlen = len - at;
    if (out) *out = r;
    return true;
}

/* the records' bytes up to and including row_top: the page; past it, UNDO's hidden rows */
uint32_t ab_rows_visible_len(const ab_calc *c)
{
    uint32_t at = 0;
    /* nothing hidden (the newest row is the page's top): the whole page, without walking it. Each append asks, and the
       walk over 1,300 rows in PSRAM cost the stand-in about 0.9 ms a key with the page full */
    if (c->rows_len == 0 || g32(c->rows + c->rows_last + 2) <= c->row_top) return c->rows_len;
    while (at < c->rows_len) {
        uint32_t len = g16(c->rows + at);
        if (g32(c->rows + at + 2) > c->row_top) break;
        at += len;
    }
    return at;
}

/* UNDO's hidden rows cut off the page's end (a new row takes their place) */
static void cut_hidden(ab_calc *c)
{
    uint32_t end = ab_rows_visible_len(c), at = 0, last = 0;
    if (end == c->rows_len) return;
    while (at < end) last = at, at += g16(c->rows + at);
    c->rows_len = end;
    c->rows_last = last;
}

int ab_rows_count(const ab_calc *c)
{
    int n = 0;
    uint32_t end = ab_rows_visible_len(c);
    for (uint32_t at = 0; at < end; at += g16(c->rows + at)) n++;
    return n;
}

bool ab_rows_get(const ab_calc *c, int k, ab_row *out)
{
    int n = ab_rows_count(c), i = 0;
    uint32_t end = ab_rows_visible_len(c);
    if (k < 1 || k > n) return false;
    for (uint32_t at = 0; at < end; at += g16(c->rows + at), i++)
        if (i == n - k) return ab_rows_parse(c->rows + at, end - at, out);
    return false;
}

bool ab_rows_find(const ab_calc *c, uint32_t serial, ab_row *out)
{
    uint32_t end = ab_rows_visible_len(c);
    if (serial == 0) return false;
    for (uint32_t at = 0; at < end; at += g16(c->rows + at))
        if (g32(c->rows + at + 2) == serial) return ab_rows_parse(c->rows + at, end - at, out);
    return false;
}

static ab_val *held_val(ab_calc *c, int i)
{
    ab_val *five[6] = {&c->x, &c->y, &c->z, &c->t, &c->lastx, &c->ans};
    return i < 6 ? five[i] : &c->var[i - 6];
}

int ab_rows_held(const ab_calc *c, uint32_t out[33])
{
    int n = 0;
    for (int i = 0; i < 33; i++) {
        uint32_t s = held_val((ab_calc *)c, i)->row;
        bool dup = s == 0;
        for (int j = 0; j < n && !dup; j++) dup = out[j] == s;
        if (!dup) out[n++] = s;
    }
    return n;
}

uint32_t ab_rows_cut(const ab_calc *c, uint32_t budget)
{
    uint32_t end = ab_rows_visible_len(c), at = 0;
    while (at < end && end - at > budget) at += g16(c->rows + at);
    return at;
}

void ab_rows_clear(ab_calc *c)
{
    c->rows_len = 0;
    c->row_top = 0;
}

void ab_rows_set_next(ab_calc *c, uint32_t next)
{
    c->rows_next = next;
}

void ab_rows_clear_all(ab_calc *c)
{
    c->rows_len = c->rows_last = 0;
    c->row_top = 0;
    c->rows_dropped = 0;
    c->hist_row = 0;
    c->sel_row = c->sel_entry = c->sel_top = 0;     /* (unit 093: no row left to select) */
    c->sel_drop = false;
    c->sel_view = 0;
    ab_steps_clear(c);                              /* (unit 095: CLEAR ALL keeps no steps) */
}

/* ---- unit 090: ALG's history ---- */

uint32_t ab_rows_alg_prev(const ab_calc *c, uint32_t before)
{
    uint32_t end = ab_rows_visible_len(c), found = 0;
    for (uint32_t at = 0; at < end; at += g16(c->rows + at)) {
        uint32_t s = g32(c->rows + at + 2);
        if (before && s >= before) break;
        if (g16(c->rows + at + 6) == AB_ROW_OP_ALG) found = s;
    }
    return found;
}

uint32_t ab_rows_alg_next(const ab_calc *c, uint32_t after)
{
    uint32_t end = ab_rows_visible_len(c);
    for (uint32_t at = 0; at < end; at += g16(c->rows + at)) {
        uint32_t s = g32(c->rows + at + 2);
        if (s > after && g16(c->rows + at + 6) == AB_ROW_OP_ALG) return s;
    }
    return 0;
}

int ab_rows_alg_count(const ab_calc *c)
{
    int n = 0;
    uint32_t end = ab_rows_visible_len(c);
    for (uint32_t at = 0; at < end; at += g16(c->rows + at)) n += g16(c->rows + at + 6) == AB_ROW_OP_ALG;
    return n;
}

bool ab_rows_text(const ab_calc *c, uint32_t serial, char *out, size_t size)
{
    ab_row r;
    if (size) out[0] = '\0';
    if (!ab_rows_find(c, serial, &r)) return false;
    if (size) snprintf(out, size, "%.*s", (int)r.tlen, r.text);
    return true;
}

bool ab_rows_peek(const ab_calc *c, uint32_t serial, ab_val *out, char *text, size_t tcap)
{
    ab_row r;
    if (!ab_rows_find(c, serial, &r) || !ab_val_decode(r.val, r.vlen, out, text, tcap)) return false;
    out->row = serial;
    return true;
}

abn_status ab_rows_value(ab_calc *c, uint32_t serial, ab_val *out)
{
    char t[AB_EQN_LEN];
    ab_val v;
    if (!ab_rows_peek(c, serial, &v, t, sizeof t)) return ABN_DOMAIN;
    if (v.kind == AB_EXPR) {
        abn_status st = ab_expr_new(c, t, &v);
        if (st != ABN_OK) return st;
        v.row = serial;
    }
    *out = v;
    return ABN_OK;
}

static uint32_t append(ab_calc *c, unsigned op, int32_t arg, unsigned flags, const uint32_t *link, int nlink,
                       const char *text, size_t tlen, const ab_val *v);

uint32_t ab_rows_test_stamp(ab_calc *c, ab_val *v)
{
    uint32_t s = append(c, AB_ROW_OP_NUM, 0, 0, NULL, 0, NULL, 0, v);
    if (s) v->row = s;
    return s;
}

uint32_t ab_rows_test_append(ab_calc *c, ab_val *v, const char *text, size_t tlen, const uint32_t *link, int nlink)
{
    uint32_t s = append(c, AB_ROW_OP_NUM, 0, 0, link, nlink > AB_ROW_LINKS ? AB_ROW_LINKS : nlink, text, tlen, v);
    if (s) v->row = s;
    return s;
}

void ab_rows_loaded(ab_calc *c)
{
    for (int i = 0; i < 33; i++) {
        ab_val *v = held_val(c, i);
        if (v->row && !ab_rows_find(c, v->row, NULL)) v->row = 0;
    }
}

/* the serial's end: the page starts again, every row dropped and every value's row 0 (abacus #13990 a) */
static void restart(ab_calc *c)
{
    c->rows_dropped += (uint32_t)ab_rows_count(c);
    c->rows_len = 0;
    c->row_top = 0;
    c->rows_next = 1;
    for (int i = 0; i < 33; i++) held_val(c, i)->row = 0;
    c->hist_row = 0;
    memset(c->rowctx.pre, 0, sizeof c->rowctx.pre);
    c->rowctx.pre_lastx = c->rowctx.pre_ans = 0;
    memset(c->rowctx.pre_var, 0, sizeof c->rowctx.pre_var);
    c->rowctx.run_x = 0;
}

/* A row appended: UNDO's hidden rows go first (a new row takes their place: REDO has nothing left to show), then the
   oldest while the page is full. Its serial, or 0 when its value can't be written (nothing appended). */
static uint32_t append(ab_calc *c, unsigned op, int32_t arg, unsigned flags, const uint32_t *link, int nlink,
                       const char *text, size_t tlen, const ab_val *v)
{
    uint8_t vb[AB_EQN_LEN + 64];
    size_t vn = ab_val_encode(c, v, vb, sizeof vb), n;
    uint32_t s = c->rows_next ? c->rows_next : 1;
    if (vn < 3 || vn > sizeof vb) return 0;
    if (tlen > 255) tlen = 255;
    if (s > AB_ROW_SERIAL_LAST) {
        restart(c);
        s = 1;
        nlink = 0;                              /* (the rows it linked are gone) */
        flags &= ~AB_ROWF_MORE;
    }
    n = REC_HEAD + 4u * (size_t)nlink + 1 + tlen + vn;
    cut_hidden(c);
    if (c->rows_len + n > AB_ROWS_RAM) {
        /* the page full: the oldest rows dropped AB_ROWS_DROP bytes at a time, so the page moves once for a few
           hundred rows, not for each (the stand-in measured 5,551 us a key moving 64 KB of PSRAM for every row) */
        uint32_t at = 0, k = 0;
        while (at < c->rows_len && c->rows_len - at + n > AB_ROWS_RAM - AB_ROWS_DROP) at += g16(c->rows + at), k++;
        memmove(c->rows, c->rows + at, c->rows_len - at);
        c->rows_len -= at;
        c->rows_last = c->rows_len ? c->rows_last - at : 0;
        c->rows_dropped += k;
    }
    uint8_t *p = c->rows + c->rows_len;
    p16(p, (unsigned)n);
    p32(p + 2, s);
    p16(p + 6, op);
    p32(p + 8, (uint32_t)arg);
    p[12] = (uint8_t)((flags & 0x0Fu) | (unsigned)nlink << 4);
    size_t at = REC_HEAD;
    for (int i = 0; i < nlink; i++, at += 4) p32(p + at, link[i]);
    p[at++] = (uint8_t)tlen;
    if (tlen) memcpy(p + at, text, tlen);
    at += tlen;
    memcpy(p + at, vb, vn);
    c->rows_last = c->rows_len;
    c->rows_len += (uint32_t)n;
    c->rows_next = s + 1;
    c->row_top = s;
    return s;
}

/* The links from candidates in operand order (R1): zeros left out, each row once (R2), three at most and the flag */
static int links(const uint32_t *cand, int n, uint32_t *out, bool *more)
{
    uint32_t seen[8];
    int k = 0, ns = 0;
    *more = false;
    for (int i = 0; i < n; i++) {
        bool dup = cand[i] == 0;
        for (int j = 0; j < ns && !dup; j++) dup = seen[j] == cand[i];
        if (dup) continue;
        if (ns < 8) seen[ns++] = cand[i];
        if (k < AB_ROW_LINKS) out[k++] = cand[i];
        else *more = true;
    }
    return k;
}

static bool same_val(const ab_calc *c, const ab_val *a, const ab_val *b)
{
    if (a->kind != b->kind) return false;
    if (a->kind == AB_EXPR || a->kind == AB_NAME) {
        const char *x = ab_expr_text(c, a), *y = ab_expr_text(c, b);
        return x && y && strcmp(x, y) == 0;
    }
    return a->len == b->len && !memcmp(&a->re, &b->re, sizeof a->re) && !memcmp(&a->im, &b->im, sizeof a->im) &&
           !memcmp(&a->p3, &b->p3, sizeof a->p3);
}

static ab_val *level(ab_calc *c, int i)
{
    return i == 0 ? &c->x : i == 1 ? &c->y : i == 2 ? &c->z : &c->t;
}

void ab_rows_key_begin(ab_calc *c, ab_op op, int arg)
{
    ab_rowctx *r = &c->rowctx;
    (void)op, (void)arg;
    r->on = c->stu && !c->prgm && !c->running;
    r->route = AB_ROUTE_NONE;
    if (!r->on) return;
    for (int i = 0; i < 4; i++) {
        r->pre[i] = level(c, i)->row;
        r->lv0[i] = *level(c, i);
    }
    r->pre_lastx = c->lastx.row;
    r->pre_ans = c->ans.row;
    for (int i = 0; i < 27; i++) {
        r->pre_var[i] = c->var[i].row;
        r->var0[i] = c->var[i];
    }
    r->nread = 0;
    r->more = false;
}

void ab_rows_num(ab_calc *c)
{
    ab_rowctx *r = &c->rowctx;
    uint32_t s;
    c->x.bare = 0;                              /* (unit 090: a typed number is made, in every mode) */
    if (!r->on) return;
    bool real = !c->cpart && !c->vlen && c->entry.len > 0 && c->entry.len <= (int)sizeof c->entry.text;
    s = append(c, AB_ROW_OP_NUM, 0, 0, NULL, 0, real ? c->entry.text : NULL, real ? (size_t)c->entry.len : 0, &c->x);
    if (!s) return;
    c->x.row = s;
    r->pre[0] = s;                              /* the op reads X as this row (R3) */
    r->lv0[0] = c->x;
}

void ab_rows_read(ab_calc *c, const ab_val *src)
{
    ab_rowctx *r = &c->rowctx;
    uint32_t s = src->row;
    if (!r->on || !c->alg_eval || !s) return;
    for (int i = 0; i < r->nread; i++)
        if (r->read[i] == s) return;
    if (r->nread < AB_ROW_LINKS) r->read[r->nread++] = s;
    else r->more = true;
}

uint32_t ab_rows_alg(ab_calc *c, const char *text, const ab_val *v)
{
    ab_rowctx *r = &c->rowctx;
    if (!r->on) return 0;
    return append(c, AB_ROW_OP_ALG, 0, AB_ROWF_ALG | (r->more ? AB_ROWF_MORE : 0), r->read, r->nread, text,
                  strlen(text), v);
}

/* A level or a letter whose value the key changed while its row stayed: no row is its value's now */
static void stale(ab_calc *c, bool run)
{
    ab_rowctx *r = &c->rowctx;
    for (int i = 0; i < 4; i++) {
        ab_val *v = level(c, i);
        if (v->row && v->row == r->pre[i] && !same_val(c, v, &r->lv0[i])) v->row = 0;
    }
    for (int i = 0; i < 27; i++) {
        ab_val *v = &c->var[i];
        if (same_val(c, v, &r->var0[i])) continue;
        if (run && same_val(c, v, &c->x)) v->row = c->x.row;   /* SOLVE's root stored in its letter, however written */
        else if (v->row == r->pre_var[i]) v->row = 0;
    }
}

static void make_op(ab_calc *c, ab_op op, int arg, const ab_rowop *o)
{
    ab_rowctx *r = &c->rowctx;
    for (int part = 0; part < 2; part++) {
        if (!(o->makes >> part & 1)) continue;
        unsigned reads = op == AB_CMPLX ? ab_rows_cmplx_reads(arg, part) : o->reads;
        uint32_t cand[5], link[AB_ROW_LINKS];
        int n = 0;
        bool more;
        for (int lv = 3; lv >= 0; lv--)                         /* the deepest level read first (R1) */
            if (reads >> lv & 1) cand[n++] = r->pre[lv];
        if (o->src == AB_ROWSRC_VAR) cand[n++] = arg >= 0 && arg < 27 ? r->pre_var[arg] : 0;
        else if (o->src == AB_ROWSRC_LASTX) cand[n++] = r->pre_lastx;
        else if (o->src == AB_ROWSRC_ANS) cand[n++] = r->pre_ans;
        int k = links(cand, n, link, &more);
        ab_val *v = level(c, part);
        uint32_t s = append(c, op, arg, (part ? AB_ROWF_PART2 : 0) | (more ? AB_ROWF_MORE : 0), link, k, NULL, 0, v);
        v->row = s;
    }
}

/* STO+ − × ÷ A (unit 090): A's new value a row A holds, linked to A's old row, then X's (A is the left operand) */
static void make_letter(ab_calc *c, ab_op op, int arg)
{
    ab_rowctx *r = &c->rowctx;
    uint32_t cand[2], link[AB_ROW_LINKS];
    bool more;
    if (arg < 0 || arg >= 27) return;           /* ((I) and (J): a number register, not a letter) */
    cand[0] = r->pre_var[arg];
    cand[1] = r->pre[0];
    int k = links(cand, 2, link, &more);
    uint32_t s = append(c, op, arg, 0, link, k, NULL, 0, &c->var[arg]);
    if (s) c->var[arg].row = s;
}

static void end_run(ab_calc *c)
{
    ab_rowctx *r = &c->rowctx;
    uint32_t link[1] = {r->run_x};
    r->run = false;
    uint32_t s = append(c, r->run_op, r->run_arg, 0, link, r->run_x ? 1 : 0, NULL, 0, &c->x);
    c->x.row = s;
}

void ab_rows_key_end(ab_calc *c, ab_op op, int arg, abn_status st)
{
    ab_rowctx *r = &c->rowctx;
    bool run = false;
    if (!r->on) return;
    if (st == ABN_OK || st == ABN_CLAMPED) {
        if (r->route == AB_ROUTE_DO && !(c->entry.active || c->cpart || c->vlen)) {
            /* (a key the entry took, CHS or E inside a number, is part of it: R3) */
            ab_rowop o = ab_rows_op(op, arg);
            bool lines = op == AB_XEQ || op == AB_RS || op == AB_SST;
            if (o.kind == AB_ROWK_RUN && lines && c->run_lines == 0 && c->stop != AB_STOP_SLICE) {
                /* (R/S or SST with no program line to run: nothing ran, no row) */
            } else if (o.kind == AB_ROWK_RUN) {
                run = true;
                r->run = true;
                r->run_op = (uint16_t)op;
                r->run_arg = arg;
                r->run_x = r->pre[0];
                if (c->stop != AB_STOP_SLICE) end_run(c);
            } else if (o.kind == AB_ROWK_OP) {
                make_op(c, op, arg, &o);
            } else if (o.kind == AB_ROWK_LETTER) {
                make_letter(c, op, arg);
            }
        } else if (r->route == AB_ROUTE_REPORT && (op == AB_ENTER || op == AB_CASEVAL) &&
                   !same_val(c, &c->x, &r->lv0[0])) {
            /* 048's report: ENTER (the answer copied, 094) or EVAL (its value) placed it in X: a SOLVE row linked to
               the row the solve read (088's X, which Y now keeps; none from the list: 094 fixed 089's link to Y,
               which on the list's path was whatever X had held) */
            uint32_t link[1] = {c->cas_rep_row};
            c->x.row = append(c, AB_CASSOLVE, 0, 0, link, c->cas_rep_row ? 1 : 0, NULL, 0, &c->x);
        } else if (r->route == AB_ROUTE_EQN && op == AB_EQDROP) {
            c->x.row = append(c, AB_EQDROP, 0, 0, NULL, 0, NULL, 0, &c->x);     /* the list's equation on the stack */
        }
    }
    stale(c, run);
    r->on = false;
}

void ab_rows_continue_end(ab_calc *c, abn_status st)
{
    (void)st;
    if (!c->rowctx.run || c->stop == AB_STOP_SLICE) return;
    c->rowctx.on = true;                        /* (append reads nothing of it; stale's letters use the key's) */
    end_run(c);
    stale(c, true);
    c->rowctx.on = false;
}

/* ---- unit 090 (B′): the bare mark ---- */

static void unbare(ab_calc *c, ab_op op, int arg)
{
    ab_rowop o = ab_rows_op(op, arg);
    if (op == AB_LIT || op == AB_EQLINE) o.makes = 1, o.kind = AB_ROWK_OP;     /* (a program's number, its equation) */
    if (o.kind != AB_ROWK_OP && o.kind != AB_ROWK_RUN) return;
    for (int part = 0; part < 2; part++)
        if (o.makes >> part & 1) level(c, part)->bare = 0;
}

void ab_rows_bare_key(ab_calc *c, ab_op op, int arg, abn_status st)
{
    if ((st == ABN_OK || st == ABN_CLAMPED) && c->rowctx.route == AB_ROUTE_DO && !(c->entry.active || c->cpart || c->vlen))
        unbare(c, op, arg);
}

void ab_rows_bare_line(ab_calc *c, ab_op op, int arg)
{
    unbare(c, op, arg);
}

/* ---- unit 093: the selection's walk ---- */

int ab_rows_levels(const ab_calc *c)
{
    const ab_val *lv[4] = {&c->x, &c->y, &c->z, &c->t};
    int n = 4;
    while (n > 1 && lv[n - 1]->bare) n--;
    return n;
}

static bool page_row(const ab_calc *c, uint32_t s)
{
    const ab_val *lv[4] = {&c->x, &c->y, &c->z, &c->t};
    int n = ab_rows_levels(c);
    for (int i = 0; i < n; i++)
        if (lv[i]->row == s) return false;
    return true;
}

uint32_t ab_rows_page_prev(const ab_calc *c, uint32_t before)
{
    uint32_t end = ab_rows_visible_len(c), found = 0;
    for (uint32_t at = 0; at < end; at += g16(c->rows + at)) {
        uint32_t s = g32(c->rows + at + 2);
        if (before && s >= before) break;
        if (page_row(c, s)) found = s;
    }
    return found;
}

uint32_t ab_rows_page_next(const ab_calc *c, uint32_t after)
{
    uint32_t end = ab_rows_visible_len(c);
    for (uint32_t at = 0; at < end; at += g16(c->rows + at)) {
        uint32_t s = g32(c->rows + at + 2);
        if (s > after && page_row(c, s)) return s;
    }
    return 0;
}

void ab_rows_copy(ab_calc *c, uint32_t src)
{
    ab_rowctx *r = &c->rowctx;
    uint32_t link[1] = {src};
    if (!r->on) return;
    uint32_t s = append(c, AB_COPY, 0, 0, link, 1, NULL, 0, &c->x);
    if (!s) return;
    c->x.row = s;
    for (int i = 3; i > 0; i--) r->pre[i] = r->pre[i - 1], r->lv0[i] = r->lv0[i - 1];    /* (the stack lifted under it) */
    r->pre[0] = s;                              /* a verb after it reads X as this row */
    r->lv0[0] = c->x;
}

static ab_rows_window_fn window_fn;

void ab_rows_set_window(ab_rows_window_fn fn)
{
    window_fn = fn;
}

bool ab_rows_window(const ab_calc *c, uint32_t *top, uint32_t *bottom)
{
    *top = *bottom = 0;
    if (!window_fn) return false;
    window_fn(c, top, bottom);
    return true;
}
