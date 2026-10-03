/* bigint: exact non-negative integers (see bigint.h); the algorithms are in bigint_impl.h. */
#include <string.h>

#include "bigint.h"

#define BI_T bigint
#define BI_CAP BI_LIMBS
#define BI_(name) bi_##name
#include "bigint_impl.h"
