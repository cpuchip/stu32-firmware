# Work unit 059: Rev A's instruction cache, 32 KB

**From:** soroban (proposed #5988, from a profile at 1556ade) · **Ruled:** abacus #5989 · **Status:** built and
measured (a156e49; tools 886f63d, 3e48a19).

## What the measurement showed

- **The profile pointed at the decimal library and was wrong about why.** At 1556ade, prof.sh ranked abn_mul first in
  SOLVE and in the integral. Read as time per call, though, the same multiply cost 75 µs in SOLVE and 186 µs after the
  integral's transcendentals, while OPBENCH times it alone at 5.0 µs (a short X) and 13.5 µs (34 digits). A cost that
  depends on what ran before is the cache, not the arithmetic. 050's lesson again: the profile points, a bench measures.
- **051 had already found the 16 KB instruction cache's misses** and rejected the 32 KB cache because the stand-in runs
  out of memory at boot with it. That rejection was measured on the stand-in only. Rev A has the room, and since 053
  its main runs head-less on the same S3. I should have measured it there when 053 landed.

## The change

**`CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB=y`** in device/reva/sdkconfig.defaults, with the measurement in its comment.
- **What it costs:** SRAM0's 16 KB block, where the linker put IRAM code first, becomes cache. That code moves into the
  shared SRAM, so static internal RAM grows by the block (+16,383 B) and the heap shrinks by it.
- **IRAM code is unchanged** (78,266 B in both builds): `.iram0.text` is 0x12db7 and `.iram0.vectors` 0x403 in both
  ELFs, only placed 16 KB higher. idf_size books the vectors to DIRAM as `.vectors` once the block is gone; resources.py
  didn't count them and read the same code 1,027 B smaller (3e48a19 counts them).
- **The stand-in stays at 16 KB.** Its keyboard image needs 128,768 B after the calc stack in one region, and the RGB
  interrupt reads it with the cache off, so it must stay internal (052; abacus #5989). With the 32 KB cache its largest
  block is 122,880 B and it stops at boot with "out of memory".

## Measured and rejected: a 64 KB data cache

On the stand-in, 1556ade against the 64 KB data cache with 32 B lines (226f5f2) and 64 B lines (a4795d5), throwaway:

| | 32 KB (1556ade) | 64 KB, 32 B lines | 64 KB, 64 B lines |
|---|---|---|---|
| SOLVE, µs | 29,295 | 29,259 | 29,539 |
| the integral, µs | 50,227 | 50,201 | 50,193 |
| internal heap free after boot, B | 62,827 | 30,831 | 30,831 |

Nothing gained, and 32 KB of internal heap lost. The data side isn't where the time goes.

## The checks

- **Rev A head-less at a156e49** (device/reva/headless.sh, 053's set): accept, babal, cas2, casdef, casrows and speed
  all met, each log naming abacus_reva.
- **The same answers:** SOLVE's root, the integral, and both PROGBENCH X values, with their statuses and evaluation
  counts, are digit-identical at 1556ade and a156e49.
- **all-checks passes** at a156e49 (make test, accuracy, clang-check, casim-config, casim-build).

**On the board** (resources.py board 1556ade a156e49 --targets reva,standin):

| Rev A head-less | 1556ade | a156e49 | change |
|---|---|---|---|
| internal heap free after boot, B | 201,879 | 185,495 | −16,384 |
| largest internal block after boot, B | 106,496 | 90,112 | −16,384 |
| stack free, 047's deepest site / 19 SIN( levels, B | 8,196 / 8,932 | 8,196 / 8,932 | +0 |
| SOLVE X^3-2×X-5, µs (best) | 29,595 | 17,412 | −41% |
| the integral, µs (best) | 49,403 | 29,341 | −41% |
| PROGBENCH SIN / LN, ms (best) | 266 / 380 | 198 / 255 | −26% / −33% |

EQNBENCH alone, Rev A head-less (prototype 6ca565c, the same configuration):

| | 16 KB | 32 KB |
|---|---|---|
| X^3-2×X-5 at X = 1.234 / 34 digits, µs | 358 / 1,048 | 127 / 264 |
| SIN(X)^2+X×EXP(−X) at X = 1.234 / 34 digits, µs | 5,395 / 6,356 | 4,727 / 5,685 |

The transcendental-heavy expression gains least (−11%). Why is not measured; a guess would be that its time sits in a
few long library loops that stay cached at 16 KB.

**The stand-in** is the negative control (its configuration didn't change): heap 62,827 both, stack +0, SOLVE 29,276 →
29,340 µs and the integral 50,165 → 50,234 (layout noise), PROGBENCH 269 / 380 both.

## Decision 68

resources.py static, Rev A, 1556ade → a156e49:

| | before | after | change |
|---|---|---|---|
| flash (the app's .bin) | 3,970,944 | 3,970,912 | −32 |
| internal RAM static (DIRAM, of 341,760) | 203,515 | 219,898 | +16,383 |
| IRAM code (the 16 KB block + DIRAM's .text and .vectors) | 78,266 | 78,266 | +0 |
| calc_task's frame, B | 128 | 128 | +0 |

The stand-in's image is unchanged by this unit. Nothing new goes into IRAM.

**The new ceiling:** Rev A's largest internal block after boot is 90,112 B. The calc stack (32 KB) is the largest
internal allocation today; the LS027 frame, Casimir's arena and WAMR's structures are in PSRAM, and a game costs 152 B
internal. Wi-Fi's internal buffers, if Rev A ever brings Wi-Fi up, would have to be measured against it (abacus #5989).

## Tools along the way

- **resources.py** (886f63d, 3e48a19):
  - a build with no 16 KB IRAM block no longer crashes static (KeyError), and IRAM code counts DIRAM's `.vectors`;
  - `board --targets standin,reva` runs Rev A's main head-less through the same keys and benches, with the largest
    internal block beside the heap. From here a unit's speed is reported on Rev A, the stand-in beside it.
  - **Every log must name its target's project.** While prototyping, a head-less build failed, and my throwaway
    script's fallback talked to the stand-in image left on the board, giving two identical "results". Reading the logs'
    project names caught it. The guard was falsified: a Rev A talk to the stand-in image stops the run.
- **prof.sh** flashes and talks in two containers and resolves the by-id path again between, since the CH340 can come
  back under another ttyACM number (its talk failed EIO at 1556ade).

## For Rev A's bring-up (abacus adds it to step 8)

Light sleep, the ULP's wake and the 100-minute refresh with the 32 KB cache: head-less drives no Rev A pins.
