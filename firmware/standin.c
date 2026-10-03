/* standin: the 4.3" board's screen and touch map (see standin.h). */
#include "standin.h"

#include <string.h>

#include "calc.h"
#include "keymap.h"
#include "screen.h"

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define SI_FAST IRAM_ATTR                       /* si_line runs in the RGB driver's interrupt */
#else
#define SI_FAST
#endif

const uint16_t SI_PALETTE[SI_COLOURS] = {
    [SI_BG] = 0x2104,                           /* #202020 */
    [SI_KEY] = 0x4208,                          /* #404040 */
    [SI_KEY_DOWN] = 0x8410,                     /* #808080 */
    [SI_FACE] = 0xFFFF,
    [SI_GOLD] = 0xFD20,                         /* #FFA500 */
    [SI_BLUE] = 0x4D9F,                         /* #48B4F8 */
    [SI_LETTER] = 0xBDF7,                       /* #B8BCB8 */
    [SI_EDGE] = 0x6B4D,                         /* #686868 */
    [SI_LCD_DARK] = 0x1943,                     /* the memory LCD's dark and light pixels */
    [SI_LCD_LIGHT] = 0xCE78,
};

/* ---- where the keys are: each row's keys share 400 px by width units ---- */

static int units(int row)
{
    return row <= 3 ? 6 : 5;                    /* rows 0-2 six keys; row 3 ENTER (two) and four; 4-7 five */
}

static int unit_start(int row, int col)
{
    return row == 3 && col > 0 ? col + 1 : col; /* ENTER takes row 3's first two units */
}

static int unit_width(int row, int col)
{
    return row == 3 && col == 0 ? 2 : 1;
}

bool si_key_rect(int key, int *x, int *y, int *w, int *h)
{
    if (key < 0 || key >= KB_KEYS) return false;
    int row = key / KB_COLS, col = key % KB_COLS;
    if (!kb_exists(row, col)) return false;
    int u = units(row), a = unit_start(row, col), b = a + unit_width(row, col);
    *x = SI_KEYS_X + (SI_W - SI_KEYS_X) * a / u;
    *w = SI_KEYS_X + (SI_W - SI_KEYS_X) * b / u - *x;
    *y = row * SI_ROW_H;
    *h = SI_ROW_H;
    return true;
}

int si_key_at(int x, int y)
{
    if (x < SI_KEYS_X || x >= SI_W || y < 0 || y >= SI_H) return -1;
    int row = y / SI_ROW_H;
    for (int col = 0; col < KB_COLS; col++) {
        int kx, ky, kw, kh;
        if (si_key_rect(row * KB_COLS + col, &kx, &ky, &kw, &kh) && x >= kx && x < kx + kw) return row * KB_COLS + col;
    }
    return -1;
}

bool si_on_frame(int x, int y)
{
    return x >= 0 && x < SI_KEYS_X && y >= SI_FRAME_Y && y < SI_FRAME_Y + LS027_H;
}

/* ---- drawing into the 4-bit image (x in screen coordinates) ---- */

static void put(uint8_t *img, int x, int y, int colour)
{
    x -= SI_KEYS_X;
    if (x < 0 || x >= SI_IMG_W || y < 0 || y >= SI_H) return;
    uint8_t *b = &img[y * (SI_IMG_W / 2) + x / 2];
    *b = x % 2 ? (uint8_t)((*b & 0xF0) | colour) : (uint8_t)((*b & 0x0F) | colour << 4);
}

static void fill(uint8_t *img, int x, int y, int w, int h, int colour)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) put(img, i, j, colour);
}

static int text_width(const char *s, int scale)
{
    uint32_t cells[32];
    int n = screen_decode(s, cells, 32);
    return n ? n * 6 * scale - scale : 0;
}

/* text with its glyphs' top row at y (a mark's row is the one above) */
static void text(uint8_t *img, int x, int y, int scale, const char *s, int colour)
{
    uint32_t cells[32];
    int n = screen_decode(s, cells, 32);
    for (int i = 0; i < n; i++) {
        uint8_t rows[8];
        screen_cell_bitmap(cells[i], rows);
        for (int r = 0; r < 8; r++)
            for (int col = 0; col < 5; col++)
                if (rows[r] >> (4 - col) & 1) fill(img, x + i * 6 * scale + col * scale, y + (r - 1) * scale, scale, scale, colour);
    }
}

static const char *role_text(const km_letter *l)
{
    static const char *const A[4] = {"+", "-", "\xc3\x97", "\xc3\xb7"};
    switch (l->kind) {
    case KL_IND_I: return "(I)";
    case KL_IND_J: return "(J)";
    case KL_ESC: return "ESC";
    case KL_ARITH: return A[l->value == AB_ADD ? 0 : l->value == AB_SUB ? 1 : l->value == AB_MUL ? 2 : 3];
    default: return "";
    }
}

void si_draw_keys(uint8_t *img, const si_keys_state *s)
{
    memset(img, SI_BG << 4 | SI_BG, SI_IMG_BYTES);
    for (int k = 0; k < KB_KEYS; k++) {
        int x, y, w, h;
        if (!si_key_rect(k, &x, &y, &w, &h)) continue;
        const km_action *face = km_lookup(k, KM_FACE);
        bool shift_key = face->kind == KM_SHIFT, held = shift_key && s->shift == face->arg;
        int body = k == s->down ? SI_KEY_DOWN : SI_KEY;
        int shift_colour = shift_key && face->arg == 1 ? SI_GOLD : SI_BLUE;
        fill(img, x + 2, y + 2, w - 4, h - 4, held ? shift_colour : body);
        fill(img, x + 2, y + 2, w - 4, 1, SI_EDGE);
        fill(img, x + 2, y + h - 3, w - 4, 1, SI_EDGE);
        fill(img, x + 2, y + 2, 1, h - 4, SI_EDGE);
        fill(img, x + w - 3, y + 2, 1, h - 4, SI_EDGE);
        if (s->letters) {                       /* the letter layer: each key's letter or role, large */
            const km_letter *l = km_letter_of(k);
            if (l->kind == KL_LETTER) {
                char t[2] = {(char)('A' + l->value), 0};
                text(img, x + (w - text_width(t, 4)) / 2, y + (h - 28) / 2, 4, t, SI_LETTER);
            } else if (l->kind != KL_NONE) {
                const char *t = role_text(l);
                text(img, x + (w - text_width(t, 2)) / 2, y + (h - 14) / 2, 2, t, SI_FACE);
            }
            continue;
        }
        if (shift_key) {                        /* a band of its colour, and its name (#3865) */
            const char *name = face->arg == 1 ? "GOLD" : "BLUE";
            if (!held) fill(img, x + 3, y + 3, w - 6, 6, shift_colour);
            text(img, x + (w - text_width(name, 2)) / 2, y + 24, 2, name, held ? SI_BG : SI_FACE);
            continue;
        }
        const km_action *g = km_lookup(k, KM_GOLD), *b = km_lookup(k, KM_BLUE);
        if (g->legend[0]) text(img, x + 5, y + 6, 1, g->legend, SI_GOLD);
        if (b->legend[0]) text(img, x + w - 5 - text_width(b->legend, 1), y + 6, 1, b->legend, SI_BLUE);
        if (face->legend[0]) text(img, x + (w - text_width(face->legend, 2)) / 2, y + 24, 2, face->legend, SI_FACE);
        const km_letter *l = km_letter_of(k);
        if (l->kind == KL_LETTER) {             /* the faceplate letter (decision 25), lower right */
            char t[2] = {(char)('A' + l->value), 0};
            text(img, x + w - 5 - 5, y + h - 5 - 7, 1, t, SI_LETTER);
        }
    }
}

/* ---- composing a line: tables only ---- */

static uint16_t lut1[256][8];                   /* a frame byte at x1: 8 pixels, bit 0 the leftmost */
static uint16_t lut2[256][16];                  /* at x2 */
static uint16_t lutk[256][2];                   /* a keyboard image byte: its two pixels */

void si_init(void)
{
    for (int b = 0; b < 256; b++) {
        for (int i = 0; i < 8; i++) {
            uint16_t c = SI_PALETTE[b >> i & 1 ? SI_LCD_DARK : SI_LCD_LIGHT];
            lut1[b][i] = c;
            lut2[b][2 * i] = lut2[b][2 * i + 1] = c;
        }
        lutk[b][0] = SI_PALETTE[(b >> 4) < SI_COLOURS ? b >> 4 : SI_BG];
        lutk[b][1] = SI_PALETTE[(b & 15) < SI_COLOURS ? b & 15 : SI_BG];
    }
}

void SI_FAST si_line(const si_view *v, int y, uint16_t *out)
{
    if (v->x2) {
        const uint8_t *row = v->frame->px[y / 2];
        for (int i = 0; i < LS027_ROW_BYTES; i++) memcpy(out + i * 16, lut2[row[i]], 32);
        return;
    }
    if (y >= SI_FRAME_Y && y < SI_FRAME_Y + LS027_H) {
        const uint8_t *row = v->frame->px[y - SI_FRAME_Y];
        for (int i = 0; i < LS027_ROW_BYTES; i++) memcpy(out + i * 8, lut1[row[i]], 16);
    } else {
        for (int x = 0; x < SI_KEYS_X; x++) out[x] = SI_PALETTE[SI_BG];
    }
    const uint8_t *k = v->keys + y * (SI_IMG_W / 2);
    for (int i = 0; i < SI_IMG_W / 2; i++) {
        out[SI_KEYS_X + 2 * i] = lutk[k[i]][0];
        out[SI_KEYS_X + 2 * i + 1] = lutk[k[i]][1];
    }
}
