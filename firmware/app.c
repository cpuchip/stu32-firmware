/* red: unit 018 device part 1's stub, replaced by the implementation in the green commit */
#include <string.h>
#include "app.h"
void app_init(app_state *a, ab_calc *c, uint32_t now_ms) { memset(a, 0, sizeof *a); a->c = c; a->prompt_op = -1; (void)now_ms; }
void app_key(app_state *a, int key, uint32_t now_ms) { (void)a; (void)key; (void)now_ms; }
app_action app_tick(app_state *a, const pwr_inputs *in, uint32_t now_ms) { (void)a; (void)in; (void)now_ms; return APP_IDLE; }
void app_saved(app_state *a) { (void)a; }
void app_ui(const app_state *a, screen_ui *ui) { (void)a; screen_ui_default(ui); }
