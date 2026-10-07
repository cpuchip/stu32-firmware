/* SPDX-License-Identifier: MIT */
/* draw (draw.h). From stu32-arcade sdk/draw.c at 7f0e37c (arcade, MIT), taken into the framework through soroban's
   gate (apps.h v1.2, abacus #4734): int64_t where it widened with long (32 bits on the device). */
#include <limits.h>
#include <string.h>

#include "draw.h"
#include "font5x7.h"

void draw_clear(ls027_frame *f)
{
    memset(f, 0, sizeof *f);
}

void draw_pixel(ls027_frame *f, int x, int y, int dark)
{
    if (x < 0 || x >= LS027_W || y < 0 || y >= LS027_H)
        return;
    uint8_t m = (uint8_t)(1u << (x & 7));
    if (dark)
        f->px[y][x >> 3] |= m;
    else
        f->px[y][x >> 3] &= (uint8_t)~m;
}

/* Pixels [x0, x1) of row y take shade byte m (both set and cleared). */
static void span(ls027_frame *f, int y, int x0, int x1, uint8_t m)
{
    uint8_t *row = f->px[y];
    int b0 = x0 >> 3, b1 = (x1 - 1) >> 3;
    uint8_t first = (uint8_t)(0xff << (x0 & 7)), last = (uint8_t)(0xff >> (7 - ((x1 - 1) & 7)));
    if (b0 == b1) {
        uint8_t k = first & last;
        row[b0] = (uint8_t)((row[b0] & ~k) | (m & k));
        return;
    }
    row[b0] = (uint8_t)((row[b0] & ~first) | (m & first));
    for (int b = b0 + 1; b < b1; b++)
        row[b] = m;
    row[b1] = (uint8_t)((row[b1] & ~last) | (m & last));
}

void draw_fill(ls027_frame *f, int x, int y, int w, int h, draw_shade shade)
{
    if (w <= 0 || h <= 0)
        return;
    int64_t x0 = x, y0 = y, x1 = (int64_t)x + w, y1 = (int64_t)y + h;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > LS027_W)
        x1 = LS027_W;
    if (y1 > LS027_H)
        y1 = LS027_H;
    for (int64_t r = y0; r < y1 && x0 < x1; r++)
        span(f, (int)r, (int)x0, (int)x1, (uint8_t)((r & 1) ? shade >> 8 : shade & 0xff));
}

/* v to int, saturated (a coordinate past the frame stays past it) */
static int sat(int64_t v)
{
    return v < INT_MIN ? INT_MIN : v > INT_MAX ? INT_MAX : (int)v;
}

void draw_frame_rect(ls027_frame *f, int x, int y, int w, int h, int dark)
{
    if (w <= 0 || h <= 0)
        return;
    draw_shade s = dark ? DRAW_DARK : DRAW_LIGHT;
    draw_fill(f, x, y, w, 1, s);
    draw_fill(f, x, sat((int64_t)y + h - 1), w, 1, s);      /* (int64_t: y + h may pass INT_MAX, soroban's gate) */
    draw_fill(f, x, y, 1, h, s);
    draw_fill(f, sat((int64_t)x + w - 1), y, 1, h, s);
}

/* floor(k * minor / major + 1/2), exactly, for 0 <= k <= major <= 2^32 and minor <= major: with k = q1 * major + r1
   (q1 0 or 1), r1 * minor < 2^64 */
static uint64_t nearest(uint64_t k, uint64_t minor, uint64_t major)
{
    uint64_t q1 = k / major, r1 = k % major, p = r1 * minor;
    uint64_t q = q1 * minor + p / major, r = p % major;
    return q + (2 * r >= major);
}

/* Along the major axis, step k (0..|major|) puts the pixel nearest the true line on the minor axis,
 * a tie going away from the start: minor = start + s * floor((2k|minor| + |major|) / (2|major|)).
 * Only the steps that land on the frame are walked, so a line costs at most 400 steps however far
 * its ends lie. Exact for any int ends (soroban's gate: the ends were clamped to +-2^29 each, which bent a far
 * line's slope, and long was 32 bits on the device). */
void draw_line(ls027_frame *f, int x0, int y0, int x1, int y1, int dark)
{
    int64_t ax = x0, ay = y0, bx = x1, by = y1;
    int64_t adx = bx > ax ? bx - ax : ax - bx, ady = by > ay ? by - ay : ay - by;
    int64_t sx = bx >= ax ? 1 : -1, sy = by >= ay ? 1 : -1;
    int xmajor = adx >= ady;
    int64_t major = xmajor ? adx : ady, minor = xmajor ? ady : adx;
    int64_t m0 = xmajor ? ax : ay, n0 = xmajor ? ay : ax, sm = xmajor ? sx : sy, sn = xmajor ? sy : sx;
    int64_t size = xmajor ? LS027_W : LS027_H, nsize = xmajor ? LS027_H : LS027_W;
    /* the steps k for which m0 + sm * k lies in [0, size) */
    int64_t klo = sm > 0 ? -m0 : m0 - (size - 1), khi = sm > 0 ? size - 1 - m0 : m0;
    if (klo < 0)
        klo = 0;
    if (khi > major)
        khi = major;
    for (int64_t k = klo; k <= khi; k++) {
        int64_t n = major ? n0 + sn * (int64_t)nearest((uint64_t)k, (uint64_t)minor, (uint64_t)major) : n0;
        int64_t m = m0 + sm * k;
        if (n < 0 || n >= nsize)
            continue;
        if (xmajor)
            draw_pixel(f, (int)m, (int)n, dark);
        else
            draw_pixel(f, (int)n, (int)m, dark);
    }
}

void draw_blit(ls027_frame *f, int x, int y, const uint8_t *bits, int w, int h, enum draw_mode mode)
{
    int stride = (w + 7) / 8;
    for (int r = 0; r < h; r++) {
        int64_t py = (int64_t)y + r;
        if (py < 0 || py >= LS027_H)
            continue;
        for (int c = 0; c < w; c++) {
            int64_t px = (int64_t)x + c;
            if (px < 0 || px >= LS027_W)
                continue;
            int on = (bits[r * stride + c / 8] >> (c % 8)) & 1;
            uint8_t m = (uint8_t)(1u << (px & 7));
            uint8_t *b = &f->px[py][px >> 3];
            switch (mode) {
            case DRAW_OR: if (on) *b |= m; break;
            case DRAW_SET: *b = on ? (uint8_t)(*b | m) : (uint8_t)(*b & ~m); break;
            case DRAW_XOR: if (on) *b ^= m; break;
            case DRAW_CLEAR: if (on) *b &= (uint8_t)~m; break;
            }
        }
    }
}

static const font_glyph *glyph(uint32_t cp)
{
    for (int i = 0; i < FONT5X7_COUNT; i++)
        if (FONT5X7[i].cp == cp)
            return &FONT5X7[i];
    return NULL;
}

int draw_text_width(const char *s, int scale)
{
    return sat((int64_t)strlen(s) * 6 * scale);
}

int draw_text(ls027_frame *f, int x, int y, int scale, const char *s, int dark)
{
    int64_t cx = x;                             /* (int64_t: a large scale passed INT_MAX, soroban's gate) */
    for (; *s; s++) {
        const font_glyph *g = glyph((uint8_t)*s);
        if (g)
            for (int r = 0; r < 7; r++)
                for (int c = 0; c < 5; c++)
                    if (g->rows[r] & (0x10 >> c))
                        draw_fill(f, sat(cx + (int64_t)c * scale), sat((int64_t)y + (int64_t)r * scale), scale, scale,
                                  dark ? DRAW_DARK : DRAW_LIGHT);
        cx += (int64_t)6 * scale;
    }
    return sat(cx);
}
