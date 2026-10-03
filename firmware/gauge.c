/* red: unit 018 part 2's stub, replaced by the implementation in the green commit */
#include "gauge.h"
bool gauge_read(const gauge_bus *b, uint8_t reg, uint16_t *v) { (void)b; (void)reg; (void)v; return false; }
bool gauge_write(const gauge_bus *b, uint8_t reg, uint16_t v) { (void)b; (void)reg; (void)v; return false; }
uint32_t gauge_microvolts(uint16_t v) { (void)v; return 0; }
int gauge_percent(uint16_t v) { (void)v; return 0; }
int32_t gauge_crate(uint16_t v) { (void)v; return 0; }
bool gauge_init(const gauge_bus *b, uint16_t *v) { (void)b; (void)v; return false; }
bool gauge_sample(const gauge_bus *b, gauge_reading *o) { (void)b; (void)o; return false; }
