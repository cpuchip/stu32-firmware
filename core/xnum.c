/* xnum: exact decimal numbers on bigint (see xnum.h); the algorithms are in xnum_impl.h, their
 * scratch numbers from the wide pool (core/arena.h). */
#include <string.h>

#include "arena.h"
#include "xnum.h"

#define BI_T bigint
#define BI_(name) bi_##name
#define XN_T xnum
#define XN_(name) xn_##name
#define AR_MARK() ar_mark()
#define AR_RELEASE(m) ar_release(m)
#define BI_TAKE(...) ar_bi(__VA_ARGS__)
#define XN_TAKE(...) ar_xn(__VA_ARGS__)
#include "xnum_impl.h"
