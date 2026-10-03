/* arena: one pool for the wide exact numbers (see arena.h). */
#include <stdarg.h>

#include "arena.h"

#ifndef AB_POOL_ATTR
#define AB_POOL_ATTR                /* a device may place the pool: the stand-in puts it in PSRAM */
#endif
static xnum pool[AB_ARENA_WIDE] AB_POOL_ATTR;
static int top, peak;

int ar_mark(void)
{
    return top;
}

void ar_release(int mark)
{
    top = mark;
}

bool ar_xn(int n, ...)
{
    va_list ap;
    if (n < 0 || top + n > AB_ARENA_WIDE) return false;
    va_start(ap, n);
    for (int i = 0; i < n; i++) *va_arg(ap, xnum **) = &pool[top++];
    va_end(ap);
    if (top > peak) peak = top;
    return true;
}

bool ar_bi(int n, ...)
{
    va_list ap;
    if (n < 0 || top + n > AB_ARENA_WIDE) return false;
    va_start(ap, n);
    for (int i = 0; i < n; i++) *va_arg(ap, bigint **) = &pool[top++].m;
    va_end(ap);
    if (top > peak) peak = top;
    return true;
}

int ar_peak(void)
{
    return peak;
}
