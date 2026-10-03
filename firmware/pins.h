/* pins: Rev A's GPIO assignment (unit 018; docs/hw/circuit.py, the MCU block's nets map, which is
 * the pin truth) and the level each output takes at boot, before anything else runs. */
#ifndef PINS_H
#define PINS_H

#include <stdbool.h>

/* the key matrix (keys.h): rows are RTC GPIOs so a key can wake the chip */
#define PIN_ROWS {1, 2, 4, 5, 6, 7, 8, 9}
#define PIN_COLS {10, 11, 12, 13, 14, 15}
/* the LS027 (ls027.h) and its 5 V boost */
#define PIN_EXTCOMIN 16
#define PIN_BOOST_EN 17
#define PIN_SCS 18
#define PIN_DISP 21
#define PIN_SCLK 38
#define PIN_SI 39
/* the fuel gauge's I2C (gauge.h) and the charger (BQ24072) */
#define PIN_SDA 40
#define PIN_SCL 41
#define PIN_CHG_N 42            /* input: low while the battery charges (open drain, pulled up) */
#define PIN_PGOOD_N 47          /* input: low while a valid input source is present */
#define PIN_EN1 3
#define PIN_EN2 48

typedef struct {
    int pin;
    bool level;
    const char *why;
} pin_boot;

/* Every output and its first level, in the order they are set. */
extern const pin_boot PIN_BOOT[];
extern const int PIN_BOOT_COUNT;

#endif
