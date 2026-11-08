# Blueprint: the decimal library built in sections (the S3's unused tables and code left out)

**From:** soroban · **To:** abacus · **Asked:** abacus #13646 (Michael: "Go for all 4") · **Unit 087.** **Base:** 86b3fbf.

**Why.** On the S3, libbid is built without `-ffunction-sections -fdata-sections`. So a whole object is linked
whenever one function in it is called. Measured in a scratch copy of 5b9450a (#13630):
- bid_binarydecimal.o brings in 1,175,494 B of conversion tables that nothing calls;
- libbid's code is 182,366 B once the uncalled functions are dropped, against 363,918;
- Rev A's image is 2,241,792 B, against 3,603,712.

**The change** (one line, plus VER):
- `scripts/fetch-intel-dfp.sh`: the esp32s3 build's `CFLAGS_OPT` gains `-ffunction-sections -fdata-sections`.
  - The script's flag stamp already forces a clean library build when the flags change.
  - Every S3 path goes through it: device/reva/build.sh, headless.sh, device/standin/run.sh, prof.sh and
    resources.py.
  - ESP-IDF already links with `--gc-sections`.
- `firmware/version.h`: "0.87".
- **Not changed:**
  - The host library (`TARGET=""`) keeps its flags, so the tests run the same library code.
  - The esp32c6 recipe: no device builds it. The same line would apply if one ever does.
  - The core, Casimir and the games.

**The oracle: nothing a student sees changes.**
1. **`make test`:** byte for byte against 86b3fbf's output, with the host library unchanged. This is a check that
   nothing else moved, not evidence for the change: the host tests never link the S3 library.
2. **D68,** `scripts/resources.py static 86b3fbf NEW` (stand-in and Rev A):
   - flash falls about 1.36 MB;
   - DIRAM .data falls by libbid's dropped writable tables;
   - IRAM code and .bss stay +0;
   - every other library stays +0.
3. **On the board** (the stand-in, 1a86 56E1012198):
   - **Answers:** smoke.txt and accept.txt give ALL MET with the same answers at the base and the new build, and
     each script's own control row is NOT MET as it was written to be.
   - **Board run,** `resources.py board 86b3fbf NEW`: heap after boot, the stack through the keys, and the speed
     benches.
   - **Flash time:** esptool's own "Wrote … in N s" for the app, at the base and the new build.
   - **Rev A boot:** Rev A boots once, as its head-less image on the stand-in board, as D68 does. The two
     Espressif USB-JTAG ports on lepton aren't mine, and I won't touch them unless you name one as a Rev A board.
4. **`make clang-check`.**
5. **The flags took effect:**
   - **Positive control:** the new map has per-function sections from libbid (`.text.__bid128_add`).
   - **Negative control:** the base map has none.
   - Without this check, a stale libbid.a would pass every check above while measuring the old library.

**Predictions** (from the scratch build; written before the run):
- **Rev A image:** 2,241,792 B ± 16 (VER and the commit string are the same lengths).
- **libbid on Rev A:**
  - code 363,918 -> 182,366;
  - rodata 2,349,568 -> 1,174,074;
  - .data 37,176 -> 32,512 (−4,664).
- **The stand-in:** about the same cut. Not measured in scratch.
- **Heap after boot:** +4,664 ± 16 on both targets, since the dropped .data returns to the heap.
- **Stack:** +0 (the same code and frames).
- **Speed:** the code is laid out differently in flash, so cache behaviour may shift. I predict every bench stays
  within ±5% and every answer is identical. A bench outside ±5% gets reported, not explained away.
- **Flash time:** shorter. I won't guess how much; esptool compresses before writing, and the cut tables may
  compress well.
- **Host `make test`:** identical output.

**Risks.**
- **Code that moves:** the flags change where code lands, not what it does.
- **A table dropped that is still needed:** only an unreferenced section can be dropped, and anything reached
  through a reference is kept. The device answer scripts are the end-to-end check.
- **A stale library:** check 5 catches it.

## The build (0a7de57) and its results

**Approved** at 4efec9e (abacus #13654: "Rev A's head-less image on the stand-in board is what I meant. Diff the smoke
and accept transcripts byte for byte, base against new").
- **The change:** `scripts/fetch-intel-dfp.sh` (the esp32s3 `CFLAGS_OPT`, with a comment saying why) and VER 0.87.

**1. `make test`:** rc 0 at both commits, 502 lines each.
- **Not identical byte for byte as captured:**
  - the new run recompiles after version.h changed (40 cc lines);
  - VER's check line names 0.87 and unit 087;
  - timings differ (eqn_fuzz, golden-v1 load);
  - gen_games_test's GAMES_PINS_ID differs. That test makes throwaway git repos, so their pins change on every run.
- **With those four things masked, the outputs are identical** (`diff` empty). The masking is written in the scratch
  `norm()`: cc lines out, numbers before s/us/ms, the fixture digest, the VER line.

**2. D68,** `scripts/resources.py static 86b3fbf 0a7de57`:

| | stand-in | Rev A head-less |
|---|---|---|
| flash (the app's .bin) | 3,664,368 -> 2,306,160 (−1,358,208) | 3,603,712 -> 2,241,792 (−1,361,920) |
| DIRAM static | 211,847 -> 207,239 (−4,608) | 219,798 -> 215,190 (−4,608) |
| IRAM code | +0 (59,022) | +0 (78,086) |
| calc_task's frame | +0 (144) | +0 (128) |
| libbid.a, all memory types | 2,793,267 -> 1,435,826 | 2,750,662 -> 1,388,952 |

- The internal-RAM symbols that left are libbid's `__bid_*` writable tables: 164,823 -> 160,235 on the stand-in.
- **Other libraries moved,** where I predicted +0:
  - Rev A: libmain −615, libc +212, esp_hw_support +32, and three more at ±4;
  - the stand-in: libmain −555, esp_driver_i2c −128, esp_lcd −115, libc −80 and a few at ±20 or less.
- **They are all flash .text.** The object files are identical byte for byte at both commits (casim.c.obj and
  player.c.obj checked by sha256). Only their linked sizes changed: casim's .text −335 and .literal −96, libc's
  literal pools +16 to +60.
- **My reading:** the Xtensa linker relaxes calls and literals by distance, and the distances changed when libbid
  shrank. I haven't confirmed this with a `--no-relax` control. The board's answers are the check that nothing a
  student sees moved.

**3. `make clang-check`:** 71 files, 0 failing (the control refused).

**4. The flags took effect** (the maps):

| | per-function sections from libbid | `.text.__bid128_add` |
|---|---|---|
| 86b3fbf | 0 (its one sectioned entry is `.rodata.str1.4`, the compiler's merged strings, with or without the flags) | 0 |
| 0a7de57 | 643 | 1 |

- The library's flag stamp at each commit shows the flags (or their absence).

**5. On the board** (the stand-in, 1a86; transcripts in the scratch 087-board/{base,new}):
- **Flash time** (esptool's own line for the app at 0x10000):

| image | base | new |
|---|---|---|
| stand-in | 3,664,368 B (3,097,475 compressed) in 68.4 s | 2,306,160 B (1,897,659) in 42.0 s |
| Rev A head-less | 3,555,040 B (3,020,199) in 66.7 s | 2,193,008 B (1,818,748) in 40.2 s |

- **Answers:** accept.txt and smoke.txt are ALL MET with 0 FAIL on both targets at both commits.
  - The diffs below set aside ESP-IDF's boot log, which moves with the image's segment sizes.
  - **Every answer line is identical.**
  - **What differs, and why:**
    - VER, the core commit and the boot id;
    - heap: **+4,608 on both targets**. The stand-in goes 67,291 -> 71,899 (heap= 62,747 -> 67,355), Rev A
      185,415 -> 190,023. Its largest block goes 90,112 -> 94,208, and the stand-in's calc stack leaves 106,496 ->
      110,592.
    - the stored state each smoke run loaded, which is whatever the run before had saved;
    - the code addresses in `Saved PC`;
    - the save's milliseconds;
    - accept's sliced program, which counts for `sleep 3` and is then stopped by R/S. It counted further on the new
      image: the stand-in reached 79,850 -> 81,400 at SLICE and 82,780 -> 84,351 at STOP (+1.9%), Rev A 89,700 ->
      89,800 and 93,350 -> 93,500 (+0.2%). That is a timing figure, not an answer.
- **Stack-free:** identical at every reading on the stand-in. On Rev A, one reading differs: the first state after
  the second boot is 28,644 at the base and 28,788 at the new build. Every later reading is equal (28,324, then
  28,788). That 144 B is within the 384 B interrupt allowance the board gate uses (unit 069, 7e0951d). The deep
  readings are in the board run below.
- **A miss of my own, in the run itself:** the first stand-in passes gave device/standin/run.sh the by-id link, and
  it needs the resolved path, so neither flashed.
  - On the second try, the base flashed on ACM1, then the port re-enumerated to ACM3 and talk.py lost it. That
    attempt is kept in 087-board/base-try1.
  - The new pass then ran clean on ACM3, and the base pass was run once more, on ACM3. The Rev A head-less passes
    (headless.sh opens the by-id link itself) ran clean the first time.

**Predictions against results:**
- **Rev A image:** 2,241,792 ± 16 predicted, **2,241,792 measured**.
- **libbid on Rev A:** as predicted (1,388,952 for all types, from 182,366 code + 1,174,074 ro + 32,512 data).
- **Heap:** I predicted +4,664 ± 16 and measured **+4,608: a miss.** I took libbid's .data cut (37,176 −
  32,512) as the heap's change. The DIRAM total moved −4,608, and the heap follows the DIRAM total.
- **Other libraries +0:** a miss (see 2).
- **Stack +0:** holds. On the masked readings, which are the path (abacus #6675), every row is +0 on Rev A, and
  the stack's figure is 7,308 -> 7,308. The stand-in's rows are all +0. Rev A's unmasked rows move by -32 to +160,
  which is one interrupt frame with the timing, as the table's own note says.
- **Speed within ±5%:** **a miss, in the faster direction.** SOLVE is -10.0% and the integral -8.6% on the
  stand-in; Rev A's integral is -5.6%. The rest are inside ±5%. My reading, not tested: libbid's called functions
  now sit together in flash, so they miss the instruction cache less. The stand-in's 16 KB cache gains more than
  Rev A's 32 KB (unit 059). A cache-miss count would test it, and none was run.
- **Answers identical:** hold.

**6. The board run,** `scripts/resources.py board 86b3fbf 0a7de57`, 07:08-08:59Z:

| | stand-in | Rev A head-less |
|---|---|---|
| heap free after boot | 62,747 -> 67,355 (+4,608) | 185,415 -> 190,023 (+4,608) |
| largest internal block | +0 (31,744) | 90,112 -> 94,208 (+4,096) |
| stack: worst unmasked reading | +0 (7,668) | +0 (7,660) |
| stack: masked rows / the figure | (no masked image) | all +0 / 7,308 -> 7,308 |
| SOLVE, µs (best) | 31,676 -> 28,504 (-10.0%) | 17,738 -> 17,348 (-2.2%) |
| integral, µs (best) | 52,955 -> 48,399 (-8.6%) | 30,729 -> 29,003 (-5.6%) |
| PROGBENCH SIN, ms | 271 -> 270 | 202 -> 205 (+1.5%) |
| PROGBENCH LN, ms | 382 -> 377 | 259 -> 263 (+1.5%) |

- The interrupt's largest cost on a row went from 192 to 32. That is timing, not the change; the figure takes 192
  either way.

**Claims** (lepton, old shim, `soroban-*` 4 GiB):
- docker: 06:53:39-06:56:09Z for D68 and clang-check, peak 481 MiB.
- docker and the stand-in board: 06:57-08:59Z for the board passes and the board run, peak 717 MiB (docker
  stats, 1-2 s samples).
- Both released.
