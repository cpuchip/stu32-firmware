/* bigint: exact non-negative integers (see bigint.h); the algorithms are in bigint_impl.h, their
 * scratch numbers from the wide pool (core/arena.h). */
#include <string.h>

#include "arena.h"
#include "bigint.h"

#define BI_T bigint
#define BI_CAP BI_LIMBS
#define BI_(name) bi_##name
#define AR_MARK() ar_mark()
#define AR_RELEASE(m) ar_release(m)
#define BI_TAKE(...) ar_bi(__VA_ARGS__)
#include "bigint_impl.h"
