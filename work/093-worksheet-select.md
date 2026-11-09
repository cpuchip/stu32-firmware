# Unit 093: the worksheet, part 3, selecting a row

**From:** the blueprint work/worksheet-3-select.md (9e79ec8; revision 1 a10885a with abacus #14132's rulings, accepted
#14134); the runner's answers #14135; h093 frozen by abacus before any src (#14137: abacus 6340e59, sha256 ea0d1d08…8631),
with its three rules (COPY's row prints COPY; 5 returns to SEL's row; the window rule). The build: b204453 (src),
cfb683f (the mutants' predictions, before any run), f0b67ba (the mutant run and the stand-in's script). VER 0.93.

## What was built

- **The selection is in the core,** in the RAM tail after hist_row: `sel_row` (the serial, 0 none), the level holding it
  (-1 a page row, 0-3 X-T), SEL's row (for 5), the window's top when scrolled, the dropped-line flag, and a view of a row
  in X's place. Loading an image, MEMORY CLEAR and CLEAR ALL each clear it (state.c's load end calls `ab_sel_clear`).
- **SEL** (op AB_SEL; the app's GOLD then soft 2, or soft 3, on STU's home in RPN, not in program, ALG, Equation, TABLE or
  GRAPH): completes a number being typed (its row made), then selects the lowest page line's row, or X's when the page is
  empty. With no row at all it selects nothing. The 33s and 35s modes don't have it.
- **The keys while a row is selected** (do_arg, after SOLVE's report and before program or ALG): 8 up and 2 down over the
  page's rows, then the levels T to X (a level with no row, or one B′ hides, skipped); 5 back to SEL's row; 4 and 6
  nothing; C and BKSP leave, nothing changed; ENTER is COPY; 0 1 3 7 9 . E CHS leave and act; any other key leaves, then
  acts on X (the app also drops the selection before any key that isn't a core op).
- **Scrolling:** past the window's top the selected row becomes its top line (top-anchored); past the oldest kept row the
  dropped line, then no further. The core asks the screen for the window through `ab_rows_set_window` (rows.c), which
  app_init, vectors' main and screen_test register with `screen_page_window`.
- **The actions:** COPY appends a row AB_COPY linked to the selected row (R4's rule), its value in X with lift; ROWSTO
  stores the selected row in a letter, a move (no row made); ROWSHOW, ROWFRAC and ROWINFO are views in X's place until the
  next key, the selection kept; a verb (CASSIMP, CASEXP, CASSOLVE) is COPY, then the verb on X. INFO's text is revision
  1's: `OP OF V1, V2` (`, MORE` past three), ` (OFF SCREEN)` after a link not drawn, `(DROPPED)` for one gone, `TYPED`,
  `LINE` for an ALG row.
- **ALG:** the row ▲ ▼ show (hist_row) is the selected row: the ROW ALG menu (EDIT, STO, SHOW, FRAC, _, INFO) on GOLD's
  bank of the ALG LINE bar, ROWEDIT putting its text on the line, and its links dotted.
- **The screen:** the selected row's lines inverted (XOR) on the page and in the levels, a 4 × 4 dot at x = 13 on each
  line of a row it links to, "SEL" in RPN's place in the band, the row menus (gen_keymap: ROW NUM, ROW EXPR, ROW ALG).
- **Tests and dumps:** vectors' tokens SEL, ROWCOPY, ROWSTO:, ROWSHOW, ROWFRAC, ROWINFO, ROWEDIT and dumps %SEL,
  %PAGEMARK-k, %LEVELMARK-L, %SOFT-n, %XLINE, %BAND (the blueprint's grammar); tests/093-soroban.txt (35 vectors, 105
  expectations, each worked out by hand before the first run) in make test and under the sanitizers; app_test's 093 block;
  screen_test's frames 59-63 with their checks; HELP entries for the six new ops and help_test's sweep roots for a
  selected number, a selected expression and ALG's shown row. The loader refuses the new ops as program lines (load_fuzz
  found that first). opnames.h regenerated (243 ops).
- **Not in make accuracy's save-and-load pass:** 093's file isn't there, because a load clears the selection by design,
  so a load after every key would end every selection. Its Makefile comment says so.

## What moved, and why

- **Frame 55** (ws-alg-shown): ALG's shown row now dots its link, as lean 7 says. The blueprint said "no existing frame
  moves"; its own lean 7 moves this one. My miss, in the blueprint, not the code.
- **The key's time and the frame's,** which the blueprint said wouldn't move ("A key's time isn't touched"). They did;
  see the timing below.
- Nothing else: every vector file, every other frame, and page090's board CRC are unchanged.

## Results

- **make test** at f0b67ba: rc 0; 211 lines, 21,402 vectors, 57,992 expectations (every `n/n vectors` line, repeats
  included). Against 090's 209 / 21,332 / 57,782 that's 093's 35 and 105 twice (plain and sanitized). 093 35/35 and
  105/105; app_test 890/890; screen_test 63/63 frames and 9,032 checks; h090 20/20 and h090-Bp 10/10. Peak RSS 412 MiB,
  3:51 (work/093-evidence/make-test.txt).
- **make accuracy:** rc 0, 3:48, peak RSS 1.23 GiB (the venv python; work/093-evidence/make-accuracy.txt). Its recipe
  is 27 tab lines, 26 commands and one comment (counted with grep on the Makefile), unchanged by 093.
- **clang-check:** 73 files, 0 failing, the control refused (work/093-evidence/clang-check.txt).
- **Mutants** (work/093-evidence/mutants-predicted.txt, committed at cfb683f before any run; run 1 in mutants-run1.txt):
  10 of 11 exact, M0 (the control) green as predicted.
  - **N1** (the window not following) was my miss: I predicted W01, W02 and W04 red, and W04 stayed green. W04 moves to
    the oldest row, and that path sets `sel_top` itself (calc.c:4988, `c->sel_drop = true, c->sel_top = c->sel_row`),
    so removing the scroll line (4992) doesn't reach it. Its other predictions held: W01, W02, the scrolled check, frame
    63.
  - N2-N10 went red exactly as predicted, including N6's and N10's frame 62 and N8's frames 55, 59, 60, 61, 62.
- **The stand-in** (image f0b67ba; work/093-evidence/board/):
  - sel093 (device/standin/sel093.txt, a replay of the host): 6 PASS; the selection's frame 828E790B and INFO's
    8EF1CF36, both the host's; stack free 26,764; only the control red. Its first run lost the port at the reset after the
    flash (ACM3 to ACM1, try1-port-lost.out); the run without a flash passed.
  - page090 (unedited): 8 PASS; 2CE8A469 before and after a restart, 090's; red only `ver=0.90` and the control.
  - rowbench090 (unedited): 8 PASS; red only `ver=0.90` and the control.
- **D68** (scripts/resources.py static b59caec f0b67ba, the commit before 093's src; work/093-evidence/d68.txt):

  | | flash | DIRAM | IRAM | calc_task's frame |
  |---|---|---|---|---|
  | the stand-in | +8,864 | +16 | +0 | +0 |
  | Rev A | +8,512 | +8 | +0 | +0 |

  The internal-RAM symbols: compose's plan `comp` +8 (the selection and the marks) and `window_fn` +4. The blueprint
  said about +16 B DIRAM. An earlier run from 03e0808 showed +324 DIRAM; that was 091's SAVEBENCH buffer (256 B, in
  the range because 03e0808 predates 091), so the base was moved to b59caec.
- **The timing (ROWBENCH on the stand-in, µs).** The first run at f0b67ba was 4,035 µs for the full page's frame,
  against 3,663 at a66f409 in 090's record. That's two images several commits apart, one run each, so I ran a control:
  b59caec (the commit before 093) and f0b67ba flashed alternately, A1 B1 A2 B2, each measured on a run without a flash
  (work/093-evidence/frame-control/, one.sh):

  | | b59caec A1 | A2 | f0b67ba B1 | B2 | change |
  |---|---|---|---|---|---|
  | a frame, no page | 444 | 424 | 427 | 427 | none past the spread |
  | a frame, the full page | 3,734 | 3,751 | 4,033 | 4,036 | +292 (+7.8%) |
  | a frame, the worst page | 2,932 | 2,922 | 3,088 | 3,077 | +156 (+5.3%) |
  | the walk of the rows, full | 755 | 767 | 767 | 768 | none |
  | a key (1 +), no page | 189 | 188 | 206 | 207 | +18 (+9.5%) |
  | a key, the full page | 190 | 190 | 210 | 210 | +20 (+10.5%) |
  | a key, the worst page | 208 | 209 | 231 | 230 | +22 (+10.5%) |

  (Change: the means of the two runs.) The runs of one image agree within 20 µs, so the change is real. The blueprint
  was wrong on the key: it said a key's time wasn't touched, and it costs 18-22 µs more.
  - **Not isolated.** The bench's key is `ab_do` alone (dev_bench.h:340, no app, no screen), and 093's code on that path
    without a selection is one store (`c->sel_view = 0`), one test of `sel_row`, and new case labels. A throwaway host
    loop of the same keys (frame-control/hostkey.c, the core's sources at each commit, best of 200 runs of 200 ops) gives
    0.378-0.380 µs at b59caec and 0.393-0.408 at f0b67ba, three runs each: about +6%, on a machine where an op is 500 times faster than on
    the S3. So the core's own path did get a little slower on the host. On the S3, code size and placement (the stand-in
    runs from flash through a 16 KB instruction cache, and calc.c grew 205 lines) are a hypothesis, not tested.
  - **What it costs the user:** the bench's one rule is 090's 1 ms for the walk, which didn't move. A key is 0.23 ms and
    the slowest frame 4.0 ms. I'm offering a bisection, one difference at a time (the tail's 20 bytes, do_arg's lines,
    the cases), if abacus wants the cause.
- **Claims:** lepton docker soroban-* 4 GiB and the stand-in board 01:34:43-01:40:30Z (the board, sel093, page090,
  rowbench090), docker again to 01:46:38Z (clang-check, the two D68 runs), both 01:52:25-02:08:53Z (the frame control);
  peaks 715.7 MiB (docker stats every 2 s, soroban-* containers, 125 samples over the first two windows) and 602.7 MiB
  (222 samples, the third).
  The host's make test and make accuracy ran without a claim (412 MiB and 1.23 GiB peak RSS, under the small-run line).
  The board is left on the stand-in's image at f0b67ba.

## For the next unit

- Unit 094 (decision 98: SOLVE's report ENTER copies the answer as an expression, as a selected row's COPY does; EVAL
  does today's ENTER): blueprint first, small, before 092's build (#14144), with tutor's four items (#14150).
- The key's +20 µs: offered above, not run.
