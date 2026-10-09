# Work unit 052: the resource report as a tool, and Rev A's IRAM

**From:** soroban (proposed #5379) · **Ruled by:** abacus #5382, after Michael's decision 68 (#5376) ·
**Status:** built and measured.

## The tool: scripts/resources.py

Decision 68 asks every unit for its cost in flash, internal RAM, IRAM, the worst stack through the keys, and S3 time.
The tool gives the table in one run per unit.

- **`static BASE NEW [--targets standin,reva]`** (no board):
  - each commit built in a worktree under build/res/, in Espressif's pinned image, with the main tree's casim clone
    and staged games;
  - **flash:** the app's .bin;
  - **internal RAM (static):** DIRAM;
  - **IRAM code:** see below;
  - **libraries:** each one's change across all memory types;
  - **symbols:** the internal-RAM nm diff (0x3FC88000–0x3FD00000), grouped by name so a renumbered static ($19 to $20)
    is not a change.
  - A commit's plain build is cached, since it can't change.
- **`board BASE NEW --port PORT`** (the stand-in): each commit flashed in turn, then:
  - the internal heap free after boot;
  - the calc task's stack free at 047's deepest site, for the 153-unit mix and 19 levels of SIN(;
  - the speed benches (device/standin/speed.txt).
  The board is left on NEW. The key sequences come from tests/stackkeys.c (built as build/stackkeys), checked against
  the scratch generators 047 and 048 used: the same keys for every shape tried.
- **`iram-options [--commit C]`:** Rev A built once per ESP-IDF placement option that could move code out of IRAM,
  with the option's own help text.

## IRAM on the S3: the 16 KB block is not the limit

**idf_size reports Rev A's IRAM as 16,383 of 16,384 B.** That region is a fixed 16 KB block the linker fills first; the
rest of the IRAM code lies in the shared SRAM and is booked to DIRAM's .text.

**The control** (be3617e's Rev A plus one 1,163 B `IRAM_ATTR` function, called from app_main):
- it links, at 0x40378554;
- the IRAM region stays at 16,383;
- DIRAM's .text goes from 61,823 to 62,995 (+1,172, with alignment).

**So IRAM code costs internal RAM byte for byte.** The table reports "IRAM code" as the block plus DIRAM's .text:
78,206 B on Rev A at be3617e. abacus corrected decision 68's "1 B free" in place (3d0c4f4). arcade's WAMR needs 51 B
of IRAM, which is 51 B of internal RAM.

## The placement options, both boards (be3617e; a measurement for a proposal, nothing changed)

Each option set alone, then the three that combine. "Freed" is internal RAM (DIRAM), where IRAM code lives:

| option | set to | Rev A freed | stand-in freed | its constraint (ESP-IDF's help) |
|---|---|---|---|---|
| HEAP_PLACE_FUNCTION_INTO_FLASH | y | 12,228 | 6,548 | safe only if no esp_heap_caps.h / heap_trace function is called from an IRAM ISR that runs with the cache off. It turns SPI_MASTER_ISR_IN_IRAM off (which depends on it): on Rev A that option's 5,712 is inside this figure |
| SPI_FLASH_ROM_IMPL | y | 8,564 | 8,492 | the ROM's esp_flash; on the S3 it lacks SPI_FLASH_AUTO_SUSPEND (not used here) and "support for new flash chips" |
| FREERTOS_PLACE_FUNCTIONS_INTO_FLASH | y | 7,544 | 7,400 | the non-ISR FreeRTOS functions go to flash and can't be called with the cache off; the ISR ones stay in IRAM |
| SPI_MASTER_ISR_IN_IRAM | n | 5,712 | 0 | the SPI master's ISR (Rev A's LS027) from flash: a cache miss per transfer, and masked during flash writes |
| RINGBUF_PLACE_FUNCTIONS_INTO_FLASH | y | 0 | 1,036 | non-ISR ringbuf functions to flash, not callable with the cache off |
| LCD_RGB_ISR_IRAM_SAFE | n | n/a | 412 | **must not move:** the stand-in's RGB interrupt runs through flash writes (it is the cache-off path) |
| the other eight (GDMA, GPTIMER, SPI slave, bus lock) | | 0 | 0 | not linked |
| **heap + ROM flash + FreeRTOS, combined** | | **28,344** (DIRAM 203,455 → 175,111; IRAM code 78,206 → 50,546; flash −10,912) | **22,448** (DIRAM 219,343 → 196,895; IRAM code 66,422 → 44,562; flash −10,672) | all three constraints |

abacus #5438: no option changes now; they are ruled when a feature needs the internal RAM.

**What runs with the cache off on Rev A.** Its one ISR (key_isr) is registered without ESP_INTR_FLAG_IRAM
(`gpio_install_isr_service(0)`). So it is masked during flash writes, and it calls only xSemaphoreGiveFromISR, an ISR
function that stays in IRAM. Flash writes come from the calc task; IDF stalls the other core for them. Nothing found so
far calls heap or non-ISR FreeRTOS functions with the cache off. **The stand-in is the harder case:** its RGB LCD
interrupt is IRAM-safe and runs through flash writes (CONFIG_LCD_RGB_ISR_IRAM_SAFE).

## The board half: run time, stack, speed (resources.py board, the stand-in)

**051's pair (0594bae → 2d56c8e)** reproduces 051's measurements through the tool:

| resource | 0594bae | 2d56c8e | change |
|---|---|---|---|
| internal heap free after boot, B | 55,223 | 55,223 | 0 |
| calc task stack free, the 153-unit mix at 047's deepest site, B | 7,156 | 7,156 | 0 |
| calc task stack free, 19 levels of SIN( there, B | 7,892 | 7,892 | 0 |
| SOLVE, µs (best) | 56,902 | 34,568 | −22,334 |
| integral, µs (best) | 90,951 | 54,843 | −36,108 |
| PROGBENCH SIN / LN, ms | 357 / 546 | 267 / 377 | −90 / −169 |

**What the tool found: 432 bytes of stack lost to the benches.**
- **The symptom:** the deepest site's 7,156 B free was 432 B less than 049 measured (7,588 at 24308a2).
- **Bisected with the tool's stack-only mode:** 24308a2 gave 7,588, ed2518d 7,540 (−48), 0594bae 7,156 (−384).
- **The cause, read from each build's prologue** (xtensa's `entry a1, N`): calc_task's frame grew from 736 B at
  24308a2 to 1,168 at 0594bae. gcc had inlined the stand-in's benches (EQNBENCH's extension, OPBENCH) and its state
  dump into calc_task, and every deep path on the calc task sits on that frame.
- **Fixed in this unit:** dump and every bench are noinline (they run only on a console command). calc_task's frame is
  now 144 B.

MEASURED_BELOW


## The back-fill: units 048–051 (the static half)

| unit | commits | stand-in flash | Rev A flash | internal RAM | IRAM code |
|---|---|---|---|---|---|
| 048 CAS keys | 50be06f → ffa443f | +37,696 | +37,600 | +16 (comp) | 0 |
| 049 parser frames | 3d520c8 → 24308a2 | +864 (EQNBENCH in it) | +112 | 0 | 0 |
| 050 yˣ rounding | dbe1cdc → ed2518d | +48 | +48 | 0 | 0 |
| 051 QIO (stand-in, Rev A) | 0594bae → 2d56c8e, → 7dd07ce | 0 | 0 | 0 | 0 |

**048's flash is Casimir's three operations** (libmain +37 KB), inside its 128 KiB budget. Its 16 B of internal RAM is
the screen's composed lines' tail field. The run-time and stack halves of each unit are in their own reports (047–051,
measured through the keys).
