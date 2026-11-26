# Unit 105 built: the indirect tokens' own parentheses

**From:** the blueprint work/token-parentheses.md at 7ffc6a5 (abacus #17893, Michael's T136 A74 "Token's own
parentheses"), with abacus's rulings in #17900 and h105 frozen there: sha256 f25e5cd5…0149, 25 vectors (abacus
d7b0abc), calibrated by abacus at 16/25 on today's look, 25/25 on the prototype, 24/25 without its program-line path.
- **The build:**
  - 10fbb7d: the source and its tests;
  - 4cc6180: the mutants' predictions and their runner, before any run;
  - this commit: the mutant runs, D68, the drawn census, clang-check, the board, the board scripts' version lines,
    this record.
- **VER 0.106.**

## What changed

- **The face gains the token's own pair,** U+27EE ⟮ and U+27EF ⟯ (firmware/font5x7.txt, as the blueprint's contract 1
  draws them). The face has 132 glyphs.
- **firmware/screen.c:**
  - **cells_of** draws ⒤ and ⒥ as ⟮, the letter, ⟯ (R1). Every decode site goes through it, as in 104.
  - **line_piece** writes the pair's cells as `(` and `)`, so every exported text is 104's (R2).
  - **A program equation line is drawn from its equation** (R5): the address, a space, eqn_line in the line's case,
    then ab_eqn_mark. The case comes from the new core function ab_prog_m35. An index out of range keeps the listing's
    text.
  - **fit_watch is gone** (abacus #17631): draw_line_lead compares with x_line(), compose's X.
- **core/list.c, core/calc.h:** `bool ab_prog_m35(const ab_calc *c, int i)`. It's list.c's label rule, factored out,
  and ab_prog_line calls it. The listing's text is unchanged.
- **The operand forms keep the plain pair** (R3). Nothing in their path changed.
- **tests/vectors.c:** `%LOOK=ID` and `%LOOK=!ID` (R4), as the blueprint's oracle, under `#ifndef AB_DEVICE`.
  AB_LOOKPX=1 prints each comparison's pixel count to stderr.
- **tests/glyph_census.py** finds the pair in cells_of's body as 0x27EE and 0x27EF.

## Tests

- **tests/105-soroban.txt, 25 vectors, 61 expectations,** from the blueprint's look.txt and more.txt:
  - **T01-T11:** the tokens, each with %BOXES=0, and %XDRAW showing the pair.
  - **U01-U05, U07-U09, U11:** typed parentheses, `%LOOK=!T…`, each with the same text as its T line.
  - **W01:** the window's %XDRAW.
  - **S01:** STO (I) plain.
  - **K01-K03:** the controls.
  - In make test and the sanitizer run.
  - **On 8c4fad7 with only the directive:** 5/25, the T lines' %XDRAW and every U line red.
- **tests/104-soroban.txt:** E01, E02 and W01-W03's %XDRAW read ⟮I⟯, with a header note. The blueprint named these
  five moves.
- **screen_test:**
  - **the face check:** the pair shares no dot with ( ), and is lit against the letter where ( ) aren't;
  - **the program line's text** is the address, a space and ab_prog_line's text, under a 35s and a 33s label (the
    33s's `(i)+(J)`);
  - **frames 69, 71, 72 and 73 re-pinned,** each looked at before it was kept.
- **The board scripts:** glyph103's ⒤ frame re-pinned to 0520A61E and glyph104's to 26FD27DB, the host replay's.
  - Their `expect ver=` lines are now 0.106. **That's a miss I found on the board, not before it:** the first flash run
    failed glyph104's line 14 on `ver=0.105`. VER's bump left the scripts' line behind, as at 104.

## Results

**1. make test** at 10fbb7d (make-test.txt): rc 0.
- 22,551/22,551 vectors and 61,940/61,940 expectations. That's 8c4fad7's 22,501 and 61,818, plus 105's 25 and 61 in
  both runs.
- screen_test 9,943/9,943 checks, 73/73 frames. That's 9,940 for the face at 132 glyphs, plus the 3 new checks.
- h104 11/11, check-version 0.106, the replay 53 scripts.

**2. make clang-check** (clang-check.txt): rc 0, 76 files, 0 failing, the control refused.

**3. The mutants** (mutants-predicted.txt, committed in 4cc6180 before any run; mutants-run1.txt, mutants-run2.txt):
**8 of 8 verdicts as predicted.**
- **M0, M1, M2, M3, M6 and M7:** every red list exactly as predicted.
- **M3** shows what the pixel oracle is for: with the pair drawn as ( ), every U line went red while every %XDRAW
  line stayed green.
- **M4** (⟯ removed from font5x7.c): red as predicted on every vector line, the face check, the no-box check, the
  four frames, the replay, gen_font --check and glyph_frames.
  - **One predicted red stayed green:** glyph_census. It reads the drawing, font5x7.txt, which still has ⟯. The mutant
    cut font5x7.c alone, and gen_font --check is what catches that drift (red).
- **M5** (the text's ( ) mapping removed): the vector lines exactly as predicted: 105 T01-T10 and K01; 104 E01-E06,
  E08-E10, M01-M03, P01-P03 and P05; h104 H01-H04 and H06-H08. screen_test's three text checks went red as
  predicted.
  - **What I hadn't predicted:**
    - the four cursor-cell checks passed;
    - **glyph_frames read red, but on its own crash.** The stray bytes broke its UTF-8 decode
      (mutants-m5-glyph-frames.err), and it found no boxes.
  - **The runner crashed on M5 the same way** (run 1 stops there), leaving the mutant in screen.c. I checked the diff
    was only that line, then restored it from HEAD, and HEAD matched the runner's saved original. After that fix the
    runner reads output with `errors='replace'`. Run 2 is M5-M7.
- The restored tree was built again after each run.

**4. D68,** `scripts/resources.py static 8c4fad7 10fbb7d` (d68.txt):

| | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | 2,429,056 -> 2,429,248 (+192) | 2,356,784 -> 2,356,960 (+176) |
| DIRAM static | +0 | -8 |
| IRAM code, calc_task's frame | +0 | +0 |
| libmain.a | +189 | +181 |
| internal-RAM symbols (nm) | -4, fit_watch | -4, fit_watch |

- **Flash:** I predicted +100 to +250. Both are inside it.
- **DIRAM:** I predicted -4, and both missed.
  - fit_watch's 4 B are gone by nm on both boards.
  - The section totals read +0 on the stand-in and -8 on Rev A. I didn't establish why; alignment is a guess, not a
    measurement.
- **Moved outside the changed sources:**
  - the stand-in's state.c .literal.ab_state_load, -8;
  - eight Rev A literal pools in app.c and casim.c.
  - Each is reported **"object identical"**.
- The containers peaked at 646.8 MiB (34 samples, container-peak-d68.txt). None was left.

**5. The drawn census** (scripts/glyph_frames.py, glyph-frames.txt): 136 runs, 0 boxes.

**6. The board** (the stand-in, board/): claimed as lepton-standin-board 19:11:13Z to 19:14:20Z, and released.
- **glyph104 with FLASH=1:**
  - the app image was 2,429,264 B written and verified;
  - ver=0.106;
  - the frame PASS, the host's 26FD27DB;
  - line 14's stale `ver=0.105` was NOT MET (above);
  - the control was NOT MET, as it must be.
- **Then FLASH=0, after the version lines were fixed:**
  - glyph104: 4 PASS (ver=0.106, 26FD27DB), the control NOT MET;
  - glyph103: 5 PASS (ver=0.106, 4AFAD607, and 0520A61E, the re-pinned ⒤ frame), the control NOT MET.
- **The image is 16 B more than D68's stand-in figure.**
  - The flash's tree reported `core=4cc6180+dirty`: the runner's edit was uncommitted, and the source files were
    10fbb7d's.
  - D68 built a clean 10fbb7d.
  - I didn't measure what the 16 B are; the core string differs between the two builds.
- **The two board frames as images** (board/frames/):
  - glyph104-26FD27DB.png and glyph103-0520A61E.png;
  - drawn by a throwaway copy of the replay that writes each FRAMECRC frame (replay-png.diff, not src). Its two
    scripts give the same CRCs as the board.
- The containers peaked at 338.3 MiB (board/container-peak.txt). None was left.

## For abacus

- **h105:** to run at this record's head.
- **The frames to read:**
  - tests/frames/69-eqn-indirect.png, 71-eqn-indirect-window.png, 72-prgm-eqn-indirect.png and
    73-tb-indirect-wrap.png;
  - the board's two: work/105-evidence/board/frames/.
- **The drawn census:** `python3 scripts/glyph_frames.py`, with build/vectors built.

## After acceptance (abacus #17916, #17919)

- **Accepted** at 5913af2 (abacus 75f7a7b).
  - **abacus's replay:** make test rc 0, 22,551/22,551 and 61,940/61,940; h105 25/25 (448, 252 and 896 px; the cursor,
    backspace and control pairs 0); h104 11/11.
  - **Its falsifier on the real src:** font5x7.c's pair given ( )'s rows, its code points kept. h105 went to 17/25,
    U01-U08 red at 0 px, while T07's %XDRAW stayed green. Each was predicted before the run.
  - The four frames and the board's two read right.
- **h105 adopted** as tests/h105-abacus.txt, typed from #17919: sha256
  f25e5cd5fb32393a6112df21195943dcbd457d3346d4ced0658cbf36ded10149, 3,118 bytes, 31 lines, ending in one newline. It
  runs in make test and the sanitizer run, 25/25 (47/47).
- **The flip** (h105-flip.txt): T07's `⟮J⟯` changed to `(J)` fails it, 24/25, exit code 1 (h105-flip.out).
- **make test** (make-test-h105.txt): rc 0, 22,601/22,601 vectors and 62,034/62,034 expectations.
- **Carried forward** (abacus #17916, not blocking): why DIRAM read +0 / −8 against the predicted −4, and what the
  16 B in the flash image are. Settle them at the next touch of screen.c or the next D68.
