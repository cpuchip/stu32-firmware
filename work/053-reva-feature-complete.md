# Work unit 053: Rev A feature-complete before the boards land

**From:** soroban (proposed #5437) · **Ruled by:** abacus #5438 (items 1–4; the placement options not in it) ·
**Status:** built; the head-less runs below.

## What Rev A already had

device/reva/main/reva_main.c, read in full before proposing:
- the LS027 (only changed rows sent; frame in PSRAM);
- the matrix scan with C and R/S interrupting a run, and light sleep between keys;
- the piezo (LEDC, VOL, the uses);
- the MAX17048 every 30 s;
- both stores, with the apps' saves and their migration;
- deep sleep that keeps the image (the ULP's EXTCOMIN, a 100-minute refresh wake);
- the RTC clock, SYS, UNDO, the graph and app buffers, app_seed;
- QIO (7dd07ce).

**Casimir and the games are in its CMake,** so 048's CAS2 page and Babal come from the shared code.

## What 053 adds

1. **Console parity** (device/dev_common.h: dev_dump):
   - **One X format on both boards:** the state line (X, the stop, the program, the app's shift and prompt, the message,
     the internal heap, the calc task's stack high-water mark), the clock and SYS line, the app running, the equation
     with 048's note and report, and SHOW. Each board adds its own line after it: the stand-in's pixels; Rev A's
     soc, vbat and ULP edges.
   - **SAVE, WIPE and RESTART on Rev A,** and the stand-in's save and load lines.
   - **Boot lines:** Rev A prints "reva: heap internal …" and "REVA READY".
   - **The stand-in's scripts now wait board-neutrally** ("READY", ": heap internal"), so they run unchanged on either
     main.
2. **The benches as dev_bench.h** (EQNBENCH, OPBENCH, PROGBENCH), compiled with AB_BENCH:
   - on for the stand-in;
   - off in Rev A's default image (decision 68);
   - on in its head-less build, or with -DAB_BENCH=1 for bring-up timing.
   Each is noinline, the lesson of 052's 432 bytes.
3. **REVA_HEADLESS** (device/reva/headless.sh): Rev A's own main on the stand-in's S3, with an 8 MB flash header (Rev A's
   partitions end at 0x7A2000) and the console on UART0.
   - **It drives none of Rev A's pins.** Several are the stand-in's RGB LCD lines, and a third-party board's wiring
     isn't worth reasoning about when it can be avoided.
   - **Inert:** the LCD sends (frames are still made and encoded), the ULP, the piezo (its tones are printed), the
     gauge's I2C, the matrix scan and its wake interrupts, light sleep (the console must stay awake), and deep sleep
     (OFF prints and returns).
   - Keys come from the console.
4. **What the head-less runs showed missing:** one gap. accept.txt waits for "run: started" and "run: stopped (STOP)",
   which the stand-in prints when a program starts and stops and Rev A didn't. Rev A now prints them. Nothing else
   failed.

## The head-less runs (Rev A's main on the stand-in's S3)

Rev A's boot, head-less: `SPI Mode : QIO`, "gauge: head-less", "load: 728 bytes ok", "reva: heap internal 201703
(largest block 106496)", "REVA READY".

| script | Rev A head-less | the stand-in |
|---|---|---|
| accept.txt (keys, a program in slices stopped by a key, WIPE, a state across RESTART, MEMORY CLEAR) | ALL MET (after item 4) | ALL MET |
| babal.txt (APPS, Babal running and leaving) | ALL MET | ALL MET |
| cas2.txt (048's SOLVE, ∫DX and FACTR through the keys, read on X's eqn: line) | ALL MET | ALL MET |
| speed.txt | ALL MET | ALL MET |

**Speed, Rev A head-less:**
- SOLVE of X^3-2×X-5: 33,717 µs (the stand-in 34,500).
- The integral: 52,499 µs (55,102).
- OPBENCH's cube at a 34-digit X: 537 µs.
- PROGBENCH SIN / LN: 266 / 378 ms (269 / 378).
Rev A's main is a little faster with no RGB panel to feed.

## What only the real board can test (for docs/hw/rev-a-bring-up.md)

Head-less runs prove everything above the pins. These need Rev A itself:

| area | what to check on the board | head-less covered |
|---|---|---|
| flash | the boot log's "SPI Mode : QIO"; a save and a setting kept across a pinhole reset; the 16 MB header (head-less used 8 MB) | the store's code, save/load and WIPE (accept.txt) |
| the LS027 | SPI mode 0, LSB first, 1 MHz; SCS setup 3 µs and hold 1 µs; the power-on order (BOOST_EN, clear, DISP, 30 µs); only changed rows reaching the glass; no flicker | the frame, its dirty rows and their encoding (ls027_test on the host) |
| EXTCOMIN | the ULP's 1 Hz COM inversion awake and in deep sleep (ulp-edges in X's reva line climbing); the image held in deep sleep; the 100-minute refresh wake rewriting it and sleeping again | none (ULP inert) |
| the matrix | open-drain columns, rows on their pull-ups; debounce; two or three keys at once (ghosting, the scan's order); a key held at power-up making no event; C and R/S interrupting a run | the keymap and the app's keys (console keys) |
| C on IO0 | waking deep sleep (EXT1, ANY_LOW, its external pull-up); IO0 as a strapping pin at reset | none |
| light sleep | the rows' and IO0's wake interrupts; the usb, tone and app locks (no light sleep on USB power, during a tone, while an app ticks); current between keys | none (no light sleep head-less) |
| the gauge | the MAX17048 on I2C 40/41 at 100 kHz: version, percent, voltage; CHG_N and PGOOD_N; BATT's latch through OFF | the gauge's code (power_test on the host) |
| the piezo | LEDC on IO46 (a strapping pin: must read 0 at reset); VOL's five levels; the uses' tones | the uses and their tones (printed) |
| power | EN1 and EN2; deep sleep's current with BOOST_EN and DISP held; the RTC clock across deep sleep and a cell change | the clock's code |
| PSRAM ECC | Rev A's config has it on; the speed benches with it (head-less ran with it too, on the stand-in's octal PSRAM) | covered (the same config) |
