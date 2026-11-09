# Unit 092 built: the store's ring

**From:** the blueprint work/092-store-wear.md (revision 1, b59caec; abacus #14138's finding and rulings, #14140's
assumptions for store_sim), h092 frozen at abacus c645975 before any src (1,752 lines). Built after 094, as ordered
(#14144). The build:
- 2ae28ca: the ring, store_sim, the fixtures, store_test's additions.
- 34efbda: VER 0.95.
- 577ab71 and d5b0d72: the mutants' predictions, each before its run.
- 8e259ef: the remembered newest.
- 2ef668d and fc35ef5: the mutant runs and the board's evidence.

## What was built

- **One ring over the whole partition** (firmware/store.c). The partition is 2 × slot_size, as the devices already gave
  it: 128 sectors for the state, 18 for the apps.
  - A save starts at the sector after the newest good image's last, modulo the partition. It erases the sectors its
    header and image take (one range or two), writes the image (one piece or two), and writes the header last, as before.
  - An image is at most slot_size − 32 bytes, half the partition. That's the old limit, and it keeps the new image's
    sectors clear of the newest's wherever the ring stands (#14138's counterexample is a store_sim case, A).
  - The header's format is unchanged.
- **The load** reads the 32-byte header at every sector's start. It takes the highest (seq, sector) whose own CRC and
  payload CRC are right. An older firmware's two-slot images, and apps v1.1's old second slot inside v1.2's first, start
  at sector starts, so they load where they are. `ab_store_migrate` does nothing now; it's kept for its two callers.
  `ab_store_seq` (the newest's sequence number) is new, for store_sim.
- **The remembered newest** (the blueprint's lean 3; 8e259ef).
  - `ab_flash` gains `ring`, an `ab_store_ring`: the newest's sector, seq, length and CRC, set by a load or a save.
  - A save reads that one header back (32 bytes) before trusting it. A partition erased or written behind the store's
    back (the console's WIPE) then costs one scan, never a wrong place.
  - One ring per partition, shared by every `ab_flash` over it: SAVEBENCH's timing wrapper copies `*fl`, ring and all.
    A ring keyed by the flash pointer would have let the wrapper's saves get ahead of the real flash's next save.
  - NULL scans every time (the tests' flashes). The devices give each partition its own ring.
- **tests/store_sim** (the hidden set's interface; work/092-store-wear.md):
  - Commands: PART, OLD, SAVE, CUT, CUTALL, EXPECT LOAD, EXPECT ERASES, EXPECT MAXERASES, REBOOT. PART refuses an odd
    n, since the store's partition is two halves.
  - CUTALL replays the uncut save's operations a prefix at a time. The store reads only before its first erase, so a
    save cut after b operations leaves exactly the first b applied, and at an erase, that sector garbage. Without the
    replay, a 64-sector CUTALL would be about 262,000 full saves.
  - "This one if its header is whole" is judged by comparing the header's 32 bytes with the uncut save's. That matters
    because the header's padding is 0xFF, the erased value: the header is whole before its last byte is written.
  - store_sim drops the ring at PART, OLD, CUT and REBOOT.
- **tests/092-soroban-store.txt** (99 expectations, each worked out by hand from the rule before the first run):
  - A: the counterexample (newest at 61-76, then a 64-sector save at 77-127 and 0-12, cut at every moment).
  - B: the apps' version.
  - C: 16 CUTALLs from 1 byte to the cap, four times round the ring.
  - D and E: an older firmware's slots with the newest in each.
  - F: an empty partition, and the two-sector partition.
  - G: CUT.
- **store_test:**
  - **Fixtures:** two written by the two-slot store itself (b59caec's store.c, built by
    work/092-evidence/make-fixtures.c):
    - tests/store-fixture-twoslot.bin, the state's shape scaled: seq 3 in slot 0 newest, seq 2 in slot 1. It loads,
      and a ring save from it is cut at every moment: 6,035 of 6,035 leave the newest that finished.
    - tests/store-fixture-apps-v12.bin, the apps' 18 sectors with v1.1's 8 KiB slots, the newest in the old second
      slot. It loads, migrate returns 0, and the next save goes to sector 4.
  - **Skips and refusals:** a corrupted newest is skipped for the one before; an image one byte past half is refused,
    and exactly half is taken.
  - **Wear:** 10,000 saves each of 1,237 B, 9,138 B and 27,683 B on 128 sectors. Every sector is erased 78-79, 234-235
    and 546-547 times: 10,000 × k / 128, rounded down or up, the blueprint's table. The two-slot store put 5,000 on
    each slot's first sector.
  - **The remembered newest:** a save after a load reads 32 bytes, where a scan reads 5,333; WIPE behind its back; a
    shared ring.
  - Both store_sim and store_test are in make test, and store_sim runs under the sanitizers too.
- **VER 0.95** (abacus #14190).
  - Not 0.92 (backwards, after 094's 0.94), and not 0.94.1: talk.py's `expect` matches a substring, so `ver=0.94`
    would pass a 0.94.1 build.
  - 0.95 is 094 + 092, and the next unit takes 0.96. work/version-extra.txt records it.
  - check-version.sh adds that file's count to the unit number. Checked both ways: it refuses 0.94 and takes 0.95.

## What moved

- **store_test's migrate check.** Its first line asserted the loss ("without the migration: the older one"), which the
  ring removes. It now says the newest loads without a migration, and migrate returns 0.
- **Every `ab_flash` initializer** gained the ring field: NULL in the tests, a ring per partition on the devices.
- **Nothing else.** The vector totals are unchanged, store_sim's lines count expectations, not vectors, and no frame
  moved.

## Results

- **make test** at fc35ef5: rc 0; 215 lines, 21,485 vectors, 58,216 expectations (every `n/n vectors` line, repeats
  included). That's 094's 214 / 21,475 / 58,189 plus n094 (10, 27), adopted in this stretch.
  - store_sim's file is 99/99 expectations twice, plain and sanitized; those lines aren't `n/n vectors` lines and aren't
    in the totals.
  - store_test: every line above held.
  - The rest: app_test 893/893; screen_test 63/63; standin_replay 46 scripts; check-version 0.95.
  - Peak RSS 463 MiB, 2:56 (work/092-evidence/make-test.txt).
- **make accuracy:** rc 0, 3:43, peak RSS 1.23 GiB.
- **clang-check:** 74 files (store_sim.c the new one), 0 failing, the control refused.
- **Mutants** (predictions committed before each run; work/092-evidence/mutants-run1.txt to -run3.txt):
  - **Runs 1 and 2, on the scanning store:**
    - As predicted: Q0 (the control, green), Q1 (the start not moving), Q2 (the struck no-wrap rule: A, B, C and the 3-
      and 7-sector wear lines), Q4 (the header first: every CUTALL red, store_test green, since its cut test takes
      either image) and Q5 (the refusal removed: only the one-byte-past-half check).
    - Q3 (the load taking the first header found) also reddened store_test's migrate check and store_sim F's
      two-sector part. Both have an older image at the lowest sector. My misses.
    - Q6's plant (`if (1)`) didn't compile: `payload_good` was left unused under -Werror. My runner's fault. Fixed in
      run 2, and it reddens the corrupted-newest check alone, as predicted.
  - **Run 3, on the store with the remembered newest:** 7 of 9 exactly.
    - Q7 (the remembered header not read back) reddens the ring check alone.
    - Q8 (the ring not dropped while a save is in flight) survives, as predicted. After a failed save, the remembered
      header is still the newest's, so the next save starts where it would have anyway. The line is defensive.
    - Q1 and Q3 also reddened the ring check, which run 1 didn't have. I wrote "as run 1 found them" without thinking
      it through.
- **D68** (`scripts/resources.py static 8041123 fc35ef5`, the commit before the src; work/092-evidence/d68.txt):

  | | flash | DIRAM | IRAM | calc_task's frame |
  |---|---|---|---|---|
  | the stand-in | +320 | +48 | +0 | +0 |
  | Rev A | +320 | +48 | +0 | +0 |

  The DIRAM is state_ring and apps_ring (20 B each), and each `ab_flash` 4 B more for its ring pointer. Without the
  remembered newest (2ef668d), D68 was +96 and +112 flash, DIRAM +0.
- **SAVEBENCH** (work/092-evidence/board/, one session 03:19-03:41Z, alternating images; min / median / max ms over 20
  saves each).

  **The stand-in's image** (its own partition; this chip's slots there erase in 18-32 ms):

  | run | image | no page | the full page | the worst page | reads (no / full / worst, median) |
  |---|---|---|---|---|---|
  | SA | 8041123 (two slots) | 30 / 31 / 46 | 171 / 202 / 217 | 483 / 549 / 560 | 0 / 10 / 33 |
  | SB | 2ef668d (ring, scanning) | 41 / 49 / 59 | 172 / 181 / 229 | 461 / 562 / 590 | 13 / 18 / 28 |
  | SC | the ring, remembering | 36 / 40 / 49 | 180 / 200 / 211 | 495 / 525 / 553 | 0 / 0 / 0 |
  | SD | 8e259ef | 36 / 42 / 49 | 185 / 202 / 210 | 504 / 524 / 551 | 0 / 0 / 0 |
  | SA2 | 8041123 again | 36 / 42 / 50 | 197 / 229 / 238 | 546 / 590 / 609 | 0 / 10 / 33 |
  | SE | 8e259ef again | 36 / 40 / 57 | 179 / 194 / 210 | 488 / 517 / 555 | 0 / 0 / 0 |

  - SC's image was built from 8e259ef's code uncommitted (`core=2ef668d+dirty`); SD and SE are the commit.
  - On this partition the ring's saves are as long as the two slots', within the run-to-run spread: the same
    before-image gave a 31 ms median in SA and 42 ms in SA2. The reads are gone (0/10/33 to 0).
  - SB, without the remembered newest, read 13, 18 and 28 ms (medians) where the two slots read 0, 10 and 33: the
    scan's 128 header reads, about 12 ms. That's more than the old examine for the two smaller states and a little less
    for the worst. That's why 8e259ef exists.
  - SB's sectors were mostly blank, never written in this partition (14 of 20, 53 of 60, 45 of 140). SC, SD and SE's
    were not blank.

  **Rev A's main head-less** (Rev A's partition on this chip, whose slots' first sectors 091 found worn, 150-230 ms):

  | run | image | no page | the full page | the worst page | blank sectors |
  |---|---|---|---|---|---|
  | HA | 8041123 (two slots) | 22 / 178 / 218 | 407 / 527 / 590 | 737 / 828 / 891 | 2, 0, 1 |
  | HB | 8e259ef, first lap | 18 / 20 / 162 | 91 / 93 / 577 | 248 / 318 / 750 | 14, 53, 45 |
  | HB2 | 8e259ef, again | 31 / 39 / 184 | 120 / 123 / 438 | 300 / 319 / 675 | 0, 0, 0 |

  - **Medians:** HB2, past the first lap, is the steady state. The medians fall from 178, 527 and 828 ms to 39, 123
    and 319: the worn sectors take one save's erase in a lap, not every second save.
  - **Maxima:** they still reach them (184, 438, 675 ms) when the ring passes. The worst save's maximum is 675 ms
    against 891, so 090's 1 s rule now has a 33% margin on this chip, where it was 11%.
  - **First lap:** HB flatters the ring, because a blank sector erases fast.
- **The real path, both ways, on the stand-in:**
  - **Upgrade:** 8e259ef, flashed over the partition the two-slot store had just written (SA2), booted with
    `load: 1163 bytes ok` (savebench-SE.out). The older firmware's image was found by the scan.
  - **Downgrade:** SA2's 0.94 image, booting over the ring's partition, printed `load: no image: MEMORY CLEAR`. That's
    the downgrade cost the blueprint stated ("an older state, or MEMORY CLEAR"), seen on the board.
- **Claims:** lepton docker soroban-* 4 GiB and the stand-in board 03:19:15-03:41:41Z (clang-check, D68, nine
  SAVEBENCH runs); peak 596.4 MiB (docker stats every 2 s, soroban-* containers, 229 samples). Docker again for D68 and
  clang-check at fc35ef5, 03:45-03:47Z; peak 599.8 MiB (20 samples). The board is left on the stand-in's image at
  8e259ef.

## Misses, owned

- **The blueprint's "keeps today's migrate test unchanged and green"** (revision 1, Q3) couldn't hold. The test
  asserted the loss the ring removes.
- **The same test quietly stopped testing the v1.2 layout.** It writes its "old" images with today's `ab_store_save`,
  which is the ring now, so its second image lands at sector 1, not 0x2000. It passed without testing what it says. The
  real old layouts are now the fixtures, written by b59caec's store.c.
- **The blueprint's "the overlap check removed mutant must go red on the counterexample"** couldn't either. The
  counterexample's save is exactly half, which the refusal allows; the ring, not the refusal, keeps it off the newest.
  Q5 reddens the refusal's own check.
- **I dropped lean 3** (the remembered newest) to keep the first build simple, and wrote that a scan "still reads less".
  SAVEBENCH said otherwise on this flash: 12 ms of header reads per save, more than the old examine's for the two
  smaller states. 8e259ef builds it as the blueprint said, with the one change named above (a ring per partition, not
  per pointer).
- **The mutant misses:** Q3's two lines, Q6's plant, run 3's Q1 and Q3 lines (above).

## For the next unit

- abacus's h092 run (store_sim is the tool, at fc35ef5 or later).
- The erases a year, as the blueprint's table: the most-worn sector takes k/128 of the saves.
- Rev A's own flash is measured at bring-up (FLASH.md step 6, savebench090.txt).
- The next unit takes VER 0.96 (work/version-extra.txt).
