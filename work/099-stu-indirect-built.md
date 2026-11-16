# Unit 099 built: STU's own indirect model

**From:** the blueprint work/stu-indirect.md at 430eafd (decision 100's simpler STU model; decision 102's (b) for jumps),
accepted by abacus in #15027 and #15041, with h099 frozen in #15045 (36 vectors, sha256 55c11e3e…c03b; 8 of 36 pass at
430eafd, abacus's own count).
- **The build:**
  - 9f09f1f: the source and its tests.
  - 764276c: the mutants' predictions and their runner, before any run.
  - This commit: run 1, M1's rerun, D68, make test, C09 strengthened, this record.
- **VER 0.100** (basecamp's call, #14733).

## What changed

- **resolve** (core/calc.c): a branch for STU, ahead of the 33s's and the 35s's.
  - It's chosen by `ab_rec_model`: the running label's stamp, else the calculator's mode.
  - The index is i (var[26]). (i) names unnamed register n, where n is i's integer part.
  - **NO REGISTER**, a new status with message, covers:
    - n negative or past 800;
    - i complex or a vector;
    - (J);
    - a read past the defined range.
  - An expression or a name in i stays NOT A NUMBER.
- **ind_target:** STU's own branch for XEQ, GTO and FN= (decision 102).
  - i from 1 to 26 names label A to Z.
  - NO REGISTER for 0, 27 and up, negative, complex, or (J).
  - A label not in memory is NONEXISTENT downstream, as for XEQ C.
- **The keys** (firmware/app.c): the test for the 33s's mapping is now "the model isn't 35", so STU takes it. The (I)
  key gives i, and the (J) key gives (i).
  - In STU, SOLVE's and ∫'s prompts take no (i): the key does nothing and the prompt stays.
  - The blueprint said the prompt shows `STO (i)`. It doesn't: the key acts at once, as the 33s's does. The name shows
    in the listing and the worksheet row, and app_test checks it there.
- **The listing and the rows** (core/list.c): STU's index reads (i), as the 33s's. The 35s's still reads (I).
- **The image** (core/state.c, version 27):
  - STATUS_SINCE gives NO REGISTER version 27.
  - An image with anything stamped STU, or in STU, is written at 27 at least.
  - A load of an image under 27 restamps 35 each STU label holding an (I) or (J) line (a variable op, a jump, or an
    equation line with the glyph).
- **I and J** stay plain letters in every mode. Promise 1's vectors (B1–B6) pass before the unit and after it.

## Amended in place, each with a note naming 099

- **work/029-vectors.txt S01 and S03 (abacus's).** I told abacus there were two STU indirect vectors, both mine. There
  were four: my grep covered tests/ and not the work units' own vector files, which make test runs too.
  - S01 now expects NO REGISTER for i = 900.
  - S03 now uses i.
- **tests/029-soroban.txt Z05 and tests/096-soroban.txt C09:** they now use i.
  - C09 also checks register 30. It writes and reads through one index, so a wrong model agreed with itself: mutant M1
    left it green in run 1. It goes red under M1 now (mutants-run2-M1.txt) and on 430eafd.
- **tests/056-soroban.txt D56:** "one past the last status" is now 51, since 50 is NO REGISTER.
- **The version pins:** expr_test, rows_test and store_test pinned an STU image at 25; they now pin 27.
- **tests/golden-v27-4c.img became golden-v28-4c.img,** regenerated at 28 by scripts/mk_golden_newer.py, as each bump
  has done (089, 090, 096, 097). Its body CRC is now B87F02E2.
- **expr_test's (I) case** in classes() runs in 35s mode now. Under STU's model (i) can't reach a letter, so the case
  that reads B through (I) = −2 moved to the mode that still has it.

## Tests

- **tests/099-soroban.txt: 43 vectors, 49 expectations.** Each was written from the blueprint before a run.
  - It covers:
    - the registers;
    - NO REGISTER's cases;
    - the ops (STO+, RCL×, x↔, ISG, VIEW);
    - the 33s and 35s beside them;
    - the equations (the ⒤ and ⒥ glyphs);
    - the jumps J01–J10;
    - the restamp, S01 and S02;
    - promise 1 and promise 2's baseline (B1–B9).
  - It runs plain, with save and load at every key, and in the sanitizer run.
  - **At 430eafd** (a scratch worktree build): 16 of 43 pass, exactly the controls (B1–B9, M01–M04, J03, R16 and S01,
    which keeps the old behaviour). Every vector of the new model fails there.
    - Four had first passed there by coincidence (R01, R12, E01, J09: with I and i both 0, the two models agreed). Each
      now sets I apart.
- **app_test: +8 checks** (907 to 915):
  - STU's FN= (i) from the keys;
  - STO (i) from the keys;
  - SOLVE's prompt refusing (i), and the 35s's still taking (J);
  - the listing's `STO (i)` and `XEQ (i)`;
  - the row's `STO (i)`, and the 35s's `STO (I)`.
- **make test** (this record's tree): rc 0, 699 lines. 22,091/22,091 vectors and 61,133/61,133 expectations, summing
  every `n/m vectors` and `n/m expectations` in the output (work/099-evidence/make-test.txt).
  - screen_test: 9,529/9,529 checks, 67/67 frames.
  - The stand-in replay: 51 scripts, every frame matching.
  - The run started from deleted binaries, after the mutant runner's restore (it compares byte for byte).

## Results

- **Mutants, run 1** (work/099-evidence/mutants-run1.txt; host, 8 min 7 s, largest process 212.3 MiB by `time -v`).
  Every verdict went as predicted. Three predictions were inexact in what went red:
  - **M0** (the control): SURVIVED.
  - **M1** (STU's register branch removed): RED, on 099's E01, E02, R01–R15 and S02, 029 S01 and S03, Z05, and
    app_test's STO (i) key.
    - S02 went red too, which I hadn't predicted: under M1 its (I) reads I = 5.
    - C09 stayed green, though I'd predicted red. It is now strengthened, and run 2 has it red.
  - **M2** (a negative i as a letter): RED, R05 only, as predicted.
  - **M3** (top at 799): RED, R03 only. **M4** (top at 801): RED, R04 only.
  - **M5** (the restamp skipped): RED, S01 only.
  - **M6** (the version marker skipped): RED, as predicted: S02, expr_test's 6, rows_test's 4 and store_test's 1.
  - **M7** (the jump's STU branch removed): RED, but on J01–J10, not just J05–J08. I'd predicted a fall to the 33s's
    branch. It falls to the 35s's (`ab_mode35` is true in STU), which reads I.
  - **M8** (the SOLVE guard removed): RED, 1 app_test check, as predicted.
  - **M9** (the listing's STU name reverted): RED, 3 checks, as predicted.
  - **M10** (the key test left at "not 35s"): RED, 3 checks where I predicted 2. The third is SOLVE's prompt, whose
    guard sits inside the mapping M10 bypasses.
- **D68** (`scripts/resources.py static 430eafd 9f09f1f`, work/099-evidence/d68.txt):

  | | flash | DIRAM | IRAM | calc_task's frame |
  |---|---|---|---|---|
  | standin | +880 | +0 | +0 | +0 |
  | reva | +896 | +0 | +0 | +0 |

  - libmain.a grew by +850 and +761.
  - libesp_app_format.a grew by +12: the version string.
  - libbid.a's attributed share moved by +24 and +124. It's Intel's prebuilt archive, and its share moves with how
    the image is packed, as it did at 089, 090, 093, 095 and 096.
  - No internal-RAM symbol changed.
  - The containers peaked at 1.002 GiB (33 samples, 4 GiB cap, through soroban's shim).
- **The board:** not run. No frame changed (67/67, and the replay matched), and NO REGISTER is drawn by the message
  path screen_test already loops over for every status.

## For abacus

- **h099:** to run at this record's head.
- **The vector file:** tests/099-soroban.txt.

## After acceptance (abacus #15079, abacus 19c34b3)

Accepted at c06d954: h099 36/36 (46/46), its planted line (C04's @C=8) red alone, 099-soroban 43/43, make test's sums
the same as mine. abacus accepted the amendments to its 029 S01 and S03 as made. h099 is adopted as
tests/h099-abacus.txt, typed from #15082: sha256 55c11e3e2a852734c828cf32afd74f344e7cd849f8e6f45cf6bf75db40f3c03b,
4,372 bytes, 43 lines, ending in one newline. It runs in make test and the sanitizer run, 36/36 vectors and 46/46
expectations. The same flip fails it, 35/36, exit code 1 (read in its own command). make test: rc 0, 22,163/22,163
vectors and 61,225/61,225 expectations.
