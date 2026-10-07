/* settings: the device's own settings (unit 022, provisional homes in RADIX and SETUP), kept with the
 * state image so they survive a power cut. The core keeps none of them: the radix mark and the
 * grouping reach the screen through screen_ui, the rest are app.c's and power.c's.
 *
 * The saved image is the core's state image (state.h) followed by a device record:
 *
 *   "ABDS" version:u16 0:u16 radix:u8 group:u8 off_min:u8 vol:u8 uses:u8 h24:u8 crc:u32
 *
 * (version 2, unit 027; version 1 had beep:u8 where uses is, and no h24: it loads with every use on
 * if its BEEP was on, none if it was off, and the 12-hour clock)
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

#define DEV_SETTINGS_VERSION 2

/* The buzzer's uses (unit 027 rule 3), each on or off: */
enum {
    DEV_USE_CLICK = 1 << 0,     /* a short tick on every key */
    DEV_USE_ERROR = 1 << 1,     /* the error tone */
    DEV_USE_PRGM = 1 << 2,      /* the BEEP instruction (ours) */
    DEV_USE_BATT = 1 << 3,      /* once when the battery first reads low */
    DEV_USE_ON = 1 << 4,        /* a chirp at power-on (a wake from OFF) */
    DEV_USE_DONE = 1 << 5,      /* the end of a run that took longer than 2 s */
    DEV_USE_APP = 1 << 6,       /* an app's tones (app_beep, unit 037) */
    DEV_USE_ALL = (1 << 7) - 1
};

typedef struct {
    char radix;             /* '.' or ',' (UG p.1-24) */
    char group;             /* digit grouping: 0 none, 1 the guide's mark (',', or '.' when the radix
                               is ','), ' ' a space (ours) */
    uint8_t off_min;        /* the auto-off time in minutes: 2, 5, 10 or 30 (ours; the 35s's 10) */
    uint8_t vol;            /* the piezo's level, 0..PIEZO_LEVELS-1 (piezo.h) */
    uint8_t uses;           /* DEV_USE_*: the buzzer's uses on (unit 027; unit 022 had one BEEP switch) */
    bool h24;               /* the clock in 24-hour form, else 12 (unit 027: Michael is in the US) */
} dev_settings;

/* radix '.', grouping 1,000, 10 minutes, the loudest level, every use on, the 12-hour clock (unit 022
   rule 3, with abacus #3965: the 35s groups by default, p.1-23 and p.B-3, and the 33s always does;
   unit 027 rules 3 and 4) */
void dev_settings_default(dev_settings *s);

/* The core's image of c followed by the record of s, into buf when it fits in cap; returns the
   whole length either way (0 when the core writes no image). */
size_t dev_image_save(const ab_calc *c, const dev_settings *s, uint8_t *buf, size_t cap);

/* Loads the core's image into c (ab_state_load) and the record into s, or s's defaults when the
   record is missing, from a later version or damaged. Returns the core's status: ABN_BAD_IMAGE
   when the core refuses its image, and then c is unchanged and s holds the defaults. */
abn_status dev_image_load(ab_calc *c, dev_settings *s, const uint8_t *buf, size_t n);

#endif
