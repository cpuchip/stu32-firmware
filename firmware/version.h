/* version: the firmware's version for SYS's VER (unit 027 rule 5). The core's commit comes from the
 * build (AB_CORE_COMMIT, written at every device build from git, +dirty for an uncommitted tree),
 * the build date from the compiler. */
#ifndef VERSION_H
#define VERSION_H

#define AB_FW_VERSION "0.67"        /* "0.<the last work unit>" until 1.0; each unit bumps it
                                       (abacus #6810; scripts/check-version.sh) */
#ifdef AB_HAVE_COMMIT_H
#include "ab_commit.h"              /* generated at every build (scripts/gen_commit.cmake) */
#endif
#ifndef AB_CORE_COMMIT
#define AB_CORE_COMMIT "dev"        /* a build that did not say (the host's tests) */
#endif
#define AB_BUILT __DATE__

#endif
