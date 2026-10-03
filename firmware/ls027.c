/* ls027: the memory LCD's serial format (see ls027.h). */
#include "ls027.h"

bool ls027_one_is_white = true;

size_t ls027_encode(const ls027_frame *f, const bool *dirty, uint8_t *out, size_t cap)
{
    size_t n = 1, need = 1;
    for (int y = 0; y < LS027_H; y++)
        if (!dirty || dirty[y]) need += 2 + LS027_ROW_BYTES;
    need += 1;
    if (need > cap) return need;
    out[0] = 0x01;                              /* M0 = 1 (update), M1 = 0, M2 = 0, dummies 0 */
    for (int y = 0; y < LS027_H; y++) {
        if (dirty && !dirty[y]) continue;
        out[n++] = (uint8_t)(y + 1);            /* the line number, AG0 first on an LSB-first wire */
        for (int i = 0; i < LS027_ROW_BYTES; i++)
            out[n++] = ls027_one_is_white ? (uint8_t)~f->px[y][i] : f->px[y][i];
        out[n++] = 0x00;                        /* 8 dummy bits */
    }
    out[n++] = 0x00;                            /* the last 8 of the 16 trailing dummy bits */
    return n;
}

size_t ls027_display_mode(uint8_t *out)
{
    out[0] = 0x00;                              /* M0 = 0, M2 = 0: keep the image (p. 17) */
    out[1] = 0x00;                              /* 13 or more dummy clocks in all */
    return 2;
}

size_t ls027_all_clear(uint8_t *out)
{
    out[0] = 0x04;                              /* M2 = 1: all clear (p. 18) */
    out[1] = 0x00;
    return 2;
}
