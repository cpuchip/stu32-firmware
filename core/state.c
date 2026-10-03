/* state: unit 016 red stub (the image arrives with the green commit). */
#include "state.h"

uint32_t ab_crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

size_t ab_state_save(const ab_calc *c, uint8_t *buf, size_t cap)
{
    (void)c;
    (void)buf;
    (void)cap;
    return 0;
}

abn_status ab_state_load(ab_calc *c, const uint8_t *buf, size_t len)
{
    (void)c;
    (void)buf;
    (void)len;
    return ABN_BAD_IMAGE;
}
