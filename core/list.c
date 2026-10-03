/* red: unit 018 part 3's stub, replaced by the implementation in the green commit */
#include <stdio.h>
#include "calc.h"
int ab_prog_line(const ab_calc *c, int i, const ab_display *d, char *out, int size)
{
    (void)c; (void)i; (void)d;
    if (size > 0) snprintf(out, (size_t)size, "?");
    return -1;
}
