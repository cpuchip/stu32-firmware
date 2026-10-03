/* xnum: exact decimal numbers on bigint (see xnum.h); the algorithms are in xnum_impl.h. */
#include <string.h>

#include "xnum.h"

#define BI_T bigint
#define BI_(name) bi_##name
#define XN_T xnum
#define XN_(name) xn_##name
#include "xnum_impl.h"
