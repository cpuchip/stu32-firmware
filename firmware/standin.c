/* red: unit 018 device part 2's stub, replaced by the implementation in the green commit */
#include <string.h>
#include "standin.h"
const uint16_t SI_PALETTE[SI_COLOURS];
bool si_key_rect(int key, int *x, int *y, int *w, int *h) { (void)key; (void)x; (void)y; (void)w; (void)h; return false; }
int si_key_at(int x, int y) { (void)x; (void)y; return -1; }
bool si_on_frame(int x, int y) { (void)x; (void)y; return false; }
void si_draw_keys(uint8_t *img, const si_keys_state *s) { (void)s; memset(img, 0, SI_IMG_BYTES); }
void si_line(const si_view *v, int y, uint16_t *out) { (void)v; (void)y; memset(out, 0, SI_W * 2); }
void si_init(void) {}
