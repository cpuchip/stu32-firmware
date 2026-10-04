/* screen: layout v0 (see screen.h). */
#include "screen.h"

#include <stdio.h>
#include <string.h>

#define W LS027_W
#define H LS027_H
#define LINE_MAX_CP 256         /* code points in one line before it is fitted */
#define NUM_X 20                /* where a line's text may start: right of its letter */
#define NUM_R 398               /* and where it ends */
#define FMT_WIDTH 21            /* the formatter's width: what scale 3 holds (ours, v0) */
#define FMT_WIDE 31             /* the fall-back width, drawn at scale 2 */

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
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) put(f, i, j, dark);
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
} line;

static void line_add(line *l, const char *s)
{
    while (*s && l->n < LINE_MAX_CP) l->cp[l->n++] = next_cell(&s);
}

static void line_add_cp(line *l, uint32_t cp)
{
    if (l->n < LINE_MAX_CP) l->cp[l->n++] = cp;
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
    if (n > room) {
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

/* A variable's name as the prompts show it (stop_var's ids: A..Z, an unnamed address, the
   statistics registers); "" for none. */
static void var_name(int id, char *out, int size)
{
    static const char *const STATS[] = {"n", "\xce\xa3x", "\xce\xa3y", "\xce\xa3x\xc2\xb2", "\xce\xa3y\xc2\xb2",
                                        "\xce\xa3xy"};
    if (id >= 0 && id < 26) snprintf(out, (size_t)size, "%c", 'A' + id);
    else if (id >= 1000) snprintf(out, (size_t)size, "(%d)", id - 1000);
    else if (id <= -27 && id >= -32) snprintf(out, (size_t)size, "%s", STATS[-id - 27]);
    else if (size > 0) out[0] = '\0';
}

/* ---- the bands ---- */

static void status_band(ls027_frame *f, const ab_calc *c, const screen_ui *ui, int ind)
{
    if (ui->shift == 1 || ui->shift == 2) {     /* the shift held, light on dark */
        screen_fill(f, 0, 0, 52, 18, true);
        screen_text(f, 3, 2, 2, ui->shift == 1 ? "GOLD" : "BLUE", false);
    }
    if (c->angle == AB_ANGLE_RAD) screen_text(f, 58, 2, 2, "RAD", true);
    else if (c->angle == AB_ANGLE_GRAD) screen_text(f, 58, 2, 2, "GRAD", true);
    if (c->base != 10) screen_text(f, 112, 2, 2, c->base == 16 ? "HEX" : c->base == 8 ? "OCT" : "BIN", true);
    if (c->prgm) screen_text(f, 154, 2, 2, "PRGM", true);
    if (c->eqn_mode) screen_text(f, 208, 2, 2, "EQN", true);
    if (ind) screen_text(f, 250, 2, 2, ind > 0 ? "\xe2\x96\xb2" : "\xe2\x96\xbc", true);
    for (int n = 0; n <= 4; n++) {
        char d[2] = {(char)('0' + n), '\0'};
        if (ab_flag(c, n)) screen_text(f, 268 + n * 12, 2, 2, d, true);
    }
    screen_text(f, 338, 2, 2, c->m35 ? "35" : "33", true);   /* the mode (unit 017), the brief's annunciator */
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
    }
}

void screen_lines(const ab_calc *c, const screen_ui *ui, screen_page *out)
{
    (void)c;
    (void)ui;
    memset(out, 0, sizeof *out);
}

void screen_render(const ab_calc *c, const screen_ui *ui, ls027_frame *f)
{
    screen_clear(f);
    int ind = 0, unused;
    line x = {.n = 0, .cursor = -1};
    bool x_left = false, x_letter = false, prompt = false;
    const char *m;

    /* the X line, first that applies (screen.h) */
    if (ui->busy && (m = ab_busy_message((ab_op)ui->busy)) != NULL) {
        line_add(&x, m);
        x_left = true;
    } else if (ui->prompt) {                    /* an op waiting for its argument (app.h) */
        line_add(&x, ui->prompt);
        x_left = true;
    } else if ((m = ab_error_message(c)) != NULL) {
        line_add(&x, m);                        /* every message is one line (#3822) */
        x_left = true;
    } else if (c->entry.active || c->cpart || c->vlen) {
        entry_line(c, ui->radix == ',' ? ',' : '.', &x);
        x_left = x_letter = true;
    } else if (c->prgm) {                       /* the pointer's address and its line (list.c) */
        char a[16], t[AB_EQN_LEN];
        ab_prog_addr(c, c->pc, a);
        if (strcmp(a, "TOP") == 0) {
            line_add(&x, "PRGM TOP");
        } else {
            ab_display d = display_of(c, ui, FMT_WIDE);
            d.dec.mode = AB_ALL;                /* a typed number as typed: 14-19 lists 1.01 under FIX 4 */
            d.fdisp = false;
            ab_prog_line(c, c->pc, &d, t, sizeof t);
            line_add(&x, a);
            line_add_cp(&x, ' ');
            line_add(&x, t);
        }
        x_left = true;
    } else if (c->eqn_mode && c->eqn_typing) {
        eqn_line(c->ebuf, c->ecur, &x);
        x_left = true;
    } else if (c->eqn_mode) {
        if (c->eqn_cur < 0) line_add(&x, "EQN LIST TOP");
        else eqn_line(c->eqn[c->eqn_cur], -1, &x);
        x_left = true;
    } else {
        val_line(&c->x, c, ui, &x, &ind);
        x_letter = true;
    }

    /* the Y line: a prompt or a VIEW takes it */
    line y = {.n = 0, .cursor = -1};
    char name[16];
    if (c->stop == AB_STOP_INPUT) {
        var_name(c->stop_var, name, sizeof name);
        line_add(&y, name);
        line_add_cp(&y, '?');
        prompt = true;
    } else if ((c->stop == AB_STOP_VIEW || (c->stop == AB_STOP_PSE && c->stop_var != -1 && c->stop_var != -2)) &&
               c->stop_var >= 0 && c->stop_var < 26) {
        var_name(c->stop_var, name, sizeof name);
        line_add(&y, name);
        line_add_cp(&y, '=');
        val_line(&c->var[c->stop_var], c, ui, &y, &unused);
        prompt = true;
    } else {
        val_line(&c->y, c, ui, &y, &unused);
    }

    line z = {.n = 0, .cursor = -1}, t = {.n = 0, .cursor = -1};
    val_line(&c->z, c, ui, &z, &unused);
    val_line(&c->t, c, ui, &t, &unused);

    status_band(f, c, ui, ind);
    draw_label(f, BOT_T, "T");
    draw_line(f, BOT_T, 2, &t, false);
    draw_label(f, BOT_Z, "Z");
    draw_line(f, BOT_Z, 2, &z, false);
    if (!prompt) draw_label(f, BOT_Y, "Y");
    draw_line(f, BOT_Y, prompt ? 3 : 2, &y, prompt);
    if (x_letter) draw_label(f, BOT_X, "X");     /* only when the line is X */
    draw_line(f, BOT_X, 4, &x, x_left);
    soft_band(f, ui);
}
