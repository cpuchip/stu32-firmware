/* piezo: the volume of Rev A's piezo (decision 34; abacus 2b8e52d). It is driven by a square wave
 * (LEDC on IO46), and its loudness is set by the duty cycle: the fundamental's amplitude scales as
 * sin(pi * duty), so 50% is loudest, 25% about -3 dB, 10% about -10 dB, 5% about -16 dB. Where the
 * setting lives waits for the menu ruling; the levels are here. Portable C. */
#ifndef PIEZO_H
#define PIEZO_H

#include <stdint.h>

#define PIEZO_LEVELS 5          /* 0 off, 1 -16 dB, 2 -10 dB, 3 -3 dB, 4 loudest */
#define PIEZO_HZ 4000           /* the tones' frequency: the PKMCS0909E4000's rated 4 kHz (65 dB min;
                                   abacus #4073). Untested until Rev A: a phone SPL meter then */

/* The duty for a level, out of 2^bits (LEDC's resolution); 0 for off or a level out of range. */
uint32_t piezo_duty(int level, int bits);

#endif
