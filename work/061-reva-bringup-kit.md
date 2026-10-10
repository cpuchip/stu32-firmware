# Work unit 061: Rev A's bring-up kit

**From:** soroban (abacus asked, #6090; proposed #6103) · **Ruled:** abacus #6106 (all four parts), #6112 (fermion,
Windows, is the host) · **Status:** built and measured (c469247, c409b9f; the bench image and FLASH.md's timing step in the commit
that adds this file).

## What a pre-board run found

Rev A's console was UART0 primary with USB-Serial-JTAG secondary (IDF's default, never set in sdkconfig.defaults).
ESP-IDF v5.4.2's secondary console is output only: `components/esp_vfs_console/vfs_console.c`'s console_read reads
fd_primary alone, and the Kconfig help says "If you want ... input through this secondary port, please change the
primary config to this port". Rev A's UART0 is only the TX/RX test pads, and its USB-C reaches the S3's own USB.

Measured on the stand-in's S3 at f9401bd, Rev A head-less, both ports open through a reset:

| | the native USB port (Rev A's USB-C) | UART0 (the CH340; Rev A's pads) |
|---|---|---|
| ROM and bootloader log, "SPI Mode : QIO" | shown | shown |
| "Project name: abacus_reva", the heap line, REVA READY | shown | shown |
| X sent on that port | no reply in 3 s | a reply in 40 ms |

So on Tuesday Michael would have seen the boot log and been unable to send X, BEEP, VOL, TIME or SAVE without a
3.3 V USB-UART adapter on the pads.

**Also seen:**
- Opening the USB port on Linux resets the chip.
- A reset over USB (rst 0x15) and RESTART (rst 0xc) keep the USB link, and the boot log comes through whole.
- EN and deep sleep drop the link: the port re-enumerates, and lepton gave it another ttyACM number.

## What 061 adds

1. **The console on USB-C:** `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` in device/reva/sdkconfig.defaults.
   - With no host reading, a write waits at most 50 ms once and then drops (usb_serial_jtag_vfs.c,
     usb_serial_jtag_tx_char_no_driver). That is the path Rev A already took as the secondary, so the battery case
     is unchanged.
   - The app's output no longer reaches UART0's pads (the ROM still prints there).
   - Head-less keeps UART0 (headless.defaults), so 053's scripts run over the CH340 unchanged.
   - `USB=1 device/reva/headless.sh <by-id>` runs Rev A's own console head-less on the stand-in's native port
     (headless-usb.defaults). It shares /dev with the container, so a renumbered port is found again.
2. **The console, for bring-up** (reva_main.c):
   - **KEYS** toggles an echo of each key: `key: 15 r2 c3 SIN down`, with the face legend from km_lookup, SOFT1-6
     for the soft row, GOLD and BLUE for the shifts.
   - **X's Rev A line** adds chg_n and pgood_n.
   - **A new line, `reva: set radix=. off=10 vol=4 h24=0`,** makes step 8's "a setting kept across a pinhole reset"
     something the log can judge: X's state line always prints with a point.
   - **The gauge's version** is printed at boot ("gauge: version 0x…"); before, only "gauge: not found" was.
3. **talk.py** (device/standin/talk.py):
   - new commands: `say`, `confirm` (y or n on stdin, n a FAIL), `expect-range`, `expect-grows` (the ULP's edges
     climbing), `expect-none` (a bounce, a ghost);
   - the port opened again when it goes away, logged as "talk.py: port open again" so a script can wait for it;
   - sends time out after 2 s instead of blocking (c409b9f, below);
   - runs on Windows: no realpath on a COM name, and a UTF-8 log, since the legends hold ←, ↔ and ÷.
4. **device/reva/bringup.txt,** made by gen_bringup.py from firmware/keymap.c, the table the echo prints from.
   `make test` checks it is current.
   - It follows step 8 (abacus #6106) row by row: an expect where the log can judge, a confirm with what to look
     for where it can't (flicker, the tones, the currents, the image held).
   - It includes a debounce check and a ghosting square: SIN, COS and E held, so a ghost would be ←.
5. **device/reva/package.sh <commit>:** the image from a worktree at that commit, checked to be USB-console and QIO.
   It writes a merged image for one write at 0x0, the build's three parts and flash_args, talk.py, bringup.txt,
   the sdkconfig, FLASH.md (Windows first) and SHA256SUMS.
   - flash_args says `--flash_mode dio`: ESP-IDF writes a DIO header and its bootloader switches to QIO, so the
     boot log's QIO line is the check (abacus #6106).
   - **A bench image too** (abacus_reva-bench-<sha>.bin, with speed.txt). Step 8's PSRAM ECC and 32 KB cache rows ask
     for the speed benches on the board, and the default image has none (053, decision 68). FLASH.md's step 6
     flashes it for timing only, runs speed.txt, then flashes the default image again.
   - `AB_BENCH=1 device/reva/build.sh` builds it, in build-bench. 053's CMake already took AB_BENCH; build.sh
     didn't pass it.

## The checks

- **Rev A's console over USB, head-less** (device/reva/usb-headless.txt, `USB=1 headless.sh`), ALL MET:
  - the boot facts;
  - X's charger fields;
  - KEYS's echo for SIN down and up, SOFT1, GOLD, ← and x↔y, and silence once it is off;
  - the stack (X=1.5) and RADIX's comma kept across a reset over the port;
  - RESTART with the link kept.
- **Controls, each red as it should be:**
  - **The UART-console image** over the CH340: `expect-grows ulp-edges` FAILs on head-less's frozen count, and
    `expect-range pgood_n 1 1` FAILs on 0.
  - **The same image over USB:** the boot log arrives, and the first X gets nothing ("got None"). This is the
    finding, as a script.
  - **A typed 2 after the reset** makes X=2, so bringup.txt's "a key held at power-up makes no event" check
    (X=1.5 after the release) can fail.
  - **KEYS's first build** read "KEYS" as K<n> (key 0) and echoed nothing: the script's nine KEYS lines failed.
    KEYS is now matched before K<n>.
  - **gen_bringup.py --check:** red on a one-word edit of bringup.txt, green when restored.
- **The hang it found:** the USB control first hung at its second X. The UART-console image never reads USB input,
  the S3's receive FIFO filled, and pyserial's write blocked for good. talk.py's sends now time out after 2 s
  (c409b9f).
- **FLASH.md's flash command, on the S3:** Rev A's head-less USB build, merged into one image as package.sh merges,
  written at 0x0 over USB-Serial-JTAG with the documented command (esptool 4.9.0, `--before default_reset --after
  hard_reset write_flash 0x0`).
  - esptool: "Chip is ESP32-S3", "Hash of data verified", "Hard resetting via RTS pin...".
  - Then usb-headless.txt, without a flash: ALL MET.
  - The write ran from 0x0 to the app's end, so it blanks the NVS and PHY areas, which Rev A doesn't use (no
    `nvs_` call); abstate and abapps are past it. FLASH.md says so.
- **The package from c409b9f:** the merged image is byte-identical to the bootloader, the partition table and the app
  at their offsets, and FLASH.md's Python check prints OK for every line of SHA256SUMS.
- **A second accidental control:** a speed run over USB failed with every send timing out, because resources.py
  board had just reflashed the board with UART-console images. After reflashing the USB build, it was ALL MET.
- **053's head-less set over the CH340** at c409b9f (UART0's console, unchanged): accept, babal, cas2, casdef,
  casrows and speed all met, each log naming abacus_reva.
- **all-checks passes** at c409b9f (make test with the bringup check, accuracy, clang-check, casim-config,
  casim-build).

## Decision 68

resources.py static, f9401bd → c469247:

| | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | +0 | +0 (3,970,912 both) |
| internal RAM static (DIRAM) | +0 | −180 |
| IRAM code | +0 | −180 |
| calc_task's frame | +0 | +0 |

- **The stand-in's image is unchanged.**
- **On Rev A,** libmain grows 358 B (the echo, the set line, the gauge's version), in flash. libesp_pm (−256) and
  libesp_system (−165) shrink, because the UART console's IRAM paths are gone. In internal RAM, key_echo is +1 and
  s_panic_uart −4.
- **Nothing new in IRAM.**

**At run time, Rev A head-less** (resources.py board f9401bd c409b9f --targets reva, over the CH340, so UART0's
console):

| | f9401bd | c409b9f | change |
|---|---|---|---|
| internal heap free after boot, B | 185,495 | 185,495 | +0 |
| largest internal block after boot, B | 90,112 | 90,112 | +0 |
| stack free, 047's deepest site / 19 SIN( levels / the worst through the keys, B | 8,196 / 8,932 / 8,196 | 8,196 / 8,932 / 8,196 | +0 |
| SOLVE / the integral, µs (best) | 17,406 / 29,341 | 16,980 / 29,288 | −426 / −53 |
| PROGBENCH SIN / LN, ms (best) | 197 / 255 | 197 / 259 | +0 / +4 |

The speed moves are within the noise 059 saw for an unchanged image. **With Rev A's own USB console**, head-less at
c469247's firmware:
- the heap after boot is 185,759 B (+264 against UART0's console), and the largest block is still 90,112;
- speed.txt over USB gives SOLVE 17,196 µs, the integral 28,630, PROGBENCH 199 / 253 ms: the same as over UART0.
