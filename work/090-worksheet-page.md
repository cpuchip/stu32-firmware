# Unit 090: the worksheet, part 2, the page

**From:** decision 95 step 4; the blueprint work/worksheet-2-page.md (8263671, revision 1 ad31f3d); abacus's rulings #14041,
#14043 (accepted, h090 frozen at abacus 2b82cb3 before any src), #14045 (48 slots kept, the dropped line's form, R/S),
#14047 (CLEAR ALL); Michael's pick of B′ (#14050). The build: 378ae7e (src), ec60c85 (the shared bench, the board scripts,
the replay's fix, the mutants), 1095994 (four vectors written with the mutants' predictions).

## What was built

- **STU's home is the worksheet** (firmware/screen.c, RPN and ALG; the 33s and 35s modes, and STU's other screens, as
  before):
  - **The levels at the bottom:** X at scale 4 (its bottom at y 200), then Y, Z, T upward every 20 px, each with its
    letter, and for a level whose row is on the page that row's op beside the letter (list.c's name). X's expression in
    2D (082) keeps its drawing, its bottom at X's, and the levels go above it.
  - **B′ (Michael's pick):** the levels at the top that are **bare** are hidden; X always drawn, Y kept when it shows a
    prompt or a label. Bare is a mark, `ab_val.bare`, one byte in the tail padding (ab_val stays 64 B: rows.c's static
    assert, on both boards). The clears set it where they write their 0: CLx and ← with no entry, CLSTK, CLEAR ALL, C
    on an entry, MEMORY CLEAR and the fresh state. It is cleared from each level an op makes, by part 1's table, in every
    mode: after a key (`ab_rows_bare_key`, in ab_do_arg), after each program line (`ab_rows_bare_line`, at calc.c's step
    loop), and when a typed number ends (`ab_rows_num`). Every copy carries it, so moves carry it (CLx ENTER: two
    cleared zeros). The image keeps it in T_BARE (one byte, a 7 B section, STU only, only when a level is bare;
    image version 24). A 33s or 35s image is byte for byte as before.
  - **The page above the levels:** the newest rows that fit, newest lowest, each once: a row a level holds is drawn in
    its level, never again above. A row is one line, its left (a typed number none, an ALG row its text, else the op's
    name) and its value as a level formats it, when both fit in 31 cells; else two, its left above its value. A dotted
    rule between the page and the levels.
  - **The dropped line**, "n OLDER ROWS DROPPED", the page's first line, drawn only when the page's oldest row is on the
    screen and there's a line for it.
  - **One walk of the rows per frame** (blueprint lean 8): compose's `page_plan` keeps a ring of the newest lines' record
    offsets and each level's own record, so the page and the ops beside the letters cost one walk. No line buffer of its
    own: the page's lines are drawn through T's after T is drawn.
  - **screen_lines** keeps T, Z, Y and X with their text, and appends what's drawn: `page_on`, `levels` ("TZYX" .. "X",
    "-" in ALG), `page_n`, `page[]` (left, value) and `pagetop` (screen.h). vectors.c reads them as abacus #14043's
    grammar: %PAGEN, %PAGE-k=LEFT|VALUE, %LEVELS, %PAGETOP.
- **ALG's history is the page's ALG rows** (029 rule 5 as ruled, #14041 4a: every ALG row the page holds):
  `hist[16][192]`, `hist_val[16]` and `hist_n` are gone (ab_calc −4,096 B); `hist_row` is the serial shown. ▲ ▼ step
  through the page's ALG rows (rows.c `ab_rows_alg_prev`, `_next`), skipping RPN's; ENTER copies the row's text; the
  shown answer is decoded from its row, carrying its serial, so STO of it links the row (`ab_rows_value`). In ALG, the
  line shown and its result are Y's and X's places and the page above holds the rows before it. The expression store
  keeps its 48 slots (#14045: a 34-slot build would refuse a valid 089 image holding more distinct expressions).
- **STO+ − × ÷ A make a row** A holds (rows.c, kind LETTER): op STOADD etc., arg the letter, linked A's old row, then X's
  (#14041 5: A is the left operand); X's alone when A had none. In 33s and 35s mode, no row.
- **CLEAR ALL empties the page** as MEMORY CLEAR does, the dropped count too; the serials go on, never given again
  (`ab_rows_clear_all`, #14047).
- **ab_op_text** (list.c): an op's name as a row shows it, from list.c's one table, so a row and a program line can't
  differ, with the one exception ruled in #14045: R/S's row reads R/S (its line reads STOP); and a keyboard XEQ or GTO
  of a label alone (A000) reads XEQ A.
- **ROWBENCH is shared** (device/dev_bench.h, `dev_rowbench`): the stand-in and Rev A's head-less build run the same
  bench, now with a frame drawn (20 times) and each state's save to the flash, for three states: no page, the page full
  with nothing more held, and the worst (089's).
- **The tests:** tests/090-soroban.txt (39 vectors); app_test's 090 block (UNDO and ALG's history, CLx then UNDO, UNDO
  after CLEAR ALL, 4a's long RPN run dropping ALG's oldest lines); screen_test's frames 49-58 with their checks;
  rows_test's version checks; device/standin/page090.txt (replayed) and rowbench090.txt; stackkeys `page`; vectors.c's
  DROPSEC:n (a section taken out of a saved image, its length and CRC redone).

## What moved, and why

- **On purpose (the blueprint and its rulings):**
  - 089 L03: STO+ A now makes a row (`%ROWS=3 %ROW-1=3:STOADD:1,2:6 %LBL:A=3`).
  - work/029-vectors.txt H06: `%HIST=16` → 18 (#14041 4a); H08's text (its result unchanged: SAVE's image holds no
    page); H09 added (SAVEROWS keeps the history).
  - app_test's 029 UNDO line: the undone line's row is hidden from the history (#14041 4b).
  - CLEAR ALL: the page no longer survives it, and ALG's history never did (#14047).
  - Frames 42-48 (STU's home) and the stand-in scripts arith084, cas088 and expr082 (their STU frames); the 4C fixture
    golden-v24-4c → golden-v25-4c (an image newer than this build, which is 24 now).
  - Version pins: rows_test (a fresh STU's image is 24, with T_BARE; a new check keeps "no row, no level bare" at 20
    and "a row, no level bare" at 23); store_test's 082 check (24, the marks loaded).
- **Test construction:** L09, D55, S49, S50, S51 and K26 relabel an STU image older (IMGVER); an image labelled under 21
  can't hold a section past T_CASCPLX (state.c), so they take T_BARE out first (DROPSEC:24) and are refused or read for
  their own reason. D56's "one past the last status" was 46, MEMORY FULL since 082, refused only while its image stayed
  under version 21: now 50.
- **Fixes found on the way:**
  - standin_replay's SAVE wrote `ab_state_save` (no page) where the board's has written the page since 089
    (dev_image_save): page090's frame after a restart differed. arith084's after-restart CRC, first taken from the
    faulty replay, is corrected.
  - expr_test's store tests counted the 16 history places; 16 program literals, which expr_mark marks, stand in, so the
    store still fills at 48.

## Results

All at 6081504 unless named (465bcb4 is 1095994 with clang's one-line fix; 6081504 adds ROWBENCH's walk line).

- **make test** rc 0 (work/090-evidence/make-test.txt): 207 `n/n vectors` lines, repeats included, 21,302 vectors, 57,676
  expectations, all passing (089's final: 204, 21,197, 57,341). tests/090-soroban.txt 39/39 vectors, 113/113
  expectations (plain and under the sanitizer); app_test 880/880; screen_test 9,029/9,029 checks and 58/58 frames;
  rows_test 138/138; store_test 14/14; standin_replay: 42 scripts, every current frame matching.
- **make accuracy** (abacus #14038) rc 0 (work/090-evidence/make-accuracy.txt): 27 recipe lines by awk over the target
  (abacus counted 28 at 915bcdb; the target is unchanged since, so the counts differ by method, not by what ran), each
  run; 3:53 wall, peak RSS 1.26 GB on lepton's host. The host's python3 has no mpmath: run with
  `PYTHON=~/scratch/soroban/pdfvenv/bin/python` (mpmath there). The worst of fn_fuzz's units 0.500 (ASIN GRAD, ATANH);
  load_fuzz 100,000 cases, 0 violations.
- **clang-check** 73 files, 0 failing, the control refused (work/090-evidence/clang-check.txt). Its first run refused
  screen.c's `"TZYX" + 4 - k->levels` (-Wstring-plus-int): fixed at 465bcb4.
- **D68** (915bcdb → 1095994; work/090-evidence/d68.txt):

  | | stand-in | Rev A |
  |---|---|---|
  | flash | +6,336 | +4,048 |
  | internal RAM static (DIRAM) | +120 | +112 |
  | IRAM code | +0 | +0 |
  | calc_task's frame | +0 | +0 |

  The DIRAM is compose's `comp`, 8,276 → 8,392 B: the page's plan (the levels' bottoms and records, eight lines'
  offsets). The blueprint's target was +0; this misses it by the plan's 116 B (no line buffer was added). ab_calc on the
  host 188,496 → 184,400 B (−4,096, ALG's history), in PSRAM on both boards; ab_val 64 B; screen_page (the host's only)
  4,936 → 8,056 B.
- **Mutants** (work/090-evidence/: mutants-predicted.txt written before any run, mutants-runner.py, runs 1 and 2): 11
  planted faults and a control (M0, no fault: nothing red). M2-M8, M10 and M11 red exactly as predicted. M9 red as
  predicted (090 P05 P06; frames 01-13, 16, 17, 19 and 35 layout checks). M1 red, wider than predicted: my misses were
  N01 (R/S's run row, held by X, drawn again above), frames 42-47 (X's expression holds a row there) and frame 58's
  check (it stayed green: STO+ A's row is still the newest). Run 1's M1 and M9 were the runner's: M1's fault left a
  variable set and unused (-Werror), and M9's screen.c fault rebuilt only the binaries using screen.c, so the others
  kept their times and read as failed builds. Run 2 rebuilds every binary (they're deleted first).
  - The bare hooks' shape: every op writes a whole value (bare 0) except CHS, which negates X in place (calc.c:5507).
    So B10 (the key) and B11 (a program line) are what M4 and M3 turn red; B06, abacus's fn-02 T02 shape, passes
    without the line's hook, since a program's literal is a whole value.
- **The stand-in** (work/090-evidence/board/):
  - page090 (465bcb4): every expect met but the control; the frame's CRC C21C35FD before and after a restart, as the
    host's replay gives.
  - The moved scripts, run unedited on 465bcb4: arith084 12 PASS, cas088 18, expr082 14; each misses only its `ver=`
    line (0.84, 0.88, 0.83) and its control, so their new CRCs, the after-restart ones included, are the board's.
  - rows089 (089's own) 18 PASS; misses only its three `ver=0.89` lines and its control.
  - ROWBENCH (6081504's code, standin-rowbench090-walk.out; the first run, standin-rowbench090.out, agrees within a
    few percent):

    | | no page | the full page (1,416 rows, 57,348 B) | the worst (33 held 409 B rows) |
    |---|---|---|---|
    | a frame | 478 µs | 4,768 µs | 3,235 µs |
    | of which a walk of the rows | 0 | 768 µs | 613 µs |
    | the device's save to the flash | 758 B, 71 ms | 9,138 B, 181 ms | 27,683 B, 496 ms |
    | a key (1 +) | 190 µs | 187 µs | 195 µs |

    The walk is under lean 8's 1 ms, so there is no ring. The frame's cost is mostly formatting: a page row is formatted
    once to plan it and once to draw it, about 250 µs a format on the stand-in (the frame less the empty page's and the
    walk, over 14 formats). A frame of 4.8 ms is under a sixtieth of a second, so not seen, but formatting each row
    once would take about half of it off; not built, offered.
  - PROGBENCH (speed.txt; the per-line hook runs in every mode, and PROGBENCH runs in the 35s mode): 915bcdb SIN 270,
    270 ms, LN 381, 381 ms; 6081504 SIN 274, 274 ms, LN 381, 381 ms (two runs each, 500 lines each).
- **Rev A's main head-less** (headless.sh: Rev A's code on the stand-in's S3 and flash chip, not the Rev A board, which
  isn't on lepton; reva-headless-rowbench090.out at 465bcb4 with the walk line):

  | | no page | the full page | the worst |
  |---|---|---|---|
  | a frame | 400 µs | 3,208 µs (walk 855) | 2,659 µs (walk 682) |
  | the device's save to the flash | 758 B, 25 ms | 9,138 B, 299 ms | 27,683 B, 788 ms |

  Lean 7: the worst save is under 1 s, so no cap on the held rows. Its own state's save (1,068 B) took 168-210 ms here.
- **Not done:** abacus's tap during the worst save on Rev A (#14041 6, not blocking) needs a finger on a key: the board
  isn't on lepton and nothing here can press one.
- **Claims:** lepton docker soroban-* 4 GiB 23:07-23:25Z (D68, clang-check, the boards' builds), peak 716 MiB (docker
  stats every 2 s, 194 samples); lepton stand-in board 1a86 56E1012198 23:11-23:25Z. Host runs: make accuracy peak RSS
  1.26 GB; the mutants and make test not sampled.

## For the next part

- Part 3 (abacus #14041 1): selecting a row, the row menus, moving and scrolling in RPN (stu-nav), what-made-this.
- Offered from here: format each page row once a frame (about half of the full page's 4.8 ms on the stand-in).
- Michael's note on the levels (#14050: "The HP 50g just has them always numbered. Maybe we should just drop x y z t in
  stu mode?") is abacus's councilling; nothing here depends on it beyond the letters' drawing (draw_page).
