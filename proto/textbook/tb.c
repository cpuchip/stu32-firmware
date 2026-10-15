/* tb: the prototype's parser, layout and drawing (tb.h). THROWAWAY: abacus #8391. */
#include "tb.h"

#include <string.h>

#include "font5x7.h"

/* ---- the face ---- */

static uint32_t utf8(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    uint32_t c = *p++;
    if (c >= 0xF0) { c = (c & 7u) << 18 | (p[0] & 63u) << 12 | (p[1] & 63u) << 6 | (p[2] & 63u); p += 3; }
    else if (c >= 0xE0) { c = (c & 15u) << 12 | (p[0] & 63u) << 6 | (p[1] & 63u); p += 2; }
    else if (c >= 0xC0) { c = (c & 31u) << 6 | (p[0] & 63u); p += 1; }
    *s = (const char *)p;
    return c;
}

static void fill(ls027_frame *f, int x, int y, int w, int h)
{
    for (int j = y < 0 ? 0 : y; j < y + h && j < LS027_H; j++)
        for (int i = x < 0 ? 0 : x; i < x + w && i < LS027_W; i++) f->px[j][i >> 3] |= (uint8_t)(1u << (i & 7));
}

int tb_glyph(ls027_frame *f, uint32_t cp, int x, int top, int s)
{
    const font_glyph *g = NULL;
    for (int i = 0; i < FONT5X7_COUNT; i++)
        if (FONT5X7[i].cp == cp) { g = &FONT5X7[i]; break; }
    if (g && f)
        for (int r = 0; r < 7; r++)
            for (int c = 0; c < 5; c++)
                if (g->rows[r] >> (4 - c) & 1) fill(f, x + c * s, top + r * s, s, s);
    return 6 * s;
}

static int cells(const char *u, int len)
{
    int n = 0;
    for (const char *p = u; p < u + len;) { utf8(&p); n++; }
    return n;
}

static int text_n(ls027_frame *f, const char *u, int len, int x, int top, int s)
{
    for (const char *p = u; p < u + len;) x += tb_glyph(f, utf8(&p), x, top, s);
    return x;
}

int tb_text(ls027_frame *f, const char *u, int x, int top, int s) { return text_n(f, u, (int)strlen(u), x, top, s); }
int tb_text_w(const char *u, int s) { int n = cells(u, (int)strlen(u)); return n ? 6 * s * n - s : 0; }

/* ---- the parser: casim's linear text ---- */

typedef struct { tb_tree *t; const char *p; } parser;

static int node(tb_tree *t, int kind, int a, int b)
{
    if (t->count >= TB_MAX_NODES) { t->err = 2; return -1; }
    tb_node *n = &t->n[t->count];
    memset(n, 0, sizeof *n);
    n->kind = (uint8_t)kind, n->a = (int16_t)a, n->b = (int16_t)b;
    return t->count++;
}

static void skip(parser *ps) { while (*ps->p == ' ') ps->p++; }

/* The next token's code point without taking it. */
static uint32_t peek(parser *ps)
{
    skip(ps);
    const char *q = ps->p;
    return *q ? utf8(&q) : 0;
}

static void take(parser *ps) { skip(ps); utf8(&ps->p); }

static int is_minus(uint32_t c) { return c == '-' || c == 0x2212; }

static int expr(parser *ps);

static int primary(parser *ps)
{
    tb_tree *t = ps->t;
    uint32_t c = peek(ps);
    const char *at = ps->p;
    if ((c >= '0' && c <= '9') || c == '.') {
        const char *q = ps->p;
        while ((*q >= '0' && *q <= '9') || *q == '.') q++;
        if (*q == 'E' && (q[1] == '-' || (q[1] >= '0' && q[1] <= '9'))) {
            q += 2;
            while (*q >= '0' && *q <= '9') q++;
        }
        int n = node(t, TB_NUM, -1, -1);
        if (n < 0) return -1;
        t->n[n].src = (uint16_t)(at - t->src), t->n[n].len = (uint16_t)(q - at);
        ps->p = q;
        return n;
    }
    if ((c >= 'A' && c <= 'Z') || c == 0x03C0) {
        const char *q = ps->p;
        if (c == 0x03C0) utf8(&q);
        else while ((*q >= 'A' && *q <= 'Z') || (*q >= '0' && *q <= '9')) q++;
        ps->p = q;
        if (*q == '(') {
            ps->p++;
            int a = expr(ps);
            if (a < 0 || peek(ps) != ')') { t->err = t->err ? t->err : 1; return -1; }
            take(ps);
            int n = node(t, TB_FN, a, -1);
            if (n < 0) return -1;
            t->n[n].src = (uint16_t)(at - t->src), t->n[n].len = (uint16_t)(q - at);
            return n;
        }
        int n = node(t, TB_SYM, -1, -1);
        if (n < 0) return -1;
        t->n[n].src = (uint16_t)(at - t->src), t->n[n].len = (uint16_t)(q - at);
        return n;
    }
    if (c == '(') {
        take(ps);
        int a = expr(ps);
        if (a < 0 || peek(ps) != ')') { t->err = t->err ? t->err : 1; return -1; }
        take(ps);
        return a;
    }
    t->err = 1;
    return -1;
}

static int power(parser *ps)
{
    int a = primary(ps);
    if (a < 0) return -1;
    if (peek(ps) == '^') {
        take(ps);
        int neg = 0;
        if (is_minus(peek(ps))) take(ps), neg = 1;
        int b = power(ps);              /* right associative */
        if (b < 0) return -1;
        if (neg && (b = node(ps->t, TB_NEG, b, -1)) < 0) return -1;
        return node(ps->t, TB_POW, a, b);
    }
    return a;
}

/* A term: a unary minus takes the whole product or quotient after it (-X^2÷4 is -(X^2÷4)), as a textbook writes it. */
static int term(parser *ps)
{
    tb_tree *t = ps->t;
    if (is_minus(peek(ps))) {
        take(ps);
        int a = term(ps);
        return a < 0 ? -1 : node(t, TB_NEG, a, -1);
    }
    int a = power(ps);
    for (;;) {
        uint32_t c = peek(ps);
        if (a < 0 || (c != 0xD7 && c != 0xF7)) return a;
        take(ps);
        int b = power(ps);
        if (b < 0) return -1;
        if (c == 0xD7) a = node(t, TB_MUL, a, b);
        else if (t->n[a].kind == TB_DIV) {
            /* a÷b÷c is drawn a over b·c, never a fraction over a fraction */
            int m = node(t, TB_MUL, t->n[a].b, b);
            if (m < 0) return -1;
            t->n[a].b = (int16_t)m;
        } else a = node(t, TB_DIV, a, b);
    }
}

static int expr(parser *ps)
{
    int a = term(ps);
    for (;;) {
        uint32_t c = peek(ps);
        if (a < 0 || (c != '+' && !is_minus(c))) return a;
        take(ps);
        int b = term(ps);
        if (b < 0) return -1;
        a = node(ps->t, c == '+' ? TB_ADD : TB_SUB, a, b);
    }
}

static int relation(parser *ps)
{
    int a = expr(ps);
    uint32_t c = peek(ps);
    if (a < 0 || !(c == '=' || c == '<' || c == '>' || c == 0x2260 || c == 0x2264 || c == 0x2265)) return a;
    take(ps);
    int b = expr(ps);
    if (b < 0) return -1;
    int n = node(ps->t, TB_REL, a, b);
    if (n >= 0) ps->t->n[n].op = (uint8_t)(c == '=' ? 0 : c == '<' ? 1 : c == '>' ? 2 : c == 0x2260 ? 3 : c == 0x2264 ? 4 : 5);
    return n;
}

int tb_parse(tb_tree *t, const char *s, const tb_style *st)
{
    parser ps = {t, s};
    t->src = s, t->count = 0, t->ncond = 0, t->err = 0, t->st = *st;
    t->root = relation(&ps);
    while (t->root >= 0 && peek(&ps) == ';') {
        take(&ps);
        int c = relation(&ps);
        if (c < 0) break;
        if (t->ncond < TB_MAX_CONDS) t->conds[t->ncond++] = c;
    }
    if (t->root >= 0 && !t->err && peek(&ps) != 0) t->err = 1;
    return t->err || t->root < 0 ? -1 : 0;
}

/* ---- the layout ---- */

#define AXIS(s) (3 * (s))       /* the face's minus is row 3: a fraction bar lines up with it */

enum { P_SRC, P_TEXT, P_NODE, P_GAP, P_LPAR, P_RPAR, P_BAR };
typedef struct { uint8_t k; int16_t v; const char *s; } piece;
#define MAXP 10

static int name_is(const tb_tree *t, int n, const char *name)
{
    return t->n[n].len == strlen(name) && memcmp(t->src + t->n[n].src, name, t->n[n].len) == 0;
}

static int is_sqrt(const tb_tree *t, int n) { return t->n[n].kind == TB_FN && name_is(t, n, "SQRT"); }
static int is_abs(const tb_tree *t, int n) { return t->n[n].kind == TB_FN && name_is(t, n, "ABS"); }
static int is_e(const tb_tree *t, int n) { return t->st.exp_as_e && t->n[n].kind == TB_FN && name_is(t, n, "EXP"); }

/* Whether child c, in parent kind k on side (0 left, 1 right), needs parentheses in 2D: a fraction or an exponent
 * groups by its shape, so those never do. */
static int paren(const tb_tree *t, int k, int c, int side)
{
    int ck = t->n[c].kind;
    switch (k) {
    case TB_ADD: return side && ck == TB_NEG;
    case TB_SUB: return side && (ck == TB_ADD || ck == TB_SUB || ck == TB_NEG);
    case TB_NEG: return ck == TB_ADD || ck == TB_SUB || ck == TB_NEG;
    case TB_MUL: return ck == TB_ADD || ck == TB_SUB || ck == TB_NEG;
    case TB_POW:
        if (side) return 0;
        return ck == TB_ADD || ck == TB_SUB || ck == TB_NEG || ck == TB_MUL || ck == TB_DIV || ck == TB_POW ||
               (ck == TB_NUM && memchr(t->src + t->n[c].src, '-', t->n[c].len));
    }
    return 0;
}

/* The first and last mark a node shows: 'd' a digit, 'a' a letter, '(' a bracket, fraction, radical or bar, '-' a
 * minus, '^' an exponent: they decide how an implied product is spaced. */
static int lead(const tb_tree *t, int n)
{
    const tb_node *x = &t->n[n];
    switch (x->kind) {
    case TB_NUM: return 'd';
    case TB_SYM: return 'a';
    case TB_FN: return is_sqrt(t, n) || is_abs(t, n) ? '(' : is_e(t, n) ? 'a' : 'w';
    case TB_MUL: return paren(t, TB_MUL, x->a, 0) ? '(' : lead(t, x->a);
    case TB_POW: return paren(t, TB_POW, x->a, 0) ? '(' : lead(t, x->a);
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
    case TB_MUL: return paren(t, TB_MUL, x->b, 1) ? '(' : trail(t, x->b);
    case TB_POW: return '^';
    case TB_NEG: return trail(t, x->a);
    case TB_FN: return is_e(t, n) ? '^' : '(';
    default: return '(';
    }
}

static int add_node(piece *p, int i, int c, int par)
{
    if (par) p[i++] = (piece){P_LPAR, (int16_t)c, NULL};
    p[i++] = (piece){P_NODE, (int16_t)c, NULL};
    if (par) p[i++] = (piece){P_RPAR, (int16_t)c, NULL};
    return i;
}

static const char *const REL[] = {"=", "<", ">", "\xe2\x89\xa0", "\xe2\x89\xa4", "\xe2\x89\xa5"};

/* A row-shaped node as pieces; 0 when the node has its own shape (a fraction, a power, a radical, e^u, lines). */
static int pieces(const tb_tree *t, int n, int s, piece *p)
{
    const tb_node *x = &t->n[n];
    int i = 0, sp = s >= 2 ? 2 * s : 0; /* the space either side of + and −, none in an exponent */
    switch (x->kind) {
    case TB_NUM:
    case TB_SYM: p[i++] = (piece){P_SRC, (int16_t)n, NULL}; break;
    case TB_ADD:
    case TB_SUB:
    case TB_REL:
        i = add_node(p, i, x->a, 0);
        p[i++] = (piece){P_GAP, (int16_t)sp, NULL};
        p[i++] = (piece){P_TEXT, 0, x->kind == TB_ADD ? "+" : x->kind == TB_SUB ? "-" : REL[x->op]};
        p[i++] = (piece){P_GAP, (int16_t)sp, NULL};
        i = add_node(p, i, x->b, paren(t, x->kind, x->b, 1));
        break;
    case TB_LEAD:
        p[i++] = (piece){P_TEXT, 0, x->op == '+' ? "+" : "-"};
        p[i++] = (piece){P_GAP, (int16_t)sp, NULL};
        i = add_node(p, i, x->a, 0);
        break;
    case TB_NEG:
        p[i++] = (piece){P_TEXT, 0, "-"};
        i = add_node(p, i, x->a, paren(t, TB_NEG, x->a, 0));
        break;
    case TB_MUL: {
        int pa = paren(t, TB_MUL, x->a, 0), pb = paren(t, TB_MUL, x->b, 1);
        int l = pa ? '(' : trail(t, x->a), r = pb ? '(' : lead(t, x->b);
        i = add_node(p, i, x->a, pa);
        if (!t->st.implicit_mul) {
            p[i++] = (piece){P_GAP, (int16_t)sp, NULL};
            p[i++] = (piece){P_TEXT, 0, "\xc3\x97"};
            p[i++] = (piece){P_GAP, (int16_t)sp, NULL};
        } else if (r == 'd' || r == '-') p[i++] = (piece){P_TEXT, 0, "\xc2\xb7"};     /* 2·3, X·2 */
        else if ((l == 'd' && r != 'w') || (l == '(' && r == '(')) {                     /* 3X, (X-1)(X+1) */
        } else p[i++] = (piece){P_GAP, (int16_t)(r == 'w' && l != 'd' ? 2 * s : s), NULL}; /* X  ATAN(X), 0.2 LN|X| */
        i = add_node(p, i, x->b, pb);
        break;
    }
    case TB_FN:
        if (is_sqrt(t, n) || is_e(t, n)) return 0;
        if (is_abs(t, n)) {
            p[i++] = (piece){P_BAR, x->a, NULL};
            p[i++] = (piece){P_NODE, x->a, NULL};
            p[i++] = (piece){P_BAR, x->a, NULL};
            break;
        }
        p[i++] = (piece){P_SRC, (int16_t)n, NULL};
        if (is_abs(t, x->a)) p[i++] = (piece){P_GAP, (int16_t)s, NULL};    /* LN |X-2|: the bars are the brackets */
        i = add_node(p, i, x->a, !is_abs(t, x->a));
        break;
    default: return 0;
    }
    return i;
}

/* A piece's box. */
static void pbox(const tb_tree *t, const piece *q, int s, int *w, int *h, int *ax)
{
    const tb_node *c = q->k == P_TEXT || q->k == P_GAP ? NULL : &t->n[q->v];
    switch (q->k) {
    case P_SRC: { int k = cells(t->src + c->src, c->len); *w = 6 * s * k - s, *h = 7 * s, *ax = AXIS(s); return; }
    case P_TEXT: *w = tb_text_w(q->s, s), *h = 7 * s, *ax = AXIS(s); return;
    case P_GAP: *w = q->v, *h = 0, *ax = 0; return;
    case P_NODE: *w = c->w, *h = c->h, *ax = c->axis; return;
    default: {          /* a bracket: as tall as what it holds, a little taller when that is more than a line */
        int extra = c->h > 7 * s || q->k == P_BAR ? s : 0;      /* a bar is taller than a letter, never an I */
        *w = q->k == P_BAR ? s : 3 * s, *h = (c->h < 7 * s ? 7 * s : c->h) + 2 * extra;
        *ax = (c->h < 7 * s ? AXIS(s) : c->axis) + extra;
    }
    }
}

static void row_box(tb_tree *t, tb_node *x, const piece *p, int np, int s)
{
    int w = 0, ax = 0, below = 0, gap = 0;
    for (int i = 0; i < np; i++) {
        int pw, ph, pa;
        pbox(t, &p[i], s, &pw, &ph, &pa);
        if (p[i].k == P_GAP) { w += pw; continue; }
        w += (gap ? s : 0) + pw, gap = 1;
        if (pa > ax) ax = pa;
        if (ph - pa > below) below = ph - pa;
    }
    x->w = (int16_t)w, x->axis = (int16_t)ax, x->h = (int16_t)(ax + below);
}

/* A stroke s thick from (x0, y0) to (x1, y1). */
static void stroke(ls027_frame *f, int x0, int y0, int x1, int y1, int s)
{
    int dx = x1 - x0, dy = y1 - y0, n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    for (int i = 0; i <= n; i++) {
        int x = n ? x0 + (dx * i + (dx >= 0 ? n / 2 : -n / 2)) / n : x0, y = n ? y0 + (dy * i + (dy >= 0 ? n / 2 : -n / 2)) / n : y0;
        fill(f, x, y, s, s);
    }
}

static void bracket(ls027_frame *f, int k, int x, int top, int h, int s)
{
    if (k == P_BAR) { fill(f, x, top, s, h); return; }
    int ramp = h / 4 > 2 * s ? h / 4 : 2 * s;
    for (int y = 0; y < h; y++) {
        int d = y < ramp ? (ramp - y) * 2 * s / ramp : y >= h - ramp ? (y - (h - ramp - 1)) * 2 * s / ramp : 0;
        fill(f, k == P_LPAR ? x + d : x + 2 * s - d, top + y, s, 1);
    }
}

#define EGAP(s) ((s) > 1 ? 2 : 1)     /* between a base and its exponent */
#define LGAP(s) (3 * (s))                /* between wrapped lines */

static int expo_scale(int s) { return s > 1 ? s - 1 : 1; }

int tb_layout(tb_tree *t, int n, int s)
{
    tb_node *x = &t->n[n];
    piece p[MAXP];
    x->scale = (uint8_t)s;
    int np = pieces(t, n, s, p);
    if (np) {
        for (int i = 0; i < np; i++)
            if (p[i].k == P_NODE) tb_layout(t, p[i].v, s);
        row_box(t, x, p, np, s);
        return x->w;
    }
    switch (x->kind) {
    case TB_DIV: {
        tb_node *a = &t->n[x->a], *b = &t->n[x->b];
        tb_layout(t, x->a, s), tb_layout(t, x->b, s);
        int w = (a->w > b->w ? a->w : b->w) + 2 * s;
        x->w = (int16_t)w, x->h = (int16_t)(a->h + s + s + s + b->h), x->axis = (int16_t)(a->h + s);
        break;
    }
    case TB_FN:
        if (is_sqrt(t, n)) {
            tb_node *a = &t->n[x->a];
            tb_layout(t, x->a, s);
            x->w = (int16_t)(5 * s + a->w + s), x->h = (int16_t)(s + s + a->h), x->axis = (int16_t)(2 * s + a->axis);
            break;
        }
        /* fall through: EXP(u) as e^u */ __attribute__((fallthrough));
    case TB_POW: {
        int e = x->kind == TB_POW ? x->b : x->a, es = expo_scale(s), bw, bh, bax;
        if (x->kind == TB_POW) {
            int par = paren(t, TB_POW, x->a, 0);
            tb_layout(t, x->a, s);
            bw = t->n[x->a].w, bh = t->n[x->a].h, bax = t->n[x->a].axis;
            if (par) {          /* the brackets' box: 3s each, and a gap of s inside each */
                piece q = {P_LPAR, x->a, NULL};
                pbox(t, &q, s, &bw, &bh, &bax);
                bw = t->n[x->a].w + 2 * (3 * s + s);
            }
        } else bw = 5 * s, bh = 7 * s, bax = AXIS(s);
        tb_layout(t, e, es);
        int eh = t->n[e].h, r = 3 * s, extra = eh > r ? eh - r : 0;
        x->w = (int16_t)(bw + EGAP(s) + t->n[e].w), x->h = (int16_t)(extra + bh), x->axis = (int16_t)(extra + bax);
        break;
    }
    case TB_ROWS: {
        int w = 0, h = 0;
        for (int r = n; r >= 0; r = t->n[r].b) {
            tb_node *l = &t->n[t->n[r].a];
            tb_layout(t, t->n[r].a, s);
            if (l->w > w) w = l->w;
            h += (h ? LGAP(s) : 0) + l->h;
        }
        x = &t->n[n];
        x->w = (int16_t)w, x->h = (int16_t)h, x->axis = t->n[x->a].axis;
        break;
    }
    }
    return x->w;
}

void tb_draw(tb_tree *t, int n, ls027_frame *f, int x, int top)
{
    tb_node *nd = &t->n[n];
    int s = nd->scale;
    piece p[MAXP];
    int np = pieces(t, n, s, p);
    if (np) {
        int gap = 0;
        for (int i = 0; i < np; i++) {
            int pw, ph, pa;
            pbox(t, &p[i], s, &pw, &ph, &pa);
            if (p[i].k == P_GAP) { x += pw; continue; }
            x += gap ? s : 0, gap = 1;
            int y = top + nd->axis - pa;
            switch (p[i].k) {
            case P_SRC: text_n(f, t->src + t->n[p[i].v].src, t->n[p[i].v].len, x, y, s); break;
            case P_TEXT: tb_text(f, p[i].s, x, y, s); break;
            case P_NODE: tb_draw(t, p[i].v, f, x, y); break;
            default: bracket(f, p[i].k, x, y, ph, s);
            }
            x += pw;
        }
        return;
    }
    switch (nd->kind) {
    case TB_DIV: {
        tb_node *a = &t->n[nd->a], *b = &t->n[nd->b];
        tb_draw(t, nd->a, f, x + (nd->w - a->w) / 2, top);
        fill(f, x + s / 2, top + nd->axis, nd->w - 2 * (s / 2), s);    /* inset: a minus beside it stays a minus */
        tb_draw(t, nd->b, f, x + (nd->w - b->w) / 2, top + nd->axis + 2 * s);
        return;
    }
    case TB_ROWS:
        for (int r = n; r >= 0; r = t->n[r].b) {
            tb_node *l = &t->n[t->n[r].a];
            tb_draw(t, t->n[r].a, f, x + nd->w - l->w, top);
            top += l->h + LGAP(s);
        }
        return;
    }
    if (nd->kind == TB_FN && is_sqrt(t, n)) {
        tb_node *a = &t->n[nd->a];
        int h = nd->h, rw = 5 * s;
        stroke(f, x, top + h - 3 * s - s, x + s, top + h - 3 * s - s, s);       /* the tick */
        stroke(f, x + s, top + h - 4 * s, x + 2 * s, top + h - s, s);           /* down */
        stroke(f, x + 2 * s, top + h - s, x + rw - s, top, s);                  /* up to the bar */
        fill(f, x + rw - s, top, a->w + 2 * s, s);                              /* the overbar */
        tb_draw(t, nd->a, f, x + rw, top + 2 * s);
        return;
    }
    /* a power, or e^u */
    int e = nd->kind == TB_POW ? nd->b : nd->a, eh = t->n[e].h, r = 3 * s, extra = eh > r ? eh - r : 0, bw;
    if (nd->kind == TB_POW) {
        int par = paren(t, TB_POW, nd->a, 0), bx = x;
        tb_node *a = &t->n[nd->a];
        if (par) {
            piece q = {P_LPAR, nd->a, NULL};
            int pw, ph, pa;
            pbox(t, &q, s, &pw, &ph, &pa);
            int py = top + extra + (nd->axis - extra) - pa;
            bracket(f, P_LPAR, bx, py, ph, s);
            bx += 3 * s + s;
            tb_draw(t, nd->a, f, bx, top + nd->axis - a->axis);
            bx += a->w + s;
            bracket(f, P_RPAR, bx, py, ph, s);
            bw = bx + 3 * s - x;
        } else {
            tb_draw(t, nd->a, f, bx, top + nd->axis - a->axis);
            bw = a->w;
        }
        tb_draw(t, e, f, x + bw + EGAP(s), top + extra + r - eh);
    } else {
        tb_glyph(f, 'e', x, top + extra, s);
        tb_draw(t, e, f, x + 5 * s + EGAP(s), top + extra + r - eh);
    }
}

/* ---- lines and conditions ---- */

int tb_wrap(tb_tree *t, int maxw, int s)
{
    int term[48], op[48], k = 0, n = t->root;
    while ((t->n[n].kind == TB_ADD || t->n[n].kind == TB_SUB) && k < 47) {
        term[k] = t->n[n].b, op[k++] = t->n[n].kind == TB_ADD ? '+' : '-';
        n = t->n[n].a;
    }
    term[k] = n, op[k++] = 0;
    for (int i = 0; i < k / 2; i++) {           /* left to right */
        int a = term[i], b = op[i];
        term[i] = term[k - 1 - i], op[i] = op[k - 1 - i], term[k - 1 - i] = a, op[k - 1 - i] = b;
    }
    int sp = s >= 2 ? 2 * s : 0, opw = 5 * s + 2 * sp + 2 * s, lines = 0, rows = -1, last = -1, i = 0;
    while (i < k) {
        int first = op[i] ? node(t, TB_LEAD, term[i], -1) : term[i];
        if (first < 0) return 0;
        if (op[i]) t->n[first].op = (uint8_t)op[i];
        int line = first, w = tb_layout(t, first, s);
        if (w > maxw) return 0;
        for (i++; i < k; i++) {
            int tw = tb_layout(t, term[i], s);
            if (w + opw + tw > maxw) break;
            line = node(t, op[i] == '+' ? TB_ADD : TB_SUB, line, term[i]);
            if (line < 0) return 0;
            w += opw + tw;
        }
        int r = node(t, TB_ROWS, line, -1);
        if (r < 0) return 0;
        if (last >= 0) t->n[last].b = (int16_t)r;
        else rows = r;
        last = r, lines++;
    }
    t->root = rows;
    tb_layout(t, rows, s);
    return lines;
}

int tb_cond_layout(tb_tree *t, int s, int *h)
{
    int w = 0;
    *h = 0;
    for (int i = 0; i < t->ncond; i++) {
        w += (i ? tb_text_w(", ", s) + s : 0) + tb_layout(t, t->conds[i], s);
        if (t->n[t->conds[i]].h > *h) *h = t->n[t->conds[i]].h;
    }
    return w;
}

void tb_cond_draw(tb_tree *t, ls027_frame *f, int x, int top, int s)
{
    for (int i = 0; i < t->ncond; i++) {
        if (i) x = tb_text(f, ", ", x, top, s) + s;
        tb_draw(t, t->conds[i], f, x, top);
        x += t->n[t->conds[i]].w;
    }
}
