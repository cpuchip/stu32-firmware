/* gauge: the MAX17048 (see gauge.h). */
#include "gauge.h"

bool gauge_read(const gauge_bus *b, uint8_t reg, uint16_t *v)
{
    uint8_t r[2];
    if (!b->xfer(b->ctx, GAUGE_ADDR, &reg, 1, r, 2)) return false;
    *v = (uint16_t)(r[0] << 8 | r[1]);          /* the high byte first (p.10) */
    return true;
}

bool gauge_write(const gauge_bus *b, uint8_t reg, uint16_t v)
{
    uint8_t w[3] = {reg, (uint8_t)(v >> 8), (uint8_t)v};    /* a whole word: 8-bit writes do nothing */
    return b->xfer(b->ctx, GAUGE_ADDR, w, 3, NULL, 0);
}

uint32_t gauge_microvolts(uint16_t vcell)
{
    return (uint32_t)vcell * 625u / 8u;         /* 78.125 uV = 625/8 uV a bit, exactly */
}

int gauge_percent(uint16_t soc)
{
    int p = (soc + 128) >> 8;                   /* 1/256 % a bit, to the nearest percent */
    return p > 100 ? 100 : p;
}

int32_t gauge_crate(uint16_t crate)
{
    return (int32_t)(int16_t)crate * 208;       /* 0.208 %/hr a bit; negative while discharging */
}

bool gauge_init(const gauge_bus *b, uint16_t *version)
{
    uint16_t st;
    if (!gauge_read(b, GAUGE_VERSION, version) || (*version & 0xFFF0) != 0x0010) return false;
    if (!gauge_read(b, GAUGE_STATUS, &st)) return false;
    /* RI: the part reset and holds its ROM model, which is the one Rev A uses; clear the bit so
       a later reset shows (p.14) */
    if (st & GAUGE_STATUS_RI) return gauge_write(b, GAUGE_STATUS, (uint16_t)(st & ~GAUGE_STATUS_RI));
    return true;
}

bool gauge_sample(const gauge_bus *b, gauge_reading *out)
{
    uint16_t v, s, cr, st;
    if (!gauge_read(b, GAUGE_VCELL, &v) || !gauge_read(b, GAUGE_SOC, &s) || !gauge_read(b, GAUGE_CRATE, &cr) ||
        !gauge_read(b, GAUGE_STATUS, &st))
        return false;
    out->microvolts = gauge_microvolts(v);
    out->percent = gauge_percent(s);
    out->crate = gauge_crate(cr);
    out->was_reset = (st & GAUGE_STATUS_RI) != 0;
    return true;
}
