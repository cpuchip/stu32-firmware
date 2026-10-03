/* red: unit 018 part 2's stub, replaced by the implementation in the green commit */
#include "screen.h"
#include <string.h>
long screen_missed;
void screen_ui_default(screen_ui *ui) { memset(ui, 0, sizeof *ui); ui->battery = -1; ui->radix = '.'; }
void screen_render(const ab_calc *c, const screen_ui *ui, ls027_frame *f) { (void)c; (void)ui; memset(f, 0, sizeof *f); }
void screen_clear(ls027_frame *f) { memset(f, 0, sizeof *f); }
void screen_fill(ls027_frame *f, int x, int y, int w, int h, bool dark) { (void)f; (void)x; (void)y; (void)w; (void)h; (void)dark; }
int screen_text(ls027_frame *f, int x, int y, int scale, const char *s, bool dark) { (void)f; (void)x; (void)y; (void)scale; (void)s; (void)dark; return 0; }
const font_glyph *screen_glyph(uint32_t cp) { (void)cp; return NULL; }
int screen_cells(const char *s) { (void)s; return 0; }
int screen_missing(const char *s) { (void)s; return 0; }
