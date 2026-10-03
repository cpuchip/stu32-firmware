/* gauge: the MAX17048 fuel gauge over I2C (unit 018 scope 5). From Maxim's datasheet 19-6171
 * Rev 7: the 7-bit address 0x36 (p.16: 0x6C write, 0x6D read); every register a 16-bit word, its
 * high byte first, an 8-bit write ignored (p.10); VCELL 0x02 at 78.125 uV a bit, SOC 0x04 at
 * 1/256 %, VERSION 0x08 reading 0x001_, CRATE 0x16 at 0.208 %/hr, STATUS 0x1A with RI (bit 8)
 * set at power-up "so the model should be loaded and the bit should be cleared" (pp.10-14).
 * Rev A leaves /ALRT unconnected, so the gauge is polled; it keeps its ROM model.
 *
 * Portable C: the bus is the caller's (ESP-IDF's I2C master on the device, a register file in
 * the test). */
#ifndef GAUGE_H
#define GAUGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GAUGE_ADDR 0x36
#define GAUGE_VCELL 0x02
#define GAUGE_SOC 0x04
#define GAUGE_MODE 0x06
#define GAUGE_VERSION 0x08
#define GAUGE_CONFIG 0x0C
#define GAUGE_CRATE 0x16
#define GAUGE_STATUS 0x1A
#define GAUGE_STATUS_RI 0x0100

typedef struct {
    void *ctx;
    /* writes wn bytes to the 7-bit address, then (rn > 0) reads rn with a repeated start; false on
       a NACK or a bus error */
    bool (*xfer)(void *ctx, uint8_t addr, const uint8_t *w, size_t wn, uint8_t *r, size_t rn);
} gauge_bus;

typedef struct {
    uint32_t microvolts;    /* the cell */
    int percent;            /* state of charge, 0..100 */
    int32_t crate;          /* charge (+) or discharge (-) rate, in 0.001 %/hr */
    bool was_reset;         /* the gauge had reset (RI) since it was last cleared */
} gauge_reading;

bool gauge_read(const gauge_bus *b, uint8_t reg, uint16_t *v);
bool gauge_write(const gauge_bus *b, uint8_t reg, uint16_t v);

uint32_t gauge_microvolts(uint16_t vcell);
int gauge_percent(uint16_t soc);        /* rounded to the nearest, at most 100 */
int32_t gauge_crate(uint16_t crate);    /* in 0.001 %/hr */

/* At boot: false unless the part answers with its version (0x001_); clears RI if set. */
bool gauge_init(const gauge_bus *b, uint16_t *version);
bool gauge_sample(const gauge_bus *b, gauge_reading *out);

#endif
