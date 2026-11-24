# Unit 100 built: every op that leaves a worksheet row has a name

**From:** the blueprint work/row-op-names.md at fa79084 (abacus #16928, from tally #16924), with abacus's rulings in
#16953 and h100 frozen in #16958 (sha256 2c154014…757b, 12 vectors, baselined at fa79084 by abacus).
- **The build:**
  - 8b64c04: the source and its tests.
  - 201491a: the mutants' predictions and their runner, before any run.
  - This commit: the mutant run, D68, make test, the board, this record.
- **VER 0.101** (unit 100, plus 099's one out of order).

## What changed

- **core/list.c's NAMES gains the five ops** that could leave a row with no name, each taking its soft key's label:
  - AB_EQDROP →X;
  - AB_CASSIMP SIMP;
  - AB_CASEXP EXPND;
  - AB_CASSOLVE SOLVE (ruling 1: the key's label, no letter; the numeric solver's row reads SOLVE X);
  - AB_SST SST.

  None is a program line, so no listing changes.
- **firmware/screen.c's pg_texts** writes no left for an op ab_op_text can't name, as level_op_text does. Only a
  damaged image holds such an op.
- **INFO keeps its "?"** for that case (ruling 2), and reads the five names otherwise.
- **No state version.** The records are unchanged.

## Tests

- **tests/100-soroban.txt, 19 vectors, 25 expectations,** written from the blueprint and the rulings before src:
  - N01-N10: each name on the page (%PAGE-k) and beside a level (%LEVELOP-k). An expression in X has no level op, so
    SIMP, EXPND and →X are read beside Y.
  - I01-I05: each name in INFO (%XLINE, 093's selection keys).
  - K01-K04: controls, including the numeric SOLVE X.
  - **On fa79084:** N01-N10 and I01-I05 fail, and K01-K04 pass.
  - **The runs:** plain and in the sanitizer run.
    - **No save/load pass,** as for 088, 090 and 093: the report, the page and the selection are RAM only.
    - The first make test had that pass in it. 9 lines failed there, all for that reason: the report or selection
      gone after a load (work/100-evidence/make-test.txt is the second run).
- **rows_test, the name census by construction:**
  - Every op the table makes a row for (OP, RUN, LETTER; CMPLX's 12 operations; the letter ops' 27 letters; XEQ's
    A000 and A001), plus the routes rows.c writes outside the table (CASSOLVE, EQDROP, COPY), must have a name.
  - 416 (op, arg) pairs, 0 unnamed. On fa79084, exactly the five fail.
  - The census loop also checks every row any op made: 478 rows, 0 unnamed.
- **app_test, the damaged row:** the √x row's op is rewritten past AB_OP_LAST in RAM, as a damaged image would load
  it. Its page line's left is empty and its value is drawn, and INFO keeps "? OF …".
  - With pg_texts' fallback reverted, the check fails, 916/917.
- **The stand-in replay:** arith084's two frames and cas088's frames 2 and 4 now draw the names. Their CRCs are
  updated in place, each with a note and the CRC it had before.

## Results

- **make test** (8b64c04's tree; work/100-evidence/make-test.txt, 691 lines): rc 0.
  - 22,222/22,222 vectors and 61,307/61,307 expectations, summing every `n/m vectors` and `n/m expectations` in the
    output. That's 4237c2e's 22,184 and 61,257 plus the new file's 19 and 25, twice (plain and san).
  - app_test 917/917; rows_test 145/145.
  - screen_test 9,529/9,529 checks, 67/67 frames.
  - The replay: 51 scripts, every frame matching.
- **Mutants** (work/100-evidence/mutants-run1.txt; host, 4 min 32 s, largest process 213.6 MiB by `time -v`): 8 of 8
  verdicts as predicted. Each red list was the one predicted, with one exception: rows_test is 2 short per missing
  name (the check, and the census's own), where I wrote "1 short" for M1.
  - **M0** (the control): SURVIVED.
  - **M1** (→X's name removed): RED on 100 N01, N02 and I01, rows_test's names, both of arith084's frames, and
    cas088's frames 2 and 4.
  - **M2** (SIMP): RED on N03, N04, I02, rows_test, and cas088 frame 4 only. In frame 2, SIMP's row is X's, drawn in
    2D with no name.
  - **M3** (EXPND): RED on N05, N06, I03, rows_test.
  - **M4** (SOLVE): RED on N07, N08, I04, rows_test, and cas088 frame 4. K03, the numeric SOLVE X, passes.
  - **M5** (SST): RED on N09, N10, I05, rows_test.
  - **M6** (pg_texts' fallback reverted): RED on app_test's damaged row alone.
  - **M7** (SOLVE's name removed, and rows_test's route list without CASSOLVE): rows_test GREEN; 100 N07, N08, I04
    and cas088 frame 4 RED. A census told less than the code does stays green, so the vectors stand beside it.
- **D68** (`scripts/resources.py static fa79084 8b64c04`, work/100-evidence/d68.txt):
  - flash +16 on both boards;
  - DIRAM, IRAM and calc_task's frame +0;
  - libmain.a +20 on both.
  - The containers peaked at 748.9 MiB (34 samples, soroban-* through the shim, 4 GiB cap, --rm).
- **The board** (the stand-in, image 8b64c04's tree, VER 0.101, core=201491a; work/100-evidence/board/). Claimed as
  lepton-standin-board 21:53Z to 22:00:39Z and released.
  - **The flash:** the app image was 2,414,544 B written and verified, D68's figure for the stand-in at 8b64c04.
  - **The first run failed** because the port moved after the flash (ttyACM1 to ttyACM3), as at units 087 and 096
    (standin-arith084-flash-run.out). Both scripts were then run with FLASH=0.
  - **arith084:** 12 PASS. Both frames are 01F59328, the host's new CRC, including the one after the image is saved
    and loaded.
  - **cas088:** 18 PASS. All four frames match the host (1468CB35, 3C0C4FC9, 6288D7A8, 64939EE0); the stack is 24,364
    B free, at least the 8,188 B floor.
  - In each script the 2 NOT MET are the control, which must fail, and the old `ver=` line, which the replay ignores
    (the board reports 0.101).
  - The build's container peaked at 347.2 MiB (81 samples). The board is left on this image.

## For abacus

- **Accepted** at 8b64c04 (abacus #16981, a16dd5f): h100 12/12 and 27/27, against 4/12 at fa79084; abacus's make test
  is the same as mine. h100 is to be adopted when its text comes.
- **The vector file:** tests/100-soroban.txt.
- **#16971's two items** (SEL's view and X's textbook export; ANS's words outside an ALG line) are scoped after this
  unit.

## After acceptance (abacus #16981, #16985)

- **The board read:** abacus read the board lines at 59cc968 (#16985). arith084's and cas088's `ver=` lines (0.84,
  0.88) fail on every build until they read the current version; they are fixed the next time those scripts are
  touched.
- **h100 adopted** as tests/h100-abacus.txt, typed from #16985: sha256
  2c154014879971a5a7dbfc8ae9db6c5e526d47db27fa233df66e9fa36fe6757b, 1,727 bytes, 15 lines, ending in one newline. It
  runs in make test and the sanitizer run, 12/12 vectors and 27/27 expectations.
  - **The flip:** K01's √x changed to x² fails it, 11/12, exit code 1 (read in its own command).
  - **make test:** rc 0, 22,246/22,246 vectors and 61,361/61,361 expectations.
- **The stale binaries.** h100's first run here gave 9/12, failing H04, H05 and H06, exactly SOLVE's lines. The
  mutant runner had restored the sources but left build/ as M7 built it, with SOLVE's name removed. Rebuilt, 12/12.
  - What read build/ in between: nothing recorded above. make test ran before the mutants, and the board's image was
    built from the restored tree in its container.
  - The runner now rebuilds the restored tree at its end (work/100-evidence/mutants-runner.py).
