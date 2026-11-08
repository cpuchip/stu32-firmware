# Unit 089: the worksheet, part 1, the rows

**From:** decision 95 step 4 (Michael: "Do the worksheet improvement as you recommend").
- **Blueprint:** work/worksheet-1-rows.md (3999c83; a-e answered at b80103c; R1-R4 and CMPLX at bb306db).
- **Rulings:** abacus #13989 numbered it 089 and took leans 1, 2, 3, 5 and 6. #13991 gave R1-R4 and accepted a-e.
  #13996 accepted CMPLX's rule and froze h089 (abacus f3e8930, sha256 305ed4f0…10b2, 24 vectors) before this src.

**Status:** built. The src is at 7a30b0f, with its faults' predictions at 4d1d963. After the measurements, three
fixes followed: 270a1a4 (stale's run rule), 56947a9 (the full page's drop) and 0b60736 (the visible end). The final
tree is 0b60736. Results below.

## What was built

- **core/rows.c, rows.h:** the rows.
  - Each keyboard op in STU that makes a value appends a record: length first, serial, op, arg, flags, 0-3 links as
    serials, the typed text, and the value as the image writes one.
  - A table says what each op makes and reads. Its classes are op, run, move, clear and none.
  - `ab_val.row` sits in what was padding after kind. ab_val stays 64 B (a static assert compares it with the struct
    without the field), and every copy carries the row.
- **Hooks in calc.c:**
  - `ab_do_arg` is wrapped: `key_begin` before, `key_end` after. Every keyboard key goes through it, and the core
    never calls it from inside.
  - A route is set where the key dispatches: do_key, ALG, Equation mode, or SOLVE's report.
  - The four places an entry ends (do_key, a suffix, [, ENG) make the NUM row first (R3).
  - `alg_evaluate` makes the ALG row.
  - `ab_continue` makes a run's row when it stops.
- **Hook in eqn.c:** `take_val`'s three reads (ANS, a letter, i) record what an ALG line reads.
- **The rules:**
  - **R1-R4** as ruled.
  - **CMPLX:** per part, from its census.
  - **ALG (c):** reads in order, at most three, then the more-read flag.
  - **A run:** one row, linked to X's row at its start, and only when a line ran. R/S and SST with no program make
    none. SOLVE and ∫ always make one.
  - **SOLVE's report:** ENTER placing a root links the equation's row in Y.
  - **→X (EQDROP):** a row with no links.
  - **Clears:** no row.
  - **Moves (ENTER, R↓, R↑, x↔y, x<>A, STO, →EQN):** no row.
- **A stale row is dropped.** A level or letter whose value a key changed while its row stayed loses it: DSE's
  letter, and anything the table doesn't cover.
  - After a run or solver, a changed letter equal to X gets X's row, however it was written (SOLVE's root, G03).
  - At 7a30b0f it got X's row only when the letter's row was unchanged, and the solver writes it fresh. So the root's
    letter held 0. Found by writing G03 after M4 survived; fixed at 270a1a4.
- **The image (core/state.c):**
  - **T_ROWREF** holds the newest shown row and the 33 values' rows (X, Y, Z, T, LASTx, ANS, A-Z, i). It's written
    only when one is non-zero, and makes the image version 23.
  - **T_ROWS** is the page: the next serial, the dropped count, the records. Only `ab_state_save_rows` writes it,
    which is `dev_image_save` (SAVE and power-off). It keeps the newest rows within 8,192 B and every held row
    before them.
  - **UNDO's image** (`ab_state_save`) never holds the page.
  - **After any load,** a value's row that isn't on the page becomes 0.
- **The page full (56947a9, 0b60736):** the oldest rows go 8,192 B at a time (AB_ROWS_DROP), and the visible end is
  kept (rows_last), so a new row walks nothing unless UNDO hid some.
- **ab_calc's tail:** the page (64 KB), the next serial, the dropped count and the key's bookkeeping sit after the
  history. ab_calc is 120,784 -> 188,496 B on the host (+67,712), in PSRAM on both boards. An image's load keeps them, and MEMORY CLEAR empties them. The tail's static assert moved to the new last
  field.
- **The fixture:** tests/golden-v23-4c.img was remade as golden-v24-4c.img (body CRC E2E6A59C, 1,206 B). Red was
  seen first with the fixture left at 23 (work/089-evidence/red-4c-fixture-left-at-23.txt): expr_test's nine 4C
  checks and load_fuzz's seed 8.
- **The tests:**
  - tests/rows_test.c: the census against the table, the invariant, and the format.
  - tests/089-soroban.txt: 33 vectors through the %ROW dump, as proposed in #13990.
  - app_test: UNDO and REDO.
  - load_fuzz: a rows seed, and the page's round trip on every accepted image.
  - sanitize: rows_test and the 089 file added.
  - scripts/gen_opnames.py writes tests/opnames.h, the names for the dump, and its `--check` runs in make test.
- **The device:**
  - The console's X dump gains `rows: n top= next= dropped= x= y=`.
  - The stand-in gains ROWBENCH (below).
  - tests/stackkeys.c gains `rows`.
- **VER 0.89.**

## What moved, and why

- **The vectors, every one byte for byte:**
  - 202 lines, 21,127/21,127 vectors and 57,127/57,127 expectations at the 1.6.0 baseline (6e89b45).
  - 203 lines, 21,160 and 57,225 now: exactly 089's file (+1 line, +33 vectors, +98 expectations).
  - The method: every line of make test's output with `n/n vectors`, repeats included.
- **The frames:** screen_test 48/48 and standin_replay's 39 scripts, all as before.
- **One check moved, the planned version move (e):** expr_test's "after →X the image is version 21".
  - In STU, →X now makes rows, so that image is 23.
  - The check now says 23. It also checks that a fresh calculator gets no rows from that image (the rows aren't on its
    page), and that the same image loaded over its own page, UNDO's case, gives the rows back.
- **A count that moved and moved back:** app_test's "1464 keys after a change, 729 changed nothing".
  - It read 727 when R/S and SST with no program made a run row.
  - That rule was wrong (nothing ran), and it was narrowed to "a line ran". The count is 729 again.

## Results (the final tree, 0b60736)

**1. make test:** rc 0 (evidence: make-test.txt).
- **The vectors:** 204 lines, 21,197/21,197 vectors, 57,341/57,341 expectations. The method is every
  `n/n vectors` line, repeats included.
  - That is the baseline's 202 lines, 21,127 and 57,127.
  - Plus tests/089-soroban.txt twice, plain and under the sanitizers (35 vectors and 107 expectations each).
- **tests/089-soroban.txt:** 35/35 vectors, 107/107 expectations.
- **rows_test 136/136.**
  - Census and invariant: 100 (op, argument) pairs make rows, 444 rows' links match the table's, 0 failures of
    either. CMPLX counts once per operation.
  - 2,000 rows past 64 KB: 1,592 kept in 64,476 B, 408 dropped in 2 batches, the smallest 8,225 B.
- **The rest:** app_test 870/870 (5 new), expr_test 428/428 (1 new), screen_test 48/48 frames, standin_replay 40
  scripts every current frame matching. load_fuzz 20,000 and 200,000 (sanitized) cases with 0 violations, the rows
  seed and the page's round trip included. gen_opnames current. check-version 0.89.

**2. Falsified.** There were three runs of 11 planted faults, each run's predictions committed before it ran
(mutants-predicted*.txt, mutants-run*.txt).
- **Run 1:** 7 as predicted.
  - M1 and M7 didn't compile, so they tested nothing.
  - M3's rows_test crashed silently, and the runner read that as green. The runner was fixed, and so was the test's
    unguarded check.
  - **M4 survived:** no test reached stale(). Writing the tests that reach it found the root's-letter bug.
- **Run 2:** all 11 red, each as predicted, except that M3 also left S02 green (unpredicted).
- **Run 3, the final tree:** all 11 red, each as predicted (mutants-run3.txt).
  - M5 first didn't build: its line had become `cut_hidden`'s call, and the function was then unused under -Werror.
    It was rewritten to never cut and run alone: red, as predicted.
  - That non-compiling fault is the third of the unit (M1 and M7 in run 1). A fault that doesn't build is reported as
    such, never as a pass.
- **Also seen red:** the 4C fixture left at 23 (red-4c-fixture-left-at-23.txt), and the batch check with
  AB_ROWS_DROP 0 (red-drop-per-row.txt: 191 batches of 44 B).

**3. D68,** `scripts/resources.py static 6e89b45 0b60736` (evidence: d68.txt):

| | stand-in | Rev A head-less |
|---|---|---|
| flash (the app's .bin) | 2,310,304 -> 2,319,280 (+8,976, ROWBENCH with it) | 2,245,952 -> 2,251,680 (+5,728) |
| DIRAM, IRAM code, calc_task's frame | +0 | +0 |
| libmain.a | +8,369 | +5,770 |

The page is PSRAM (inside ab_calc). make clang-check: 73 files, 0 failing.

**4. The stand-in's ROWBENCH** (board/standin-rows089.out; a second calculator in PSRAM, STU's RPN):

| | no page | the worst page |
|---|---|---|
| rows, bytes | 0 | 1,269 in 62,128 B, 33 held |
| UNDO's per-key save + same | 650 + 605 us (733 B) | 2,305 + 2,194 us (6,595 B) |
| the same with no row held | | 2,246 + 2,150 us (6,453 B) |
| the device's save (serialised) | 691 us (751 B) | 13,595 us (27,683 B) |
| the device's save to the flash | (SAVE: 1,068 B, 48-72 ms) | 27,683 B, 426 ms |
| a key (1 +), 200 keys | 177 us | 170 us (204 rows dropped in them) |

- **UNDO's per-key walk:** 089 adds T_ROWREF, 142 B (6,595 - 6,453), which is 59 + 44 us in the worst state. The rest
  of the worst state's 6,595 B is its values: every letter, ANS and LASTx holding a 191-byte expression, as 082
  allows.
- **The worst save:** the page is 8,192 B of newest rows plus 33 held rows of the largest kind (29 × 411 + 4 × 239 =
  12,875 B), which with the state's 6,595 makes 27,683 B. It writes to the flash in 426 ms, against a typical save's
  48-72 ms.
- **A key with the page full, as first built: 5,551 us** (board/try2): every new row moved the 64 KB in PSRAM.
  - With the 8 KB drop it was 1,055 us (board/try3): each new row walked 1,300 records for the visible end.
  - With rows_last it is 170 us, the same as with no page.

**5. The boards** (rows089.txt; Rev A head-less is the same without ROWBENCH, work/089-evidence/rows089-reva.txt):
- **The stand-in, 21 PASS, the control failing:**
  - from MEMORY CLEAR, STU's 5 ENTER 3 + from the keys gives `rows: 3 top=3 next=4 dropped=0 x=3 y=0`;
  - after SAVE (1,068 B) and RESTART, the same rows come back from the flash;
  - ROWBENCH, then a RESTART, and the same three rows again.
  - Stack-free at least 28,260.
- **Rev A head-less:** 14 PASS, the control failing, the same rows through SAVE and RESTART. Stack-free at least
  28,324.
- **cas088.txt unedited on 089's image:** 18 PASS. The two moves are ver=0.88 (089's 0.89) and its own control.
  Every frame, soft key and report expect passes. Stack-free at least 25,268.
- **The first ROWBENCH timed the wrong thing** (board/try1): its keys acted on X holding a 191-byte expression, so
  they ended in RESULT TOO LONG. Fixed before any number was used: the levels hold numbers, and the worst state is
  measured before the keys.

**Claims** (lepton, old shim, `soroban-*` 4 GiB, and the stand-in board 1a86 56E1012198 for the boards):
- docker: 16:18-16:21Z and 16:44-16:46Z, D68 and clang-check, peaks 730 and 489 MiB.
- board and docker: 16:21-16:44Z, five board sessions, the peak 181 MiB.
- All released. The host runs (make test, the mutant runs) were not sampled; they build and run the host tests as
  every unit's make test does.

## For the next part

- **Part 2 (the page):** draws the rows, the labels (T's copy-down, lean 4), the dropped count at the top, and
  replaces ALG's 16-entry history with rows.
- **The %LBL dump:** `%LBL:@L` names a letter. `%LBL:X` is the level, so the letters X to T need the @.
