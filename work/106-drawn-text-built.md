# Unit 106 built: each line's drawn text

**From:** the blueprint work/drawn-text.md at 83c3efe (abacus #17953, inside Michael's T136), with abacus's rulings in
#17960 and h106 frozen there: sha256 8a56ff23…510d, 14 vectors (abacus d02fb1d). abacus calibrated it at 14/14 on the
prototype and 4/14 with drawn copied from text.
- **The build:**
  - 024687e: the source and its tests;
  - cbcd5c5: the mutants' predictions and their runner, before any run;
  - this commit: the mutant run, D68 with its section diff, clang-check, 105's two open measurements, this record.
- **VER 0.107.**

## What changed

- **firmware/screen.h:** screen_line gains `char drawn[SCREEN_TEXT_MAX]`, the line's cells as drawn (R1).
  - The comment says how a host reads it:
    - every cell is one code point and its marks, so a mark counts with its base;
    - drawn has as many cells as text;
    - `cursor` is the same cell in both (abacus #17960).
- **firmware/screen.c, page_line:** writes drawn by running cell_u8 over the line's own cells, the function %XDRAW
  already uses, so there's no second mapping. It stops at a whole cell when the buffer is full.
  - A token's pair is ⟮ ⟯ where text writes ( ).
  - ŷ and ȳ are y followed by U+0302 or U+0304 (R2).
  - text is unchanged.
- **A sibling, found by the new check, fixed:** screen_lines left T, Z and Y with cursor 0 when no branch fills them
  (beside STU's area, since unit 080). That's the memset's 0, where screen.h says −1 means none, so a host was told its
  cursor sat at cell 0 of an empty line. All four start at −1 now.
- **tests/vectors.c:** `%XCELLS`, X's drawn string, in %XLINE's branch, blanks `_` (R3).

## Tests

- **tests/106-soroban.txt, 14 vectors, 35 expectations,** from the blueprint's measure.txt:
  - C01-C04: a token beside typed parentheses under 35, 33s and STU, the cursor inside the line;
  - C05-C08: the program line, STO (I) plain, the list, STU's list row;
  - C09-C10: ŷ, ȳ decomposed, and x̂ the same bytes both ways;
  - C11: the whole line, against %XDRAW's window;
  - K01-K03: controls.
  - %XDRAW is paired with %XCELLS where the line fits. In make test and the sanitizer run.
  - **C09's expectation had to be written as the bytes:** my first draft typed it precomposed, and the prototype
    failed it.
- **screen_test, three checks:**
  - **drawn_agrees on T, Z, Y and X of five pages:** 35's token beside typed with the cursor inside, a program line
    over stack values, ŷ ȳ, x̂, and plain values.
    - Both strings have the same cell count (screen_cells).
    - The cursor's cell is `_` in both.
    - The decoded cells are equal except text's ( ) against drawn's ⟮ ⟯.
  - **⟮ in drawn on the two token pages.**
  - **screen_page is 12,312 B.**
- **The check found the sibling:** it failed on STU's pages, T, Z and Y with cursor 0. It passed after the fix.

## Results

**1. make test** at 024687e (make-test.txt): rc 0.
- 22,629/22,629 vectors and 62,104/62,104 expectations. That's 5742f06's 22,601 and 62,034, plus 106's 14 and 35 in
  both runs.
- screen_test 9,946/9,946 checks, 73/73 frames; check-version 0.107.

**2. make clang-check** (clang-check.txt): rc 0, 76 files, 0 failing, the control refused.

**3. The mutants** (mutants-predicted.txt, committed in cbcd5c5 before any run; mutants-run1.txt): **6 of 6 as
predicted, every red list exact.**
- **M1** (drawn copied from text, the ruled-out second mapping): C01-C05, C07-C09, C11 and the ⟮ check.
- **M2** (cut at the cursor): C01-C04, C11, K03 and drawn_agrees.
- **M3** (cut at 31 cells): C11 alone.
- **M4** (the cursor fix removed): drawn_agrees alone.
- **M5** (the marks dropped): C09, C10 and drawn_agrees.

**4. D68,** `scripts/resources.py static 5742f06 024687e` (d68.txt). **The flash prediction missed:**

| | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | 2,429,248 -> 2,428,800 (−448) | 2,356,960 -> 2,356,944 (−16) |
| DIRAM, IRAM code, calc_task's frame | +0 | +0 |

- **I predicted +40 to +120, and it went down.** The input sections placed in each image (d68-sections.txt,
  mapdiff.py; the map's discarded sections, at address 0, left out) show why:
  - **Stand-in, −455 B in all, libmain's −454:**
    - page_line and cell_u8 now have sections of their own (+142, +178);
    - screen_lines shrank by 560 and draw_line_lead by 208, as the copies inlined into them went.
    - So the compiler stopped inlining page_line into screen_lines (18 call sites) and cell_u8 into draw_line_lead,
      now that cell_u8 has two callers.
  - **Rev A, −21 B, libmain's −20:**
    - screen_lines and page_line are discarded there: only the stand-in's dev_bench calls screen_lines.
    - So only cell_u8's move out of line shows: +178 +4 against draw_line_lead's −208 −4, and +3 of strings.
- **The device's RAM is as predicted:** +0. dev_bench's PSRAM malloc of a screen_page grows by 4,096 B.
- The containers peaked at 447.1 MiB (container-peak-d68.txt). None was left.
- My first map parse counted discarded sections and couldn't reconcile Rev A (−394 against libmain's −20). Leaving
  out address 0 reconciled both boards.

**5. The board:** not run. No frame changes (73/73), and nothing on the device reads drawn. dev_bench's screen_lines
only grows its PSRAM buffer.

## 105's two open measurements (abacus #17916, settled here: 105-questions.txt)

- **DIRAM +0 / −8 against −4:** the only .bss or .data input section that changed from 8c4fad7 to 10fbb7d is
  fit_watch, −4 on both boards. The output section .dram0.bss reads +0 on the stand-in and −8 on Rev A, so the
  alignment fill between its input sections moved +4 and −4.
- **The flash image's 16 B:**
  - Between D68's 10fbb7d build and the board's flash build (`core=4cc6180+dirty`), three string-literal pools grew
    by 8 B each, and .flash.rodata by 16, as the image.
  - The two images hold different core commits ("10fbb7d"; "4cc6180+dirty", by strings).
  - The literal's padded size grows by exactly 8 (8 B to 16). That's consistent, but I didn't read the pools byte by
    byte.

## For abacus

- **h106:** to run at this record's head.
- **The directive:** `%XCELLS`.
- **The cursor rule for the hosts** is in screen.h's comment on drawn.

## After acceptance (abacus #17970, #17974)

- **Accepted** at 91dd74e (abacus 384d54f).
  - **abacus's replay:** make test rc 0, 22,629 and 62,104; h106 14/14; h105 25/25.
  - **Its falsifier on the real src:** cell_u8 writing the pair as ( ). h106 went to 5/14: every pair line red on
    %XCELLS, H01's %XDRAW with them, ȳ and the controls green. Each was predicted before the run.
  - abacus is telling the hosts about the cursor fix (0 → −1 on unfilled lines), since it changes what they read.
- **h106 adopted** as tests/h106-abacus.txt, from #17974: sha256
  8a56ff2336df3328a61bdd9780b1a9d5d40c5f90eb54c01e4e98c20c7bca510d, 2,232 bytes, 18 lines, ending in one newline.
  - H09's bytes were written explicitly, as #17974 warned: %XLINE's precomposed U+0233 (C8 B3) and %XCELLS's y and
    U+0304 (79 CC 84).
  - It runs in make test and the sanitizer run, 14/14 (30/30).
- **The flip** (h106-flip.txt): H09's %XCELLS made precomposed fails it, 13/14, exit code 1 (h106-flip.out).
- **make test** (make-test-h106.txt): rc 0, 22,657/22,657 vectors and 62,164/62,164 expectations.
