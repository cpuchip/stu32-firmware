/* fmt_drive: formats one value per input line, for tests/fmt_fuzz.py.
 *
 *   input:  value MODE n width radix sep      (e.g. "0.125 FIX 2 22 . 1")
 *   output: the text, or <none> when nothing fits
 */
#include <stdio.h>
#include <string.h>

#include "fmt.h"

int main(void)
{
    char value[128], mode[8], radix[4];
    int n, width, sep;
    while (scanf("%127s %7s %d %d %3s %d", value, mode, &n, &width, radix, &sep) == 6) {
        ab_fmt f = {AB_ALL, n, width, radix[0], sep != 0};
        if (strcmp(mode, "FIX") == 0) f.mode = AB_FIX;
        else if (strcmp(mode, "SCI") == 0) f.mode = AB_SCI;
        else if (strcmp(mode, "ENG") == 0) f.mode = AB_ENG;
        abn v;
        char out[512];
        if (abn_from_text(&v, value) != ABN_OK) {
            puts("<bad value>");
            continue;
        }
        if (ab_format(&v, &f, out, sizeof out) < 0) puts("<none>");
        else puts(out);
    }
    return 0;
}
