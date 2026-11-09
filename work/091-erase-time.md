# Unit 091: the erase time on Rev A's main

**From:** abacus #14107 (unit 090's lean 7 left a 4.7% margin and an erase 8 times the stand-in's on the same chip),
#14108 (the plan), #14109 (the number, the method: one difference at a time, predictions first, a control each session,
and the 27-line diff named item by item if nothing moves). Predictions: work/091-evidence/predictions.txt, committed at
846173b before any run, each later block appended before its run.

## The answer

**It isn't Rev A's firmware. It's two sectors of the stand-in's flash chip.** The first sector of each of Rev A's state
slots (0x710000 and 0x750000) takes 150-230 ms to erase under either image. Every other sector timed takes 18-32 ms,
the stand-in's own slots' first sectors included. Every save erases its slot's first sector whatever the image's size,
so every Rev A head-less save paid it. The Rev A board has its own flash chip, so this bench says nothing about it. The
bring-up now measures it (below).

## The runs (Rev A's main head-less on the stand-in's S3; SAVEBENCH's no-page erase, one sector, min / median / max ms)

| | what changed | result | predicted |
|---|---|---|---|
| C0 | nothing (control, 846173b) | 131 / 226 / 228 | 120-230: as predicted |
| C1 | CONFIG_PM_ENABLE and FREERTOS_USE_TICKLESS_IDLE compiled out | 117 / 227 / 229 | no move: as predicted |
| C2 | the ULP compiled out (config, its embed and its includes; c2-ulp-off.patch) | 113 / 194 / 228 | no move: as predicted |
| C3 | FreeRTOS's functions in flash, as the stand-in's | 114 / 220 / 229 | no move: as predicted |
| C4 | struck, not run: head-less lcd_init returns before SPI2 (reva_main.c:269-274) | | |
| D1 | a raw erase at 0x78F000, and the partition's own erase at its last sector, under Rev A's image | 20 / 22 / 22 and 18 / 20 / 22 | slow: **wrong** |
| D2 | raw erases under Rev A's image at 0x710000, 0x750000, 0x78F000, each with 256 B and with 4,096 B written | 0x710000 186 / 215 / 227 and 170 / 200 / 227; 0x750000 158 / 181 / 218 and 182 / 199 / 219; 0x78F000 20 / 22 / 22 and 20 / 20 / 20 | the address decides, not the fill: as predicted |
| D3 | the same under the stand-in's image, at six addresses | 0x710000 178 / 190 / 229; 0x750000 151 / 171 / 216; 0x716000 28 / 30 / 32; 0x720000 20 / 21 / 23; 0x74F000 18 / 21 / 31; 0x510000 21 / 23 / 23 | five of six: 0x716000 predicted slow, **wrong** |

- The earlier session (unit 090's record) had already falsified, one build each: power management held off at run time,
  the 100 Hz tick, PSRAM ECC off, a 16 KB instruction cache.
- **abacus's hypothesis** (#14107: other cores' wakes make the driver suspend, resume or yield the erase many times):
  falsified by D1. Under Rev A's own running image a raw erase is as fast as the stand-in's.
- **abacus's three candidates** (#14107: the scan task, the ULP, the gauge) aren't in the head-less image: the scan task
  isn't created (reva_main.c:1174), the ULP isn't loaded (1058-1063), the gauge isn't opened (1066), and PM isn't
  configured (1151). They can't have caused the head-less number. On the board they exist, and the board's own
  SAVEBENCH will include them.

## The 27-line diff, item by item (stand-in 846173b's sdkconfig against Rev A head-less's)

Ruled out by a run: FREERTOS_HZ (100 Hz build), FREERTOS_USE_TICKLESS_IDLE and PM_ENABLE with
PM_LIGHTSLEEP_RTC_OSC_CAL_INTERVAL, PM_SLP_DISABLE_GPIO and FREERTOS_IDLE_TIME_BEFORE_SLEEP (C1, which removes all five),
SPIRAM_ECC_ENABLE (090), FREERTOS_PLACE_FUNCTIONS_INTO_FLASH (C3), ULP_COPROC_ENABLED, ULP_COPROC_TYPE_RISCV,
ULP_COPROC_RESERVE_MEM, ULP_RISCV_I2C_RW_TIMEOUT and ULP_RISCV_UART_BAUDRATE (C2), and the instruction cache (090, 32 KB
→ 16 KB; its two lines aren't in this diff because the last build had it at 16).
Not run, and moot after D1-D3 (a raw erase under Rev A's own image is fast, so no build option can be what made the
store's erase slow): LCD_RGB_ISR_IRAM_SAFE, MBEDTLS_FS_IO, SEMIHOSTFS_MAX_MOUNT_POINTS, SUPPORT_TERMIOS,
SUPPRESS_SELECT_DEBUG_OUTPUT, VFS_INITIALIZE_DEV_NULL, VFS_MAX_COUNT, VFS_SEMIHOSTFS_MAX_MOUNT_POINTS, VFS_SUPPORT_DIR,
VFS_SUPPORT_IO, VFS_SUPPORT_SELECT, VFS_SUPPORT_TERMIOS, VFS_SUPPRESS_SELECT_DEBUG_OUTPUT (the stand-in's; the VFS ones
and the semihosting and termios ones are the console's file layer).

## What isn't known

- **Why those two sectors.** They are the sectors Rev A's head-less store erases on every save. But the stand-in's own
  slot 0's first sector (0x510000; slot 1's, 0x550000, wasn't timed) carries the same duty under the stand-in's image
  and erases in 23 ms, so "wear from the store's cycling" isn't shown. The cycle counts of any of these sectors aren't recorded. A
  wear test (cycle a fast, unused sector of the bench chip, such as 0x720000, some thousands of times, timing every
  hundredth erase) would test it in about ten minutes. It's offered, not run: it spends that sector's endurance on the
  bench chip.
- **Whether it matters for the product.** If cycling slows a sector's erase on this flash, the store's design (every
  save erases the same first sector of its slot) concentrates the wear on two sectors for the calculator's life. That's
  a question for the wear test, not a finding.

## Built

- **No fix to the firmware.** abacus's rule was to build one only once a candidate moved the erase near 21-23 ms. No
  firmware candidate did. The slowness belongs to the chip's sectors, not to anything the firmware does.
- **The bring-up measures the board's own flash:** device/reva/FLASH.md.in step 6 runs savebench090.txt on the bench
  image after speed.txt, and says which line matters (the worst page's max, with 090's 1,000 ms rule) and what this
  bench chip gives. package.sh ships the script. savebench090.txt is now a field session like speed.txt (no version,
  no control): the stand-in ran it to ALL MET at bc37c08 (restore-standin-savebench.out, worst 524 / 550 / 571 ms).
- Claims: lepton docker soroban-* 4 GiB and the stand-in board, 2026-11-09 00:20Z-00:42Z; peak 734 MiB (docker stats
  every 2 s, 252 samples). The board is left on the stand-in's image at bc37c08.

## The wear test (abacus #14121; predictions committed first at 3f8e7c4)

The stand-in's image with SAVEBENCH replaced by the test (work/091-evidence/wear.patch, not committed as code;
device/standin/wear091.txt; wear-standin.out). 0x720000 cycled 5,000 times (a 4,096 B write, then the timed erase);
every hundredth erase logged with its cycle count and its block of 100's min, median and max. 0x730000, the control,
erased 20 times at the start and 20 at the end, never otherwise.

**It's wear. My prediction ("not wear") was wrong.** Block medians of 0x720000's erase, in ms, by cycle count:

| 100 | 500 | 1,000 | 1,500 | 2,000 | 2,500 | 3,000 | 3,500 | 4,000 | 4,500 | 5,000 |
|---|---|---|---|---|---|---|---|---|---|---|
| 20.3 | 21.1 | 22.4 | 26.2 | 29.6 | 29.7 | 29.4 | 36.2 | 32.9 | 36.2 | 37.6 |

All 50 blocks are in the log. The rise is steady: about 3.5 ms per 1,000 cycles, 1.85 times the first block's median by
the last, past the predicted 1.5-times line. The control: 21.9 ms median at the start, 20.2 at the end (−8%), so the board
didn't drift; the test sector did. This makes cycling a sufficient explanation for 0x710000 and 0x750000's 150-230 ms,
though their cycle counts still aren't known, and neither is why the stand-in's own slot 0 (0x510000, 23 ms) is fast
after its own years of saves. Its cycle count isn't known either.

## (a) The chips

- **The stand-in's:** JEDEC id 0xC84017, 8,388,608 B (read on the board by esp_flash_read_id; boot's "detected chip: gd").
  C8 40 17 is GigaDevice's GD25Q64 family's id. The GD25Q64C datasheet's RDID table gives C8 40 17 (p.15). The id
  doesn't tell the C from the family's other revisions, so these figures are the C's: "Minimum 100,000 Program/Erase
  Cycles" and "20-year data retention typical" (p.4); the 4 KB Sector Erase Time tSE 50 ms typical, and maximum "200ms"
  at ≤50K cycles and "300ms" past 50K to 100K (p.56 and its note 1; pp.58 and 60, the other temperature grades: maximum
  400 and 500 ms). Read with pypdfium2 from the PDF at espruino.com/datasheets/GD25Q64C.pdf, the datasheet's own
  revision line not extracted. This chip's fresh 20-23 ms is under the 50 ms typical; 0x710000's 150-230 ms sits near
  the 200 ms maximum the datasheet allows up to 50K cycles.
- **Rev A's:** an ESP32-S3-WROOM-1-N16R8, 16 MB of Quad SPI flash (abacus-firmware docs/hw/2026-10-03-datasheet-facts.md
  §2, from the module's Table 1-1). The module's datasheet doesn't name the flash's maker, so it can only be read on the
  board. SAVEBENCH now prints the JEDEC id first (3c39e51; the stand-in printed 0xC84017 and ran to ALL MET), so
  FLASH.md step 6's save.log will carry Rev A's chip with its numbers.

## (b) The store's erases per year of each slot's first sector

From the code: every key the core handles marks the state dirty (app.c:252); a quiet save follows once 30 s pass with
no key and no run (APP_QUIET_MS, app.c:1439); OFF always saves before it sleeps (reva_main.c:983-984), and the
10-minute idle off takes the same path. Every save erases its slot's first sector (store.c: the sectors from the slot's
start that the image needs), and saves alternate between the two slots, so each slot's first sector is erased on every
second save.

Stated use: a student's hour a day, 365 days a year, one session (one OFF) a day. The quiet saves an hour are the
pauses of 30 s or more after keys:

| use | quiet saves an hour | saves a year (quiet + OFF) | erases a year of each first sector | to 50K | to 100K (the C's minimum) |
|---|---|---|---|---|---|
| a pause every 6 minutes | 10 | 4,015 | 2,008 | 24.9 y | 49.8 y |
| a pause every 2 minutes | 30 | 11,315 | 5,658 | 8.8 y | 17.7 y |
| the bound: a key, then 30 s, all hour | 116 | 42,705 | 21,352 | 2.3 y | 4.7 y |

So it is a product question. A calculator used in the upper case reaches the datasheet's minimum endurance on its two
slot-first sectors in under five years, and the slower erases sooner (the 200 ms maximum at 50K applies in 2.3 years).
The store puts all of that wear on two of the slot's 64 sectors. Spreading the image's start across the slot (or a
header that moves) would divide it. That's the design question for abacus and Michael, not built.

- Claims for the wear test and the restore: lepton docker soroban-* 4 GiB and the stand-in board 00:43Z-00:53Z; peak
  148 MiB (docker stats every 2 s, 95 samples). The board is left on the stand-in's image at 3c39e51.
