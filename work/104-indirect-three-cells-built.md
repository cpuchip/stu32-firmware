# Unit 104 built: ⒤ and ⒥ drawn as three cells

**From:** the blueprint work/indirect-three-cells.md at a6c4e89 (abacus #17595, Michael's T120 "Three cells"), with
abacus's rulings in #17600 and h104 frozen in #17607: sha256 81cf5cb8…367d, 11 vectors on %XLINE and %BOXES, 4/11 at
a6c4e89.
- **The build:**
  - 5141220: the source and its tests.
  - 2de858a: the mutants' predictions and their runner, before any run, with screen_test's frame 73 (below).
  - This commit: the mutant run, D68, the drawn census, the board, make test, this record.
- **VER 0.105.** R4 is built as (a), plain parentheses, until Michael answers T136.

## What changed

- **firmware/screen.c's cells_of, one decoder for every place the screen turns text into cells:**
  - **The six decode sites:** screen_cells, screen_decode, screen_text, line_add, eqn_line and the wheel's cut.
  - **The tokens:** ⒤ is `(`, its letter, `)`, and ⒥ is `(` `J` `)`.
  - **The letter's case is the text's model (R1, R2):**

    | Model | ⒤ draws as |
    |---|---|
    | 35 | `(I)` |
    | 33 and STU | `(i)` |
    | ⒥, any model | `(J)` |
  - **Where the model comes from:**
    - typing: ab_rec_model;
    - a list entry, the wheel and the textbook's text: the entry's stamp;
    - ALG's rows and the CAS rows: ab_rec_model, which is STU there.
  - **The public decoders take `(i)`.**
  - **A token's second and third cells carry TOK_MORE** (bit 31, above the marks). The marks are now read through
    MARK_OF, so the flag never reads as a mark.
- **fit_line, a pure function split out of draw_line_lead.** It does the scale, the window and the ends' ellipses; the
  draw draws from its result.
  - **A cut never shows part of a token (R3):**
    - the window's left side, its right side and the ∫ tail's cut each move to the token's boundary, and the line draws
      a cell or two short;
    - the page rows' end cut does the same;
    - the wheel's cut and the textbook's text wrap stop before a token that wouldn't fit.
- **screen_x_fit (screen.h):** X's line as screen_render fits it, through a static pointer that draw_line_lead fills.
  It's the seam for vectors' new `%XDRAW=s:cells` (under `#ifndef AB_DEVICE`).
- **The textbook (firmware/tbook.c) counts with the screen's decoder** through the new screen_cells_n, for its atoms and
  its text wrap.
- **The texts the screen exports carry the three characters:**
  - screen_page's lines;
  - the wheel's text;
  - screen_drawn (drawn=).
  - The core's text keeps the token.
- **core/list.c:** an equation line writes its tokens as the line's other lines do: `(I)` or `(i)` by the line's mode,
  and `(J)`.
- **The one-cell glyphs U+24A4 and U+24A5 are retired (R5).** The face has 130 glyphs.
- **tests/glyph_census.py** finds the two tokens drawn as three cells, from cells_of's own body.
- **A sibling, fixed with eqn_line** (its cell appends were rewritten anyway): a cell carrying a combining mark wrote
  its text from the cell value.
  - **The effect:** x̂ in an equation exported four stray bytes, F0 80 81 B8, in screen_page's text, in the list and
    while typing (measured at dc0cdeb).
  - **Now:** the cell's own bytes are written (104 X01, X02).

## Tests

- **tests/104-soroban.txt, 28 vectors, 36 expectations,** written from the blueprint's table and its measurements:
  - **E01-E10:** widths, the cursor's cell and editing across a token under 35.
  - **M01-M06:** the case under 33, STU and 35, the 35- and 33-stamped entries' drawn text.
  - **P01-P05:** the listing under each label's mode, beside STO (I).
  - **W01-W03:**
    - five tokens: scale 4 → 3;
    - nine: a scale-2 window `…+(I)+…` with no stray `)`;
    - a long entry cut at its end.
  - **A01-A02:** (a)'s likeness of the token and parentheses typed by the user.
  - **X01-X02:** x̂'s text.
  - Plain and in the sanitizer run, with no save/load pass (the cursor and the typing are RAM).
- **%XDRAW** in tests/vectors.c.
- **screen_test:**
  - frame 69 re-pinned (STU's `(i)+(J)×2`);
  - frame 71 (nine tokens, 35's window);
  - frame 72 (`A002 (I)+1`) and its text check;
  - four checks of the cursor's cell: screen_page's cursor is the `_`'s cell in its text, counted in cells, the token
    three;
  - frame 73 (fifteen tokens in the textbook's text, wrapped at 2).
  - Each was looked at before it was kept, and the other 69 frames wrote back the same.
  - **Frame 73 was added after the src commit, before the predictions.** The blueprint's M4 (the textbook's atom count)
    would survive by construction: the textbook's parser takes no token, so its atoms never hold one. The text wrap is
    the only place it meets one, and no test read the wrap.
- **The board scripts:**
  - **device/standin/glyph103.txt:** its ⒤ frame re-pinned, AC98D863 → CCA1527E, and its stale `expect ver=0.104` made
    0.105, since the script was touched (the carry-forward from abacus #16985).
  - **device/standin/glyph104.txt (new):** from MEMORY CLEAR, MODE's 35s, then EQN RCL A + RCL (I) × 2 ENTER. The frame
    is 9A392C27: the replay's, and the core's own render of the same state, which reads `A+(I)×2`.

## Results

- **make test** (this record's tree; work/104-evidence/make-test.txt): rc 0.
  - 22,461/22,461 vectors and 61,757/61,757 expectations, summing every `n/m vectors` and `n/m expectations` in the
    output. That's cb92737's 22,405 and 61,685 plus 104's 28 and 36, twice (plain and san).
  - screen_test 9,671/9,671 and 73/73 frames: 269 fewer face checks at 130 glyphs, and 5 new.
  - glyph_census: 0 without a glyph, 2 drawn as three cells. app_test 919/919. The replay: 53 scripts, every frame
    matching.
  - clang-check: 76 files, 0 failing (the control refused), run at 5141220.
- **Mutants** (work/104-evidence/mutants-run1.txt; host, the largest process 113.2 MiB): 8 verdicts as predicted, 7 of
  the 8 red lists exactly as predicted.
  - **M0** (the control): SURVIVED.
  - **M1** (cells_of's expansion removed): RED.
    - **As predicted:**
      - 104 E01-E06, E08-E10, M01-M05, P05, W01-W03;
      - 103 B03, B04;
      - h103 H04, H05;
      - screen_test's frames 69, 71 and 73, the no-box check and 3 of the 4 cursor checks;
      - both replay frames;
      - glyph_frames: 219 boxes in 124 frames.
    - **One miss:** glyph_census failed only U+24A4, where I predicted U+24A5 too.
      - The census checks that each code point's hex text is in cells_of's body.
      - M1 disabled the branch's test, but `0x24A5` still stands in the letter's line below it, so ⒥ passed.
      - The census is a textual check: it proves the expansion is written in the function, not that it runs. The vectors
        and the drawn census are what run it.
  - **M2** (upper_of always false): RED, exactly as predicted:
    - 104 E01-E06, E08, E10, M05, P05, W01-W03;
    - screen_test's frame 71;
    - the replay's glyph104.
  - **M3** (upper_of always true): RED, exactly as predicted: 104 M01, M02, M03, M06.
  - **M4** (the text wrap counting a token as one cell): RED on screen_test's frame 73 alone, as predicted.
  - **M5** (TOK_MORE never set): RED on 104 W02, W03 and frame 71, as predicted.
  - **M6** (list.c's rewrite removed): RED on 104 P01 and frame 72 with its text check, as predicted.
  - **M7** (drawn='s rewrite removed): RED on 104 M04, M05, M06, as predicted.
  - **The runner** restored the three files, compared them, and built the restored tree again.
  - **Run 1's peak sampler read the wrong process** (9.5 MiB). The peak is M1's rerun under `/usr/bin/time -v`
    (work/104-evidence/mutants-m1-rerun-peak.txt), whose verdict is the same as run 1's.
- **D68** (`scripts/resources.py static a6c4e89 5141220`, work/104-evidence/d68.txt):

  | | Stand-in | Rev A |
  |---|---|---|
  | flash | +1,088 | +1,008 |
  | libmain.a | +1,021 | |
  | DIRAM | +8 | +8 |
  | IRAM | +0 | +0 |
  | calc_task's frame | +0 | +0 |

  - **My predictions missed both:**
    - I predicted a flash net of tens of bytes; the decoder, fit_line, the cut rules and the exported-text rewrites are
      about a kilobyte of code.
    - I predicted RAM +0; it's +8, screen_x_fit's two static pointers (fit_seen, fit_watch). fit_watch is always
      compose's X line, so 4 of the 8 could go if wanted.
  - **The object report:** every section that moved outside the changed sources is object identical (Xtensa
    relaxation).
  - The containers peaked at 1.314 GiB.
- **The drawn census** (scripts/glyph_frames.py, work/104-evidence/glyph-frames.txt): 133 runs, 0 boxes.
  - With the expansion removed (M1), 219 boxes in 124 frames.
- **The board** (the stand-in, work/104-evidence/board/): claimed as lepton-standin-board 02:38:38Z to 02:41:17Z, and
  released.
  - **glyph104 with FLASH=1:**
    - the app image was 2,428,096 B written and verified, D68's stand-in figure;
    - ver=0.105, core 2de858a (5141220's source);
    - the port held after the flash;
    - 4 PASS, the frame the host's 9A392C27;
    - the one NOT MET is the control.
  - **glyph103 with FLASH=0:** both frames the host's (4AFAD607, and CCA1527E, the re-pinned ⒤ frame).
    - Its `ver=0.104` line was NOT MET: the stale line this unit's edit fixed after the run. Its control was NOT MET as
      it must be.
  - The containers peaked at 132.7 MiB.

## Known limits, as ruled

- **The textbook's parser refuses ⒤,** so an STU equation holding a token is shown as its text, never in 2D (P4, as
  before 104).
- **Under (a), the token and the user's own parentheses look alike in every mode** (A01, A02). Michael's T136 decides
  (b).

## For abacus

- **h104:** to run at this record's head.
- **The vector file:** tests/104-soroban.txt.
- **The drawn census:** `python3 scripts/glyph_frames.py`, with build/vectors built.

## After acceptance (abacus #17631, #17634)

- **Accepted** at 11beaa4: h104 11/11, against 4/11 at a6c4e89. abacus read the mutants, D68, the drawn census and the
  board without rerunning them.
- **h104 adopted** as tests/h104-abacus.txt, typed from #17634: sha256
  81cf5cb89b777436cf49f70ddb1f3ceb2908ed0d8a7d09f9b16b34d5988d367d, 1,535 bytes, 15 lines, ending in one newline. It
  runs in make test and the sanitizer run, 11/11 (16/16).
- **The flip:** H06's `(J)` changed to `(j)` fails it, 10/11, exit code 1 (read in its own command).
- **make test:** rc 0, 22,483/22,483 vectors and 61,789/61,789 expectations.
- **Carried forward:** drop screen.c's fit_watch (4 B of internal RAM; it's always compose's X line) at the next touch
  of screen.c (abacus #17631, decision 68).
- **Michael's T136** decides the token's own parentheses, which would be the follow-on.
