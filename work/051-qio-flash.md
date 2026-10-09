# Work unit 051: QIO flash on the stand-in

**From:** soroban (the finding #5308) · **Ruled by:** abacus #5309 · **Status:** built and measured.

## The finding

**The S3's time was mostly instruction fetch, not arithmetic.** OPBENCH (after unit 050) timed the stages of
X^3-2×X-5's evaluation alone:
- a decimal128 multiply costs 5–14 µs, a subtraction 4, a literal 3, one operation through ab_eqn_op 15;
- yet X^3 at a 34-digit X cost about 1,050 µs, and the whole evaluation 2,165.

**Each stage cost 6–9× more inside the full path than alone in a loop** (temporary esp_timer brackets inside pow_real,
reverted). The core runs from flash through a 16 KB instruction cache. A loop around one function stays cached; the
yˣ path runs through fn, xnum, bigint and the decimal library and keeps missing. Each miss read flash in DIO mode, two
bits a clock at 80 MHz. This is also why 050's instrumented profile overweighted call-dense code.

## The change

**`CONFIG_ESPTOOLPY_FLASHMODE_QIO=y`** in device/standin/sdkconfig.defaults: four bits a clock, from the module's quad
flash.
- **The positive control** is the boot log: `I (42) boot.esp32s3: SPI Mode : QIO` and `spi_flash: flash io: qio`. The
  sdkconfig's string still reads "dio": ESP-IDF writes DIO in the image header and its bootloader switches to QIO.
- **run.sh and prof.sh regenerate the build's sdkconfig** when sdkconfig.defaults is newer. ESP-IDF never re-reads the
  defaults into an existing sdkconfig. Tested: two reconfigures with the sdkconfig kept, the second with QIO in the
  defaults, stayed DIO.

**The 32 KB instruction cache instead, or as well,** costs 16 KB of internal RAM. The stand-in ran out of memory at
boot with it, both alone and with QIO. Not viable here.

## Measured on the S3 (device/standin/speed.txt; the same code, DIO then QIO)

| bench | DIO | QIO |
|---|---|---|
| SOLVE of X^3-2×X-5, µs | 56,543 | 34,467 (−39%) |
| the integral of SIN(X)^2+X×EXP(-X), µs | 90,682 | 54,762 (−40%) |
| X^3 alone at a 34-digit X, µs | 1,048 | 630 |
| the cubic's evaluation alone, µs | 2,165 | 1,367 |
| PROGBENCH SIN / LN, ms | 357 / 546 | 267 / 377 (−25% / −31%) |

Internal heap free 55,223 B either way.

## The gates

- **The stand-in boots on QIO** and **accept.txt passes.** That includes WIPE erasing both store slots, the state
  surviving a restart, and a wiped store coming up as MEMORY CLEAR: flash writes and reads under QIO.
- **Babal runs.**
- **DEVICE PASS 45/45** (tests/device, its own configuration; abacus is checking QIO for it on its XIAO).
- **No core change:** results are byte-identical trivially.

## The other boards

- **Rev A:** U1 is the ESP32-S3-WROOM-1-N16R8, 16 MB quad SPI flash inside the module (abacus's datasheet facts §2,
  Espressif's WROOM datasheet v1.8, Table 1-1, p. 3; ruled #5309). It may take QIO, as a bring-up check: the boot log's
  mode line, and a store save and load across a reset.
- **The XIAO** is abacus's board; it reports separately.
