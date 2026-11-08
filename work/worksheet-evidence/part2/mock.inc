#define PITCH 20
#define XBOT 200
static void dotted(ls027_frame *f, int y) { for (int x = 0; x < W; x += 3) put(f, x, y, true); }
static int room2(void) { return (NUM_R - NUM_X + 2) / 12; }
static bool bare(const ab_val *v) { return v->row == 0 && v->kind == AB_REAL && abn_is_zero(&v->re); }

static int draw_row(ls027_frame *f, const ab_calc *c, const screen_ui *ui, const ab_row *r, int bot)
{
    char nb[16], tb[AB_EQN_LEN], left[AB_EQN_LEN + 1]; ab_val v; int ind;
    line val, lt; memset(&val, 0, sizeof val); memset(&lt, 0, sizeof lt); val.cursor = lt.cursor = -1;
    if (r->op == AB_ROW_OP_ALG) snprintf(left, sizeof left, "%.*s", (int)r->tlen, r->text);
    else snprintf(left, sizeof left, "%s", opname(r, nb, sizeof nb));
    line_add(&lt, left);
    if (ab_val_decode(r->val, r->vlen, &v, tb, sizeof tb)) { if (v.kind == AB_EXPR) line_add(&val, tb); else val_line(&v, c, ui, &val, &ind); }
    else line_add(&val, "?");
    if (lt.n + val.n + 2 <= room2()) {
        draw_line(f, bot, 2, &lt, true); draw_line(f, bot, 2, &val, false); return bot - PITCH;
    }
    draw_line(f, bot, 2, &val, false); bot -= PITCH;
    if (bot - 14 < 22) return bot;
    draw_line(f, bot, 2, &lt, true); return bot - PITCH;
}

static void block_op(ls027_frame *f, const ab_calc *c, uint32_t serial, int bot, int scale)
{
    ab_row r; char nb[16];
    if (!serial || !ab_rows_find(c, serial, &r) || r.op == AB_ROW_OP_NUM) return;
    screen_text(f, 22, bot - 14, 2, r.op == AB_ROW_OP_ALG ? "=" : opname(&r, nb, sizeof nb), true);
    (void)scale;
}

static void mock(const char *name, ab_calc *c, bool hide)
{
    static ls027_frame f; screen_ui ui; screen_ui_default(&ui);
    composed *k = &comp;
    screen_clear(&f); compose(c, &ui, k); status_band(&f, c, &ui, k->ind);
    int bot;
    if (!c->alg) {
        if (k->x_letter) draw_label(&f, XBOT, "X");
        draw_line(&f, XBOT, 4, &k->x, k->x_left);
        if (k->x_letter && !c->entry.active) block_op(&f, c, c->x.row, XBOT, 4);
        const ab_val *lv[3] = {&c->y, &c->z, &c->t}; line *ll[3] = {&k->y, &k->z, &k->t}; const char *lb[3] = {"Y", "Z", "T"};
        int top = 3;
        if (hide) while (top > 0 && bare(lv[top - 1])) top--;
        bot = XBOT - 28 - 6;
        for (int i = 0; i < top; i++) { draw_label(&f, bot, lb[i]); draw_line(&f, bot, 2, ll[i], false); block_op(&f, c, lv[i]->row, bot, 2); bot -= PITCH; }
    } else {
        draw_line(&f, XBOT, 4, &k->x, k->x_left);
        bot = XBOT - 28 - 6;
        draw_line(&f, bot, 2, &k->y, true);
        bot -= PITCH;
    }
    dotted(&f, bot + 5);
    uint32_t held[4] = {c->x.row, c->y.row, c->z.row, c->t.row};
    int n = ab_rows_count(c), shown_oldest = n == 0;
    for (int i = 1; i <= n && bot - 14 >= 22; i++) {
        ab_row r; bool h = false;
        if (!ab_rows_get(c, i, &r)) break;
        for (int j = 0; j < 4 && !c->alg; j++) h |= held[j] == r.serial;
        if (i == n) shown_oldest = 1;
        if (h) continue;
        bot = draw_row(&f, c, &ui, &r, bot);
    }
    if (shown_oldest && c->rows_dropped && bot - 14 >= 22) {
        char m[48]; snprintf(m, sizeof m, "%u OLDER ROWS DROPPED", (unsigned)c->rows_dropped);
        line l; memset(&l, 0, sizeof l); l.cursor = -1; line_add(&l, m);
        draw_line(&f, bot, 2, &l, true);
    }
    ui.soft[0] = NULL;
    soft_band(&f, &ui);
    char p[256]; snprintf(p, sizeof p, "%s/%s%s.png", getenv("OUT"), name, hide ? "-B" : "-A");
    write_png(p, &f);
}

