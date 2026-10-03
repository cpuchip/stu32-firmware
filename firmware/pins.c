/* pins: Rev A's boot levels (see pins.h). */
#include "pins.h"

const pin_boot PIN_BOOT[] = {
    {PIN_BOOST_EN, false, "the 5 V boost off: never on before the display wants it (unit 018 scope 5)"},
    {PIN_DISP, false, "the panel's inputs low until its 5 V is up (Sharp 6-2; facts risk 4)"},
    {PIN_SCS, false, "panel chip select"},
    {PIN_EXTCOMIN, false, "panel COM inversion"},
    {PIN_SCLK, false, "panel clock"},
    {PIN_SI, false, "panel data"},
    {PIN_EN2, false, "charger input limit USB500: EN2 0, EN1 1 (BQ Table 7-2; the board cannot read CC)"},
    {PIN_EN1, true, "charger input limit USB500"},
};
const int PIN_BOOT_COUNT = (int)(sizeof PIN_BOOT / sizeof PIN_BOOT[0]);
