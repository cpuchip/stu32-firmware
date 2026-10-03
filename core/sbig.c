/* sbig: short bigint and xnum (see sbig.h), from the same algorithms as the wide ones. */
#include <string.h>

#include "sbig.h"

#define BI_BASE 1000000000u
#define BI_T sbig
#define BI_CAP SB_LIMBS
#define BI_(name) sb_##name
#include "bigint_impl.h"

#define XN_T sxnum
#define XN_(name) sxn_##name
#include "xnum_impl.h"
