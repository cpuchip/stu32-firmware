/* settings: the device's own settings (unit 022, provisional homes in RADIX and SETUP), kept with the
 * state image so they survive a power cut. The core keeps none of them: the radix mark and the
 * grouping reach the screen through screen_ui, the rest are app.c's and power.c's.
 *
 * The saved image is the core's state image (state.h) followed by a device record:
 *
 *   "ABDS" version:u16 0:u16 radix:u8 group:u8 off_min:u8 vol:u8 beep:u8 crc:u32
 *
 * the CRC-32 (state.h's) over the record before it. The core's image says its own length, so the
 * record is found after it. A record that is missing (an image from before this unit), from a
 * later version, or damaged gives the defaults; the core's image loads either way. CLEAR ALL keeps
 * the settings (the core never touches them); MEMORY CLEAR, which the device does after a refused
 * load, gives the defaults. */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "calc.h"

#define DEV_SETTINGS_VERSION 1

typedef struct {
    char radix;             /* '.' or ',' (UG p.1-24) */
    char group;             /* digit grouping: 0 none, 1 the guide's mark (',', or '.' when the radix
                               is ','), ' ' a space (ours) */
    uint8_t off_min;        /* the auto-off time in minutes: 2, 5, 10 or 30 (ours; the 35s's 10) */
    uint8_t vol;            /* the piezo's level, 0..PIEZO_LEVELS-1 (piezo.h) */
    bool beep;              /* the buzzer's uses on or off as a whole */
} dev_settings;

/* radix '.', no grouping, 10 minutes, the loudest level, BEEP on (unit 022 rule 3) */
void dev_settings_default(dev_settings *s);

/* The core's image of c followed by the record of s, into buf when it fits in cap; returns the
   whole length either way (0 when the core writes no image). */
size_t dev_image_save(const ab_calc *c, const dev_settings *s, uint8_t *buf, size_t cap);

/* Loads the core's image into c (ab_state_load) and the record into s, or s's defaults when the
   record is missing, from a later version or damaged. Returns the core's status: ABN_BAD_IMAGE
   when the core refuses its image, and then c is unchanged and s holds the defaults. */
abn_status dev_image_load(ab_calc *c, dev_settings *s, const uint8_t *buf, size_t n);

#endif
