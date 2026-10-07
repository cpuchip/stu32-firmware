/* ls027_test: the frame encoder against an independent model of Sharp's bit sequence (unit 018).
 *
 * The model works in bits, in Sharp's order (facts 1.4): M0 M1 M2, 5 dummies, then per line
 * AG0..AG7, D1..D400, 8 dummies, and 8 more at the end; it then packs them as an LSB-first SPI
 * sends them (the first bit of each byte is its bit 0). The encoder's bytes must equal the
 * model's for a test pattern, for every-row and partial updates, in both pixel polarities; the
 * line addresses must match p. 20's table (L1 H L L L L L L L, L2 L H..., L3 H H..., L240
 * L L L L H H H H). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ls027.h"

static uint8_t bits[1 + 8 + 240 * (8 + 400 + 8) + 8 + 64];
static int nbits;

static void bit(int b) { bits[nbits++] = (uint8_t)(b & 1); }

static int pixel(const ls027_frame *f, int x, int y) { return f->px[y][x / 8] >> (x % 8) & 1; }

static size_t model(const ls027_frame *f, const bool *dirty, bool one_white, uint8_t *out)
{
    nbits = 0;
    bit(1); bit(0); bit(0);                     /* M0 = update, M1, M2 */
    for (int i = 0; i < 5; i++) bit(0);
    for (int y = 0; y < 240; y++) {
        if (dirty && !dirty[y]) continue;
        int line = y + 1;
        for (int k = 0; k < 8; k++) bit(line >> k);         /* AG0 first */
        for (int x = 0; x < 400; x++) {
            int dark = pixel(f, x, y);
            bit(one_white ? !dark : dark);
        }
        for (int i = 0; i < 8; i++) bit(0);
    }
    for (int i = 0; i < 8; i++) bit(0);
    size_t n = (size_t)(nbits + 7) / 8;
    memset(out, 0, n);
    for (int i = 0; i < nbits; i++) out[i / 8] |= (uint8_t)(bits[i] << (i % 8));   /* LSB first */
    return n;
}

int main(void)
{
    static ls027_frame f;
    static uint8_t a[20000], b[20000];
    static bool dirty[240];
    int bad = 0;
    /* a pattern: diagonals, a border, a checker in one corner */
    for (int y = 0; y < 240; y++)
        for (int x = 0; x < 400; x++) {
            int on = x == y || x == 399 - y || x == 0 || y == 0 || x == 399 || y == 239 ||
                     (x < 40 && y < 40 && ((x / 4 + y / 4) & 1)) || (x * 7 + y * 13) % 97 == 0;
            if (on) f.px[y][x / 8] |= (uint8_t)(1u << (x % 8));
        }
    for (int i = 0; i < 240; i++) dirty[i] = i % 7 == 3 || i == 0 || i == 239;
    for (int pol = 0; pol < 2; pol++) {
        ls027_one_is_white = pol;
        for (int part = 0; part < 2; part++) {
            const bool *d = part ? dirty : NULL;
            size_t na = ls027_encode(&f, d, a, sizeof a), nb = model(&f, d, pol, b);
            if (na != nb || memcmp(a, b, na) != 0) {
                printf("ls027_test: polarity %d, %s: encoder and model differ (%zu vs %zu bytes)\n", pol,
                       part ? "partial" : "full", na, nb);
                bad++;
            }
        }
    }
    /* p. 20's address table, AG0..AG7 as H/L */
    {
        static const struct { int line; const char *ag; } T[] = {
            {1, "HLLLLLLL"}, {2, "LHLLLLLL"}, {3, "HHLLLLLL"}, {238, "LHHHLHHH"}, {239, "HHHHLHHH"}, {240, "LLLLHHHH"}};
        for (size_t t = 0; t < sizeof T / sizeof T[0]; t++) {
            static bool one[240];
            memset(one, 0, sizeof one);
            one[T[t].line - 1] = true;
            ls027_one_is_white = true;
            size_t n = ls027_encode(&f, one, a, sizeof a);
            uint8_t addr = a[1];
            for (int k = 0; k < 8; k++)
                if (((addr >> k) & 1) != (T[t].ag[k] == 'H')) {
                    printf("ls027_test: line %d's address bits do not match p. 20\n", T[t].line);
                    bad++;
                    break;
                }
            if (n != 1 + 52 + 1) bad++;
        }
    }
    /* the full update's size: 1 + 240 x 52 + 1 */
    if (ls027_encode(&f, NULL, a, sizeof a) != 1 + 240 * 52 + 1) bad++;
    /* apps.h v1.2 item 1: the framework sends only the lines that changed since the frame last sent */
    {
        static ls027_frame shown, next;
        static bool d[240];
        memset(&shown, 0, sizeof shown);
        memset(&next, 0, sizeof next);
        int same = ls027_dirty(&shown, &next, d), first = ls027_dirty(NULL, &next, d);
        next.px[7][49] ^= 0x80;                 /* the last pixel of line 7 */
        next.px[200][0] ^= 0x01;                /* the first of line 200 */
        int two = ls027_dirty(&shown, &next, d);
        bool which = true;
        for (int y = 0; y < 240; y++) which = which && d[y] == (y == 7 || y == 200);
        if (same != 0 || first != 240 || two != 2 || !which) {
            printf("ls027_test: ls027_dirty: same %d, first %d, two %d\n", same, first, two);
            bad++;
        }
    }
    printf("ls027_test: %s\n", bad ? "FAIL" : "the encoder matches Sharp's bit order (full, partial, both polarities, p. 20 addresses)");
    return bad ? 1 : 0;
}
