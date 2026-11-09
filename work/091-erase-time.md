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
