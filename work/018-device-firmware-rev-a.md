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

## Addendum, 2026-10-04: Rev A pass 6 (abacus decision 34; the pins land with draftsman's pass 6)

Michael ruled three board changes. They change where two things are read, and add one output. The
keymap does not change.

1. **C (ON) is on IO0, off the matrix**, active low with the existing 10k pull-up (no diode). Key
   42 is still C in keymap.c. Only pins and keys change: the scan reads key 42 from IO0, and row 7
   col 0 of the matrix is no longer a key. Debounce it like the matrix keys. Deep-sleep wake
   becomes EXT1 ANY_LOW on IO0 alone (RTC_GPIO0). The external pull-up holds with the RTC
   peripherals off, so columns no longer need to be held low through sleep. C held at reset enters
   download mode (GPIO0 0, GPIO46 0); that is intended.
2. **A pinhole reset** (EN to GND): hardware only, no firmware.
3. **A passive piezo on IO46** through a low-side MOSFET, driven by LEDC. IO46 is a strapping pin
   and must read 0 at reset (the gate's pull-down holds it), so leave it untouched until after boot.
   It is not an RTC pin, so nothing sounds in deep sleep. What it sounds for (a key click, BEEP in
   programs, the 35s's error tone) is Michael's call, so build the driver only.

Also for the list: the R8 module is rated 65 °C ambient, or 85 °C with PSRAM ECC, which costs 1/16
of the PSRAM (WROOM-1 p.3). A calculator left in a car may want ECC. Measure what 1/16 costs the
two-calculator state in PSRAM before turning it on.

Host-testable now: the scan with key 42 from a separate input (the simulated matrix plus one GPIO),
and the sleep configuration's wake mask, a pure function of the pin table.

Power, from the 3.3 V regulator's margins (abacus with draftsman #3924/#3925; facts §6): the
RT9080 is adequate, not generous.
- Cap Wi-Fi TX power while on USB. Sustained full-power TX at OUT = V_BAT + 225 mV reaches the
  TSOT-23-5's 0.43 W rating.
- Keep the radio off below about V_BAT 3.5 V (from the gauge). TX peaks brown the module out below
  3.2-3.35 V.
- Bring-up check on Rev A: a sustained-TX soak with the LDO's case temperature measured.

Superseded 2026-10-04 (abacus decision 36): Rev A now has a TPS63802 buck-boost, which holds 3.3 V
across the whole cell. The TX cap on USB and the 3.5 V radio floor came from the RT9080's dropout
and heat. Drop the TX cap. Keep a radio floor only as a reserve for the save at power-off (the
gauge's empty threshold, or about 3.3 V).

Piezo volume (Michael asked, 2026-10-04): the driver takes a level as well as a frequency. With a
low-side switch, the square wave's fundamental scales as sin(π·duty): 50% duty is loudest, 10% is
about -10 dB, 5% about -16 dB. So a few steps (for example 50 / 20 / 8 / 3% and off) make a volume
setting with no board change. Driving away from the part's resonance (about 7.5 kHz for the
PKMCS0909) is a second, coarser control. Where the setting lives (SETUP, or a mode) waits for the
menu ruling, so build the levels only. This is unverified until a piezo is on a bench: measure the
steps with a phone's SPL meter when Rev A arrives.
