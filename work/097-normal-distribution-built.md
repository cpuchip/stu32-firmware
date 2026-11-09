# Unit 097 built: erf, erfc and the normal distribution (decision 95's second)

**From:** the blueprint work/normal-distribution.md (measured d3ae2c8, revision 1 32ad875, its evidence a4fd932), abacus
#14270 (the corrected Φ to try, the house bar), #14272 (accepted, the names and labels), #14274/#14276 (the table
interface; herf-table and herf-v frozen at abacus fd7ade9), #14277 (tutor's two cases); tutor #14275.
- **The build:**
  - e7d3d3b: the source.
  - dc350de: the mutants' predictions, before any run.
  - 396ae79: run 1, and D68 at e7d3d3b.
  - c2db894: the fixes after run 1 (below), and hmark adopted.
  - This commit: run 2, D68 again, the board, this record.
- **VER 0.98.**

## What was built

- **The functions** (core/fn.c):
  - **ERF and ERFC** are the library's (bid128_erf, bid128_erfc; Intel DFP, BSD).
  - **Φ(z) and 1−Φ(z)** are erfc(∓z/√2)/2 with abacus's correction (#14270): the argument formed in hp at 64 digits,
    erfc at its 34-digit rounding u0, two Taylor terms in r = u − u0, every term in hp, halved and rounded once.
    - 1−Φ comes from erfc, never as 1 − Φ.
    - Past |z| = 200 the tail is an underflow (0, rounded) and the other side 1, rounded.
  - **Φ⁻¹(p), 0 < p < 1**, by Newton:
    - on erf for 0.1 ≤ p ≤ 0.5, where 2p − 1 is exact, so the centre keeps its relative digits;
    - on the corrected Φ below 0.1;
    - above 0.5 as −Φ⁻¹(1 − p), 1 − p being exact there.
    - The start is a double from ln p (the form of A&S 26.2.23, its coefficients recalled, only a start).
    - It's converged when a step moves x by at most 4 units of its 34th digit. The last step is formed in hp and
      rounded once. Φ⁻¹(0.5) is 0.
  - **Working values:** fn.c's shared sets (fw, u44[0]), so no new internal RAM.
- **Ops** AB_ERF, AB_ERFC, AB_NCDF, AB_NCDFC, AB_NCDFINV, appended:
  - one-number functions: LASTx, lift, a row each;
  - programmable;
  - STU ONLY in 33s and 35s modes;
  - NOT A NUMBER on an expression, INVALID DATA on a complex number or a vector;
  - their results rounded, as untracked (096).
- **The keys:** PROB's sixth key opens NORM: Φ, 1−Φ, Φ⁻¹, ERF, ERFC, ▸ (back to PROB).
  - The labels' glyphs are exact: U+2212 in 1−Φ; U+207B and U+00B9 in Φ⁻¹, both new in the font.
  - Outside STU the sixth key is empty and does nothing, as CAS's key is.
  - A menu closes after its op, as every menu does.
- **The names:** ERF( ERFC( NCDF( NCDFC( NCDFINV( in STU's lines and equations; SYNTAX in 33s and 35s ones.
  - ALG's keys type them (tutor #14275 1: the Φ key puts NCDF( on the line).
  - NCDF(−1,1) is SYNTAX, with the line kept to edit (tutor #14275 2).
- **The CAS:** casim's language has no such names: an unknown name is SYNTAX there (casim docs/language.md). So the
  firmware finds the five names before calling casim and answers UNSUPPORTED with the name (UNSUPPORTED ERF), as 041 D
  does for 33s names casim lacks. The blueprint's line that casim itself refuses them as UNSUPPORTED was wrong and is
  struck in place.
- **The image:** version 26 when a program line, or the message showing, holds a 097 op's code. An older build refuses
  such an image whole rather than misread it, and this build refuses those codes below 26.
  tests/golden-v26-4c.img is now golden-v27-4c.img, still newer than this build (as 090 and 096 moved it).
- **HELP:** the five ops, plus NORM's key in one line. 322 of 322 entries.
- **FNBENCH:** gains the five.

## The accuracy (the house bar, 8 units of the 34th digit)

- **tests/fn_table.py** (stdlib decimal, in make test): a "NAME x TRUE" table through build/fn_drive, each answer's
  error in units of TRUE's 34th digit.
  - ERR lines pass only on a refusal.
  - `--control` moves one TRUE by 1,000 units in a copy and must fail. It's in make test too, so the bar is seen
    failing on every run, not once.
- **My own table**, tests/erf-table.txt (scripts/gen_erf_table.py, mpmath pinned at 1.3.0, 200 digits, TRUE to 60), 824
  lines, all within 8. The worst: ERF 0.91, ERFC 1.69 (at 2.33, the blueprint's figure), Φ 2.67, 1−Φ 3.15, Φ⁻¹ 2.00.
  - Its Φ⁻¹ references are checked as they're made: Φ of each gives p back to 1E-150. My first reference was wrong in
    the deep tail (findroot's tolerance is absolute, which a tiny p meets anywhere: Φ⁻¹(1E-6000) came out −162.25, not
    −166.19). It now solves on log Φ.
- **The censuses**, kept in work/097-evidence and run at the shipped code:
  - **Φ⁻¹ over 5,000 p in five shapes** (uniform, 1E-k to 6000, near 0.5, 1 − 1E-k, near the branch points): the worst
    2.64 units, 0 refused.
  - **Φ and 1−Φ over 5,163 z** (revision 1's set, its random tails reseeded): the worst 3.77 and 3.48.
- **What the census found before the code was kept:**
  - 2p − 1 isn't exact for a 34-digit p above 0.5 (501 units off at 0.50081…). The fold moved from 0.9 to 0.5.
  - Newton could cycle in the last digit (a 3-cycle at p = 0.499999996, refused as RANGE). Hence the 4-unit criterion.
  - Newton on erf at p = 0.5 left the range. Hence 0 there.

## Tests

- **tests/097-soroban.txt:** 50 vectors, 65 expectations, plain, through the save/load pass, and sanitized.
  - Textbook values: Φ(1.96), 1−Φ(1.645), Φ⁻¹(0.975), ERF(1), ERFC(1).
  - The tails: 1−Φ(27) = 7.389…E-161 against 1 − Φ(27) = 0 on the stack.
  - Domain errors: Φ⁻¹ of 0, 1, 1.5 and −0.1.
  - STU ONLY in both modes; the ALG keys' lines; the two-argument refusals in ALG and EQN; the CAS refusals by name.
  - Programs and their image.
- **hmark adopted** (abacus #14295): tests/hmark-abacus.txt, 2,448 B, sha256 e6c9ee96…6ff3, in make test.
- **app_test:**
  - NORM opens from PROB with its exact labels; ▸ goes back.
  - Φ acts on 1.96; in ALG it types NCDF(.
  - In 35s mode the sixth key is empty and does nothing.
- **screen_test:** frame 67, NORM's bar under Φ(1.96), read by eye (≈0.9750, Φ as X's op).
- **Moved on purpose:**
  - **app_test's 35s every-menu check** exempts PROB's sixth key, as it does MODE's ALG.
  - **expr_test's 4C pins** (v27, its CRC).
  - **tests/eqn_depth.py**, the parser's depth model, now reads the STU-only range from eqn.c and refuses those names
    outside STU, as the C does. eqn_depth_fuzz found 165 differing equations before it.
  - **The keymap's menu indices** after PROB moved by one (keymap.c regenerated). Every test passed with them
    moved; I didn't search for a reader by number beyond make test.

## Results

- **make test** (c2db894): rc 0. Counting every `n/n vectors` line, repeats included: 225 lines and 21,875 vectors.
  58,862 expectations, counted on the lines that carry both. That's 096's 220 / 21,689 / 58,619, plus hmark's 18 and 24
  (plain, sanitized) and 097's 50 and 65 (plain, save/load, sanitized).
  - fn_table 824/824 and its control.
  - app_test 907/907; help_test 322 entries; screen_test 67/67 frames.
  - standin_replay 51 scripts; keymap_test 3,020/3,020.
  - Evidence: work/097-evidence/make-test.txt.
- **make accuracy:** rc 0, 3:46, peak RSS 1.23 GiB (work/097-evidence/make-accuracy.txt).
- **clang-check:** 76 files, 0 failing, the control refused (work/097-evidence/clang-check.txt).
- **Mutants** (predictions at dc350de, runs 1 and 2 in work/097-evidence): 6 of 11 exactly as predicted.
  - **Exactly:** J0, the control; J1, 1−Φ as 1 − Φ (79 table lines and L01, L06); J3, Newton's last step dropped
    (survived: x within 4 units, still under the bar); J4, Φ⁻¹'s domain line dropped (survived: the start's ln refuses
    the same p); J5, STU ONLY dropped (M01-M03); J9, the correction dropped (125 table lines and the tails).
  - **J6, the CAS name check dropped:** my first spelling didn't compile (nd_name_in unused under -Werror). Respelled,
    the fault unchanged, run 2 gave E05-E07, as predicted.
  - **My misses:**
    - **J2, Φ with Q's sign:** red as predicted, but I didn't list I03, L04 and T03 (each reaches the tail branch or
      the stack's 1 − Φ), and L08 stayed green (the |z| > 200 branch comes before the sign).
    - **J7, the program line's version rule:** P02 and P01 as predicted, plus screen_test's sweep of op lines through
      an image, which I didn't predict.
    - **J8, the message's version rule:** I predicted the save/load pass red; it survived. No test reloaded an image
      with Φ⁻¹'s message showing. P06 now does, and run 2 reddens it.
    - **J10, the fold at 0.9:** I predicted the table red; it survived. My table had no 34-digit p in (0.5, 0.9). The
      table now holds the census's own four, and run 2 reddens one line.
  - **A gap left, by name:** J3's cost shows only in the census's units, not in a test.
- **D68** (`scripts/resources.py static 1ca9364 c2db894`, 096's record commit):

  | | flash | DIRAM | IRAM | calc_task's frame |
  |---|---|---|---|---|
  | the stand-in | +8,112 | +0 | +0 | +0 |
  | Rev A | +7,872 | +0 | +0 | +0 |

  - libbid +4,177 (the blueprint measured +4,173).
  - The first measure, at e7d3d3b, had DIRAM +2,104: Φ⁻¹'s seven hp values were its own statics (about 300 B each).
    c2db894 puts them on fn.c's shared sets with results unchanged, and DIRAM is +0 (the internal-RAM symbols
    unchanged too).
- **The stand-in** (image c2db894, VER 0.98; work/097-evidence/board/):
  - **norm097** from the keys: 1.96, PROB, NORM, Φ, then PROB, NORM, Φ⁻¹ back to 1.96. Both frames are the host's
    (69A4A992, 92E301E0). The stack had 27,388 B free. Only the control is red.
  - **FNBENCH**, 40 arguments each, median / worst:

    | | median | worst |
    |---|---|---|
    | ERF | 0.39 ms | 0.66 ms |
    | ERFC | 0.70 ms | 0.97 ms |
    | Φ | 4.5 ms | 5.1 ms |
    | 1−Φ | 4.5 ms | 5.0 ms |
    | Φ⁻¹ | 41 ms | 42 ms |

    - ERF and ERFC are under the blueprint's 1 ms.
    - Φ and 1−Φ cost the hp correction, which the blueprint didn't time: its 1 ms was the library's erfc alone.
    - Φ⁻¹'s worst came at a tail p (4.35E-187), about nine of Φ's 4.5 ms (41 ÷ 4.5): Newton's steps on Φ.
  - The first run after the flash failed at line 7: the port renumbered (to ttyACM1). It was rerun with FLASH=0. Both
    outputs are kept.
  - The board is left on the stand-in's image at c2db894.
- **Docker:** soroban-* containers, sampled every 2 s.
  - D68 at e7d3d3b peaked at 579.6 MiB (31 samples), at c2db894 at 731.1 MiB (16), the board's build at 748.6 MiB
    (60). No claim: under 4 GiB, --rm, under 30 minutes.
  - The board was claimed 07:26:53-07:31:36Z and released.

## For abacus

- herf-table and herf-v (fd7ade9) to run: `python3 tests/fn_table.py build/fn_drive herf-table.txt` and
  `build/vectors herf-v.txt`.

## After acceptance (abacus #14303)

- **J3 closed.** I09 types a 34-digit p whose Φ⁻¹ the build gives correctly rounded (0.32 units from mpmath). Without
  Newton's last step it's 6.3 units off: still under the table's bar, which is why J3 survived, but caught by I09's
  exact digits. Run 3 (work/097-evidence/mutants-run3.txt) reddens I09 in both passes. J3's spelling was updated for
  c2db894's pointers, the fault unchanged. The p is typed without its leading 0: the entry counts that 0 as one of its
  34 digits, and my first I09 typed a 33-digit p by it.
- **herf adopted** (abacus #14305, #14306):
  - tests/herf-abacus-table.txt from abacus's tests/herfgen.py under mpmath 1.3.0: 180 lines, sha256
    db47211a…eaab (= abacus's). Through fn_table 177/177 within 8; the worst Φ⁻¹ 1.34.
  - tests/herf-abacus-v.txt: 510 B, sha256 8e9e9924…ee14, 3/3.
  - Both are in make test.
- **make test rc 0:** 227 lines, 21,892 vectors (every `n/n vectors` line), 58,887 expectations.
