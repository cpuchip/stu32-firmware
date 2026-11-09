# Unit 096 built: the exact-or-rounded mark (decision 95's first)

**From:** the blueprint work/mark-exact-rounded.md (measured e7d1ef6, 3286a4a; revision 1 08d9531; grammar 84a314d, c4aaff6;
the two INFO texts found in the build added in place), abacus #14264 (the rulings), #14270 (the grammar asked), #14272
(accepted; hmark frozen at abacus e6f89ac; build to look A; rulings (a) ROUNDED first, (b) a number out of CAS's report is
rounded), #14280 (ruled A: T_ROUNDED always in STU).
- **The build:**
  - 358da4f: the source.
  - ff72522: the mutants' predictions, before any run.
  - ccffbbc: run 1, D68, and R5's fault respelled (it didn't compile).
  - This commit: run 2 (R5), the board script and its run, this record.
- **VER 0.97:** 096 plus the out-of-order 092 (work/version-extra.txt).
- **Also, a4fd932:** erf's evidence that its revision 1 (32ad875) cites and left out (phifix.c, phijudge2.py,
  phifix-judge.out). Found while staging this unit.

## What was built

- **The value's bit** (ab_val.rounded, in padding: ab_val stays 64 B).
  - **The engine's flag:** accept() notes the library's INEXACT, and the core's hp and xnum paths round through it.
  - **The core's own roundings** note it too:
    - an overflow's largest number (note_overflow) and an underflow's 0 (settle);
    - fn.c's shortcuts that return a constant or x itself (tiny x, TANH at |x| ≥ 100, ATAN at ≥ 1E40, e^x → −1, the
      power's |1/x| cases, RMDR's, COS below the range);
    - **every sure() function at a nonzero x and result:** e^x, ln, sin, cos, tan, the hyperbolics and their inverses,
      and →DEG/→RAD are transcendental there (Lindemann–Weierstrass). The library's cos(1E-35) is 1 with no inexact flag,
      and hp's 1 − 5E-71 rounds to 1 exactly. 10^x and LOG are left to the flag: they're exact at integers and powers
      of ten.
  - **An untracked op is rounded** (revision 1's rule): mark_tracked names the ops whose path the flag covers; every
    other op's result is rounded. That covers π, the statistics, RAND, CAS on X, and a constant off the exact list.
  - **The back-checks** prove exact:
    - a root of index 1 to 99, and POW with exponent 1/n: rⁿ by abn_mul_exact equals the base;
    - inverse trig in DEG or GRAD answered from its exact angles (0, ±0.5, ±1 only). The angle restores an
      expression's earlier rounding rather than clearing it: in ALG, 1÷3+ASIN(0.5) stays rounded.
    - 0's are exact: COS 0 and COSH 0 return 1 before the library. The census showed both marked rounded; I didn't
      trace which rounding reported it.
- **Carried:**
  - the stack, LASTx and ANS; STO and RCL; x<>v; STO and RCL arithmetic;
  - an indirect register holds no bit, so its RCL is rounded;
  - the rows: flag 0x08 in RAM, stripped in the image, which carries T_ROUNDED instead;
  - ALG's line (the result goes to ANS and its row, the stack untouched) and EQN's evaluation and A=expr, through
    eqn.c: a rounded letter, ANS or REGX read, or an untracked op run, marks the line;
  - a typed fraction (1 1/3) from its parse. A typed decimal can't round: the entry takes no 35th digit.
  - **Runs and solvers:** the stack and LASTx a program, SOLVE or ∫ leave are rounded; a letter written during a run
    or a solve is rounded (store_val); SOLVE's root in its letter is rounded, the interrupted estimate too; the linear
    solver's X, Y, Z are rounded.
  - **094's report (ruling (b)):** ENTER's copy and EVAL's value out of it are rounded.
- **Constants (revision 1's list).**
  - Exact, stored whole by definition: c, g, NA, qe, k, h, R (NA·k), F (NA·qe), t, atm.
  - Rounded: π, e, the measured ones (G, Rinf, me, mp, mn, mmu, a0, eps0, u, mu0, muB, muN, mup, mue, mun, mumu, re,
    Z0, lambdac, lambdacn, lambdacp, alpha, gammap), and those exact by definition but cut to 34 digits (Vm, hbar,
    phi0, sigma, C1, C2, G0).
- **The image:** T_ROUNDED (tag 25, version 25), always written in STU (ruled A, #14280).
  - **Contents:** X Y Z T LASTx ANS as six bits, the 27 letters, then the serials of the kept rows that are rounded.
  - **An STU image without it** is older than the mark, so its values and rows load rounded.
  - **082's rule amended where it's stated** (state.h): an STU image's content always needs 25; a 33s or 35s image is
    as before.
- **The screen (look A):** ≈ before X's number on STU's home, fitted as one more cell of the line, when the value is
  rounded or the display drops a nonzero digit of it.
  - **Shown short** is decided in screen.c with the line's own display: each part formatted as the levels format it,
    read back (the radix as the point, fmt.c's grouping mark dropped), and compared. A near fraction (its indicator) is
    short.
  - **Not drawn** in the 33s and 35s modes, in ALG, on the row view, or anywhere but STU's home.
  - **The font's ≈:** two waves, U+2248.
- **INFO (ROWINFO):** after 093's text, `, ROUNDED` (the row's bit), then `, SHOWN TO n PLACES` (FIX n),
  `, SHOWN TO m DIGITS` (SCI or ENG n, m = n + 1), `, SHOWN AS A NEAR FRACTION` or `, SHOWN SHORT` (ALL past the
  width). Ruling (a) puts ROUNDED first. The blueprint's older example `+ OF 1.0000, 0.3333, ROUNDED` is superseded
  and struck in place.
- **HELP:** INFO's entry says ROUNDED, SHOWN TO n PLACES and ≈. help_test: 316 of 316 entries.
- **Dumps:**
  - %ROUNDED-L (L = X, Y, Z, T)
  - %ROWROUNDED-k
  - %XMARK (≈ or _)

## The census, kept as a test

- **The table:** tests/mark-table.txt (scripts/gen_mark_table.py; mpmath pinned at 1.3.0, which the script checks;
  sha256 226b3999c4b511b9dedbfe554e84dd137c171abf87c4e8c25556b0d90a292be5, in its header).
  - **7,203 cases**, "OP MODE x [y]", the blueprint's chosen arguments plus 28 random (seed 95), for 29 one-number ops
    in DEG (the trig and inverse trig in RAD too) and 9 two-number ops.
  - **The verdict:** E with the exact decimal when the true value has at most 34 significant digits (checked at 300
    digits and again at 1,500), else R.
  - **TANH past |x| = 1,500** is R by the function (1 − |tanh x| = 2/(e^2|x| + 1) > 0): no precision mpmath can carry
    sees the difference.
- **The judge:** tests/mark_table.py (stdlib) runs every case through build/mark_drive (the core in STU, X and Y set,
  the op by ab_do).
  - A false exact is a hard fail: marked exact where the verdict is R, or where the core's value isn't the true one.
  - False rounded is held to bounds per op: XROOT 60 and POW 8 (the index or exponent isn't a whole number's
    reciprocal, as scoped), 0 for every other op.
  - Its controls: a tampered table fails its hash; a verdict flipped to R where the core marks exact fails as a false
    exact.
- **At the build:** 6,648 marked right, 68 false rounded (XROOT 60, POW 8), **0 false exact**, 487 refused, every one Cn,r
  and DOMAIN: 242 with r > n, 221 with a non-integer argument, 24 with a negative one (the core's Cn,r domain,
  which mpmath extends through gamma).
- **Its first runs found, and the build fixed:**
  - 14 overflow clamps (SINH, COSH, FACT, POW, XROOT past the range);
  - COS of 1E-35 in RAD marked exact, and COS 0 and COSH 0 marked rounded;
  - a judge of my own that couldn't see tanh(272) ≠ 1 at 300 digits, now checked at 1,500.

## Tests

- **tests/096-soroban.txt:** 69 vectors, 120 expectations, plain and sanitized.
  - **No save/load pass:** INFO's row view is RAM, and the image cases SAVE and LOAD themselves.
  - **Its fixtures:**
    - tests/mark-fixture-v25.img, saved by this build.
    - tests/mark-fixture-v24.img, saved by the build before (c4aaff6, VER 0.96). The 0.96 → 0.97 update loads it with
      every value rounded (#14280).
- **The older-build proof:** the build before (c4aaff6, from a git worktree) loads mark-fixture-v25.img with OLDER
  FIRMWARE showing and every value as saved (work/096-evidence/older-build.txt). It's a one-time run, not in make
  test: make test can't build an older tree.
- **Moved on purpose, each with its reason:**
  - **Image-version pins**, because an STU image is 25 now (A):
    - rows_test: 4 checks, plus a new 35s check that stays below 25;
    - expr_test: 12. golden-v20-082, golden-v20-082-full, golden-v21-082 and the repin's golden-v21-087-div0 now
      load with their values marked rounded and re-save at 25. With T_ROUNDED taken out and the old version put back,
      each equals its file byte for byte (`unmark`), so nothing else moved.
    - store_test: 1.
  - **tests/golden-v25-4c.img is now golden-v26-4c.img** (scripts/mk_golden_newer.py), still newer than this build, as
    090 moved it from 24 to 25.
  - **DROPSEC:25 beside DROPSEC:24** in the six vectors that relabel an STU image older: 056 D55, 057 L09, 070 S49-S51,
    cas004 K26.
  - **Frames:** 49, 50, 59, 60 and 62 draw ≈ before X's rounded value (read by eye). cas088's ENTER frame draws ≈
    before the root copied out of the report (ruling (b)): A98892E5 → EEC9832B.
- **load_fuzz, sanitized, found an older hole.** Its seeds changed (the 4C seed, and STU images with T_ROUNDED), and
  it reached an image whose pending equation prompt names something other than a letter (stop_var −1). R/S then
  wrote var[−1]. That's code from before 096 (prog_resume, and R/S at an equation's prompt). Both writes are guarded
  now; red before the guard, green after.

## Results

- **make test** (358da4f, with ccffbbc's evidence): rc 0. Counting every `n/n vectors` line, repeats included: 220
  lines and 21,689 vectors. 58,619 expectations, counted on the lines that carry both, as 095 counted them.
  - Against 095's record (217 lines, 21,539 vectors, 58,352): h095's 12 vectors and 27 expectations, adopted after it,
    and 096's 69 and 120, plain and sanitized.
  - Within it:
    - mark_table 0 false exact;
    - rows_test 139/139;
    - expr_test 430/430;
    - screen_test 66/66 frames, 9,165 checks;
    - help_test 316/316 entries;
    - standin_replay 49 scripts;
    - load_fuzz 0 violations.
  - Evidence: work/096-evidence/make-test.txt.
- **make accuracy:** rc 0, 3:53, peak RSS 1.23 GiB (work/096-evidence/make-accuracy.txt).
- **clang-check:** 76 files (mark_drive.c new), 0 failing, the control refused (work/096-evidence/clang-check.txt).
- **Mutants** (predictions at ff72522 before any run; runs 1 and 2 in work/096-evidence): 12 of 15 exactly as
  predicted.
  - **Exactly:**
    - R0, the control: green.
    - R2, the clamp unmarked: 14 false exact.
    - R3, the flush unmarked: survived (below).
    - R4, TANH's shortcut: 17 and K12.
    - R5, the back-check not comparing: K08b only, census 0 (run 2).
    - R6, a near angle within 1E-9 taking the table: 26 (ASIN 9, ACOS 8, ATAN 9) and K10.
    - R8, the transcendental rule: 2 and K14.
    - R9, a run's letter: U02.
    - R11, an older image's rule: I03, I04 and three expr_test checks.
    - R12, the indirect RCL: C09.
    - R13, a typed fraction: K18.
    - R14, ruling (b): R01, R02 and cas088's frame.
  - **My misses:**
    - **R1, the flag not carried** (1,834 false exact). I predicted frames 49, 50, 59, 60 and 62 and D03 red; they
      stayed green. Look A's display half still draws ≈ there, because those values are shown short whatever their bit.
    - **R7, the mark outside STU:** D05 and frames 08, 10 and 12, as predicted, but I also predicted replay frames,
      and none went red.
    - **R10, T_ROUNDED unread:** I01, I05 and the round trip, as predicted, plus a second expr_test check and
      page090's replay frame, which I didn't predict: an image loaded back draws ≈ on its exact X.
  - **R5's first spelling** (`return true;`) didn't compile (base unused under -Werror). It was respelled
    `(void)base, true`, the fault unchanged, before run 2.
  - **R3 survives:** no test reaches a core flush without the library's inexact flag. The census has no case that
    underflows to 0, and K16's product underflows inside the library, which flags it. The note is kept for the core's
    own flushes. That's a named gap, not a test.
- **D68** (`scripts/resources.py static a4fd932 358da4f`, the commit before the src):

  | | flash | DIRAM | IRAM | calc_task's frame |
  |---|---|---|---|---|
  | the stand-in | +5,728 | +0 | +0 | +0 |
  | Rev A | +5,808 | +0 | +0 | +0 |

  The two one-byte flags (rounded_seen, exact_hit) sit in alignment: the internal-RAM symbols +2, the section +0.
- **The stand-in** (image ccffbbc, VER 0.97; work/096-evidence/board/): mark096, 10 PASS; only the control is red.
  - The four frames are the host's: ≈1.4142 (E25B3087); 2 with no mark (241167AD); the same after SAVE and RESTART,
    so the device's save keeps the bits; 0.1250 at FIX 4 with no mark (403D7A07).
  - The calc task's stack had 27,548 B free.
  - The first run, straight after the flash, failed at line 8: the port renumbered (ttyACM1 → ttyACM3). It was rerun
    with FLASH=0. Both outputs are kept.
  - The board is left on the stand-in's image at ccffbbc.
- **Docker** (D68's builds and the board's): soroban-* containers, sampled every 2 s. D68 peaked at 593.7 MiB (32
  samples), the stand-in's build at 725 MiB (74 samples). No claim (under 4 GiB, --rm, under 30 minutes). The board was
  claimed 06:35:40-06:41:24Z and released.

## For abacus

- hmark (hmark.txt and hmark-A.txt, abacus e6f89ac) to run. Only A's file applies until Michael picks.
- The two INFO texts found in the build are stated in the grammar (#14280).
