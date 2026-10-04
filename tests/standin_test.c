/* standin_test: the stand-in's screen and touch map (standin.h).
 *
 *     build/standin_test tests/frames/standin.crc            checks; screens compared by CRC
 *     build/standin_test --write tests/frames/standin.crc    checks; the CRC list written
 *
 * Every screen is also written to build/standin/NAME.png (palette PNG, stored deflate) for review
 * by eye; the checked-in reference is the CRC-32 of each screen's RGB565 lines (the PNGs would be
 * large uncompressed). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "keymap.h"
#include "screen.h"
#include "standin.h"
#include "state.h"

static int bad, checks;

static void expect(int ok, const char *what)
{
    checks++;
    if (!ok && bad++ < 40) printf("standin_test: %s\n", what);
}

static uint8_t nib(const uint8_t *img, int x, int y)
{
    uint8_t b = img[y * (SI_IMG_W / 2) + x / 2];
    return x % 2 ? b & 0x0F : b >> 4;
}

static int count(const uint8_t *img, int x0, int y0, int w, int h, int colour)
{
    int n = 0;
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) n += nib(img, x - SI_KEYS_X, y) == colour;
    return n;
}

/* ---- PNG (palette, 8 bits a pixel, stored deflate in blocks of at most 65535 bytes) ---- */

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static int chunk(FILE *o, const char *type, const uint8_t *d, size_t n)
{
    uint8_t *b = malloc(n + 4), len[4], crc[4];
    int ok;
    if (!b) return 0;
    memcpy(b, type, 4);
    if (n) memcpy(b + 4, d, n);
    be32(len, (uint32_t)n);
    be32(crc, ab_crc32(b, n + 4));
    ok = fwrite(len, 1, 4, o) == 4 && fwrite(b, 1, n + 4, o) == n + 4 && fwrite(crc, 1, 4, o) == 4;
    free(b);
    return ok;
}

static int write_png(const char *path, const uint16_t *px)
{
    enum { ROW = 1 + SI_W, RAW = ROW * SI_H };
    static uint8_t raw[RAW], z[2 + RAW + (RAW / 65535 + 1) * 5 + 4];
    uint8_t hdr[13] = {0}, plte[SI_COLOURS * 3];
    for (int i = 0; i < SI_COLOURS; i++) {
        uint16_t c = SI_PALETTE[i];
        plte[i * 3] = (uint8_t)((c >> 11) * 255 / 31);
        plte[i * 3 + 1] = (uint8_t)((c >> 5 & 63) * 255 / 63);
        plte[i * 3 + 2] = (uint8_t)((c & 31) * 255 / 31);
    }
    for (int y = 0; y < SI_H; y++) {
        raw[y * ROW] = 0;
        for (int x = 0; x < SI_W; x++) {
            int k = 0;
            while (k < SI_COLOURS - 1 && SI_PALETTE[k] != px[y * SI_W + x]) k++;
            raw[y * ROW + 1 + x] = (uint8_t)k;
        }
    }
    size_t n = 0;
    uint32_t a = 1, b2 = 0;
    z[n++] = 0x78;
    z[n++] = 0x01;
    for (size_t at = 0; at < RAW;) {
        size_t len = RAW - at > 65535 ? 65535 : RAW - at;
        z[n++] = at + len == RAW ? 1 : 0;
        z[n++] = (uint8_t)(len & 0xFF);
        z[n++] = (uint8_t)(len >> 8);
        z[n++] = (uint8_t)~(len & 0xFF);
        z[n++] = (uint8_t)~(len >> 8);
        memcpy(z + n, raw + at, len);
        n += len;
        at += len;
    }
    for (size_t i = 0; i < RAW; i++) {
        a = (a + raw[i]) % 65521;
        b2 = (b2 + a) % 65521;
    }
    be32(z + n, b2 << 16 | a);
    n += 4;
    be32(hdr, SI_W);
    be32(hdr + 4, SI_H);
    hdr[8] = 8;
    hdr[9] = 3;
    FILE *o = fopen(path, "wb");
    if (!o) return 0;
    int ok = fwrite("\x89PNG\r\n\x1a\n", 1, 8, o) == 8 && chunk(o, "IHDR", hdr, 13) &&
             chunk(o, "PLTE", plte, sizeof plte) && chunk(o, "IDAT", z, n) && chunk(o, "IEND", NULL, 0);
    return fclose(o) == 0 && ok;
}

/* ---- the screens ---- */

static uint16_t screen[SI_W * SI_H];
static char crc_lines[4096];
static int screens;

static uint32_t compose(const si_view *v)
{
    for (int y = 0; y < SI_H; y++) si_line(v, y, screen + y * SI_W);
    return ab_crc32((const uint8_t *)screen, sizeof screen);
}

static void shot(const char *name, const si_view *v)
{
    char path[128], line[96];
    uint32_t crc = compose(v);
    snprintf(path, sizeof path, "build/standin/%s.png", name);
    expect(write_png(path, screen), "a PNG written");
    snprintf(line, sizeof line, "%s %08lX\n", name, (unsigned long)crc);
    strncat(crc_lines, line, sizeof crc_lines - strlen(crc_lines) - 1);
    screens++;
}

int main(int argc, char **argv)
{
    static uint8_t keys[SI_IMG_BYTES];
    static ls027_frame frame;
    static ab_calc c;
    int writing = argc == 3 && !strcmp(argv[1], "--write");
    const char *ref = argc >= 2 ? argv[argc - 1] : NULL;
    char m[160];
    si_init();

    /* the touch map: every key is where it says, the keys tile the right half, nothing else is a key */
    {
        static uint8_t owner[SI_H][SI_W / 2];
        int keys_seen = 0;
        memset(owner, 0xFF, sizeof owner);
        for (int k = 0; k < KB_KEYS; k++) {
            int x, y, w, h;
            bool is = si_key_rect(k, &x, &y, &w, &h);
            snprintf(m, sizeof m, "key %d: a rect exactly when the layout has the key", k);
            expect(is == kb_exists(k / KB_COLS, k % KB_COLS), m);
            if (!is) continue;
            keys_seen++;
            expect(x >= SI_KEYS_X && x + w <= SI_W && y >= 0 && y + h <= SI_H, "a key inside the right half");
            expect(y == (k / KB_COLS) * SI_ROW_H && h == SI_ROW_H, "a key's row is its layout row, 60 px");
            for (int yy = y; yy < y + h; yy++)
                for (int xx = x; xx < x + w; xx++) {
                    if (owner[yy][xx - SI_KEYS_X] != 0xFF) expect(0, "two keys overlap");
                    owner[yy][xx - SI_KEYS_X] = (uint8_t)k;
                }
            expect(si_key_at(x + w / 2, y + h / 2) == k, "a key's centre maps to it");
        }
        expect(keys_seen == 43, "43 keys");
        int holes = 0, wrong = 0;
        for (int yy = 0; yy < SI_H; yy++)
            for (int xx = SI_KEYS_X; xx < SI_W; xx++) {
                holes += owner[yy][xx - SI_KEYS_X] == 0xFF;
                wrong += si_key_at(xx, yy) != (owner[yy][xx - SI_KEYS_X] == 0xFF ? -1 : owner[yy][xx - SI_KEYS_X]);
            }
        expect(holes == 0, "the keys tile the right half");
        expect(wrong == 0, "si_key_at agrees with the rects at every pixel");
        int x, y, w, h;
        si_key_rect(3 * KB_COLS + 0, &x, &y, &w, &h);
        expect(w == 133 || w == 134, "ENTER: two of row 3's six units");
        si_key_rect(4 * KB_COLS + 0, &x, &y, &w, &h);
        expect(w == 80, "row 4: five keys of 80 px");
        expect(si_key_at(10, 10) == -1 && si_key_at(399, 479) == -1 && si_key_at(-1, 0) == -1 && si_key_at(800, 0) == -1 &&
                   si_key_at(500, 480) == -1, "the left half and off the screen: no key");
        expect(si_on_frame(0, 120) && si_on_frame(399, 359) && !si_on_frame(0, 119) && !si_on_frame(0, 360) &&
                   !si_on_frame(400, 200), "the frame's area: x 0..399, y 120..359");
    }

    /* the keys as drawn: each legend where the layout puts it, in its colour */
    {
        si_keys_state ks = {0, false, -1};
        screen_missed = 0;
        si_draw_keys(keys, &ks);
        expect(screen_missed == 0, "every legend drawn from the face");
        for (int k = 0; k < KB_KEYS; k++) {
            int x, y, w, h;
            if (!si_key_rect(k, &x, &y, &w, &h)) continue;
            const km_action *f = km_lookup(k, KM_FACE), *g = km_lookup(k, KM_GOLD), *b = km_lookup(k, KM_BLUE);
            bool face_text = f->legend[0] != 0;
            snprintf(m, sizeof m, "key %d: its face legend in the face colour", k);
            if (face_text) expect(count(keys, x, y + 22, w, 30, SI_FACE) > 0, m);
            snprintf(m, sizeof m, "key %d: its gold legend at the top left, gold", k);
            if (g->legend[0]) expect(count(keys, x, y, w / 2, 20, SI_GOLD) > 0, m);
            snprintf(m, sizeof m, "key %d: its blue legend at the top right, blue", k);
            if (b->legend[0]) expect(count(keys, x + w / 2, y, w - w / 2, 20, SI_BLUE) > 0, m);
            snprintf(m, sizeof m, "key %d: no gold unless it has a gold legend", k);
            if (!g->legend[0] && f->kind != KM_SHIFT) expect(count(keys, x, y, w, h, SI_GOLD) == 0, m);
            const km_letter *l = km_letter_of(k);
            snprintf(m, sizeof m, "key %d: its faceplate letter, small, lower right", k);
            if (l->kind == KL_LETTER) expect(count(keys, x + w - 14, y + h - 14, 12, 12, SI_LETTER) > 0, m);
        }
        /* the shift held lights its key; a held key is drawn down */
        ks.shift = 1;
        ks.down = 3 * KB_COLS + 0;
        si_draw_keys(keys, &ks);
        int x, y, w, h;
        for (int k = 0; k < KB_KEYS; k++)
            if (km_lookup(k, KM_FACE)->kind == KM_SHIFT && km_lookup(k, KM_FACE)->arg == 1) {
                si_key_rect(k, &x, &y, &w, &h);
                expect(count(keys, x + 4, y + 4, w - 8, h - 8, SI_GOLD) > (w - 8) * (h - 8) / 2, "the gold shift held: its key lit gold");
            }
        si_key_rect(ks.down, &x, &y, &w, &h);
        expect(count(keys, x + 3, y + 3, w - 6, h - 6, SI_KEY_DOWN) > 0, "the key held: drawn down");
        /* the shift keys say their name (abacus #3865): light on the key, dark when it is lit */
        for (int k = 0; k < KB_KEYS; k++) {
            const km_action *f = km_lookup(k, KM_FACE);
            if (f->kind != KM_SHIFT) continue;
            si_key_rect(k, &x, &y, &w, &h);
            bool held = f->arg == ks.shift;
            int want = held ? SI_BG : SI_FACE, n = count(keys, x + 4, y + 22, w - 8, 20, want);
            snprintf(m, sizeof m, "shift key %d: its name in the face area (%s)", k, held ? "held, dark" : "light");
            expect(n > 40, m);
        }
        /* a shift held: its legends stay lit, everything else dims (as the 10" panel does; Michael,
           #3901): with GOLD held, a key's gold legend is gold, its face, blue legend and letter dim */
        ks = (si_keys_state){1, false, -1};
        si_draw_keys(keys, &ks);
        si_key_rect(6, &x, &y, &w, &h);                 /* sqrt x: gold x^2, blue ABS, letter A */
        expect(count(keys, x, y, w / 2, 20, SI_GOLD) > 0, "GOLD held: the gold legend stays gold");
        expect(count(keys, x, y, w, h, SI_BLUE) == 0, "GOLD held: no blue on the key (dimmed)");
        expect(count(keys, x, y + 22, w, 30, SI_FACE) == 0 && count(keys, x, y + 22, w, 30, SI_DIM) > 0,
               "GOLD held: the face legend dimmed");
        expect(count(keys, x + w - 14, y + h - 14, 12, 12, SI_LETTER) == 0, "GOLD held: the letter dimmed");
        ks.shift = 2;
        si_draw_keys(keys, &ks);
        expect(count(keys, x + w / 2, y, w - w / 2, 20, SI_BLUE) > 0 && count(keys, x, y, w, h, SI_GOLD) == 0,
               "BLUE held: the blue legend lit, the gold dimmed");
        ks.shift = 0;
        si_draw_keys(keys, &ks);
        expect(count(keys, x, y + 22, w, 30, SI_FACE) > 0 && count(keys, x, y, w, h, SI_DIM) == 0,
               "no shift: nothing dimmed");

        /* the letter layer while a prompt waits: each lettered key shows its letter large */
        ks = (si_keys_state){0, true, -1};
        si_draw_keys(keys, &ks);
        si_key_rect(12, &x, &y, &w, &h);                /* G */
        expect(count(keys, x, y + 10, w, 40, SI_LETTER) > 60, "the letter layer: G large on its key");
        expect(count(keys, x, y, w, h, SI_GOLD) == 0, "the letter layer: no shift legends");
    }

    /* the lines: the frame at x1 on the left, the keys on the right; x2 over everything */
    {
        static uint16_t line[SI_W];
        si_keys_state ks = {0, false, -1};
        si_draw_keys(keys, &ks);
        memset(&frame, 0, sizeof frame);
        frame.px[0][0] = 0x01;                          /* pixel (0, 0) dark */
        frame.px[239][49] = 0x80;                       /* pixel (399, 239) dark */
        si_view v = {&frame, keys, false};
        si_line(&v, SI_FRAME_Y, line);
        expect(line[0] == SI_PALETTE[SI_LCD_DARK] && line[1] == SI_PALETTE[SI_LCD_LIGHT], "x1: the frame's first row at y 120");
        bool keys_ok = true;
        for (int x = SI_KEYS_X; x < SI_W; x++) keys_ok &= line[x] == SI_PALETTE[nib(keys, x - SI_KEYS_X, SI_FRAME_Y)];
        expect(keys_ok, "x1: the right half is the keyboard image");
        si_line(&v, SI_FRAME_Y + 239, line);
        expect(line[399] == SI_PALETTE[SI_LCD_DARK] && line[398] == SI_PALETTE[SI_LCD_LIGHT], "x1: the frame's last row at y 359");
        si_line(&v, 0, line);
        bool bg = true;
        for (int x = 0; x < SI_KEYS_X; x++) bg &= line[x] == SI_PALETTE[SI_BG];
        expect(bg, "x1: above the frame, background");
        v.x2 = true;
        si_line(&v, 0, line);
        expect(line[0] == SI_PALETTE[SI_LCD_DARK] && line[1] == SI_PALETTE[SI_LCD_DARK] && line[2] == SI_PALETTE[SI_LCD_LIGHT],
               "x2: frame pixel (0,0) is screen pixels 0 and 1");
        si_line(&v, 1, line);
        expect(line[0] == SI_PALETTE[SI_LCD_DARK], "x2: and screen row 1");
        si_line(&v, 479, line);
        expect(line[799] == SI_PALETTE[SI_LCD_DARK] && line[797] == SI_PALETTE[SI_LCD_LIGHT], "x2: frame pixel (399,239) at the corner");
    }

    /* the screens, for review by eye */
    {
        static app_state a;
        screen_ui ui;
        si_keys_state ks = {0, false, -1};
        ab_init(&c);
        app_init(&a, &c, 0);
        app_key(&a, 6 * KB_COLS + 1, 10);               /* 1 */
        app_key(&a, 3 * KB_COLS + 0, 20);               /* ENTER */
        app_key(&a, 6 * KB_COLS + 2, 30);               /* 2 */
        app_ui(&a, &ui);
        screen_render(&c, &ui, &frame);
        si_draw_keys(keys, &ks);
        si_view v = {&frame, keys, false};
        shot("01-keys", &v);
        ks.shift = 1;
        si_draw_keys(keys, &ks);
        shot("02-gold-held", &v);
        app_key(&a, 2 * KB_COLS + 0, 40);               /* STO: a prompt */
        app_ui(&a, &ui);
        screen_render(&c, &ui, &frame);
        ks = (si_keys_state){0, true, 12};
        si_draw_keys(keys, &ks);
        shot("03-letter-layer", &v);
        ks = (si_keys_state){0, false, -1};
        si_draw_keys(keys, &ks);
        v.x2 = true;
        shot("04-x2-view", &v);
    }

    if (ref && writing) {
        FILE *o = fopen(ref, "w");
        expect(o && fputs(crc_lines, o) >= 0 && fclose(o) == 0, "the CRC list written");
    } else if (ref) {
        char want[4096] = "";
        FILE *f = fopen(ref, "r");
        size_t n = f ? fread(want, 1, sizeof want - 1, f) : 0;
        want[n] = 0;
        if (f) fclose(f);
        expect(strcmp(want, crc_lines) == 0, "the screens match their checked-in CRCs");
        if (strcmp(want, crc_lines)) printf("standin_test: now:\n%s", crc_lines);
    }
    printf("standin_test: %d/%d checks, %d screens%s\n", checks - bad, checks, screens, writing ? " (CRCs written)" : "");
    return bad != 0;
}
