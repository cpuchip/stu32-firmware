/* red: unit 018 part 2's stub, replaced by the implementation in the green commit */
#include "power.h"
void pwr_init(pwr_state *p, uint32_t n) { (void)p; (void)n; }
void pwr_activity(pwr_state *p, uint32_t n) { (void)p; (void)n; }
pwr_action pwr_check(pwr_state *p, const pwr_inputs *i, uint32_t n) { (void)p; (void)i; (void)n; return PWR_STAY; }
bool pwr_charging(const pwr_inputs *i) { (void)i; return false; }
bool pwr_external(const pwr_inputs *i) { (void)i; return false; }
