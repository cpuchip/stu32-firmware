/* screen: layout v0 (see screen.h). */
#include "screen.h"
#include "tbook.h"

#include <stdio.h>
#include <string.h>

#define W LS027_W
#define H LS027_H
#define LINE_MAX_CP 256         /* code points in one line before it is fitted */
#define NUM_X 20                /* where a line's text may start: right of its letter */
#define NUM_R 398               /* and where it ends */
#define FMT_WIDTH AB_FMT_LINE   /* the formatter's width: what scale 3 holds (ours, v0; fmt.h, as the core's ENG view) */
#define FMT_WIDE AB_FMT_WIDE    /* the fall-back width, drawn at scale 2 */

/* the lines' bottoms (the glyphs' last row + 1) */
#define BOT_T 52
#define BOT_Z 80
#define BOT_Y 108
#define BOT_X 156
#define SOFT_Y 206

void screen_ui_default(screen_ui *ui)
{
    memset(ui, 0, sizeof *ui);
    ui->battery = -1;
    ui->radix = '.';
}

void screen_clear(ls027_frame *f)
{
    memset(f->px, 0, sizeof f->px);
}

static void put(ls027_frame *f, int x, int y, bool dark)
{
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    uint8_t m = (uint8_t)(1u << (x % 8));
    if (dark) f->px[y][x / 8] |= m;
    else f->px[y][x / 8] &= (uint8_t)~m;
}

void screen_fill(ls027_frame *f, int x, int y, int w, int h, bool dark)
{
    /* clipped to the frame first (put would skip those pixels one by one; and gcc 15 at -O3, or mingw's at -O2,
       cannot see put's own test through the unrolled loops: -Wstringop-overflow, tally #4663) */
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) put(f, i, j, dark);
}

static void frame_box(ls027_frame *f, int x, int y, int w, int h)
{
    screen_fill(f, x, y, w, 1, true);
    screen_fill(f, x, y + h - 1, w, 1, true);
    screen_fill(f, x, y, 1, h, true);
    screen_fill(f, x + w - 1, y, 1, h, true);
}

/* One code point from UTF-8 at *s, advancing it; a malformed byte is taken as itself. */
static uint32_t next_cp(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    uint32_t cp = p[0];
    int n = cp < 0x80 ? 0 : (cp & 0xE0) == 0xC0 ? 1 : (cp & 0xF0) == 0xE0 ? 2 : (cp & 0xF8) == 0xF0 ? 3 : -1;
    if (n < 0) n = 0;
    else if (n > 0) cp &= 0x3Fu >> n;
    for (int i = 1; i <= n; i++) {
        if ((p[i] & 0xC0) != 0x80) {
            *s += 1;
            return p[0];
        }
        cp = cp << 6 | (p[i] & 0x3F);
    }
    *s += n + 1;
    return cp;
}

/* One cell: a code point, with a combining mark after it (U+0300..036F) carried in bits 24..30
   as mark - U+02FF, so it is drawn over its letter and takes no cell of its own. */
#define MARK_SHIFT 24
#define CP_MASK 0xFFFFFFu

static bool combining(uint32_t cp)
{
    return cp >= 0x0300 && cp <= 0x036F;
}

static uint32_t next_cell(const char **s)
{
    uint32_t cp = next_cp(s);
    while (**s) {
        const char *t = *s;
        uint32_t m = next_cp(&t);
        if (!combining(m)) break;
        if (!(cp >> MARK_SHIFT)) cp |= (m - 0x02FF) << MARK_SHIFT;   /* the first mark; others dropped */
        *s = t;
    }
    return cp;
}

const font_glyph *screen_glyph(uint32_t cp)
{
    int lo = 0, hi = FONT5X7_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (FONT5X7[mid].cp == cp) return &FONT5X7[mid];
        if (FONT5X7[mid].cp < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

int screen_cells(const char *s)
{
    int n = 0;
    while (*s) {
        next_cell(&s);
        n++;
    }
    return n;
}

int screen_missing(const char *s)
{
    int n = 0;
    while (*s) n += screen_glyph(next_cp(&s)) == NULL;     /* marks too: each needs its glyph */
    return n;
}

long screen_missed;

static void draw_glyph(ls027_frame *f, int x, int y, int scale, uint32_t cp, bool dark);

static void draw_cp(ls027_frame *f, int x, int y, int scale, uint32_t cell, bool dark)
{
    draw_glyph(f, x, y, scale, cell & CP_MASK, dark);
    /* a mark one row above its glyph's own rows, so a row stays clear between it and a lowercase
       letter (a hat on y touching it read as 9) */
    if (cell >> MARK_SHIFT) draw_glyph(f, x, y - scale, scale, (cell >> MARK_SHIFT) + 0x02FF, dark);
}

static void draw_glyph(ls027_frame *f, int x, int y, int scale, uint32_t cp, bool dark)
{
    const font_glyph *g = screen_glyph(cp);
    if (!g) {                                   /* the face lacks it: a hollow box */
        screen_missed++;
        for (int r = 0; r < 7; r++)
            for (int col = 0; col < 5; col++)
                if (r == 0 || r == 6 || col == 0 || col == 4)
                    screen_fill(f, x + col * scale, y + r * scale, scale, scale, dark);
        return;
    }
    for (int r = 0; r < 7; r++)
        for (int col = 0; col < 5; col++)
            if (g->rows[r] >> (4 - col) & 1) screen_fill(f, x + col * scale, y + r * scale, scale, scale, dark);
}

static int draw_cps(ls027_frame *f, int x, int y, int scale, const uint32_t *cp, int n, bool dark)
{
    for (int i = 0; i < n; i++) draw_cp(f, x + i * 6 * scale, y, scale, cp[i], dark);
    return n;
}

int screen_decode(const char *s, uint32_t *cells, int max)
{
    int n = 0;
    while (*s && n < max) cells[n++] = next_cell(&s);
    return n;
}

void screen_cell_bitmap(uint32_t cell, uint8_t rows[8])
{
    const font_glyph *g = screen_glyph(cell & CP_MASK);
    memset(rows, 0, 8);
    if (!g) screen_missed++;
    for (int r = 0; r < 7; r++) rows[r + 1] = g ? g->rows[r] : (uint8_t)(r == 0 || r == 6 ? 0x1F : 0x11);
    if (cell >> MARK_SHIFT) {                   /* the mark a row above the glyph's rows, as drawn */
        const font_glyph *m = screen_glyph((cell >> MARK_SHIFT) + 0x02FF);
        for (int r = 0; m && r < 7; r++) rows[r] |= m->rows[r];
    }
}

int screen_text(ls027_frame *f, int x, int y, int scale, const char *s, bool dark)
{
    int n = 0;
    while (*s) draw_cp(f, x + n++ * 6 * scale, y, scale, next_cell(&s), dark);
    return n;
}

/* ---- a line of text, fitted ---- */

typedef struct {
    uint32_t cp[LINE_MAX_CP];
    int n;
    int cursor;                 /* the cell the cursor is in, -1 none */
    int tail;                   /* cells at the end kept in view when the line is cut (unit 048: ∫'s " +C"), 0 none */
    char u8[SCREEN_TEXT_MAX];   /* the same cells as UTF-8, for screen_lines */
    int ulen;
} line;

static void u8_add(line *l, const char *b, int n)
{
    if (l->ulen + n >= SCREEN_TEXT_MAX) return;
    memcpy(l->u8 + l->ulen, b, (size_t)n);
    l->ulen += n;
    l->u8[l->ulen] = '\0';
}

static void line_add(line *l, const char *s)
{
    while (*s && l->n < LINE_MAX_CP) {
        const char *b = s;
        l->cp[l->n++] = next_cell(&s);
        u8_add(l, b, (int)(s - b));
    }
}

static void line_add_cp(line *l, uint32_t cp)
{
    char b[4];
    int n = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
    if (l->n >= LINE_MAX_CP) return;
    l->cp[l->n++] = cp;
    if (n == 1) b[0] = (char)cp;
    else if (n == 2) b[0] = (char)(0xC0 | cp >> 6), b[1] = (char)(0x80 | (cp & 0x3F));
    else if (n == 3) b[0] = (char)(0xE0 | cp >> 12), b[1] = (char)(0x80 | (cp >> 6 & 0x3F)), b[2] = (char)(0x80 | (cp & 0x3F));
    else b[0] = (char)(0xF0 | cp >> 18), b[1] = (char)(0x80 | (cp >> 12 & 0x3F)), b[2] = (char)(0x80 | (cp >> 6 & 0x3F)),
         b[3] = (char)(0x80 | (cp & 0x3F));
    u8_add(l, b, n);
}

static void line_cursor(line *l)
{
    l->cursor = l->n;
    line_add_cp(l, '_');
}

/* the pixels n cells take at scale s: the last cell needs no gap after it */
static int span(int n, int s)
{
    return n > 0 ? n * 6 * s - s : 0;
}

/* Draws l with its bottom at bot, at the largest scale up to max that holds it; at scale 2 a
   line still too long shows a window of it (around the cursor, else its start), a cut side marked
   with an ellipsis. Right aligned unless left. */
static void draw_line(ls027_frame *f, int bot, int max, const line *l, bool left)
{
    int s = max;
    while (s > 2 && span(l->n, s) > NUM_R - NUM_X) s--;
    int room = (NUM_R - NUM_X + s) / (6 * s);
    uint32_t cp[LINE_MAX_CP];
    int n = l->n;
    memcpy(cp, l->cp, sizeof cp[0] * (size_t)n);
    if (n > room && l->tail > 0 && l->cursor < 0 && l->tail < room - 1) {    /* its start, an ellipsis, its tail */
        memmove(cp + room - l->tail, l->cp + n - l->tail, sizeof cp[0] * (size_t)l->tail);
        cp[room - l->tail - 1] = 0x2026;
        n = room;
        left = true;
    } else if (n > room) {
        int start = 0;
        if (l->cursor >= 0 && l->cursor > room - 2) start = l->cursor - (room - 2);
        if (start > n - room) start = n - room;
        memmove(cp, cp + start, sizeof cp[0] * (size_t)room);
        if (start > 0) cp[0] = 0x2026;
        if (start + room < n) cp[room - 1] = 0x2026;
        n = room;
        left = true;
    }
    int x = left ? NUM_X : NUM_R - span(n, s);
    draw_cps(f, x, bot - 7 * s, s, cp, n, true);
}

static void draw_label(ls027_frame *f, int bot, const char *label)
{
    screen_text(f, 2, bot - 14, 2, label, true);
}

/* ---- the core's text ---- */

static ab_display display_of(const ab_calc *c, const screen_ui *ui, int width)
{
    ab_display d;
    d.dec.mode = c->disp_mode;
    d.dec.n = c->disp_n;
    d.dec.width = width;
    d.dec.radix = ui->radix == ',' ? ',' : '.';
    d.dec.separator = ui->separator;
    d.fdisp = c->fdisp;
    d.c = c->frac_c;
    d.frac_format = !c->flag8 ? 0 : (c->flag9 ? 2 : 1);   /* UG p.5-6, as the core's own */
    d.base = c->base;
    d.polar = c->cpolar;
    d.angle = (int)c->angle;
    return d;
}

/* A stack value as text at width, or -1; *ind the fraction indicator. */
static int val_text(const ab_val *v, const ab_display *d, char *out, int size, int *ind)
{
    *ind = 0;
    if (v->kind == AB_COMPLEX) return ab_format_complex(&v->re, &v->im, d, out, size);
    if (v->kind == AB_VECTOR) {
        abn el[3] = {v->re, v->im, v->p3};
        return ab_format_vector(el, v->len, d, out, size);
    }
    return ab_format_display(&v->re, d, out, size, ind);
}

/* the value as a line: the formatter's width first, the wide one when that holds nothing */
static void val_line(const ab_val *v, const ab_calc *c, const screen_ui *ui, line *l, int *ind)
{
    char buf[160];
    ab_display d = display_of(c, ui, FMT_WIDTH);
    if (val_text(v, &d, buf, sizeof buf, ind) < 0) {
        d = display_of(c, ui, FMT_WIDE);
        if (val_text(v, &d, buf, sizeof buf, ind) < 0) snprintf(buf, sizeof buf, "?");
    }
    line_add(l, buf);
}

/* One part of a number being typed, as the keys typed it: its sign, the digits with the device's
   radix mark (a second point makes a fraction: shown as a space and a slash, 1 2/3), E and the
   exponent's sign; the cursor where the next key goes when this part has it. */
static void entry_part(const ab_entry *e, char radix, bool cursor, line *l)
{
    int points = 0, seen = 0;
    for (int i = 0; i < e->len; i++) points += e->text[i] == '.';
    if (e->mant_neg) line_add_cp(l, '-');
    for (int i = 0; i <= e->len; i++) {
        if (cursor && i == e->len - e->back) line_cursor(l);
        if (i == e->len) break;
        char ch = e->text[i];
        if (ch == '.') {
            seen++;
            line_add_cp(l, points == 2 ? (seen == 1 ? ' ' : '/') : (uint32_t)radix);
        } else if (ch == 'E') {
            line_add_cp(l, 'E');
            if (e->exp_neg) line_add_cp(l, '-');
        } else {
            line_add_cp(l, (uint32_t)(unsigned char)ch);
        }
    }
}

static void entry_line(const ab_calc *c, char radix, line *l)
{
    if (c->vlen > 0) {                          /* a vector: its finished elements, then this one */
        line_add_cp(l, '[');
        for (int k = 0; k < c->vlen - 1 && k < 2; k++) {
            entry_part(&c->ventry[k], radix, false, l);
            line_add_cp(l, ',');
        }
    }
    if (c->cpart) {                             /* x i y or r theta a: the first part, then this one */
        entry_part(&c->entry1, radix, false, l);
        line_add(l, c->cpart == 1 ? "i" : "\xce\xb8");   /* as the formatter writes it */
    }
    entry_part(&c->entry, radix, true, l);
    if (c->vlen > 0) line_add_cp(l, ']');
}

/* an equation's text with the cursor at byte offset cur (-1 none) */
static void eqn_line(const char *s, int cur, line *l)
{
    if (s[0] == '\x01') s++;                    /* a built-in solver: its LCD name (unit 015) */
    const char *p = s;
    while (*p) {
        if (cur >= 0 && p - s == cur) line_cursor(l);
        line_add_cp(l, next_cell(&p));
    }
    if (cur >= 0 && p - s <= cur) line_cursor(l);
}

/* A variable's name as the prompts show it (stop_var's ids: A..Z, i, an unnamed address, the
   statistics registers); "" for none. */
static void var_name(int id, char *out, int size)
{
    static const char *const STATS[] = {"n", "\xce\xa3x", "\xce\xa3y", "\xce\xa3x\xc2\xb2", "\xce\xa3y\xc2\xb2",
                                        "\xce\xa3xy"};
    if (id >= 0 && id < 26) snprintf(out, (size_t)size, "%c", 'A' + id);
    else if (id == 26) snprintf(out, (size_t)size, "i");       /* the 33s's i (unit 025) */
    else if (id >= 1000) snprintf(out, (size_t)size, "(%d)", id - 1000);
    else if (id <= -27 && id >= -32) snprintf(out, (size_t)size, "%s", STATS[-id - 27]);
    else if (id == AB_VIEW_INTEG) snprintf(out, (size_t)size, "\xe2\x88\xab");     /* ∫ (unit 035) */
    else if (id <= AB_VIEW_LIN2 && id >= AB_VIEW_LIN3 - 2)      /* X, Y, Z of a linear solution */
        snprintf(out, (size_t)size, "%c", 'X' + (id > AB_VIEW_LIN2 - 10 ? AB_VIEW_LIN2 - id : AB_VIEW_LIN3 - id));
    else if (size > 0) out[0] = '\0';
}

/* ---- the bands ---- */

/* A graph's texts as drawn, shared by the status band's readout and draw_graph (one at a time: one buffer). */
static screen_graph_text graph_tx;

static void status_band(ls027_frame *f, const ab_calc *c, const screen_ui *ui, int ind)
{
    /* the clock at the left (unit 027 rule 4: Michael's decided layout, "Clock"), its 12-hour a/p
       half size after it; the shift held covers it while armed (both brief at that spot) */
    if (ui->shift == 1 || ui->shift == 2 || ui->help) {     /* the shift held, light on dark; HELP armed (075) */
        screen_fill(f, 0, 0, 70, 18, true);
        screen_text(f, 3, 2, 2, ui->shift == 1 ? "GOLD" : ui->shift == 2 ? "BLUE" : "HELP", false);
    } else if (ui->clock[0]) {
        int cells = screen_text(f, 3, 2, 2, ui->clock, true);
        if (ui->ampm) {
            char ap[2] = {ui->ampm, '\0'};
            screen_text(f, 3 + cells * 12, 9, 1, ap, true);     /* 12 px a cell at scale 2 */
        }
    }
    if (c->gr_on && ui->graph) {
        /* unit 040: a graph's readout here, small, in place of the annunciators (its soft keys fill the band where
           033 drew it) */
        screen_graph_texts(c, ui, &graph_tx);
        screen_text(f, SCREEN_READOUT_X, SCREEN_READOUT_Y, 1, graph_tx.readout, true);
        if (graph_tx.pad[0])                    /* unit 073: the digit pad's legend, right-aligned before STU */
            screen_text(f, SCREEN_PAD_RIGHT - 6 * screen_cells(graph_tx.pad), SCREEN_READOUT_Y, 1, graph_tx.pad, true);
        ind = 0;
        goto right;
    }
    if (c->angle == AB_ANGLE_RAD) screen_text(f, 72, 2, 2, "RAD", true);
    else if (c->angle == AB_ANGLE_GRAD) screen_text(f, 72, 2, 2, "GRAD", true);
    if (c->base != 10) screen_text(f, 124, 2, 2, c->base == 16 ? "HEX" : c->base == 8 ? "OCT" : "BIN", true);
    else if (ui->hyp) screen_text(f, 124, 2, 2, "HYP", true);   /* unit 073: the prefix armed (35s UG p.1-13), in the
                                                   base's place, free in decimal; its prompt HYP_ shows it in any base */
    if (c->tbl_on) screen_text(f, 164, 2, 2, "TABLE", true);  /* a table open (unit 032), over PRGM/EQN */
    else if (c->gr_on) screen_text(f, 164, 2, 2, "GRAPH", true);  /* a graph open (unit 033) */
    else if (c->prgm) screen_text(f, 164, 2, 2, "PRGM", true);
    else if (c->stu) screen_text(f, 164, 2, 2, c->alg ? "ALG" : "RPN", true);   /* STU's entry (unit 029) */
    if (c->eqn_mode && !c->tbl_on && !c->gr_on) screen_text(f, 216, 2, 2, "EQN", true);
    if (ind) screen_text(f, 255, 2, 2, ind > 0 ? "\xe2\x96\xb2" : "\xe2\x96\xbc", true);
    for (int n = 0; n <= 4; n++) {
        char d[2] = {(char)('0' + n), '\0'};
        if (ab_flag(c, n)) screen_text(f, 268 + n * 12, 2, 2, d, true);
    }
right:
    if (c->stu) screen_text(f, 330, 2, 2, "STU", true);     /* unit 029: three cells, clear of the battery */
    else screen_text(f, 338, 2, 2, c->m35 ? "35" : "33", true);   /* the mode (unit 017), the brief's annunciator */
    frame_box(f, 366, 2, 30, 14);               /* the battery: an outline, its knob, its charge */
    screen_fill(f, 396, 6, 3, 6, true);
    if (ui->battery >= 0) {
        int pct = ui->battery > 100 ? 100 : ui->battery;
        screen_fill(f, 368, 4, 26 * pct / 100, 10, true);
    }
    if (ui->charging || ui->battery < 0) {      /* charging: a plus over the charge, inverted; not read: ? */
        const font_glyph *g = screen_glyph(ui->charging ? '+' : '?');
        for (int r = 0; r < 7; r++)
            for (int col = 0; col < 5; col++)
                if (g->rows[r] >> (4 - col) & 1) {
                    int x = 378 + col, y = 5 + r;
                    put(f, x, y, !(f->px[y][x / 8] >> (x % 8) & 1));
                }
    }
    screen_fill(f, 0, 19, W, 2, true);
}

static void soft_band(ls027_frame *f, const screen_ui *ui)
{
    for (int k = 0; k < SCREEN_SOFT; k++) {
        const char *s = ui->soft[k];
        if (!s || !*s) continue;
        int x = k * 66 + 2, n = screen_cells(s);
        if (n > 5) n = 5;                       /* five cells hold a label at scale 2 */
        bool on = ui->soft_on[k];               /* the setting in effect: drawn inverted (unit 019) */
        frame_box(f, x, SOFT_Y, 64, 30);
        if (on) screen_fill(f, x, SOFT_Y, 64, 30, true);
        if (ui->soft_sub[k]) screen_fill(f, x, SOFT_Y, 64, 4, !on);   /* a submenu: a thick top edge, clear of
                                                                        any label (five cells fill the width) */
        line l = {.n = 0, .cursor = -1};
        line_add(&l, s);
        draw_cps(f, x + (64 - span(n, 2)) / 2, SOFT_Y + 8, 2, l.cp, n, !on);
        if (ui->soft_dim[k])                    /* dimmed: every other pixel cleared, a 50% grey (unit 041 A) */
            for (int j = SOFT_Y; j < SOFT_Y + 30; j++)
                for (int i = x + ((x + j) & 1); i < x + 64; i += 2) put(f, i, j, false);
    }
}

/* A value in at most n cells, in the display format (a graph's labels and readout, unit 033). */
static void short_val(const ab_calc *c, const screen_ui *ui, const ab_val *v, int n, char *out, int size)
{
    int ind;
    ab_display d = display_of(c, ui, n);
    if (val_text(v, &d, out, size, &ind) < 0) snprintf(out, (size_t)size, "?");
}

static void short_num(const ab_calc *c, const screen_ui *ui, const abn *a, int n, char *out, int size)
{
    ab_val v;
    memset(&v, 0, sizeof v);
    v.kind = AB_REAL;
    v.re = *a;
    short_val(c, ui, &v, n, out, size);
}

void screen_graph_texts(const ab_calc *c, const screen_ui *ui, screen_graph_text *t)
{
    const screen_graph *g = ui->graph;
    char a[48], b[48];
    memset(t, 0, sizeof *t);
    if (!g) return;
    short_num(c, ui, &g->xmin, 9, t->xmin, sizeof t->xmin);
    short_num(c, ui, &g->xmax, 9, t->xmax, sizeof t->xmax);
    if (g->ready) {
        short_num(c, ui, &g->ymax, 9, t->ymax, sizeof t->ymax);
        short_num(c, ui, &g->ymin, 9, t->ymin, sizeof t->ymin);
    }
    snprintf(t->note, sizeof t->note, "%s", g->note);
    const char *m = ab_error_message(c);        /* an erring ENTER's message, in the readout's place (033 F3) */
    if (m) {
        snprintf(t->readout, sizeof t->readout, "%s", m);
    } else if (c->gr_res_kind) {                /* unit 040: a tool's result, named as the 50g shows it (p.12-5) */
        static const char *const NAME[AB_F_N] = {"ROOT", "SLOPE", "EXTRM", "AREA", "F(X)", "TANL"};
        short_num(c, ui, &c->gr_res, 14, a, sizeof a);
        if (c->gr_res_kind == AB_F_TANL + 1) {
            short_num(c, ui, &c->gr_res_b, 14, b, sizeof b);
            snprintf(t->readout, sizeof t->readout, "Y=%s\xc2\xb7X%s%s", a, b[0] == '-' ? "" : "+", b);
        } else {
            snprintf(t->readout, sizeof t->readout, "%s: %s", NAME[c->gr_res_kind - 1], a);
        }
    } else if (g->noxy) {                       /* (X,Y) off */
        t->readout[0] = '\0';
    } else if (g->crow >= 0 && !g->ready && c->gr_auto) {     /* the free cursor before the fit: x alone */
        short_num(c, ui, &g->cx, 12, a, sizeof a);
        snprintf(t->readout, sizeof t->readout, "x=%s", a);
    } else {
        short_num(c, ui, &g->cx, 12, a, sizeof a);
        if (g->cst != ABN_OK) snprintf(b, sizeof b, "%s", ab_status_message(g->cst));
        else short_val(c, ui, &g->cy, 12, b, sizeof b);
        snprintf(t->readout, sizeof t->readout, "x=%s y=%s", a, b);
    }
    for (char *q = t->readout; *q; q++)
        if (*q == '\n') *q = ' ';
    snprintf(t->pad, sizeof t->pad, "%s", screen_pad_legend(screen_cells(t->readout)));   /* unit 073 */
}

/* Unit 073 (stu-discover, abacus #8714): the digit pad's legend while a graph is open, in the band after a readout of
   n cells, never over the plot: the longest form that leaves a cell between them. The shortest fits after the
   longest readout (33 cells, TANL's). */
const char *screen_pad_legend(int n)
{
    static const char *const PAD[3] = {"8\xe2\x96\xb2" "2\xe2\x96\xbc" "4\xe2\x97\x80" "6\xe2\x96\xb6 5HOME G\xc3\x97" "10",
                                       "8\xe2\x96\xb2" "2\xe2\x96\xbc" "4\xe2\x97\x80" "6\xe2\x96\xb6" "5",
                                       "8\xe2\x96\xb2" "2\xe2\x96\xbc" "4\xe2\x97\x80" "6\xe2\x96\xb6"};
    int end = SCREEN_READOUT_X + 6 * n + 6;     /* (6 px a cell, small; a cell's gap) */
    for (int k = 0; k < 3; k++)
        if (end <= SCREEN_PAD_RIGHT - 6 * screen_cells(PAD[k])) return PAD[k];
    return "";
}

int screen_graph_readout(const ab_calc *c, const screen_ui *ui, char *out, int size)
{
    screen_graph_text t;
    screen_graph_texts(c, ui, &t);
    return snprintf(out, (size_t)size, "%s", t.readout);
}

/* A graph (unit 033): the plot area (rows 21..205) with its axes and curve (or, while AUTO's samples are
   computed, a progress bar along its bottom), XMIN and XMAX at the lower corners and YMAX and YMIN at the
   left edge in the small font, the form's note at the top right, the trace's marks at the area's top and
   bottom; the soft band: the bar's left and right, then the trace's x and value. */
static void draw_graph(ls027_frame *f, const ab_calc *c, const screen_ui *ui)
{
    const screen_graph *g = ui->graph;
    const int top = 21, Hh = AB_GRAPH_H;
    screen_graph_text *tx = &graph_tx;          /* (the texts, as screen_graph_texts gives them: 033b) */
    screen_graph_texts(c, ui, tx);
    if (g->ready) {
        if (g->xrow >= 0) screen_fill(f, 0, top + g->xrow, W, 1, true);
        if (g->ycol >= 0) screen_fill(f, g->ycol, top, 1, Hh, true);
        for (int i = 0; i < g->done && i < AB_GRAPH_W; i++)
            if (g->lo[i] <= g->hi[i]) screen_fill(f, i, top + g->lo[i], 1, g->hi[i] - g->lo[i] + 1, true);
    } else {
        screen_fill(f, 0, top + Hh - 2, g->done, 2, true);
    }
    screen_text(f, 0, top + Hh - 8, 1, tx->xmin, true);
    screen_text(f, W - 6 * screen_cells(tx->xmax), top + Hh - 8, 1, tx->xmax, true);
    if (tx->ymax[0]) screen_text(f, 0, top + 1, 1, tx->ymax, true);
    if (tx->ymin[0]) screen_text(f, 0, top + Hh - 17, 1, tx->ymin, true);
    if (tx->note[0]) screen_text(f, W - 6 * screen_cells(tx->note), top + 1, 1, tx->note, true);
    if (g->cur >= 0 && g->cur < AB_GRAPH_W) {
        screen_fill(f, g->cur, top, 1, 3, true);
        screen_fill(f, g->cur, top + Hh - 3, 1, 3, true);
    }
    /* unit 040: TANL's line; the mark and, with the free cursor, the box BOXZ would fill; the free cursor, a
       cross 7 pixels across */
    if (g->tl_on)
        for (int i = 0; i < AB_GRAPH_W; i++)
            if (g->tl[i] >= 0) screen_fill(f, i, top + g->tl[i], 1, 1, true);
    if (g->mcol >= 0) {
        screen_fill(f, g->mcol - 1, top + g->mrow - 1, 3, 3, true);
        if (g->crow >= 0) {
            int x0 = g->mcol < g->cur ? g->mcol : g->cur, x1 = g->mcol < g->cur ? g->cur : g->mcol;
            int y0 = g->mrow < g->crow ? g->mrow : g->crow, y1 = g->mrow < g->crow ? g->crow : g->mrow;
            screen_fill(f, x0, top + y0, x1 - x0 + 1, 1, true);
            screen_fill(f, x0, top + y1, x1 - x0 + 1, 1, true);
            screen_fill(f, x0, top + y0, 1, y1 - y0 + 1, true);
            screen_fill(f, x1, top + y0, 1, y1 - y0 + 1, true);
        }
    }
    if (g->crow >= 0) {
        screen_fill(f, g->cur - 3, top + g->crow, 7, 1, true);
        screen_fill(f, g->cur, top + g->crow - 3, 1, 7, true);
    }
    soft_band(f, ui);                           /* (the readout: the status band, unit 040) */
}

/* A table's row (unit 032): the variable's value at the left, the equation's value at the right (or the
   row's error message), each in the display format in 16 cells, 33 cells in all. The row is computed with
   the variable set for it alone and everything put back (ab_table_row): c is unchanged. */
static void table_row_line(ab_calc *c, const screen_ui *ui, int32_t k, line *l)
{
    char xs[64], ys[64];
    abn x;
    ab_val y, xv;
    int ind;
    abn_status st = ab_table_row(c, k, &x, &y);
    ab_display d = display_of(c, ui, 16);
    memset(&xv, 0, sizeof xv);
    xv.kind = AB_REAL;
    xv.re = x;
    if (val_text(&xv, &d, xs, sizeof xs, &ind) < 0) snprintf(xs, sizeof xs, "?");
    if (st != ABN_OK) snprintf(ys, sizeof ys, "%s", ab_status_message(st));
    else if (val_text(&y, &d, ys, sizeof ys, &ind) < 0) snprintf(ys, sizeof ys, "?");
    line_add(l, xs);
    int pad = 33 - screen_cells(xs) - screen_cells(ys);
    for (int i = 0; i < (pad > 1 ? pad : 1); i++) line_add_cp(l, ' ');
    line_add(l, ys);
}

/* What screen_render draws and screen_lines tells: the four lines, how each is drawn, and the
   fraction indicator for the status band. */
typedef struct {
    line x, y, z, t;
    screen_kind xk, yk;
    bool x_left, x_letter, prompt;
    bool view;              /* SYS's view: four left lines at scale 2, no letters (unit 027) */
    bool table;             /* a table open (unit 032): four rows, x left and the value right, at scale 2 */
    bool show;              /* SHOW held (unit 030): each part's digits in two halves, left, no letters:
                               one part on Y and X, two on T and Z then Y and X */
    bool alg;               /* STU's ALG: the line in Y's place (left), its result in X's, the entry
                               before in T (left) and Z; no letters (unit 029) */
    bool textbook;          /* unit 080: the shown STU equation drawn in 2D in the stack's place (X keeps its text) */
    int ind;
} composed;

static composed comp;           /* compose's result, shared by screen_render and screen_lines (one at a time:
                                   about 8 KB, kept off the stack) */

/* Unit 080: whether the equation shown is drawn as a textbook would set it: STU mode, an STU-stamped equation (another
   mode's text groups differently), not one being typed (typing stays one line), not a built-in solver's entry */
static bool textbook_shown(const ab_calc *c)
{
    return c->stu && c->eqn_mode && !c->eqn_typing && !c->prgm && !c->tbl_on && !c->gr_on && c->eqn_cur >= 0 &&
           c->eqn_model[c->eqn_cur] == AB_MODEL_STU && c->eqn[c->eqn_cur][0] != '\x01';
}

static void compose(const ab_calc *c, const screen_ui *ui, composed *k)
{
    int unused;
    const char *m;
    bool eqn_shown = false;                     /* X is the equation shown in the list (unit 048's notes) */
    line *xl = &k->x;
    memset(k, 0, sizeof *k);
    k->x.cursor = k->y.cursor = k->z.cursor = k->t.cursor = -1;
    k->xk = SCREEN_VALUE;
    k->yk = SCREEN_VALUE;
    if (ui->view[0] || ui->view[1] || ui->view[2] || ui->view[3]) {     /* SYS's view (unit 027 rule 5) */
        line *ls[4] = {&k->t, &k->z, &k->y, &k->x};
        for (int i = 0; i < 4; i++)
            if (ui->view[i]) line_add(ls[i], ui->view[i]);
        k->view = k->x_left = true;
        k->xk = k->yk = SCREEN_MESSAGE;
        return;
    }
    if (ui->show_n > 0) {                       /* SHOW (unit 030): a sign with the first 17 digits, then the
                                                   last 17 */
        line *ls[4] = {&k->t, &k->z, &k->y, &k->x};
        for (int p = 0; p < ui->show_n && p < 2; p++) {
            const char *s = ui->show[p];
            int sign = s[0] == '-', half = sign + (ABN_DIGITS + 1) / 2;
            char a[AB_SHOW_LEN];
            line **two = ui->show_n == 2 ? &ls[2 * p] : &ls[2];
            snprintf(a, sizeof a, "%.*s", half, s);
            line_add(two[0], a);
            line_add(two[1], s + ((int)strlen(s) > half ? half : (int)strlen(s)));
        }
        k->show = k->x_left = true;
        k->xk = k->yk = SCREEN_VALUE;
        return;
    }
    /* the X line, first that applies (screen.h) */
    if (ui->busy && (m = ab_busy_message((ab_op)ui->busy)) != NULL) {
        line_add(xl, m);
        k->x_left = true;
        k->xk = SCREEN_MESSAGE;
    } else if (ui->prompt) {                    /* an op waiting for its argument (app.h) */
        line_add(xl, ui->prompt);
        k->x_left = true;
        k->xk = SCREEN_PROMPT;
    } else if (c->tbl_on && !ab_error_message(c)) {     /* the table: rows k-3..k, row k on X (unit 032) */
        line *ls[4] = {&k->t, &k->z, &k->y, &k->x};
        for (int i = 0; i < 4; i++)             /* (64-bit: k is at least INT32_MIN + 3, the core's and the
                                                   loader's bound) */
            table_row_line((ab_calc *)c, ui, (int32_t)((int64_t)c->tbl_k - 3 + i), ls[i]);
        k->table = k->x_left = true;
        k->xk = k->yk = SCREEN_VALUE;
        return;
    } else if (ui->notice) {                    /* a brief notice: OVERFLOW (unit 024 rule 3) */
        line_add(xl, ui->notice);
        k->x_left = true;
        k->xk = SCREEN_MESSAGE;
    } else if (c->viewing && c->view_id == AB_VIEW_TEST) {    /* a keyboard test: YES or NO (UG p.14-7) */
        line_add(xl, c->yes ? "YES" : "NO");
        k->x_left = true;
        k->xk = SCREEN_VIEW;
    } else if (c->viewing) {                    /* a keyboard VIEW: Variable=Value (UG p.3-4) */
        char name[16];
        ab_val v;
        memset(&v, 0, sizeof v);
        v.kind = AB_REAL;
        int id = c->view_id;
        if (id >= 0 && id <= 26) v = c->var[id];
        else if (id >= 1000 && id - 1000 <= AB_IND_MAX) v.re = c->ind[id - 1000];
        else if (id <= -27 && id >= -32) v.re = c->stat[-id - 27];
        else if (id == AB_VIEW_INTEG) v = c->x;                 /* unit 035: the integral's result */
        else if (id <= AB_VIEW_LIN2 && id >= AB_VIEW_LIN3 - 2)  /* a linear solution's X, Y, Z */
            v = c->var[23 + (id > AB_VIEW_LIN2 - 10 ? AB_VIEW_LIN2 - id : AB_VIEW_LIN3 - id)];
        var_name(id, name, sizeof name);
        line_add(xl, name);
        line_add_cp(xl, '=');
        val_line(&v, c, ui, xl, &unused);
        k->x_left = true;
        k->xk = SCREEN_VIEW;
    } else if ((m = ab_error_message(c)) != NULL) {
        line_add(xl, m);                        /* every message is one line (#3822) */
        k->x_left = true;
        k->xk = SCREEN_MESSAGE;
    } else if (ab_cas_report(c) && !c->prgm) { /* SOLVE's exact roots (unit 048): rows k-3..k, the cursor's on X,
                                                   as the table's; the status row first */
        line *ls[4] = {&k->t, &k->z, &k->y, &k->x};
        char row[AB_EQN_LEN];
        for (int i = 0; i < 4; i++)
            if (ab_cas_row(c, c->cas_row - 3 + i, row, sizeof row)) eqn_line(row, -1, ls[i]);
        k->view = k->x_left = true;
        k->xk = k->yk = SCREEN_MESSAGE;
        return;
    } else if (c->entry.active || c->cpart || c->vlen) {
        entry_line(c, ui->radix == ',' ? ',' : '.', xl);
        k->x_left = k->x_letter = true;
        k->xk = SCREEN_ENTRY;
    } else if (c->prgm && c->eqn_typing) {      /* an equation line being typed (UG p.13-7): the text and
                                                   its cursor, as Equation mode's typing (unit 030's review
                                                   found the line shown in its place) */
        eqn_line(c->ebuf, c->ecur, xl);
        k->x_left = true;
        k->xk = SCREEN_EQN;
    } else if (c->prgm) {                       /* the pointer's address and its line (list.c) */
        char a[16], t[AB_EQN_LEN];
        ab_prog_addr(c, c->pc, a);
        k->xk = SCREEN_PROGRAM;
        if (strcmp(a, "TOP") == 0) {
            line_add(xl, "PRGM TOP");
        } else {
            ab_display d = display_of(c, ui, FMT_WIDE);
            d.dec.mode = AB_ALL;                /* a typed number as typed: 14-19 lists 1.01 under FIX 4 */
            d.fdisp = false;
            ab_prog_line(c, c->pc, &d, t, sizeof t);
            line_add(xl, a);
            line_add_cp(xl, ' ');
            line_add(xl, t);
        }
        k->x_left = true;
    } else if (c->alg) {                        /* the result: the entry shown's, else ANS */
        val_line(c->hist_at >= 0 ? &c->hist_val[c->hist_at] : &c->ans, c, ui, xl, &k->ind);
    } else if (c->eqn_mode && c->eqn_typing) {
        eqn_line(c->ebuf, c->ecur, xl);
        k->x_left = true;
        k->xk = SCREEN_EQN;
    } else if (c->eqn_mode) {
        if (c->eqn_cur < 0) line_add(xl, "EQN LIST TOP");
        else {
            eqn_line(c->eqn[c->eqn_cur], -1, xl);
            line_add(xl, ab_eqn_mark(c, c->eqn[c->eqn_cur], c->eqn_model[c->eqn_cur]));   /* unit 031 rule 3 */
            if ((m = ab_cas_note(c)) != NULL && m[0] == '+') {
                line_add(xl, " +C");            /* unit 048: ∫'s +C beside the answer, never in its text, kept in
                                                   view when a long answer is cut (fresh eyes) */
                xl->tail = 3;
            }
        }
        k->x_left = true;
        k->xk = SCREEN_EQN;
        eqn_shown = c->eqn_cur >= 0;
        k->textbook = textbook_shown(c);
    } else if (c->eng_on && c->x.kind == AB_REAL && c->base == 10) {   /* unit 041 C (B4): ENG's view, until the next key */
        char buf[64];
        ab_display d = display_of(c, ui, FMT_WIDE);
        if (ab_format_eng_view(&c->x.re, c->eng_low, c->eng_e, &d.dec, buf, sizeof buf) < 0)
            snprintf(buf, sizeof buf, "?");
        line_add(xl, buf);
        k->x_letter = true;
    } else {
        val_line(&c->x, c, ui, xl, &k->ind);
        k->x_letter = true;
    }

    /* the Y line: a prompt or a VIEW takes it */
    char name[16];
    if (c->stop == AB_STOP_INPUT) {
        var_name(c->stop_var, name, sizeof name);
        line_add(&k->y, name);
        line_add_cp(&k->y, '?');
        k->prompt = true;
    } else if ((c->stop == AB_STOP_VIEW || (c->stop == AB_STOP_PSE && c->stop_var != -1 && c->stop_var != -2)) &&
               c->stop_var >= 0 && c->stop_var <= 26) {
        var_name(c->stop_var, name, sizeof name);
        line_add(&k->y, name);
        line_add_cp(&k->y, '=');
        val_line(&c->var[c->stop_var], c, ui, &k->y, &unused);
        k->prompt = true;
    } else {
        val_line(&c->y, c, ui, &k->y, &unused);
    }
    if (k->prompt) k->yk = SCREEN_PROMPT;
    if (eqn_shown && !k->prompt && (m = ab_cas_note(c)) != NULL && m[0] != '+') {
        memset(&k->y, 0, sizeof k->y);          /* unit 048: FACTOR's MAY FACTOR MORE above its answer, a message */
        k->y.cursor = -1;
        line_add(&k->y, m);
        k->prompt = true;
        k->yk = SCREEN_LABEL;                   /* a label, not an error (tally #5786) */
    }
    char dl[16];
    if (ab_cas_prompt_label(c, dl, sizeof dl)) {    /* 056, 057: which text is typed ("∫X FROM", "∫X TO", "÷ BY", "GCD WITH") */
        memset(&k->y, 0, sizeof k->y);
        k->y.cursor = -1;
        line_add(&k->y, dl);
        k->prompt = true;
        k->yk = SCREEN_PROMPT;
    }
    val_line(&c->z, c, ui, &k->z, &unused);
    val_line(&c->t, c, ui, &k->t, &unused);
    if (c->alg && !c->prgm) {
        /* STU's ALG (unit 029 proposal 3): the line, or the history entry shown, in Y's place; the
           entry before it in T and its value in Z (the newest's when a line is being typed) */
        int prev = c->hist_at >= 0 ? c->hist_at - 1 : c->alg_shown ? c->hist_n - 2 : c->hist_n - 1;
        k->alg = true;
        memset(&k->y, 0, sizeof k->y);
        memset(&k->z, 0, sizeof k->z);
        memset(&k->t, 0, sizeof k->t);
        k->y.cursor = k->z.cursor = k->t.cursor = -1;
        if (c->hist_at >= 0) eqn_line(c->hist[c->hist_at], -1, &k->y);
        else eqn_line(c->ebuf, c->alg_shown ? -1 : c->ecur, &k->y);
        k->yk = SCREEN_EQN;
        k->prompt = false;
        if (prev >= 0) {
            eqn_line(c->hist[prev], -1, &k->t);
            val_line(&c->hist_val[prev], c, ui, &k->z, &unused);
        }
    }
}

static void page_line(screen_line *o, const line *l, screen_kind kind, bool left, const char *label)
{
    snprintf(o->text, sizeof o->text, "%s", l->u8);
    o->cursor = l->cursor;
    o->kind = kind;
    o->left = left;
    o->label = label;
}

/* ---- unit 080: the textbook area ---- */

#define TB_TOP 22               /* the area: below the band's rule, above the soft keys, the width the stack lines had */
#define TB_BOT 204
#define TB_X 4
#define TB_W 392

/* The shown equation as the area lays it out: its tree (tb_screen) fitted at scale s, or the text's own lines when it
   can't be (s 0): their count, each one's height and the gap between them, and where the lines start (below a label) */
typedef struct {
    const char *text, *label;
    bool plus_c;
    int s, sc, n, gap, top;     /* sc: the text's scale (s 0) */
    int starts[24], lens[24];   /* the text's lines (s 0) */
} tb_area;

static void area_fit(const ab_calc *c, tb_area *a)
{
    const char *m = ab_cas_note(c);
    memset(a, 0, sizeof *a);
    a->text = c->eqn[c->eqn_cur];
    a->label = m && m[0] != '+' ? m : NULL;
    a->plus_c = m && m[0] == '+';
    a->top = a->label ? TB_TOP + 7 * 2 + 4 : TB_TOP;
    int w = TB_W - (a->plus_c ? 3 * 6 * 2 : 0);            /* room for " +C" after the last line */
    if (tb_parse(&tb_screen, a->text) == 0 && (a->s = tb_fit(&tb_screen, w, TB_BOT - a->top)) != 0) {
        a->n = tb_lines(&tb_screen), a->gap = 3 * a->s;
        return;
    }
    a->s = 0;                                               /* the text: at 3 when it fits a line, else at 2, wrapped */
    a->sc = screen_cells(a->text) <= (w + 3) / 18 ? 3 : 2;
    a->n = tb_text_lines(a->text, (w + a->sc) / (6 * a->sc), a->starts, a->lens, 24);
    a->gap = 3 * a->sc;
}

static int area_line_h(const tb_area *a, int i)
{
    return a->s ? tb_line_h(&tb_screen, i) : 7 * a->sc;
}

/* From line first, how many lines the area holds; *h their height */
static int area_holds(const tb_area *a, int first, int *h)
{
    int k = first, y = 0;
    for (; k < a->n; k++) {
        int lh = area_line_h(a, k) + (k > first ? a->gap : 0);
        if (a->top + y + lh > TB_BOT) break;
        y += lh;
    }
    *h = y;
    return k - first;
}

bool screen_drawn_text(const ab_calc *c, char *out, int size)
{
    if (!textbook_shown(c)) return false;
    if (tb_parse(&tb_screen, c->eqn[c->eqn_cur]) != 0 || tb_text(&tb_screen, false, out, size) < 0)
        snprintf(out, (size_t)size, "%s", c->eqn[c->eqn_cur]);
    return true;
}

bool screen_textbook_more(const ab_calc *c, int first)
{
    tb_area a;
    int h;
    if (!textbook_shown(c)) return false;
    area_fit(c, &a);
    return first + area_holds(&a, first, &h) < a.n;
}

/* The shown equation in 2D: whole, at the area's bottom as X sits; taller than the area, from line first (screen_ui's
   tb_line, the app's scroll), with ▲ when lines are above and ▼ when lines are below */
static void draw_textbook(ls027_frame *f, const ab_calc *c, const screen_ui *ui)
{
    tb_area a;                                  /* (on render's stack: internal RAM stays +0) */
    int h, first = ui->tb_line;
    area_fit(c, &a);
    if (first >= a.n) first = a.n - 1;
    if (first < 0) first = 0;
    int n = area_holds(&a, first, &h), top = first == 0 && first + n == a.n ? TB_BOT - h : a.top, last_w = 0;
    if (a.label) screen_text(f, TB_X, TB_TOP, 2, a.label, true);
    if (a.s) {
        tb_draw(&tb_screen, f, TB_X, top, TB_BOT, first);
        const tb_node *r = &tb_screen.n[tb_screen.root];
        last_w = r->kind == TB_ROWS ? 0 : r->w;
    } else {
        int sc = a.sc;
        for (int i = first; i < first + n; i++) {
            char b[AB_EQN_LEN];
            snprintf(b, sizeof b, "%.*s", a.lens[i], a.text + a.starts[i]);
            screen_text(f, TB_X, top, sc, b, true);
            last_w = 6 * sc * screen_cells(b) - sc;
            top += 7 * sc + a.gap;
        }
    }
    if (a.plus_c && first + n == a.n) {    /* ∫'s +C after the answer's last line, on its main row: the line's axis, not
                                              its box's middle, which an exponent raises (abacus #11101) */
        int sc = a.s ? a.s : a.sc, y = first == 0 && n == a.n ? TB_BOT - h : a.top, axis = 3 * sc;
        for (int i = first; i < a.n - 1; i++) y += area_line_h(&a, i) + a.gap;
        if (a.s) {
            const tb_node *l = &tb_screen.n[tb_screen.root];
            if (l->kind == TB_ROWS) {
                int k = tb_screen.root;
                while (tb_screen.n[k].b >= 0) k = tb_screen.n[k].b;
                l = &tb_screen.n[tb_screen.n[k].a];
            }
            last_w = l->w, axis = l->axis;
        }
        screen_text(f, TB_X + last_w + 3 * sc, y + axis - 3 * sc, sc, "+C", true);
    }
    if (first > 0) screen_text(f, TB_X + TB_W - 10, a.top, 2, "\xe2\x96\xb2", true);
    if (first + n < a.n) screen_text(f, TB_X + TB_W - 10, TB_BOT - 14, 2, "\xe2\x96\xbc", true);
}

void screen_lines(const ab_calc *c, const screen_ui *ui, screen_page *out)
{
    composed *k = &comp;
    screen_status *st = &out->status;
    compose(c, ui, k);
    memset(out, 0, sizeof *out);
    if (k->textbook) {                          /* unit 080: the area is drawn in the frame; X keeps the equation's text,
                                                   and the drawn text is beside it (tutor #10917) */
        out->textbook_on = true;
        page_line(&out->x, &k->x, k->xk, k->x_left, NULL);
        if (k->yk == SCREEN_LABEL) page_line(&out->y, &k->y, SCREEN_LABEL, true, NULL);    /* its label's text, as before */
        screen_drawn_text(c, out->drawn, sizeof out->drawn);
    } else if (k->table || k->show) {
        page_line(&out->t, &k->t, SCREEN_VALUE, true, NULL);
        page_line(&out->z, &k->z, SCREEN_VALUE, true, NULL);
        page_line(&out->y, &k->y, SCREEN_VALUE, true, NULL);
        page_line(&out->x, &k->x, SCREEN_VALUE, true, NULL);
    } else if (k->view) {
        out->view = true;
        page_line(&out->t, &k->t, SCREEN_MESSAGE, true, NULL);
        page_line(&out->z, &k->z, SCREEN_MESSAGE, true, NULL);
        page_line(&out->y, &k->y, SCREEN_MESSAGE, true, NULL);
        page_line(&out->x, &k->x, SCREEN_MESSAGE, true, NULL);
    } else if (k->alg) {
        page_line(&out->t, &k->t, SCREEN_EQN, true, NULL);
        page_line(&out->z, &k->z, SCREEN_VALUE, false, NULL);
        page_line(&out->y, &k->y, SCREEN_EQN, true, NULL);
        page_line(&out->x, &k->x, k->xk, k->x_left, NULL);
    } else {
        page_line(&out->t, &k->t, SCREEN_VALUE, false, "T");
        page_line(&out->z, &k->z, SCREEN_VALUE, false, "Z");
        page_line(&out->y, &k->y, k->yk, k->prompt, k->prompt ? NULL : "Y");
        page_line(&out->x, &k->x, k->xk, k->x_left, k->x_letter ? "X" : NULL);
    }
    if (c->gr_on && ui->graph) {               /* a graph's text (unit 033b) */
        out->graph_on = true;
        screen_graph_texts(c, ui, &out->graph);
    }
    /* the status band, as status_band draws it */
    st->shift = ui->shift == 1 || ui->shift == 2 ? ui->shift : 0;
    st->angle = c->angle == AB_ANGLE_RAD ? "RAD" : c->angle == AB_ANGLE_GRAD ? "GRAD" : NULL;
    st->base = c->base == 16 ? "HEX" : c->base == 8 ? "OCT" : c->base == 2 ? "BIN" : NULL;
    st->hyp = ui->hyp;                          /* unit 073 */
    st->prgm = c->prgm;
    st->entry = c->stu && !c->prgm ? (c->alg ? "ALG" : "RPN") : NULL;
    st->eqn = c->eqn_mode;
    st->frac = k->ind > 0 ? 1 : k->ind < 0 ? -1 : 0;
    int nf = 0;
    for (int n = 0; n <= 4; n++)
        if (ab_flag(c, n)) st->flags[nf++] = (char)('0' + n);
    st->model = c->stu ? "STU" : c->m35 ? "35" : "33";
    st->battery = ui->battery;
    st->charging = ui->charging;
    const char *parts[10];
    int np = 0;
    st->clock = ui->clock[0] ? ui->clock : NULL;
    if (st->shift) parts[np++] = st->shift == 1 ? "GOLD" : "BLUE";
    else if (st->clock) parts[np++] = st->clock;
    if (st->angle) parts[np++] = st->angle;
    if (st->base) parts[np++] = st->base;
    else if (st->hyp) parts[np++] = "HYP";     /* unit 073 */
    if (c->tbl_on) parts[np++] = "TABLE";      /* unit 032 */
    else if (c->gr_on) parts[np++] = "GRAPH";  /* unit 033 */
    else if (st->prgm) parts[np++] = "PRGM";
    else if (st->entry) parts[np++] = st->entry;
    if (st->eqn && !c->tbl_on && !c->gr_on) parts[np++] = "EQN";
    if (st->frac) parts[np++] = st->frac > 0 ? "\xe2\x96\xb2" : "\xe2\x96\xbc";
    if (nf) parts[np++] = st->flags;
    parts[np++] = st->model;
    for (int i = 0; i < np; i++) {
        size_t len = strlen(st->text);
        snprintf(st->text + len, sizeof st->text - len, "%s%s", i ? " " : "", parts[i]);
    }
    for (int i = 0; i < SCREEN_SOFT; i++) {
        out->soft[i] = ui->soft[i];
        out->soft_on[i] = ui->soft_on[i];
        out->soft_sub[i] = ui->soft_sub[i];
        out->soft_dim[i] = ui->soft_dim[i];
    }
}

void screen_render(const ab_calc *c, const screen_ui *ui, ls027_frame *f)
{
    composed *k = &comp;
    screen_clear(f);
    if (ui->blank) return;                      /* 068: the soft off */
    compose(c, ui, k);
    status_band(f, c, ui, k->ind);
    if (c->gr_on && ui->graph) {                /* a graph in place of the stack (unit 033) */
        draw_graph(f, c, ui);
        return;
    }
    if (k->textbook) {                          /* unit 080: the shown STU equation in 2D, in the stack's place */
        draw_textbook(f, c, ui);
        soft_band(f, ui);
        return;
    }
    if (k->table) {                             /* a table: four rows at scale 2 (unit 032) */
        draw_line(f, BOT_T, 2, &k->t, true);
        draw_line(f, BOT_Z, 2, &k->z, true);
        draw_line(f, BOT_Y, 2, &k->y, true);
        draw_line(f, BOT_X, 2, &k->x, true);
        soft_band(f, ui);
        return;
    }
    if (k->show) {                              /* SHOW: Y and X at scale 3 (18 cells hold a sign and 17
                                                   digits), T and Z at 2 */
        draw_line(f, BOT_T, 2, &k->t, true);
        draw_line(f, BOT_Z, 2, &k->z, true);
        draw_line(f, BOT_Y, 3, &k->y, true);
        draw_line(f, BOT_X, 3, &k->x, true);
        soft_band(f, ui);
        return;
    }
    if (k->view) {                              /* SYS's view: four lines at scale 2 */
        draw_line(f, BOT_T, 2, &k->t, true);
        draw_line(f, BOT_Z, 2, &k->z, true);
        draw_line(f, BOT_Y, 2, &k->y, true);
        draw_line(f, BOT_X, 2, &k->x, true);
        soft_band(f, ui);
        return;
    }
    if (k->alg) {                               /* STU's ALG: no letters; the lines' own sides */
        draw_line(f, BOT_T, 2, &k->t, true);
        draw_line(f, BOT_Z, 2, &k->z, false);
        draw_line(f, BOT_Y, 2, &k->y, true);
        draw_line(f, BOT_X, 4, &k->x, k->x_left);
        soft_band(f, ui);
        return;
    }
    draw_label(f, BOT_T, "T");
    draw_line(f, BOT_T, 2, &k->t, false);
    draw_label(f, BOT_Z, "Z");
    draw_line(f, BOT_Z, 2, &k->z, false);
    if (!k->prompt) draw_label(f, BOT_Y, "Y");
    draw_line(f, BOT_Y, k->prompt ? 3 : 2, &k->y, k->prompt);
    if (k->x_letter) draw_label(f, BOT_X, "X");     /* only when the line is X */
    draw_line(f, BOT_X, 4, &k->x, k->x_left);
    soft_band(f, ui);
}
