/* sbig: short bigint and xnum (see sbig.h), from the same algorithms as the wide ones, with their
 * scratch numbers from a small pool of their own (the most live at once is 7: the square root). */
#include <stdarg.h>
#include <string.h>

#include "sbig.h"

#define SB_POOL 10
static sxnum pool[SB_POOL];
static int top;

static bool take(int n, bool want_big, va_list ap)
{
    if (top + n > SB_POOL) return false;
    for (int i = 0; i < n; i++) {
        sxnum *x = &pool[top++];
        if (want_big) *va_arg(ap, sbig **) = &x->m;
        else *va_arg(ap, sxnum **) = x;
    }
    return true;
}

static bool take_bi(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    bool ok = take(n, true, ap);
    va_end(ap);
    return ok;
}

static bool take_xn(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    bool ok = take(n, false, ap);
    va_end(ap);
    return ok;
}

#define BI_BASE 1000000000u
#define BI_T sbig
#define BI_CAP SB_LIMBS
#define BI_(name) sb_##name
#define AR_MARK() top
#define AR_RELEASE(m) (top = (m))
#define BI_TAKE(...) take_bi(__VA_ARGS__)
#define XN_TAKE(...) take_xn(__VA_ARGS__)
#include "bigint_impl.h"

#define XN_T sxnum
#define XN_(name) sxn_##name
#include "xnum_impl.h"
