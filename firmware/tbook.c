/* tbook: unit 080, the textbook display (tbook.h). */
#include "tbook.h"

#include <stdio.h>
#include <string.h>

#include "screen.h"

#ifndef AB_POOL_ATTR
#define AB_POOL_ATTR            /* the devices put the pool in PSRAM (device/pool_attr.h) */
#endif

tb_tree tb_screen AB_POOL_ATTR;
tb_look tb_looks AB_POOL_ATTR;  /* the blueprint's proposals until the glass check: (a), and XY with a gap */
#ifdef TB_MEASURE
int tb_depth_seen;
#endif

/* ---- the text ---- */

static uint32_t utf8(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    uint32_t c = *p++;
    if (c >= 0xF0) c = (c & 7u) << 18 | (p[0] & 63u) << 12 | (p[1] & 63u) << 6 | (p[2] & 63u), p += 3;
    else if (c >= 0xE0) c = (c & 15u) << 12 | (p[0] & 63u) << 6 | (p[1] & 63u), p += 2;
    else if (c >= 0xC0) c = (c & 31u) << 6 | (p[0] & 63u), p += 1;
    *s = (const char *)p;
    return c;
}

static int cells_n(const char *u, int len)
{
    int n = 0;
    for (const char *p = u; p < u + len;) utf8(&p), n++;
    return n;
}

static int text_w(int cells, int s) { return cells ? 6 * s * cells - s : 0; }

/* len bytes of UTF-8 at (x, top), scale s (out of line: its copy isn't on the recursion's frames) */
static __attribute__((noinline)) void slice(ls027_frame *f, const char *u, int len, int x, int top, int s)
{
    char b[48];
    snprintf(b, sizeof b, "%.*s", len, u);
    screen_text(f, x, top, s, b, true);
}

/* ---- the parser: an STU equation's text, × written in (031 rule 7), into lists ---- */

typedef struct { tb_tree *t; const char *p; int nest; } parser;

static int node(tb_tree *t, int kind, int a, int b)
{
    if (t->count >= TB_NODES) {
        t->err = 2;
        return -1;
    }
    tb_node *n = &t->n[t->count];
    memset(n, 0, sizeof *n);
    n->kind = (uint8_t)kind, n->a = (int16_t)a, n->b = (int16_t)b;
    return t->count++;
}

static uint32_t peek(parser *ps)
{
    while (*ps->p == ' ') ps->p++;
    const char *q = ps->p;
    return *q ? utf8(&q) : 0;
}

static void take(parser *ps)
{
    peek(ps);
    utf8(&ps->p);
}

static bool is_minus(uint32_t c) { return c == '-' || c == 0x2212; }
static bool is_name(uint32_t c) { return c >= 'A' && c <= 'Z'; }

static int expr(parser *ps);

/* a group's, a function's or an exponent's operand, one level deeper: past TB_DEPTH, err 3 (its layout couldn't draw
   it anyway, and the parse's own recursion stays bounded) */
static int deeper(parser *ps, int (*f)(parser *))
{
    if (++ps->nest > TB_DEPTH) {
        ps->t->err = 3;
        return -1;
    }
    int a = f(ps);
    ps->nest--;
    return a;
}

static int leaf(parser *ps, int kind, const char *at, const char *end)
{
    int n = node(ps->t, kind, -1, -1);
    if (n >= 0) ps->t->n[n].src = (uint16_t)(at - ps->t->src), ps->t->n[n].len = (uint16_t)(end - at);
    ps->p = end;
    return n;
}

static int closing(parser *ps, int a)
{
    if (a < 0) return -1;
    if (peek(ps) != ')') {
        ps->t->err = ps->t->err ? ps->t->err : 1;
        return -1;
    }
    take(ps);
    return a;
}

static int primary(parser *ps)
{
    uint32_t c = peek(ps);
    const char *at = ps->p, *q = at;
    if ((c >= '0' && c <= '9') || c == '.') {
        while ((*q >= '0' && *q <= '9') || *q == '.') q++;
        if (*q == 'E' && (q[1] == '-' || (q[1] >= '0' && q[1] <= '9'))) {
            q += 2;
            while (*q >= '0' && *q <= '9') q++;
        }
        return leaf(ps, TB_NUM, at, q);
    }
    if (c == 0x03C0 || c == 0x221E) {           /* π, ∞ */
        utf8(&q);
        return leaf(ps, TB_SYM, at, q);
    }
    if (is_name(c)) {
        while (is_name((unsigned char)*q) || (*q >= '0' && *q <= '9')) q++;
        if (*q != '(') return leaf(ps, TB_SYM, at, q);
        ps->p = q + 1;
        int a = closing(ps, deeper(ps, expr));
        if (a < 0) return -1;
        int n = node(ps->t, TB_FN, a, -1);
        if (n >= 0) ps->t->n[n].src = (uint16_t)(at - ps->t->src), ps->t->n[n].len = (uint16_t)(q - at);
        return n;
    }
    if (c == '(') {
        take(ps);
        return closing(ps, deeper(ps, expr));
    }
    ps->t->err = 1;
    return -1;
}

static int power(parser *ps);

static int exponent(parser *ps)
{
    if (!is_minus(peek(ps))) return power(ps);
    take(ps);
    int b = power(ps);
    return b < 0 ? -1 : node(ps->t, TB_NEG, b, -1);
}

static int power(parser *ps)
{
    int a = primary(ps);
    if (a < 0 || peek(ps) != '^') return a;
    take(ps);
    int b = deeper(ps, exponent);               /* right to left (031 rule 1) */
    return b < 0 ? -1 : node(ps->t, TB_POW, a, b);
}

/* f appended to the list of the PROD or SUM at n, as an ITEM with op */
static int append(tb_tree *t, int n, int f, int op)
{
    int it = node(t, TB_ITEM, f, -1), k = t->n[n].a;
    if (it < 0) return -1;
    t->n[it].op = (uint8_t)op;
    while (t->n[k].b >= 0) k = t->n[k].b;
    t->n[k].b = (int16_t)it;
    return n;
}

static int list_of(tb_tree *t, int kind, int first, int op)
{
    int it = node(t, TB_ITEM, first, -1), n = it < 0 ? -1 : node(t, kind, it, -1);
    if (n >= 0) t->n[it].op = (uint8_t)op;
    return n;
}

static int product(tb_tree *t, int acc, int f)
{
    return t->n[acc].kind == TB_PROD ? append(t, acc, f, 0) : (acc = list_of(t, TB_PROD, acc, 0)) < 0 ? -1 : append(t, acc, f, 0);
}

/* A term: a unary minus takes the whole product or quotient after it (-X^2÷4 is -(X^2÷4)), as a textbook writes it;
   a÷b÷c is a over b·c, never a fraction over a fraction */
static int term(parser *ps)
{
    tb_tree *t = ps->t;
    if (is_minus(peek(ps))) {
        take(ps);
        int a = term(ps);
        return a < 0 ? -1 : node(t, TB_NEG, a, -1);
    }
    int acc = power(ps);
    for (;;) {
        uint32_t c = peek(ps);
        if (acc < 0 || (c != 0xD7 && c != 0xF7)) return acc;
        take(ps);
        int f = power(ps);
        if (f < 0) return -1;
        if (c == 0xD7) acc = product(t, acc, f);
        else if (t->n[acc].kind == TB_DIV) {
            int d = product(t, t->n[acc].b, f);
            if (d < 0) return -1;
            t->n[acc].b = (int16_t)d;
        } else acc = node(t, TB_DIV, acc, f);
    }
}

static int expr(parser *ps)
{
    tb_tree *t = ps->t;
    int a = term(ps), sum = -1;
    for (;;) {
        uint32_t c = peek(ps);
        if (a < 0 || (c != '+' && !is_minus(c))) return sum >= 0 ? sum : a;
        take(ps);
        int b = term(ps);
        if (b < 0) return -1;
        if (sum < 0 && (sum = list_of(t, TB_SUM, a, 0)) < 0) return -1;
        if (append(t, sum, b, c == '+' ? '+' : '-') < 0) return -1;
    }
}

static const char *const REL[] = {"=", "<", ">", "\xe2\x89\xa0", "\xe2\x89\xa4", "\xe2\x89\xa5"};

static int relation(parser *ps)
{
    int a = expr(ps);
    uint32_t c = peek(ps);
    int op = c == '=' ? 0 : c == '<' ? 1 : c == '>' ? 2 : c == 0x2260 ? 3 : c == 0x2264 ? 4 : c == 0x2265 ? 5 : -1;
    if (a < 0 || op < 0) return a;
    take(ps);
    int b = expr(ps), n = b < 0 ? -1 : node(ps->t, TB_REL, a, b);
    if (n >= 0) ps->t->n[n].op = (uint8_t)op;
    return n;
}

int tb_parse(tb_tree *t, const char *s)
{
    parser ps = {t, s, 0};
    t->src = s, t->count = 0, t->ncond = 0, t->err = 0;
    t->root = relation(&ps);
    while (t->root >= 0 && peek(&ps) == ';') {
        take(&ps);
        int c = relation(&ps);
        if (c < 0) break;
        if (t->ncond < TB_CONDS) t->conds[t->ncond++] = c;
    }
    if (t->root >= 0 && !t->err && peek(&ps) != 0) t->err = 1;
    if (t->root < 0 && !t->err) t->err = 1;
    return t->err ? -1 : 0;
}

/* ---- what a node shows at its edges, and what goes between two factors ---- */

static bool name_is(const tb_tree *t, int n, const char *name)
{
    return t->n[n].len == strlen(name) && memcmp(t->src + t->n[n].src, name, t->n[n].len) == 0;
}

static bool is_fn(const tb_tree *t, int n, const char *name) { return t->n[n].kind == TB_FN && name_is(t, n, name); }

/* Whether c needs brackets as a factor (side 0) of a product, an operand of a minus, or a power's base: a fraction or
   an exponent groups by its shape, so those never do */
static bool bracketed(const tb_tree *t, int parent, int c)
{
    int k = t->n[c].kind;
    if (parent == TB_POW)
        return k == TB_SUM || k == TB_NEG || k == TB_PROD || k == TB_DIV || k == TB_POW ||
               (k == TB_NUM && memchr(t->src + t->n[c].src, '-', t->n[c].len));
    return k == TB_SUM || k == TB_NEG;
}

/* The first and last mark a node shows: 'd' a digit, 'a' a letter, 'E' the letter E's start (a variable or a function
   word, typed after a number it's an exponent: 031 and abacus #10907), 'w' a function word, 'e' e^u, '(' a bracket,
   bar, radical or fraction, '-' a minus, '^' an exponent */
static int lead(const tb_tree *t, int n)
{
    const tb_node *x = &t->n[n];
    switch (x->kind) {
    case TB_NUM: return 'd';
    case TB_SYM: return t->src[x->src] == 'E' ? 'E' : 'a';
    case TB_FN:
        if (is_fn(t, n, "SQRT") || is_fn(t, n, "ABS")) return '(';
        if (is_fn(t, n, "EXP")) return 'e';
        return t->src[x->src] == 'E' ? 'E' : 'w';
    case TB_PROD: {
        int f = t->n[x->a].a;
        return bracketed(t, TB_PROD, f) ? '(' : lead(t, f);
    }
    case TB_POW: return bracketed(t, TB_POW, x->a) ? '(' : lead(t, x->a);
    case TB_NEG: return '-';
    default: return '(';
    }
}

static int trail(const tb_tree *t, int n)
{
    const tb_node *x = &t->n[n];
    switch (x->kind) {
    case TB_NUM: return 'd';
    case TB_SYM: return 'a';
    case TB_PROD: {
        int k = x->a;
        while (t->n[k].b >= 0) k = t->n[k].b;
        return bracketed(t, TB_PROD, t->n[k].a) ? '(' : trail(t, t->n[k].a);
    }
    case TB_POW: return '^';
    case TB_NEG: return trail(t, x->a);
    case TB_FN: return is_fn(t, n, "EXP") ? '^' : '(';
    default: return '(';
    }
}

/* Between two factors l and r (their trail and lead, r's node): SEP_NONE side by side, SEP_WORD a gap before a
   function word, SEP_DOT a centred dot where × must stay. × is dropped exactly where 031's parser reads the product
   back (rule 5: after a number, a ')' or a letter, before a letter-start operand or a '('); it stays before a number
   (rule D) and, after a number, before E (abacus #10907: 3E is an exponent being typed); a number before a fraction
   keeps it too, as 3·½ isn't a mixed number. */
enum { SEP_NONE, SEP_WORD, SEP_DOT };

static int sep(const tb_tree *t, int l, int r, int rn)
{
    if (r == 'd' || r == '-') return SEP_DOT;
    if (l == 'd' && (r == 'E' || t->n[rn].kind == TB_DIV)) return SEP_DOT;
    if (r == 'w' || r == 'E') return l == 'd' ? SEP_NONE : SEP_WORD;
    return SEP_NONE;
}

/* ---- the layout ---- */

#define AXIS(s) (3 * (s))       /* the face's minus is row 3: a fraction bar lines up with it */
#define EGAP(s) ((s) > 1 ? 2 : 1)     /* between a base and its exponent */
#define LGAP(s) (3 * (s))       /* between wrapped lines */
#define OPGAP(s) (2 * (s))      /* either side of + and − */
#define FRGAP(s) (4 * (s))      /* either side of a minus beside a fraction: it doesn't read as an underscore */

static int expo_scale(int s) { return s > 1 ? s - 1 : 1; }

static void box(tb_node *x, int w, int h, int axis) { x->w = (int16_t)w, x->h = (int16_t)h, x->axis = (int16_t)axis; }

/* a row being laid out or drawn: its pieces side by side on one axis */
typedef struct { int w, ax, below; } rowbox;

static void row_add(rowbox *r, int w, int h, int ax, int gap)
{
    r->w += (r->w ? gap : 0) + w;
    if (ax > r->ax) r->ax = ax;
    if (h - ax > r->below) r->below = h - ax;
}

static int layout(tb_tree *t, int n, int s, int d);

/* a child laid out at depth d + 1, its box widened by brackets when it takes them */
static void child_row(tb_tree *t, rowbox *r, int c, int s, int d, bool br, int gap)
{
    layout(t, c, s, d + 1);
    const tb_node *x = &t->n[c];
    if (!br) {
        row_add(r, x->w, x->h, x->axis, gap);
        return;
    }
    int extra = x->h > 7 * s ? s : 0, h = (x->h < 7 * s ? 7 * s : x->h) + 2 * extra;
    int ax = (x->h < 7 * s ? AXIS(s) : x->axis) + extra;
    row_add(r, x->w + 2 * (3 * s + s), h > x->h ? h : x->h, ax > x->axis ? ax : x->axis, gap);
}

static int sep_w(int k, int s)
{
    return k == SEP_DOT ? 2 * s + text_w(1, s) : k == SEP_WORD ? 3 * s : !tb_looks.letter_touch;
}

static bool beside_fraction(const tb_tree *t, int it)
{
    return t->n[t->n[it].a].kind == TB_DIV || (t->n[it].b >= 0 && t->n[t->n[t->n[it].b].a].kind == TB_DIV);
}

static int layout(tb_tree *t, int n, int s, int d)
{
    tb_node *x = &t->n[n];
    rowbox r = {0, 0, 0};
    if (d > TB_DEPTH) {
        t->err = 3;
        return 0;
    }
    x->scale = (uint8_t)s;
#ifdef TB_MEASURE
    if (d > tb_depth_seen) tb_depth_seen = d;
#endif
    switch (x->kind) {
    case TB_NUM:
    case TB_SYM: box(x, text_w(cells_n(t->src + x->src, x->len), s), 7 * s, AXIS(s)); break;
    case TB_SUM:
        for (int it = x->a, prev = -1; it >= 0; prev = it, it = t->n[it].b) {
            int op = t->n[it].op, g = prev >= 0 && op == '-' && (beside_fraction(t, prev)) ? FRGAP(s) : OPGAP(s);
            if (op) row_add(&r, text_w(1, s), 7 * s, AXIS(s), g);
            child_row(t, &r, t->n[it].a, s, d, op && t->n[t->n[it].a].kind == TB_NEG, op ? g : s);
        }
        box(x, r.w, r.ax + r.below, r.ax);
        break;
    case TB_PROD:
        for (int it = x->a, prev = -1; it >= 0; prev = it, it = t->n[it].b) {
            int f = t->n[it].a, br = bracketed(t, TB_PROD, f), g = s;
            if (prev >= 0) {
                int pf = t->n[prev].a, l = bracketed(t, TB_PROD, pf) ? '(' : trail(t, pf), k = sep(t, l, br ? '(' : lead(t, f), f);
                g = s + sep_w(k, s);
            }
            child_row(t, &r, f, s, d, br, g);
        }
        box(x, r.w, r.ax + r.below, r.ax);
        break;
    case TB_NEG:
        row_add(&r, text_w(1, s), 7 * s, AXIS(s), 0);
        child_row(t, &r, x->a, s, d, bracketed(t, TB_NEG, x->a), s);
        box(x, r.w, r.ax + r.below, r.ax);
        break;
    case TB_REL:
        child_row(t, &r, x->a, s, d, false, 0);
        row_add(&r, text_w(1, s), 7 * s, AXIS(s), OPGAP(s));
        child_row(t, &r, x->b, s, d, false, OPGAP(s));
        box(x, r.w, r.ax + r.below, r.ax);
        break;
    case TB_DIV: {
        const tb_node *a = &t->n[x->a], *b = &t->n[x->b];
        layout(t, x->a, s, d + 1), layout(t, x->b, s, d + 1);
        box(x, (a->w > b->w ? a->w : b->w) + 2 * s, a->h + 3 * s + b->h, a->h + s);
        break;
    }
    case TB_POW:
    case TB_FN: {
        if (x->kind == TB_FN && is_fn(t, n, "SQRT")) {
            const tb_node *a = &t->n[x->a];
            layout(t, x->a, s, d + 1);
            box(x, 5 * s + a->w + s, 2 * s + a->h, 2 * s + a->axis);
            break;
        }
        if (x->kind == TB_FN && is_fn(t, n, "ABS")) {     /* bars taller than a letter: never an I */
            const tb_node *a = &t->n[x->a];
            layout(t, x->a, s, d + 1);
            int h = (a->h < 7 * s ? 7 * s : a->h) + 2 * s;
            box(x, a->w + 2 * (s + s), h, (a->h < 7 * s ? AXIS(s) : a->axis) + s);
            break;
        }
        if (x->kind == TB_FN && !is_fn(t, n, "EXP")) {    /* NAME(arg), the brackets sized to it; LN |u|, the bars
                                                             its brackets (the prototype's) */
            row_add(&r, text_w(cells_n(t->src + x->src, x->len), s), 7 * s, AXIS(s), 0);
            child_row(t, &r, x->a, s, d, !is_fn(t, x->a, "ABS"), is_fn(t, x->a, "ABS") ? 2 * s : s);
            box(x, r.w, r.ax + r.below, r.ax);
            break;
        }
        /* a power, or e^u */
        int e = x->kind == TB_POW ? x->b : x->a, bw = text_w(1, s), bh = 7 * s, bax = AXIS(s);
        if (x->kind == TB_POW) {
            child_row(t, &r, x->a, s, d, bracketed(t, TB_POW, x->a), 0);
            bw = r.w, bh = r.ax + r.below, bax = r.ax;
        }
        layout(t, e, expo_scale(s), d + 1);
        int eh = t->n[e].h, extra = eh > AXIS(s) ? eh - AXIS(s) : 0;
        box(x, bw + EGAP(s) + t->n[e].w, extra + bh, extra + bax);
        break;
    }
    case TB_ROWS: {
        int w = 0, h = 0;
        for (int k = n; k >= 0; k = t->n[k].b) {
            layout(t, t->n[k].a, s, d + 1);
            if (t->n[t->n[k].a].w > w) w = t->n[t->n[k].a].w;
            h += (h ? LGAP(s) : 0) + t->n[t->n[k].a].h;
        }
        box(&t->n[n], w, h, t->n[t->n[n].a].axis);
        break;
    }
    }
    return t->n[n].w;
}

int tb_fit(tb_tree *t, int maxw, int maxh)
{
    for (int s = 3; s >= 2; s--) {
        int w = layout(t, t->root, s, 1);
        if (t->err) return 0;
        if (w <= maxw && t->n[t->root].h <= maxh) return s;
    }
    if (t->n[t->root].kind != TB_SUM) return 0;
    /* at 2, wrapped at the top-level + and −: each line a SUM of its own, a continuation starting with its sign */
    int rows = -1, last = -1, it = t->n[t->root].a;
    while (it >= 0) {
        int line = node(t, TB_SUM, it, -1), w = 0, end = it;
        if (line < 0) return 0;
        for (int k = it; k >= 0; k = t->n[k].b) {
            int g = t->n[k].op ? OPGAP(2) : 0, tw = layout(t, t->n[k].a, 2, 2) + (t->n[k].op ? text_w(1, 2) + 2 * g : 0);
            if (t->err) return 0;
            if (k != it && w + tw > maxw) break;
            w += tw, end = k;
        }
        it = t->n[end].b;
        t->n[end].b = -1;
        int row = node(t, TB_ROWS, line, -1);
        if (row < 0) return 0;
        if (last >= 0) t->n[last].b = (int16_t)row;
        else rows = row;
        last = row;
    }
    t->root = rows;
    for (int k = rows; k >= 0; k = t->n[k].b)
        if (layout(t, t->n[k].a, 2, 2) > maxw || t->n[t->n[k].a].h > maxh || t->err) return 0;
    layout(t, rows, 2, 1);
    return t->err ? 0 : 2;
}

int tb_lines(const tb_tree *t)
{
    int n = 0;
    if (t->n[t->root].kind != TB_ROWS) return 1;
    for (int k = t->root; k >= 0; k = t->n[k].b) n++;
    return n;
}

int tb_line_h(const tb_tree *t, int i)
{
    if (t->n[t->root].kind != TB_ROWS) return t->n[t->root].h;
    int k = t->root;
    while (i-- > 0 && t->n[k].b >= 0) k = t->n[k].b;
    return t->n[t->n[k].a].h;
}

/* ---- drawing ---- */

static void stroke(ls027_frame *f, int x0, int y0, int x1, int y1, int s)
{
    int dx = x1 - x0, dy = y1 - y0, ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, n = ax > ay ? ax : ay;
    for (int i = 0; i <= n; i++) {
        int x = n ? x0 + (dx * i + (dx >= 0 ? n / 2 : -n / 2)) / n : x0, y = n ? y0 + (dy * i + (dy >= 0 ? n / 2 : -n / 2)) / n : y0;
        screen_fill(f, x, y, s, s, true);
    }
}

static void bracket(ls027_frame *f, bool left, int x, int top, int h, int s)
{
    int ramp = h / 4 > 2 * s ? h / 4 : 2 * s;
    for (int y = 0; y < h; y++) {
        int d = y < ramp ? (ramp - y) * 2 * s / ramp : y >= h - ramp ? (y - (h - ramp - 1)) * 2 * s / ramp : 0;
        screen_fill(f, left ? x + d : x + 2 * s - d, top + y, s, 1, true);
    }
}

static void draw(tb_tree *t, int n, ls027_frame *f, int x, int top);

/* a child at x on the row's axis ax (its top at top), in brackets when br; returns the x after it */
static int child_draw(tb_tree *t, int c, ls027_frame *f, int x, int top, int ax, bool br)
{
    const tb_node *k = &t->n[c];
    int s = k->scale;
    if (!br) {
        draw(t, c, f, x, top + ax - k->axis);
        return x + k->w;
    }
    int extra = k->h > 7 * s ? s : 0, h = (k->h < 7 * s ? 7 * s : k->h) + 2 * extra;
    int bax = (k->h < 7 * s ? AXIS(s) : k->axis) + extra;
    bracket(f, true, x, top + ax - bax, h, s);
    draw(t, c, f, x + 4 * s, top + ax - k->axis);
    bracket(f, false, x + 4 * s + k->w + s, top + ax - bax, h, s);
    return x + k->w + 2 * (3 * s + s);
}

static void glyph_row(ls027_frame *f, const char *u, int x, int top, int ax, int s, int up)
{
    screen_text(f, x, top + ax - AXIS(s) - up, s, u, true);
}

static void draw(tb_tree *t, int n, ls027_frame *f, int x, int top)
{
    tb_node *nd = &t->n[n];
    int s = nd->scale, ax = nd->axis;
    switch (nd->kind) {
    case TB_NUM:
    case TB_SYM: slice(f, t->src + nd->src, nd->len, x, top, s); return;
    case TB_SUM:
        for (int it = nd->a, prev = -1; it >= 0; prev = it, it = t->n[it].b) {
            int op = t->n[it].op, frac = prev >= 0 && op == '-' && beside_fraction(t, prev);
            int g = frac ? FRGAP(s) : OPGAP(s);
            if (op) {
                if (prev >= 0) x += g;
                glyph_row(f, op == '+' ? "+" : "-", x, top, ax, s, frac ? tb_looks.minus_up : 0);
                x += text_w(1, s) + g;
            }
            x = child_draw(t, t->n[it].a, f, x, top, ax, op && t->n[t->n[it].a].kind == TB_NEG);
        }
        return;
    case TB_PROD:
        for (int it = nd->a, prev = -1; it >= 0; prev = it, it = t->n[it].b) {
            int fc = t->n[it].a, br = bracketed(t, TB_PROD, fc);
            if (prev >= 0) {
                int pf = t->n[prev].a, l = bracketed(t, TB_PROD, pf) ? '(' : trail(t, pf), k = sep(t, l, br ? '(' : lead(t, fc), fc);
                x += s;
                if (k == SEP_DOT) {
                    glyph_row(f, "\xc2\xb7", x + s, top, ax, s, 0);
                    x += 2 * s + text_w(1, s);
                } else x += sep_w(k, s);
            }
            x = child_draw(t, fc, f, x, top, ax, br);
        }
        return;
    case TB_NEG:
        glyph_row(f, "-", x, top, ax, s, 0);
        child_draw(t, nd->a, f, x + text_w(1, s) + s, top, ax, bracketed(t, TB_NEG, nd->a));
        return;
    case TB_REL:
        x = child_draw(t, nd->a, f, x, top, ax, false) + OPGAP(s);
        glyph_row(f, REL[nd->op], x, top, ax, s, 0);
        child_draw(t, nd->b, f, x + text_w(1, s) + OPGAP(s), top, ax, false);
        return;
    case TB_DIV: {
        const tb_node *a = &t->n[nd->a], *b = &t->n[nd->b];
        draw(t, nd->a, f, x + (nd->w - a->w) / 2, top);
        screen_fill(f, x + s / 2, top + ax, nd->w - 2 * (s / 2), s, true);
        draw(t, nd->b, f, x + (nd->w - b->w) / 2, top + ax + 2 * s);
        return;
    }
    case TB_ROWS:
        for (int k = n; k >= 0; k = t->n[k].b) {
            draw(t, t->n[k].a, f, x, top);
            top += t->n[t->n[k].a].h + LGAP(s);
        }
        return;
    }
    if (nd->kind == TB_FN && is_fn(t, n, "SQRT")) {
        const tb_node *a = &t->n[nd->a];
        int h = nd->h, rw = 5 * s;
        stroke(f, x, top + h - 4 * s, x + s, top + h - 4 * s, s);       /* the tick */
        stroke(f, x + s, top + h - 4 * s, x + 2 * s, top + h - s, s);   /* down */
        stroke(f, x + 2 * s, top + h - s, x + rw - s, top, s);          /* up to the bar */
        screen_fill(f, x + rw - s, top, a->w + 2 * s, s, true);         /* the overbar */
        draw(t, nd->a, f, x + rw, top + 2 * s);
        return;
    }
    if (nd->kind == TB_FN && is_fn(t, n, "ABS")) {
        const tb_node *a = &t->n[nd->a];
        screen_fill(f, x, top, s, nd->h, true);
        draw(t, nd->a, f, x + 2 * s, top + ax - a->axis);
        screen_fill(f, x + 2 * s + a->w + s, top, s, nd->h, true);
        return;
    }
    if (nd->kind == TB_FN && !is_fn(t, n, "EXP")) {
        bool bars = is_fn(t, nd->a, "ABS");
        slice(f, t->src + nd->src, nd->len, x, top + ax - AXIS(s), s);
        child_draw(t, nd->a, f, x + text_w(cells_n(t->src + nd->src, nd->len), s) + (bars ? 2 * s : s), top, ax, !bars);
        return;
    }
    /* a power, or e^u */
    int e = nd->kind == TB_POW ? nd->b : nd->a, eh = t->n[e].h, extra = eh > AXIS(s) ? eh - AXIS(s) : 0, bw;
    if (nd->kind == TB_POW) {
        int br = bracketed(t, TB_POW, nd->a);
        bw = child_draw(t, nd->a, f, x, top, ax, br) - x;
    } else {
        screen_text(f, x, top + extra, s, "e", true);
        bw = text_w(1, s);
    }
    draw(t, e, f, x + bw + EGAP(s), top + extra + AXIS(s) - eh);
}

int tb_draw(tb_tree *t, ls027_frame *f, int x, int top, int bottom, int first)
{
    if (t->n[t->root].kind != TB_ROWS) {
        if (first > 0 || top + t->n[t->root].h > bottom) return 0;
        draw(t, t->root, f, x, top);
        return 1;
    }
    int i = 0, k = t->root;
    for (; k >= 0 && i < first; k = t->n[k].b) i++;
    for (; k >= 0; k = t->n[k].b, i++) {
        const tb_node *l = &t->n[t->n[k].a];
        if (top + l->h > bottom) return i;
        draw(t, t->n[k].a, f, x, top);
        top += l->h + LGAP(t->n[k].scale ? t->n[k].scale : 2);
    }
    return i;
}

int tb_cond_fit(tb_tree *t, int s, int *h)
{
    int w = 0;
    *h = 0;
    for (int i = 0; i < t->ncond; i++) {
        w += (i ? text_w(2, s) + s : 0) + layout(t, t->conds[i], s, 1);
        if (t->n[t->conds[i]].h > *h) *h = t->n[t->conds[i]].h;
    }
    return w;
}

void tb_cond_draw(tb_tree *t, ls027_frame *f, int x, int top)
{
    for (int i = 0; i < t->ncond; i++) {
        const tb_node *c = &t->n[t->conds[i]];
        if (i) x += screen_text(f, x, top, c->scale, "; ", true) * 6 * c->scale;
        draw(t, t->conds[i], f, x, top);
        x += c->w;
    }
}

/* ---- the texts: what's drawn, and the readback ---- */

typedef struct { char *o; int n, size; bool full; } out_t;

static void put(out_t *w, const char *s, int len)
{
    if (len < 0) len = (int)strlen(s);
    if (w->n + len >= w->size) {
        w->full = true;
        return;
    }
    memcpy(w->o + w->n, s, (size_t)len);
    w->n += len;
    w->o[w->n] = '\0';
}

static bool atomic(const tb_tree *t, int n)
{
    int k = t->n[n].kind;
    return k == TB_NUM || k == TB_SYM || (k == TB_FN && !is_fn(t, n, "EXP"));
}

static bool part(const tb_tree *t, int n)
{
    int k = t->n[n].kind;
    return k == TB_SUM || k == TB_PROD || k == TB_NEG || k == TB_DIV;
}

static void text(const tb_tree *t, int n, bool rb, out_t *w);

/* The readback between two factors drawn side by side: nothing only where 031's parser reads the pair back as typed
   (a number before a letter, a word or a '('; a ')' before anything; a letter before a letter, a '(' or a fraction);
   elsewhere the ×: a gap before a word or the dot (k), after an exponent or e^u, EXP read back after a number (rule F)
   or a letter, and SQRT or ABS read back after a letter (rule E's longest name could swallow it) */
static bool rb_times(const tb_tree *t, int k, int l, int r, int fc, bool br)
{
    if (k != SEP_NONE || l == '^') return true;
    if (l == '(') return false;
    if (r == 'e') return true;
    if (l == 'a') return !(r == 'a' || br || t->n[fc].kind == TB_DIV);
    return false;
}

static void paren_text(const tb_tree *t, int n, bool rb, out_t *w, bool br)
{
    if (br) put(w, "(", 1);
    text(t, n, rb, w);
    if (br) put(w, ")", 1);
}

static void text(const tb_tree *t, int n, bool rb, out_t *w)
{
    const tb_node *x = &t->n[n];
    switch (x->kind) {
    case TB_NUM:
    case TB_SYM: put(w, t->src + x->src, x->len); return;
    case TB_SUM:
        for (int it = x->a; it >= 0; it = t->n[it].b) {
            if (t->n[it].op) put(w, t->n[it].op == '+' ? "+" : "-", 1);
            paren_text(t, t->n[it].a, rb, w, t->n[it].op && t->n[t->n[it].a].kind == TB_NEG);
        }
        return;
    case TB_PROD:
        for (int it = x->a, prev = -1; it >= 0; prev = it, it = t->n[it].b) {
            int fc = t->n[it].a, br = bracketed(t, TB_PROD, fc);
            if (prev >= 0) {
                int pf = t->n[prev].a, l = bracketed(t, TB_PROD, pf) ? '(' : trail(t, pf), r = br ? '(' : lead(t, fc);
                int k = sep(t, l, r, fc);
                if (!rb) put(w, k == SEP_DOT ? "\xc2\xb7" : k == SEP_WORD ? " " : "", -1);
                else if (rb_times(t, k, l, r, fc, br)) put(w, "\xc3\x97", 2);
            }
            paren_text(t, fc, rb, w, br);
        }
        return;
    case TB_NEG:
        put(w, "-", 1);
        paren_text(t, x->a, rb, w, bracketed(t, TB_NEG, x->a));
        return;
    case TB_REL:
        text(t, x->a, rb, w);
        put(w, REL[x->op], -1);
        text(t, x->b, rb, w);
        return;
    case TB_DIV:            /* drawn: a part bracketed when it's a sum, a product, a minus or a fraction */
        paren_text(t, x->a, rb, w, rb || part(t, x->a));
        put(w, "\xc3\xb7", 2);
        paren_text(t, x->b, rb, w, rb || part(t, x->b));
        return;
    case TB_POW:
        paren_text(t, x->a, rb, w, bracketed(t, TB_POW, x->a));
        put(w, "^", 1);
        paren_text(t, x->b, rb, w, rb || !atomic(t, x->b));
        return;
    case TB_ROWS:
        for (int k = n; k >= 0; k = t->n[k].b) text(t, t->n[k].a, rb, w);
        return;
    }
    if (is_fn(t, n, "EXP") && !rb) {
        put(w, "e^", 2);
        paren_text(t, x->a, rb, w, !atomic(t, x->a));
    } else if (is_fn(t, n, "ABS") && !rb) {
        put(w, "|", 1);
        text(t, x->a, rb, w);
        put(w, "|", 1);
    } else if (is_fn(t, n, "SQRT") && !rb) {
        put(w, "\xe2\x88\x9a", 3);
        paren_text(t, x->a, rb, w, !atomic(t, x->a));
    } else {
        put(w, t->src + x->src, x->len);
        paren_text(t, x->a, rb, w, rb || !is_fn(t, x->a, "ABS"));
    }
}

int tb_text(const tb_tree *t, bool readback, char *out, int size)
{
    out_t w = {out, 0, size, false};
    if (size > 0) out[0] = '\0';
    text(t, t->root, readback, &w);
    for (int i = 0; i < t->ncond; i++) {
        put(&w, "; ", 2);
        text(t, t->conds[i], readback, &w);
    }
    return w.full ? -1 : w.n;
}

/* ---- the text, as text: the fallback ---- */

int tb_text_lines(const char *s, int cells, int starts[], int lens[], int max)
{
    int n = 0, len = (int)strlen(s), at = 0;
    while (at < len && n < max) {
        const char *p = s + at, *cut = NULL;
        int k = 0;
        while (*p && k < cells) {
            uint32_t c = utf8(&p);
            k++;
            if (c == '+' || c == '-' || c == 0xD7 || c == 0xF7 || c == '=' || c == ';' || c == ',') cut = p;
        }
        if (!*p || !cut) cut = p;
        starts[n] = at, lens[n] = (int)(cut - (s + at)), n++;
        at = (int)(cut - s);
        while (s[at] == ' ') at++;
    }
    return n;
}
