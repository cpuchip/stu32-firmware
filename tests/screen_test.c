/* screen_test: layout v0 against its own description (screen.h), then the frames (unit 018
 * acceptance: "renderer frames checked in as images").
 *
 *     build/screen_test tests/frames            the checks, and every frame compared with its file
 *     build/screen_test --write tests/frames    the checks, and the frames written there
 *
 * The checks are written from screen.h's layout: where each line sits, at which scale, what the
 * status band shows; a glyph is checked cell by cell against the face's own bits, drawn here by a
 * loop of the test's own. The frames are PNG images, 400 x 240, one bit a pixel, written here with
 * no compressor (stored deflate blocks); they are reviewed by eye when first written and are
 * regression references after that. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screen.h"
#include "state.h"

static int bad, checks;

static void expect(int ok, const char *what)
{
    checks++;
    if (!ok && bad++ < 40) printf("screen_test: %s\n", what);
}

static int px(const ls027_frame *f, int x, int y)
{
    return f->px[y][x / 8] >> (x % 8) & 1;
}

/* the glyph for cp, at scale s, with its top left at (x, y): every pixel of its 5 x 7 cells */
static int glyph_at(const ls027_frame *f, int x, int y, int s, uint32_t cp, int dark)
{
    const font_glyph *g = screen_glyph(cp);
    if (!g) return 0;
    for (int r = 0; r < 7 * s; r++)
        for (int col = 0; col < 5 * s; col++) {
            int lit = g->rows[r / s] >> (4 - col / s) & 1;
            if (x + col >= LS027_W || y + r >= LS027_H) return 0;
            if (px(f, x + col, y + r) != (lit ? dark : !dark)) return 0;
        }
    return 1;
}

/* ASCII text from (x, y) at scale s */
static int text_at(const ls027_frame *f, int x, int y, int s, const char *t, int dark)
{
    for (int i = 0; t[i]; i++)
        if (!glyph_at(f, x + i * 6 * s, y, s, (unsigned char)t[i], dark)) return 0;
    return 1;
}

/* the x where cp first stands on the line whose top is y, at scale s; -1 if nowhere */
static int find_glyph(const ls027_frame *f, int y, int s, uint32_t cp)
{
    for (int x = 0; x + 5 * s <= LS027_W; x++)
        if (glyph_at(f, x, y, s, cp, 1)) return x;
    return -1;
}

static int dark_in(const ls027_frame *f, int x0, int y0, int w, int h)
{
    int n = 0;
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) n += px(f, x, y);
    return n;
}

static ab_op key_op(const char *k)
{
    static const struct { const char *name; ab_op op; } K[] = {
        {".", AB_KPOINT}, {"E", AB_KE}, {"CHS", AB_KCHS}, {"BKSP", AB_KBKSP}, {"ENTER", AB_ENTER},
        {"+", AB_ADD}, {"-", AB_SUB}, {"*", AB_MUL}, {"/", AB_DIV}, {"I", AB_KI}, {"THETA", AB_KTHETA},
        {"[", AB_KVEC}, {",", AB_KCOMMA}, {"RAD", AB_RAD}, {"GRAD", AB_GRAD}, {"HEX", AB_HEX},
        {"FDISP", AB_FDISP}, {"PRGM", AB_PRGM}, {"RTA", AB_RTA}, {"SQRT", AB_SQRT}, {"EQN", AB_EQN},
        {"PI", AB_PI}, {"INV", AB_INV}};
    if (k[0] >= '0' && k[0] <= '9' && !k[1]) return (ab_op)(AB_K0 + k[0] - '0');
    for (size_t i = 0; i < sizeof K / sizeof K[0]; i++)
        if (strcmp(K[i].name, k) == 0) return K[i].op;
    fprintf(stderr, "screen_test: no key %s\n", k);
    exit(2);
}

/* keys separated by blanks; a run of digits is typed one digit at a time */
static void keys(ab_calc *c, const char *s)
{
    char buf[512];
    snprintf(buf, sizeof buf, "%s", s);
    for (char *k = strtok(buf, " "); k; k = strtok(NULL, " ")) {
        if (k[0] >= '0' && k[0] <= '9') {
            for (char *d = k; *d; d++) {
                char one[2] = {*d, '\0'};
                ab_do(c, key_op(one));
            }
        } else {
            ab_do(c, key_op(k));
        }
    }
}

/* the face's holes in a message; a line break counts as one, as every message is one line (#3822) */
static int holes_in(const char *m)
{
    return screen_missing(m) + (strchr(m, '\n') != NULL);
}

/* the text of program line i (1-based in the order keyed), "?" for none */
static const char *line_text(const ab_calc *c, int i)
{
    static char buf[AB_EQN_LEN + 8];
    if (ab_prog_line(c, i, NULL, buf, sizeof buf) < 0) snprintf(buf, sizeof buf, "?");
    return buf;
}

static void expect_line(const ab_calc *c, int i, const char *want, const char *what)
{
    char m[256];
    const char *got = line_text(c, i);
    snprintf(m, sizeof m, "%s: line %d reads '%s', the guide '%s'", what, i + 1, got, want);
    expect(strcmp(got, want) == 0, m);
}

/* ---- the frames ---- */

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static int png_chunk(FILE *o, const char *type, const uint8_t *data, size_t n)
{
    static uint8_t buf[4 + 16384];
    uint8_t len[4], crc[4];
    if (n > sizeof buf - 4) return 0;
    memcpy(buf, type, 4);
    if (n) memcpy(buf + 4, data, n);
    be32(len, (uint32_t)n);
    be32(crc, ab_crc32(buf, 4 + n));
    return fwrite(len, 1, 4, o) == 4 && fwrite(buf, 1, 4 + n, o) == 4 + n && fwrite(crc, 1, 4, o) == 4;
}

/* The frame as a PNG: greyscale, one bit a pixel (1 white), each row filter 0, the image data a
   zlib stream of one stored deflate block, so no compressor is needed and the bytes are fixed. */
static int write_png(const char *path, const ls027_frame *f)
{
    enum { ROW = 1 + LS027_ROW_BYTES, RAW = ROW * LS027_H };
    static uint8_t z[2 + 5 + RAW + 4];
    uint8_t hdr[13] = {0};
    uint32_t a = 1, b = 0;
    be32(hdr, LS027_W);
    be32(hdr + 4, LS027_H);
    hdr[8] = 1;                                 /* bit depth; colour type 0, the rest 0 */
    z[0] = 0x78;                                /* zlib: deflate, 32K window, no dictionary */
    z[1] = 0x01;
    z[2] = 0x01;                                /* the final block, stored */
    z[3] = (uint8_t)(RAW & 0xFF);
    z[4] = (uint8_t)(RAW >> 8);
    z[5] = (uint8_t)~z[3];
    z[6] = (uint8_t)~z[4];
    for (int y = 0; y < LS027_H; y++) {
        uint8_t *r = z + 7 + y * ROW;
        r[0] = 0;
        for (int i = 0; i < LS027_ROW_BYTES; i++) {     /* the leftmost pixel in the top bit */
            uint8_t v = f->px[y][i], o = 0;
            for (int k = 0; k < 8; k++) o |= (uint8_t)((~v >> k & 1) << (7 - k));
            r[1 + i] = o;
        }
    }
    for (int i = 0; i < RAW; i++) {             /* Adler-32 of the raw rows */
        a = (a + z[7 + i]) % 65521;
        b = (b + a) % 65521;
    }
    be32(z + 7 + RAW, b << 16 | a);
    FILE *o = fopen(path, "wb");
    if (!o) return 0;
    int ok = fwrite("\x89PNG\r\n\x1a\n", 1, 8, o) == 8 && png_chunk(o, "IHDR", hdr, sizeof hdr) &&
             png_chunk(o, "IDAT", z, sizeof z) && png_chunk(o, "IEND", NULL, 0);
    return fclose(o) == 0 && ok;
}

static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int same = fa && fb;
    while (same) {
        int ca = fgetc(fa), cb = fgetc(fb);
        if (ca != cb) same = 0;
        if (ca == EOF || cb == EOF) break;
    }
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    return same;
}

static int frames, frames_ok;
static const char *frame_dir;
static int writing;

static void frame(const char *name, const ab_calc *c, const screen_ui *ui)
{
    static ls027_frame f;
    char mine[256], ref[256];
    screen_render(c, ui, &f);
    snprintf(mine, sizeof mine, "build/frames/%s.png", name);
    snprintf(ref, sizeof ref, "%s/%s.png", frame_dir, name);
    frames++;
    if (!write_png(mine, &f)) {
        printf("screen_test: cannot write %s\n", mine);
        return;
    }
    if (writing) {
        frames_ok += write_png(ref, &f);
    } else if (same_file(mine, ref)) {
        frames_ok++;
    } else {
        printf("screen_test: frame %s differs from %s\n", name, ref);
    }
}

int main(int argc, char **argv)
{
    static ab_calc c;
    static ls027_frame f;
    screen_ui ui;
    if (argc == 3 && strcmp(argv[1], "--write") == 0) {
        writing = 1;
        frame_dir = argv[2];
    } else if (argc == 2) {
        frame_dir = argv[1];
    } else {
        fprintf(stderr, "usage: screen_test [--write] DIR\n");
        return 2;
    }

    /* the face: ascending code points (the binary search), each glyph lit but the blank, none a
       copy of another but the minus sign of the hyphen */
    for (int i = 0; i < FONT5X7_COUNT; i++) {
        int lit = 0;
        for (int r = 0; r < 7; r++) lit |= FONT5X7[i].rows[r] & ~0x1F ? 0x100 : FONT5X7[i].rows[r];
        expect(lit < 0x100, "a glyph row wider than five columns");
        expect(i == 0 || FONT5X7[i].cp > FONT5X7[i - 1].cp, "glyphs not ascending");
        expect((lit != 0) == (FONT5X7[i].cp != ' '), "a glyph blank, or the blank lit");
        for (int j = 0; j < i; j++) {
            int same = memcmp(FONT5X7[i].rows, FONT5X7[j].rows, 7) == 0;
            int alias = FONT5X7[j].cp == '-' && FONT5X7[i].cp == 0x2212;
            expect(!same || alias, "two glyphs alike");
        }
        expect(screen_glyph(FONT5X7[i].cp) == &FONT5X7[i], "a glyph not found by its code point");
    }
    /* the characters a number is written with differ by 3 pixels at least, so none reads as another
       (a theta that differed from 8 by two corners did) */
    {
        static const uint32_t NUM[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D',
                                       'E', 'F', 'h', 'o', 'b', 'd', 'i', '.', ',', '-', '/', '[', ']',
                                       0x03B8, 0x25B2, 0x25BC, 0x2026};
        int n = (int)(sizeof NUM / sizeof NUM[0]);
        for (int i = 0; i < n; i++)
            for (int j = 0; j < i; j++) {
                const font_glyph *a = screen_glyph(NUM[i]), *b = screen_glyph(NUM[j]);
                int d = 0;
                for (int r = 0; a && b && r < 7; r++)
                    for (uint8_t v = (uint8_t)(a->rows[r] ^ b->rows[r]); v; v &= (uint8_t)(v - 1)) d++;
                char what[80];
                snprintf(what, sizeof what, "U+%04X and U+%04X differ by %d pixels", (unsigned)NUM[i],
                         (unsigned)NUM[j], d);
                expect(a && b && d >= 3, what);
            }
    }
    expect(screen_glyph(0x4E00) == NULL && screen_glyph(0) == NULL, "a glyph found that is not drawn");
    /* what the core's formatter and the layout write */
    expect(screen_missing("0123456789ABCDEF.,-+E i[]/:?=()_ !^#%x") == 0, "the formatter's characters");
    expect(screen_missing("GOLD BLUE RAD GRAD HEX OCT BIN PRGM TOP EQN LIST \xe2\x96\xb2\xe2\x96\xbc\xe2\x80\xa6") == 0,
           "the layout's own words");
    expect(screen_missing("\xce\xb8\xce\xa3\xc2\xb2\xe2\x88\xab\xcb\xa3\xe2\x88\x9a\xcf\x80\xc3\xb7\xc3\x97") == 0,
           "theta, sigma, squared, integral, superscript x, root, pi, divide, times");
    /* UTF-8: code points counted, a malformed byte taken alone */
    expect(screen_cells("a\xce\xb8\xe2\x88\xab") == 3, "UTF-8 counted by code point");
    expect(screen_cells("x\xcc\x84w y\xcc\x82") == 4, "a combining mark takes no cell");
    screen_clear(&f);
    screen_text(&f, 10, 10, 1, "y\xcc\x82", 1);
    expect(dark_in(&f, 10, 9, 5, 2) > 0 && dark_in(&f, 10, 11, 5, 1) == 0,
           "y-hat: the hat one row above the glyph's rows, a clear row before the y");
    {
        /* the y itself is unchanged under its hat: its rows 2..6 as the face draws them */
        const font_glyph *gy = screen_glyph('y');
        int same = gy != NULL;
        for (int r = 2; gy && r < 7; r++)
            for (int col = 0; col < 5; col++)
                if (px(&f, 10 + col, 10 + r) != (gy->rows[r] >> (4 - col) & 1)) same = 0;
        expect(same, "y-hat: the y's own rows unchanged");
    }
    expect(screen_cells("\xce" "A") == 2, "a broken sequence: its lead byte alone");
    expect(screen_missing("\xe4\xb8\x80") == 1, "a code point the face lacks");

    /* every message the core can show is in the face: each status's, and the specific texts over
       every op, argument form, X sign and the SOLVE or integral running */
    long holes = 0;
    for (int st = 0; st <= ABN_BAD_IMAGE; st++) {
        const char *m = ab_status_message((abn_status)st);
        if (m) holes += holes_in(m);
    }
    {
        static const int ARGS[] = {0, AB_VAR_IND_I, AB_VAR_IND_J, AB_TGT_IND_I, AB_TGT_IND_J};
        static const char *const XS[] = {"-2", "0", "2"};
        static const int ACT[] = {0, AB_SOLVE, AB_INTEG};
        ab_init(&c);
        c.msg = true;
        for (int st = 1; st <= ABN_BAD_IMAGE; st++)
            for (int op = 0; op <= AB_MODEL; op++)
                for (int a = 0; a < 5; a++)
                    for (int xs = 0; xs < 3; xs++)
                        for (int ac = 0; ac < 3; ac++) {
                            c.err_status = (abn_status)st;
                            c.err_op = op;
                            c.err_arg = ARGS[a];
                            c.err_active = ACT[ac];
                            c.err_x.kind = AB_REAL;
                            abn_from_text(&c.err_x.re, XS[xs]);
                            const char *m = ab_error_message(&c);
                            if (m) holes += holes_in(m);
                        }
        for (int op = 0; op <= AB_MODEL; op++) {
            const char *m = ab_busy_message((ab_op)op);
            if (m) holes += holes_in(m);
        }
    }
    expect(holes == 0, "a message with a character the face lacks");

    /* clipping: drawing off the frame touches nothing and does not fault (ASan watches) */
    screen_clear(&f);
    screen_text(&f, -40, -20, 4, "88", 1);
    screen_text(&f, 390, 230, 4, "88", 1);
    screen_fill(&f, -10, -10, 5, 5, 1);
    expect(dark_in(&f, 0, 0, LS027_W, LS027_H) == dark_in(&f, 0, 0, 8, 8) + dark_in(&f, 390, 230, 10, 10),
           "a clipped draw left pixels away from its corner");

    /* ---- the layout (screen.h) ---- */
    screen_ui_default(&ui);
    ab_init(&c);
    screen_render(&c, &ui, &f);
    expect(dark_in(&f, 0, 19, LS027_W, 2) == 2 * LS027_W, "the rule under the status band");
    expect(text_at(&f, 338, 2, 2, "33", 1), "the mode: 33, the default (unit 017 rule 1)");
    expect(dark_in(&f, 0, 0, 338, 18) == 0 && dark_in(&f, 360, 0, 6, 18) == 0,
           "a fresh calculator's status band shows nothing but the mode and the battery");
    expect(dark_in(&f, 366, 2, 33, 14) > 0, "the battery outline");
    expect(glyph_at(&f, 378, 5, 1, '?', 1), "the battery not read: a question mark in it");
    /* X: 0 at scale 4, right aligned to 398, its bottom at 156; its letter at scale 2 */
    expect(glyph_at(&f, 378, 128, 4, '0', 1), "X = 0 at scale 4, right aligned");
    expect(text_at(&f, 2, 142, 2, "X", 1), "the X letter");
    expect(glyph_at(&f, 388, 38, 2, '0', 1) && glyph_at(&f, 388, 66, 2, '0', 1) && glyph_at(&f, 388, 94, 2, '0', 1),
           "T, Z, Y = 0 at scale 2, right aligned");
    expect(text_at(&f, 2, 38, 2, "T", 1) && text_at(&f, 2, 66, 2, "Z", 1) && text_at(&f, 2, 94, 2, "Y", 1),
           "the T, Z, Y letters");
    expect(dark_in(&f, 0, 157, LS027_W, LS027_H - 157) == 0, "no soft labels: the bottom empty");

    /* the number being typed: left at 20, with its cursor */
    keys(&c, "12");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "12_", 1), "typing 12: '12_' at scale 4 from x = 20");
    keys(&c, ". 5 E CHS 3");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "12.5E-3_", 1), "the point, E and the exponent's sign as typed");
    ui.radix = ',';
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "12,5E-3_", 1), "the device's radix mark in the entry");
    ui.radix = '.';
    keys(&c, "ENTER");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 398 - (6 * 24 - 4), 128, 4, "0.0125", 1), "the entry ended: X in ALL, right aligned");

    /* 20 cells take scale 3, 25 scale 2, and 40 a window */
    ab_init(&c);
    keys(&c, "1234567890123456789");          /* 19 digits and the cursor */
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 135, 3, "1234567890123456789_", 1), "20 cells at scale 3");
    keys(&c, "01234");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 142, 2, "123456789012345678901234_", 1), "25 cells at scale 2");
    keys(&c, "I 123456789012345");                /* 24 + 1 + 15 + the cursor: 41 cells */
    screen_render(&c, &ui, &f);
    expect(glyph_at(&f, 20, 142, 2, 0x2026, 1), "a line past 31 cells: the cut start marked");
    expect(text_at(&f, 20 + 30 * 12, 142, 2, "_", 1), "the window ends at the cursor");
    expect(text_at(&f, 20 + 15 * 12, 142, 2, "123456789012345", 1), "the window's last digits");

    /* the status band */
    ab_init(&c);
    keys(&c, "RAD HEX PRGM");
    ui.shift = 1;
    ui.battery = 50;
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 58, 2, 2, "RAD", 1), "RAD at 58");
    expect(text_at(&f, 112, 2, 2, "HEX", 1), "HEX at 112");
    expect(text_at(&f, 154, 2, 2, "PRGM", 1), "PRGM at 154");
    expect(text_at(&f, 3, 2, 2, "GOLD", 0), "the gold shift: GOLD light on dark");
    expect(dark_in(&f, 0, 0, 2, 18) == 36 && dark_in(&f, 50, 0, 2, 18) == 36, "the shift's box, 52 wide");
    expect(dark_in(&f, 368, 4, 13, 10) == 130 && dark_in(&f, 381, 4, 13, 10) == 0, "half charged: 13 of 26 columns");
    expect(text_at(&f, 20, 128, 4, "PRGM TOP", 1), "program entry at the top: PRGM TOP");
    ui.shift = 2;
    ui.charging = true;
    keys(&c, "PRGM GRAD");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 3, 2, 2, "BLUE", 0), "the blue shift");
    expect(text_at(&f, 58, 2, 2, "GRAD", 1), "GRAD at 58");
    {
        /* 50 % charged: the plus at 378..382 light where the charge is (to 380), dark past it */
        const font_glyph *g = screen_glyph('+');
        int ok = g != NULL;
        for (int r = 0; g && r < 7; r++)
            for (int col = 0; col < 5; col++) {
                int lit = g->rows[r] >> (4 - col) & 1, x = 378 + col, y = 5 + r;
                int under = x <= 380 && y >= 4 && y <= 13;
                if (px(&f, x, y) != (lit ? !under : under)) ok = 0;
            }
        expect(ok, "charging: a plus over the battery, inverted over the charge");
    }
    expect(dark_in(&f, 154, 2, 48, 14) == 0, "program entry off: no PRGM");
    screen_ui_default(&ui);

    /* a message takes X, left at 20; the stack stays */
    ab_init(&c);
    keys(&c, "1 ENTER 0 /");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "DIVIDE BY 0", 1), "DIVIDE BY 0 in the X line");
    expect(glyph_at(&f, 388, 94, 2, '1', 1), "Y = 1 still shown");
    /* the op running takes X before all else */
    ui.busy = AB_SOLVE;
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "SOLVING", 1), "SOLVING while a SOLVE runs");
    screen_ui_default(&ui);

    /* a prompt takes Y, at scale 3, with no letter */
    ab_init(&c);
    c.stop = AB_STOP_INPUT;
    c.stop_var = 1;
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 87, 3, "B?", 1), "INPUT B: 'B?' in the Y line at scale 3");
    expect(dark_in(&f, 2, 94, 10, 14) == 0, "a prompt: no Y letter");

    /* the soft keys: a box over each key, the label centred */
    ab_init(&c);
    ui.soft[0] = "SOLVE";
    ui.soft[2] = "x";
    screen_render(&c, &ui, &f);
    expect(dark_in(&f, 2, 206, 64, 1) == 64 && dark_in(&f, 2, 235, 64, 1) == 64, "the first key's box");
    expect(text_at(&f, 2 + 3, 214, 2, "SOLVE", 1), "SOLVE centred in its box");
    expect(glyph_at(&f, 134 + 27, 214, 2, 'x', 1), "a one-cell label centred");
    expect(dark_in(&f, 68, 206, 64, 30) == 0, "an empty soft key: nothing drawn");
    screen_ui_default(&ui);

    /* the gaps a first mutation sweep showed */
    ab_init(&c);
    keys(&c, "1 2 CHS");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "-12_", 1), "a negative number being typed: its sign first");
    ab_init(&c);
    ab_do_arg(&c, AB_MODEL, 35);
    keys(&c, "E 3 ENTER");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 338, 2, 2, "35", 1), "the 35 mode shown");
    expect(text_at(&f, 20, 128, 4, "SYNTAX ERROR", 1), "35 mode, E 3 ENTER: SYNTAX ERROR, one line (#3822)");
    expect(dark_in(&f, 2, 142, 10, 14) == 0, "a message: no X letter");
    keys(&c, "ENTER");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "_E3", 1), "after the message: the cursor before the E (unit 017 rule 3)");
    ab_init(&c);
    for (int q = 0; q < c.eqn_count; q++)
        if (c.eqn[q][0] == '\x01') c.eqn_cur = q;
    c.eqn_mode = true;
    {
        long before = screen_missed;
        screen_render(&c, &ui, &f);
        expect(screen_missed == before, "a built-in solver: its mark not drawn");
        int len = screen_cells(c.eqn[c.eqn_cur] + 1);
        int sc = len <= 15 ? 4 : len <= 21 ? 3 : 2;
        expect(glyph_at(&f, 20, 156 - 7 * sc, sc, (unsigned char)c.eqn[c.eqn_cur][1], 1), "a built-in: its name from x = 20");
        expect(text_at(&f, 208, 2, 2, "EQN", 1), "Equation mode: EQN at 208");
        screen_text(&f, 0, 0, 1, "\xe4\xb8\x80", 1);
        expect(screen_missed == before + 1, "a hollow box counted");
    }
    ab_init(&c);
    keys(&c, "FDISP 2 ENTER 3 /");
    screen_render(&c, &ui, &f);
    expect(glyph_at(&f, 250, 2, 2, 0x25B2, 1), "2/3 shown above its 34 digits: up");
    ab_do_arg(&c, AB_SF, 8);
    ab_do_arg(&c, AB_SF, 9);
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 398 - (11 * 24 - 4), 128, 4, "0 2730/4095", 1), "flags 8 and 9: the fixed denominator 4095");
    ab_init(&c);
    keys(&c, "FDISP 1 ENTER 3 /");
    screen_render(&c, &ui, &f);
    expect(glyph_at(&f, 250, 2, 2, 0x25BC, 1), "1/3 shown below its 34 digits: down");
    ab_init(&c);
    ab_do_arg(&c, AB_SF, 1);
    ab_do_arg(&c, AB_SF, 3);
    screen_render(&c, &ui, &f);
    expect(glyph_at(&f, 280, 2, 2, '1', 1) && glyph_at(&f, 304, 2, 2, '3', 1), "flags 1 and 3 at their places");
    expect(dark_in(&f, 268, 2, 10, 14) == 0 && dark_in(&f, 292, 2, 10, 14) == 0 && dark_in(&f, 316, 2, 10, 14) == 0,
           "flags 0, 2, 4 clear: nothing");
    ab_init(&c);
    keys(&c, "[ 4 , 5");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "[4,5_]", 1), "a vector being typed, closed after the cursor");
    ab_init(&c);
    keys(&c, "3 I 4 ENTER RTA");
    screen_render(&c, &ui, &f);
    expect(find_glyph(&f, 156 - 28, 4, 0x03B8) > 0 || find_glyph(&f, 156 - 21, 3, 0x03B8) > 0, "polar: theta in X");
    ab_init(&c);
    keys(&c, "[ 1 E CHS 300 , 2 E CHS 300 , 3 E CHS 300 ENTER");
    screen_render(&c, &ui, &f);
    expect(glyph_at(&f, 388, 142, 2, ']', 1), "a vector too wide at 21 cells: the wide form at scale 2");
    ab_init(&c);
    c.eqn_mode = true;
    c.eqn_typing = true;
    snprintf(c.ebuf, sizeof c.ebuf, "\xe2\x88\x9a(A+B)");
    c.ecur = 4;
    screen_render(&c, &ui, &f);
    expect(glyph_at(&f, 20 + 2 * 24, 128, 4, '_', 1) && glyph_at(&f, 20 + 3 * 24, 128, 4, 'A', 1),
           "an equation being typed: the cursor at its byte offset");
    ab_init(&c);
    keys(&c, "7");
    ab_do_arg(&c, AB_STO, 2);
    c.stop = AB_STOP_VIEW;
    c.stop_var = 2;
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 87, 3, "C=7", 1), "VIEW C: 'C=7' in the Y line");
    ui.battery = 100;
    screen_render(&c, &ui, &f);
    expect(dark_in(&f, 368, 4, 26, 10) == 260, "full: all 26 columns");
    ui.battery = 0;
    screen_render(&c, &ui, &f);
    expect(dark_in(&f, 368, 4, 26, 10) == 0, "empty: none");
    screen_ui_default(&ui);

    /* ---- program lines as program entry lists them (UG Appendix G, ch.13-14 listings) ---- */
    ab_init(&c);
    keys(&c, "PRGM");
    ab_do_arg(&c, AB_LBL, 0);
    ab_do(&c, AB_SQ);
    ab_do(&c, AB_PI);
    ab_do(&c, AB_MUL);
    ab_do(&c, AB_RTN);
    expect(c.prog_len == 5, "13-8's area program keyed: five lines");
    expect_line(&c, 0, "LBL A", "13-8");
    expect_line(&c, 1, "x\xc2\xb2", "13-8");
    expect_line(&c, 2, "\xcf\x80", "13-8");
    expect_line(&c, 3, "\xc3\x97", "13-8");
    expect_line(&c, 4, "RTN", "13-8");
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "A005 RTN", 1), "PRGM: the pointer's address and its line");
    ab_init(&c);
    keys(&c, "PRGM");
    ab_do_arg(&c, AB_LBL, 'Q' - 'A');
    ab_do_arg(&c, AB_INPUT, 'R' - 'A');
    ab_do_arg(&c, AB_STOADD, 'X' - 'A');
    ab_do_arg(&c, AB_RCLSUB, 'B' - 'A');
    ab_do_arg(&c, AB_FS, 0);
    ab_do_arg(&c, AB_TEST, AB_TEST_LT);
    ab_do(&c, AB_SQRT);
    ab_do_arg(&c, AB_VIEW, 'X' - 'A');
    ab_do_arg(&c, AB_DSE, 'A' - 'A');
    ab_do_arg(&c, AB_XEQ, ('Q' - 'A') * 1000 + 1);
    ab_do_arg(&c, AB_GTO, ('Q' - 'A') * 1000 + 4);
    ab_do(&c, AB_BIN);
    expect_line(&c, 1, "INPUT R", "13-13");
    expect_line(&c, 2, "STO+ X", "14-8 (T010)");
    expect_line(&c, 3, "RCL- B", "14-17");
    expect_line(&c, 4, "FS? 0", "14-13");
    expect_line(&c, 5, "x<y?", "14-8");
    expect_line(&c, 6, "\xe2\x88\x9ax", "14-3");
    expect_line(&c, 7, "VIEW X", "13-17");
    expect_line(&c, 8, "DSE A", "14-17");
    expect_line(&c, 9, "XEQ Q001", "14-3");
    expect_line(&c, 10, "GTO Q004", "14-5");
    expect_line(&c, 11, "BIN", "13-25");
    /* a typed number in the base shown: 10 with DEC set, 10b with BIN set (13-25) */
    ab_init(&c);
    keys(&c, "PRGM 1 0 ENTER");
    expect_line(&c, 0, "10", "13-25, DEC");
    ab_init(&c);
    ab_do(&c, AB_BIN);                          /* BIN set, then program entry: BIN, 10 */
    keys(&c, "PRGM");
    ab_do(&c, AB_BIN);
    keys(&c, "1 0 ENTER");
    expect_line(&c, 1, "10b", "13-25, BIN");
    /* (I) and (J) as arguments, an equation line (13-9: C004 is pi*R^2*H), a typed number as typed
       under FIX 4 (14-19 lists 1.01) */
    ab_init(&c);
    ab_memory_clear(&c);                        /* FIX 4 */
    keys(&c, "PRGM");
    ab_do_arg(&c, AB_LBL, 'C' - 'A');
    ab_do_arg(&c, AB_STO, AB_VAR_IND_I);
    ab_do_arg(&c, AB_RCLMUL, AB_VAR_IND_J);
    ab_do_arg(&c, AB_XEQ, AB_TGT_IND_I);
    ab_eqn_add(&c, "\xcf\x80\xc3\x97R^2\xc3\x97H");
    keys(&c, "1 . 0 1 ENTER");
    expect_line(&c, 1, "STO (I)", "14-21");
    expect_line(&c, 2, "RCL\xc3\x97 (J)", "14-21");
    expect_line(&c, 3, "XEQ (I)", "14-21");
    expect_line(&c, 4, "\xcf\x80\xc3\x97R^2\xc3\x97H", "13-9");
    expect_line(&c, 6, "ENTER", "the ENTER that ended the number");
    ab_do(&c, AB_BST);                          /* the pointer back on the number */
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "C006 1.01", 1) && dark_in(&f, 20 + 9 * 24, 128, LS027_W - 20 - 9 * 24, 28) == 0,
           "14-19: a typed number lists as typed under FIX 4 (1.01, nothing after)");
    ui.radix = ',';
    screen_render(&c, &ui, &f);
    expect(text_at(&f, 20, 128, 4, "C006 1,01", 1), "a typed number listed with the device's radix mark");
    ui.radix = '.';
    expect_line(&c, 0, "LBL C", "13-9");

    /* the tests, in the menu's order (14-7) */
    ab_init(&c);
    keys(&c, "PRGM");
    for (int t = 0; t < 12; t++) ab_do_arg(&c, AB_TEST, t);
    {
        static const char *const T[12] = {"x\xe2\x89\xa0y?", "x\xe2\x89\xa4y?", "x<y?", "x>y?", "x\xe2\x89\xa5y?", "x=y?",
                                          "x\xe2\x89\xa0" "0?", "x\xe2\x89\xa4" "0?", "x<0?", "x>0?", "x\xe2\x89\xa5" "0?", "x=0?"};
        for (int t = 0; t < 12; t++) expect_line(&c, t, T[t], "14-7's tests");
    }
    /* every op the core takes as a line has a name the face can draw */
    {
        static const int ARGS[] = {0, 1, 4, 'Z' - 'A', AB_VAR_IND_I, AB_VAR_IND_J, 1001, AB_TGT_IND_I, 40, -1, 26000};
        static uint8_t img[1 << 16];
        int lines = 0, named = 0, kept = 0;
        for (int op = 0; op <= AB_MODEL; op++)
            for (int a = 0; a < 11; a++) {
                ab_init(&c);
                keys(&c, "PRGM");
                ab_do_arg(&c, AB_LBL, 1);       /* B, for a target B001 */
                int before = c.prog_len;
                ab_do_arg(&c, (ab_op)op, ARGS[a]);
                if (c.prog_len <= before || c.pc < 0) continue;
                lines++;
                char got[AB_EQN_LEN + 8], m[160];
                int n = ab_prog_line(&c, c.pc, NULL, got, sizeof got);
                /* "?" alone, or as an argument, is the mark of a line with no name */
                bool ok = n > 0 && strcmp(got, "?") != 0 && !strstr(got, " ?") && screen_missing(got) == 0;
                named += ok;
                if (!ok) {
                    snprintf(m, sizeof m, "op %d arg %d: a line with no drawable name ('%.40s')", op, ARGS[a], got);
                    expect(0, m);
                }
                /* and the core loads what it recorded: a line its own image refuses would cost a
                   MEMORY CLEAR at the next load */
                size_t n2 = ab_state_save(&c, img, sizeof img);
                bool back = n2 > 0 && n2 <= sizeof img && ab_state_load(&c, img, n2) == ABN_OK;
                kept += back;
                if (!back) {
                    snprintf(m, sizeof m, "op %d arg %d: the line '%.40s' recorded, its image refused", op, ARGS[a], got);
                    expect(0, m);
                }
            }
        char m[80];
        snprintf(m, sizeof m, "lines keyed %d, all named", lines);
        expect(lines > 100 && named == lines, m);
        snprintf(m, sizeof m, "lines keyed %d, all loaded back", lines);
        expect(kept == lines, m);
    }
    expect(ab_prog_line(&c, -1, NULL, (char[8]){0}, 8) == -1 && ab_prog_line(&c, 99999, NULL, (char[8]){0}, 8) == -1,
           "no such line: -1");

    /* ---- the frames ---- */
    screen_missed = 0;
    ab_init(&c);
    frame("01-fresh", &c, &ui);
    ab_memory_clear(&c);
    keys(&c, "1 ENTER 2 ENTER 3 ENTER 4");
    frame("02-stack-fix4", &c, &ui);
    keys(&c, "ENTER 1 2 . 5 E CHS 3");
    frame("03-typing", &c, &ui);
    ab_init(&c);
    ab_do_arg(&c, AB_MODEL, 35);
    keys(&c, "E 3 ENTER");
    frame("04-syntax-35", &c, &ui);
    keys(&c, "ENTER");
    frame("05-syntax-35-after", &c, &ui);
    ab_init(&c);
    keys(&c, "1 ENTER 0 /");
    frame("06-divide-by-0", &c, &ui);
    ab_init(&c);
    keys(&c, "RAD HEX 2 5 5 ENTER 1 6");
    ab_do_arg(&c, AB_SF, 1);
    ab_do_arg(&c, AB_SF, 3);
    ui.shift = 1;
    ui.battery = 72;
    ui.charging = true;
    frame("07-status-band", &c, &ui);
    screen_ui_default(&ui);
    ab_init(&c);
    keys(&c, "FDISP 1 ENTER 3 / 2 ENTER 3 /");
    frame("08-fractions", &c, &ui);
    ab_init(&c);
    keys(&c, "3 I 4 ENTER 5 THETA 3 0");
    frame("09-complex-typing", &c, &ui);
    keys(&c, "ENTER RTA");
    frame("10-complex-polar", &c, &ui);
    ab_init(&c);
    keys(&c, "[ 1 , 2 , 3 ENTER [ 4 , 5");
    frame("11-vectors", &c, &ui);
    ab_init(&c);
    keys(&c, "1 ENTER 7 / PI 2 SQRT");
    frame("12-all-digits", &c, &ui);
    ab_init(&c);
    keys(&c, "1234567890123456789012345 I 123456789012345");
    frame("13-window", &c, &ui);
    ab_init(&c);
    ab_eqn_add(&c, "A\xc3\x97" "B=C+SQRT(D)");     /* it turns Equation mode on */
    frame("14-equation", &c, &ui);
    c.eqn_typing = true;
    snprintf(c.ebuf, sizeof c.ebuf, "\xe2\x88\x9a(A+B)");
    c.ecur = 4;
    frame("15-equation-typing", &c, &ui);
    ab_init(&c);
    keys(&c, "4 2");
    c.stop = AB_STOP_INPUT;
    c.stop_var = 0;
    frame("16-prompt", &c, &ui);
    ab_init(&c);
    ui.busy = AB_INTEG;
    ui.battery = 5;
    frame("17-integrating", &c, &ui);
    screen_ui_default(&ui);
    ab_init(&c);
    keys(&c, "PRGM");
    ab_do_arg(&c, AB_LBL, 0);
    frame("18-program-entry", &c, &ui);
    ab_init(&c);
    {
        static const char *const SOFT[] = {"SOLVE", "\xe2\x88\xab", "PRGM", "x?y", "FLAGS", "\xe2\x88\xa1MODE"};
        for (int k = 0; k < SCREEN_SOFT; k++) ui.soft[k] = SOFT[k];
        ui.battery = 100;
    }
    frame("19-soft-keys", &c, &ui);
    screen_ui_default(&ui);
    ab_init(&c);
    keys(&c, "PRGM");
    ab_do_arg(&c, AB_LBL, 'S' - 'A');
    ab_do(&c, AB_SIGMAPLUS);
    ab_do(&c, AB_PSX);
    ab_do(&c, AB_XWBAR);
    ab_do(&c, AB_YHAT);
    frame("20-listing-marks", &c, &ui);
    expect(screen_missed == 0, "a frame drew a character the face lacks");

    printf("screen_test: %d/%d checks, %d/%d frames %s %s\n", checks - bad, checks, frames_ok, frames,
           writing ? "written to" : "match", frame_dir);
    return bad == 0 && frames_ok == frames ? 0 : 1;
}
