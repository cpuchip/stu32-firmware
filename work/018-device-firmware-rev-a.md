# Work unit 018: the STU-32 device firmware for Rev A (display, keys, power, memory)

**From:** abacus · **To:** soroban · **Status:** ready. The first unit outside `core/`: the program
that makes the Rev A board a calculator. Rev A is being re-placed now (seat `draftsman`) and ordered
after; this unit is what we bring it up with.

## Sources you may read

- abacus `hw/rev-a/circuit.py` (every net and GPIO: the MCU block's `nets` map is the pin truth;
  draftsman moves parts, not pins, and will tell us if that changes), `docs/hw/rev-a-architecture.md`
  (as built), `docs/research/rev-a/2026-10-03-datasheet-facts.md` (the LS027B7DH01A protocol,
  timing, power sequence and VCOM rules with Sharp's page numbers; the BQ24072, MAX17048, TPS61222
  and ESP32-S3 facts), `layout/stu32-v0.json` (the 43 keys: rows, columns, legends).
- The manufacturers' own datasheets the facts doc cites, and Espressif's ESP-IDF documentation.
- Not: any calculator's source (Free42, Plus42, DB48X, WP 34S, NumWorks...), HP ROMs or firmware.
  Library code under a permissive licence for a part (a display driver, for instance) only as a
  reading aid for the part's datasheet, never copied; say in the report what you read.

## Scope

1. **The 35s's error messages, in the core** (`ab_status_message`, appendix F's exact texts):
   one string per `abn_status` and stop cause, the guide's page for each. The panel, the app (seat
   `tally`) and this firmware all show these.
2. **Keys:** the 8 × 6 matrix (rows IO1, IO2, IO4-IO9 with 100k pull-ups; columns IO10-IO15;
   diodes anode at the row): drive one column low, read the rows; debounce; key-down events to the
   core in order; no ghosting by construction (the diodes). Deep sleep: the ON key (C, row 7 on IO9)
   wakes the S3 (EXT1 on an RTC GPIO), with the columns held low through sleep.
3. **Display:** the LS027B7DH01A, 400 × 240, 1 bit: the Sharp serial format (M0/M1/M2, the line
   address LSB first, 50 data bytes a line, the trailer), SCLK at most 2 MHz (SPI on IO38 SCLK,
   IO39 SI, IO18 SCS), DISP on IO21, EXTCOMIN on IO16 at 0.5-10 Hz (EXTMODE is tied high on the
   board: COM inversion follows EXTCOMIN's rising edge), the power sequence of Sharp p.11 with
   BOOST_EN (IO17) and its 30 us rule before SCS. In deep sleep the image stays: the ULP toggles
   EXTCOMIN, BOOST_EN and DISP are held (all three are RTC GPIOs for this reason).
4. **The screen's layout v0 (ours, for Michael to react to):** status line (annunciators: shift,
   angle mode, base, PRGM, the 33s/35s mode, battery), the stack (X large, Y, Z, T smaller; the
   panel shows the same and Michael has seen it), a soft-key label row for the menus. A frame
   buffer the core's text fills; draw it with a font licensed for this (name it). Keep the renderer
   portable C so it also runs on the host and on the 4.3" S3 board.
5. **Power and battery:** BOOST_EN off at boot (its pull-down; never on before the display wants
   it), the charger's CHG_N (IO42) and PGOOD_N (IO47) as inputs, EN1 (IO3) and EN2 (IO48) left in
   USB500 (the board cannot read CC); the MAX17048 on I2C (IO40 SDA, IO41 SCL) for the state of
   charge; auto power-off after an idle time (the 35s: 10 minutes, p.1-1).
6. **Continuous memory on flash:** unit 016's image in a dedicated partition, two slots with a
   sequence number, written on power-off, before deep sleep and after a quiet interval, never
   both at once, so a power cut at any moment leaves one good image; a refused load does
   MEMORY CLEAR and shows its message (state.h). The core's state in PSRAM (two calculators do not
   fit internal DRAM, your 016 finding).
7. **Program runs:** in slices (016) on their own task, so the keyboard stays live and C or R/S
   interrupts (13-19).

## Acceptance (what can be tested before the board exists)

- On the host: the matrix scan against a simulated key matrix (every key alone, pairs, bounce);
  the LS027 frame encoder bit-exact against the facts doc's format (a known image's bytes); the
  two-slot store under a power cut at every byte offset of every write (never both slots bad);
  the renderer's frames for a set of states, checked in as images.
- On the 4.3" S3 board (lepton): the whole firmware with its RGB screen standing in for the LS027
  (the 400 × 240 frame scaled ×2) and its touch screen standing in for the matrix: the
  calculator runs, programs run in slices and stop on a touch, the state survives a reset.
- The number it ends on: the power-cut test N/N, and the device vector run (your tests/device)
  passing with this firmware's store and slices in the loop.

## By Sunday night (the house's week resets)

The calculator running on the 4.3" S3 board with the Rev A screen layout, its state surviving
resets, and the LS027, matrix and power drivers written and host-tested, waiting for Rev A.
