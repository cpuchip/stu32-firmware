/* version: the firmware's version for SYS's VER (unit 027 rule 5). The core's commit comes from the
 * build (AB_CORE_COMMIT, the device's CMake from git), the build date from the compiler. */
#ifndef VERSION_H
#define VERSION_H

#define AB_FW_VERSION "0.27"        /* the last work unit in it */
#ifndef AB_CORE_COMMIT
#define AB_CORE_COMMIT "dev"        /* a build that did not say (the host's tests) */
#endif
#define AB_BUILT __DATE__

#endif
