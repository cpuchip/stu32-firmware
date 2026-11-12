# Unit 098 built: the levels' op names in screen_page

**From:** the blueprint work/level-op-export.md (3ecae74), asked by abacus in #14703 (the web page's STU frame, page-levels
B, primer #14699) and accepted as written in #14713, which numbered it 098 and froze h098 (abacus ca0fc5a, 15 vectors,
sha256 fb69a5ef...8633).
- **The build:**
  - 4e443e7: the source and its tests.
  - 4d10f40: the mutants' predictions and their runner, before any run.
  - This commit: run 1, D68, make test, this record.
- **VER 0.99.**

## What changed

- **One source.** `level_op` (firmware/screen.c) split in two.
  - `level_op_text` holds every condition for drawing a level's op:
    - the record is on the page and is no typed number or ALG line;
    - `ab_op_text` spells it;
    - the fit: the op's span plus 12 pixels must clear the value;
    - draw_page's own conditions: no op in ALG or above the levels drawn, and none on Y as a prompt. X's op only on its
      text line, as a value with its letter, not drawn in 2D, with its cells counting the ≈.
  - `level_op` draws what `level_op_text` returns.
  - `screen_lines` copies the same text into `screen_page.levelop[4][32]` (X, Y, Z, T, as `levelmark`; UTF-8; "" none).
- **No state version.** screen_page is not in the image.
- **Unchanged:** the frames, 67/67 against tests/frames. Also the replay of every stand-in script.
- **`screen_level_bottoms(c, bot[4])`.** It sits beside `screen_page_window`, so the tests can find a level's place.
- **The vector grammar (tests/vectors.c):** `%LEVELOP-k=TEXT`, with k one of X, Y, Z or T:
  - the op drawn beside that level, blanks as `_`;
  - `_` alone when the level is drawn with no op;
  - `?` when the level isn't drawn (off STU's home, in ALG, above `levels`, or k not a level).
- **The board:** not run. The device's frames are unchanged: screen_test's 67 frames match and so does the stand-in
  replay. The field is read only by hosts (the web page, dev_bench).

## Tests

- **tests/screen_test.c, `level_ops`: the pixels, judged apart from `level_op_text`.**
  - For each state, render with `screen_render`, read `screen_lines` and the level bottoms.
  - A level with an op:
    - the rectangle at x 22 (NUM_X + 2), 14 rows ending at its bottom, must equal `screen_text` of the told op alone on
      a blank frame, pixel for pixel;
    - it must have some ink;
    - the 12 columns after it must be blank.
  - A level with none, where the case sets its room bit: the first two cells there must be blank.
  - The 13 cases:
    - √x by X;
    - × by Y over a typed X;
    - a typed number ENTERed;
    - all four levels (T's copy carries +);
    - XEQ T;
    - ÷ with X's ≈;
    - Y a prompt;
    - X in 2D;
    - x² by a short Y and by a long one;
    - ALG, 33s and 35s.
  - One room bit was mine to correct: a typed X sits at the left, so no room is judged there.
- **tests/098-soroban.txt:** 16 vectors and 34 expectations in the grammar. The expected values follow the worksheet's
  rules (copies carry the row, typed numbers have no name), and I read each from the build before writing it down.
  abacus's h098 is the independent check.
- **make test** (this record's tree): rc 0, 671 lines; 21,932/21,932 vectors and 60,866/60,866 expectations (summing every
  `n/m vectors` and `n/m expectations` in the output; work/098-evidence/make-test.txt). screen_test 9,529/9,529 checks,
  67/67 frames.

## Results

- **Mutants, run 1** (work/098-evidence/mutants-run1.txt; host, 3 min 25 s, largest process 210.6 MiB by `time -v`).
  Every verdict was as predicted:
  - M0, the control: SURVIVED, as predicted.
  - M1, Y's op not told: RED, exactly as predicted (098 N03, O02, O03, O07, O08; 6 screen_test checks; frames green).
  - M2, the op drawn a row lower: RED. Frames 49-53, 57, 59, 60, 62, 63 and 67 changed, plus 10 level_ops pixel checks and
    14 replay frames; 098's vectors stayed green. I'd written "some of 50-67", and frame 49 is outside that: the
    prediction's range was one frame short. The verdict held.
  - M3, X's op told in 2D: RED, exactly as predicted (098 N02; 2 screen_test checks; frames green).
  - M4, the ≈'s cell left out of X's fit: SURVIVED, as predicted. **A named gap:** no state here sits on the fit's edge,
    and none was built to make it fail (abacus #14713).
  - M5, the prompt's test removed: RED, exactly as predicted (2 screen_test checks "Y a prompt"; no frame).
  - The runner restored screen.c to the original (compared byte for byte), and make test was run again from deleted
    binaries.
- **D68** (`scripts/resources.py static 8234192 4e443e7`, work/098-evidence/d68.txt):

  | | flash | DIRAM | IRAM | calc_task's frame |
  |---|---|---|---|---|
  | standin | +240 | +0 | +0 | +0 |
  | reva | +208 | +0 | +0 | +0 |

  libmain.a is the only library that changed. No internal-RAM symbol changed. The container peaked at 1,017 MiB (33
  samples, 4 GiB cap, through soroban's shim).

## For abacus

- **h098:** to run at this record's head.
- **Ahead:** check-version's rule gives "0.100" for the next unit. That's the last unit plus one, plus
  work/version-extra.txt's one, so stu-depth as 099 wants "0.100". Is that the form you want for VER past 0.99, or
  something else before 1.0?
- **Answered** (abacus #14734, basecamp's call in #14733; it is a version number, not one of Michael's decisions): (a).
  The next unit is 0.100, and check-version stays as it is (VER = 0.<last work unit> until 1.0). 1.0 stays Michael's.
