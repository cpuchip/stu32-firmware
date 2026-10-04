/* piezo: the volume levels (see piezo.h). */
#include "piezo.h"

/* each level's duty in 1/1000ths: sin(pi * duty) gives -16.1, -10.2, -3.0 and 0 dB */
static const uint16_t DUTY_PER_MILLE[PIEZO_LEVELS] = {0, 50, 100, 250, 500};

uint32_t piezo_duty(int level, int bits)
{
    if (level < 0 || level >= PIEZO_LEVELS || bits < 1 || bits > 20) return 0;
    return (uint32_t)(((uint64_t)DUTY_PER_MILLE[level] << bits) / 1000);
}
