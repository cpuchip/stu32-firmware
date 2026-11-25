# Unit 103 built: the characters the face lacked, and the steps view's flag

**From:** the blueprint work/glyphs-steps.md at 1bb74de (abacus #17479, from tally #17478), with abacus's rulings in
#17497 and h103 frozen in #17504: sha256 90307544…d78a, 11 vectors on %BOXES and %STEPSON. Its keys were checked to
reach their screens at 1bb74de; its control is a fault abacus plants at src.
- **The build:**
  - bb72df9: the source and its tests.
  - ca79daf: the mutants' predictions and their runner, before any run.
  - This commit: the board script, the mutant run, D68, the drawn census, make test, this record.
- **VER 0.104.**

## What changed

- **firmware/font5x7.txt** (and its generated font5x7.c) gains three glyphs of our own face:
  - `;` U+003B, `:`'s upper dot over `,`'s tail: CAS reports' and steps pages' separator;
  - `⒤` U+24A4 and `⒥` U+24A5: the indirect glyphs in equations, one cell each, the letter between the parentheses'
    curves.
  - **⒤'s and ⒥'s look is Michael's,** on T120 (abacus #17497 ruling 1). If he picks three cells, that is a unit of
    its own.
- **firmware/screen.c's next_cell** draws the statistics' ŷ (U+0177) and ȳ (U+0233) as `y` with the face's combining
  circumflex and macron, as x̂ and x̄ are drawn: one cell, no glyph.
- **screen_page gains `steps_on`,** set with `view` when compose shows the steps view. SYS's view and SOLVE's report
  leave it false, so the phone knows when to draw rows 21-205 from the frame.

## Tests

- **tests/glyph_census.py (in make test):** the static census.
  - Every printable code point in core/, firmware/ and casim.c's string literals and the golden corpus's answer
    column must have a glyph, or a mapping found in next_cell's own body.
  - Three are named as never drawn, each with its reason: help's `{` and `}` placeholders, and tbook's `|` export.
  - **Today:** 134 code points, 0 without a glyph. Before the unit it failed on the five.
- **scripts/glyph_frames.py:** the drawn census (ruling 2), run at this unit and at later units that touch the face or
  the screen. It draws every key's frame over every plain vector run (build/vectors with AB_DRAWN_CENSUS).
  - **The control:** at e83e8d2's code, 25 frames drew 27 boxes, rc 1.
  - **Now:** 131 runs, 0 boxes (work/103-evidence/glyph-frames.txt).
- **tests/vectors.c:** `%BOXES=n` (the frame's hollow boxes after the keys) and `%STEPSON` (screen_page's steps_on).
  Both are under `#ifndef AB_DEVICE`, as %XDRAWN is.
- **tests/103-soroban.txt, 10 vectors, 13 expectations:**
  - **B01-B05:** `;` on two steps pages, ⒤ and ⒥ in STU and 33s equations, ŷ and ȳ. Each drew 1 or 2 boxes on
    e83e8d2's code.
  - **S01-S04:** steps_on 0 on SOLVE's report, the list, the home, and the steps view closed.
  - **K01:** SOLVE's report draws its roots as rows split at `;`, so no box either way.
  - It runs plain and in the sanitizer run, with no save/load pass (the report and the steps are RAM).
- **screen_test:**
  - frames 68 (`;` on X^3=2's PURE POWER page under CPLX), 69 (⒤ and ⒥) and 70 (ŷ and ȳ);
  - three steps_on checks;
  - its existing no-box check over every frame.
  - The other 67 frames wrote back byte for byte the same.
- **device/standin/glyph103.txt (new, in the replay):**
  - SOLVE's steps' last page, `ANSWER: SOLVED: X=-2; X=2`, and the equation `⒤` typed from the keys (EQN, RCL, the
    (J) key);
  - each frame's CRC the host's: 4AFAD607 and AC98D863;
  - the keys checked through the app on the host first.

## Results

- **make test** (this record's tree; work/103-evidence/make-test.txt): rc 0.
  - 22,383/22,383 vectors and 61,647/61,647 expectations, summing every `n/m vectors` and `n/m expectations` in the
    output. That's e83e8d2's 22,363 and 61,621 plus 103's 10 and 13, twice (plain and san).
  - glyph_census 0 without a glyph; screen_test 9,935/9,935, 70/70 frames; app_test 919/919.
  - The replay: 52 scripts (glyph103 the new one), every frame matching.
- **Mutants** (work/103-evidence/mutants-run1.txt; host, 2 min 28 s, largest process 114.7 MiB): 6 of 6 verdicts as
  predicted, and each red list the one predicted.
  - **M0** (the control): SURVIVED.
  - **M1** (`;`'s glyph removed): RED on 103 B01 and B02; screen_test's frame 68 and its no-box check; glyph_census
    U+003B; glyph_frames, 12 boxes in 7 frames.
  - **M2** (⒤'s glyph removed): RED on B03 and B04; frame 69 and the no-box check; glyph_census U+24A4; glyph_frames,
    17 boxes.
  - **M3** (the ŷ mapping removed): RED on B05; frame 70 and the no-box check; glyph_census U+0177; glyph_frames, 1
    box (B05's own frame: no other vector shows ŷ).
  - **M4** (steps_on never set): RED on B01 and B02 (%STEPSON) and screen_test's two steps_on checks. Nothing drawn
    moved.
  - **M5** (steps_on set with every view): RED on S01 (the report) and screen_test's report check.
  - The runner regenerated the face after each font fault and after restoring, and built the restored tree again.
- **D68** (`scripts/resources.py static e83e8d2 bb72df9`, work/103-evidence/d68.txt):
  - flash +80 (stand-in) and +64 (Rev A);
  - DIRAM, IRAM and calc_task's frame +0;
  - libmain.a +80 and +76.
  - The object report lists nothing outside the changed sources.
  - The containers peaked at 571.1 MiB.
  - One thing to tidy in my report: its list of the changed sources' names includes the evidence files' (PNGs,
    notes), since it takes every basename the diff names.
- **The board** (the stand-in, work/103-evidence/board/). Claimed as lepton-standin-board 23:46:28Z to 23:51:08Z, and
  released.
  - **The flash:** the app image was 2,427,008 B written and verified, D68's stand-in figure for bb72df9.
  - **The first run lost the console** when the port moved after the flash, the fourth time (087, 096, 100, 103).
    The script was rerun with FLASH=0.
  - **glyph103:** 5 PASS. Both frames are the host's (4AFAD607, AC98D863), and ver=0.104. The one NOT MET is the
    control.
  - The flash build's container peaked at 785.9 MiB.

## For abacus

- **h103:** to run at this record's head.
- **The drawn census:** `python3 scripts/glyph_frames.py`, with build/vectors built.
- **The vector file:** tests/103-soroban.txt.
