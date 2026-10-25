# Unit 079: the CPLX note's wording, "2 CPLX ROOTS NOT SHOWN" (blueprint)

**From:** abacus #10707, Michael's ruling on the card cmplx-wording: Casimir's ", n CPLX" (CAS 028) keeps its count, but
"2 CPLX" was unclear. The calculator shows **"SOLVED, 2 CPLX ROOTS NOT SHOWN"**. The firmware rewrites the row as it
draws it, and Casimir doesn't change. A PARTIAL status is already up to 31 characters (`PARTIAL, DEGREE 10 LEFT, 2
CPLX`), so its note moves to a row of its own under the status row. Unit 079 (abacus #10726, which asked for the
census, the no-real-roots case, every reader of the report and the count's bound, below).

## What Casimir prints (its CLI at 4625452, the pin, 16,384 B; the first, fourth and X^6-64 rows the same with `--model 33`)

| equation | casim's line |
|---|---|
| (X-2)×(X^2+1) | `SOLVED, 2 CPLX: X=2` |
| X^6-64 | `SOLVED, 4 CPLX: X=-2; X=2` |
| (X-1)×(X^2+1)^5 | `SOLVED, 10 CPLX: X=1` |
| (X^2+1)×(X^3-2×X-5) | `PARTIAL, DEGREE 3 LEFT, 2 CPLX` |
| (X-3)×(X^2+1)×(X^3-2×X-5) | `PARTIAL, DEGREE 3 LEFT, 2 CPLX: X=3` |
| (X^2+1)×(X^10-2×X-5) | `PARTIAL, DEGREE 10 LEFT, 2 CPLX` |

- The count is the roots left out, two for each quadratic factor with no real root, times its multiplicity. So it is
  even, and "ROOTS" is always plural.
- **The count's bound is solve's degree cap, SOLVE_DEG 12** (casim.c; docs/language.md: "NO_ROOM (degree past 12, …)").
  SOLVED with a note has at least one real root, so its quadratics with no real root take degree 11 at most: the count
  is at most 10. Twelve complex roots fill degree 12, and that is NO REAL ROOTS, which carries no note. PARTIAL keeps a
  factor of degree 3 or more, so its count is at most 8 (casim's solve_test has `PARTIAL, DEGREE 4 LEFT, 8 CPLX`).
  Measured: `(X^2+1)^5×(X^2-2)` is `SOLVED, 10 CPLX: X=-SQRT(2); X=SQRT(2)`; `(X^2+1)^6` is NO REAL ROOTS;
  (X-1)×(X^2+1)^k for k = 6 to 10, 12, 15, 20 and 30, and X^13-1, are NO_ROOM. casim writes at most two digits
  (put_left_out), which the cap never needs past 12. Any count of one or two digits fits SOLVED's row anyway.
- The note never comes with the bit on in STU or 35s, and always comes in model 33. NO REAL ROOTS never has one.

## The census (abacus #10726): every status with CPLX in Casimir at 4625452

Every file at 4625452 that holds "CPLX" was read for status prefixes (the text before the first ": "): the golden corpus
(tests/golden/corpus.txt, decimal.txt, moved/cas-028.txt), tests/solve_test.c, docs/language.md and
docs/canonical-form.md, README.md, the work/ examples and notes (005, 020, 028, the release notes) and the scripts.
There are **8 distinct statuses, and each ends in ", <n> CPLX"**:

| status | where | row 0 | row 1 |
|---|---|---|---|
| `SOLVED, 2 CPLX` | 12 files, 336 times | `SOLVED, 2 CPLX ROOTS NOT SHOWN` | the first root |
| `SOLVED, 4 CPLX` | 7 files, 24 times | `SOLVED, 4 CPLX ROOTS NOT SHOWN` | the first root |
| `SOLVED, 10 CPLX` | work/028-examples.txt, solve_test.c | `SOLVED, 10 CPLX ROOTS NOT SHOWN` (31 cells) | the first root |
| `PARTIAL, DEGREE 3 LEFT, 2 CPLX` | 4 files | `PARTIAL, DEGREE 3 LEFT` | `2 CPLX ROOTS NOT SHOWN` |
| `PARTIAL, DEGREE 4 LEFT, 6 CPLX` | corpus, decimal, moved/cas-028 | `PARTIAL, DEGREE 4 LEFT` | `6 CPLX ROOTS NOT SHOWN` |
| `PARTIAL, DEGREE 4 LEFT, 8 CPLX` | solve_test.c | `PARTIAL, DEGREE 4 LEFT` | `8 CPLX ROOTS NOT SHOWN` |
| `PARTIAL, DEGREE 6 LEFT, 2 CPLX` | solve_test.c | `PARTIAL, DEGREE 6 LEFT` | `2 CPLX ROOTS NOT SHOWN` |
| `PARTIAL, DEGREE 10 LEFT, 2 CPLX` | 5 files | `PARTIAL, DEGREE 10 LEFT` | `2 CPLX ROOTS NOT SHOWN` |

- **None has anything after CPLX, and none can.** The one producer is casim.c's put_left_out, which builds ", " plus one
  or two digits plus " CPLX", and inserts it before the line's first ':'. So the note always ends the status.
- The only other "CPLX" text is the firmware's own hint, "NO REAL ROOTS. CAS3 CPLX ON", quoted in casim's
  work/028 notes. It has no ", <n> CPLX", so the rule leaves it alone, as it should. The rest are prose templates
  (", n CPLX") and code, not statuses.
- Two passes: a pattern for status prefixes, and a looser 32-character window around every "CPLX", sorted and counted.
  They give the same 8.

## No real roots (abacus #10726)

- Casimir prints `NO REAL ROOTS` with no note for `X^2+1`, `(X^2+1)×(X^2+4)` and `(X^2+1)^6`, in STU and in model 33
  alike (CLI 4625452, 16,384 B). CAS 028 gives NO REAL ROOTS no note.
- **So only one rewrite can apply, never both.** 076's hint rewrites the stored line at the call (core/calc.c: STU, CPLX
  off, not 33-stamped, the line exactly `NO REAL ROOTS`) to `NO REAL ROOTS. CAS3 CPLX ON`. That has no ", <n> CPLX", so
  this unit's row rule leaves it as it is. A line with the note is never exactly `NO REAL ROOTS`, so the hint never
  fires on one.
- **The oracle row:** `(X^2+1)×(X^2+4)`, STU, CPLX off: `%CASREP` and `%CASROW` are both `NO_REAL_ROOTS._CAS3_CPLX_ON`,
  and the report has one row, so SST stays on it. The screen's X row reads `NO REAL ROOTS. CAS3 CPLX ON` (27 cells),
  and the three rows above it are blank. Stamped 33s, it's plain `NO REAL ROOTS`.

## Every reader of the report (abacus #10726)

From `git grep` of cas_rep, ab_cas_report, ab_cas_row(s) and cas_row over the whole repo at 9575871, work/ aside:

| reader | what it does | after 079 |
|---|---|---|
| firmware/screen.c, the report view | draws rows cas_row−3..cas_row | **goes through ab_cas_row**: the student's screen |
| core/calc.c casrep_key, SST/BST | counts rows with ab_cas_rows | **through ab_cas_row**: the note's row is counted |
| core/calc.c casrep_key, ENTER | evaluates the row's text after "=" | **through ab_cas_row**: the note's row has no "=", so nothing happens |
| core/calc.c, the store (SOLVE's result) | writes casim's line, or 076's hint | the raw line, on purpose: the single source the rows are built from |
| firmware/app.c, the menu name | `ab_cas_report() != NULL` gives the soft keys "CAS REPORT" | truthiness only; shows no text |
| device/dev_common.h, the board's `eqn:` line | `rep=` the raw line | **raw on purpose**: the bench's serial instrument, never on the LCD. 079 adds `row=`, through ab_cas_row |
| tests/vectors.c | `%CASREP` raw, `%CASROW` through ab_cas_row | raw on purpose (the test instrument); `%CASROW` is the student's row |
| tests/app_test.c | `ab_cas_report()` as open or closed only | truthiness only |

- No history, worksheet, store, recall or copy path reads cas_rep. ENTER puts a root's value in X, never the line, and
  the report isn't in the state image (calc.h), so neither SAVE nor LOAD sees it.
- **Not checked from here:** stu32-tutor, the primer and tally's Android app aren't checked out on lepton. If any of
  them reads `ab_cas_report()` and shows the text, rather than drawing through screen.c or ab_cas_row, it would show
  the raw "2 CPLX". Each needs one grep for `ab_cas_report` where it's checked out.

## The rows the report will show

| casim's line | row 0 (status) | row 1 | then |
|---|---|---|---|
| `SOLVED, 2 CPLX: X=2` | `SOLVED, 2 CPLX ROOTS NOT SHOWN` (30 cells) | `X=2` | |
| `SOLVED, 10 CPLX: X=1` | `SOLVED, 10 CPLX ROOTS NOT SHOWN` (31, the row's room) | `X=1` | |
| `PARTIAL, DEGREE 3 LEFT, 2 CPLX` | `PARTIAL, DEGREE 3 LEFT` | `2 CPLX ROOTS NOT SHOWN` (22) | |
| `PARTIAL, DEGREE 3 LEFT, 2 CPLX: X=3` | `PARTIAL, DEGREE 3 LEFT` | `2 CPLX ROOTS NOT SHOWN` | `X=3` |
| any line with no ", n CPLX" | as now | as now | as now |

- **The rule:** the status is the text before the first ": ". When it ends in ", <digits> CPLX", the note is
  "<digits> CPLX ROOTS NOT SHOWN". If what is left is `SOLVED`, the note stays on the status row, as Michael wrote it.
  Otherwise (PARTIAL) the status row is what is left, and the note is a row of its own under it.
- **The room:** the report's rows are 31 cells (firmware/screen.c: scale 2, (398 − 20 + 2) ÷ 12). SOLVED's row fits
  for any count of one or two digits. A three-digit count would be 32 cells and drawn with an ellipsis, but none was
  reachable at 16,384 B above.
- SST and BST step through every row, the note's included, wrapping as now. ENTER on the note's row does nothing, as
  on the status row: casrep_key already needs an "=" in the row. C, backspace and any other key leave, as now.

## Where: the row, not the stored line

- **core/calc.c, ab_cas_row (and ab_cas_rows, which counts through it):** it does the rewrite on the way out. cas_rep
  keeps Casimir's line byte for byte, so `%CASREP`, the board's `rep=` and 076's exact match on NO REAL ROOTS all stay
  as they are.
- **Why not rewrite the stored line, as 076's hint does:** cas_rep is AB_EQN_LEN, 192 B, and Casimir's line may be up
  to 191 B with the note. The rewrite adds up to 16 B (" ROOTS NOT SHOWN"), so a long report would be cut, or would
  need a larger buffer in internal RAM. Rewriting the row needs no storage: each row is built into the caller's buffer
  as it is drawn.
- cas_rep isn't in the state image (calc.h), so the image's version doesn't move.
- **The board's view of it, device/dev_common.h:** the `eqn:` line gains ` | row=<the row under the cursor>` while
  the report is open, and `-` otherwise. It goes at the end, so every existing expect (`eqn: … | note=…`,
  `rep=…`) still matches its text.

## The oracle

**Rows that change, as ruled:**
- 076 C21: `%CASROW=SOLVED,_2_CPLX` becomes `SOLVED,_2_CPLX_ROOTS_NOT_SHOWN`.

**Rows that stay:** 048 S13 and 076 C05, C16 and C23 read `%CASREP`, Casimir's line, and are unchanged.

**New rows (076, or the unit's own file), each run at e56e311 before the change and red there:**
1. SOLVED's status row: `(X-2)×(X^2+1)`, CPLX off: `%CASROW=SOLVED,_2_CPLX_ROOTS_NOT_SHOWN`.
2. SOLVED's roots don't move: the same equation with SST gives `%CASROW=X=2`, and SST ENTER puts 2 in X. Both pass at
   e56e311 too; they guard against the rewrite shifting the rows.
3. SOLVED at the room: `(X-1)×(X^2+1)^5` gives `%CASROW=SOLVED,_10_CPLX_ROOTS_NOT_SHOWN`. A screen check (standin_test,
   or app_test's draw) shows all 31 cells with no ellipsis.
4. PARTIAL's status row: `(X^2+1)×(X^3-2×X-5)` gives `%CASROW=PARTIAL,_DEGREE_3_LEFT`.
5. PARTIAL's note row: the same with SST gives `%CASROW=2_CPLX_ROOTS_NOT_SHOWN`. SST SST wraps to the status row,
   because the report has 2 rows.
6. ENTER on the note's row does nothing: SST ENTER leaves X as it was and the report open (`%CASROW` the note).
7. PARTIAL with a root: `(X-3)×(X^2+1)×(X^3-2×X-5)`, SST SST gives `%CASROW=X=3`, and SST SST ENTER puts 3 in X.
8. A 33s-stamped equation with CPLX on gets the same rewrite (C23's equation): `%CASROW=SOLVED,_2_CPLX_ROOTS_NOT_SHOWN`.
9. No real roots: `(X^2+1)×(X^2+4)`, STU, CPLX off: `%CASROW=NO_REAL_ROOTS._CAS3_CPLX_ON`, the same after SST (one
   row), and `%CASREP` the same. Passes at e56e311 too: it guards the two rewrites against each other.
10. Nothing else moves: NO REAL ROOTS. CAS3 CPLX ON (C01), CPLX on in STU (C04, no note) and every existing
   `%CASREP` row. The full make test stays green.

**The board (device/standin):** repin-rc7.txt's CAS 028 step adds `expect row=SOLVED, 2 CPLX ROOTS NOT SHOWN` beside
its `rep=` line. A new step SOLVEs (X^2+1)×(X^3-2×X-5) from the keys and expects `row=PARTIAL, DEGREE 3 LEFT`, then
after SST `row=2 CPLX ROOTS NOT SHOWN`. Each run has a control that must fail.

**Mutants (each must turn a row red):** the note left on PARTIAL's status row; the note dropped; the note's row
counted as a root (ENTER on it acts); the root rows shifted by one for SOLVED; the rewrite applied to a status with no
", n CPLX".

**Decision 68:** a few hundred bytes of flash, with internal RAM +0 expected. Measured at the gate.

## Not in this unit

- Casimir's text, the NO REAL ROOTS hint, the help pages. SOLVE's page already says "Complex ones: CAS3 CPLX on.", and
  that still reads true.
- tutor, primer and tally get the change through the core's rows when they next pin the firmware.

## The gate (soroban, 2026-10-25; abacus OK'd the blueprint at 9b1e206, #10744)

- **The code:** 9ef2cb5. core/calc.c gains cas_cplx_at, and ab_cas_row rewrites the row; device/dev_common.h's `eqn:`
  line gains `row=`. tests/079-soroban.txt has W01-W17; 076 C21 moves as ruled; screen_test gets two 079 checks; the
  Makefile runs 079 after 076. 9f74728 adds the board steps to device/standin/repin-rc7.txt.
- **make test** rc 0 at the unit, peak RSS 170,792 KB: 048 65/65, 076 23/23, 079 17/17 (23/23), app_test 779/779,
  standin_test 349/349, screen_test 9004/9004 and 21/21 frames, help_test 305 of 305.
- **The oracle at e56e311** (these tests on the old code, in a scratch worktree):
  - 079 is 8/17: W01, W04, W06-W11 and W13 are red. W02, W03, W05, W12 and W14-W17 pass at both, as the guards they
    are: roots not moved, NO REAL ROOTS's hint, no note with CPLX on, a status with no note.
  - 076 is 22/23, with C21 red. screen_test is 9002/9004, with both 079 checks red.
  - The worktree's first screen_test said 0/21 frames: a fresh checkout has no build/frames, so the PNGs weren't
    written. With the directory made, e56e311's own screen_test matches 21/21. That was the rig, not the code.
- **The five mutants, each red** (079 / 076 / 048 / screen_test):
  - M1, the note left on PARTIAL's status row: 11/17 (W06-W11) / 23 / 65 / 9003.
  - M2, the note dropped: 10/17 (W01, W04, W07, W09-W11, W13) / 22 (C21) / 65 / 9002.
  - M3, the note's row read as a root (ENTER on it leaves the report): 16/17 (W09) / 23 / 65 / 9004.
  - M4, the roots shifted by one: 16/17 (W05) / 23 / 65 / 9004.
  - M5, the rewrite applied to a status with no note: 14/17 (W14, W15, W17) / 22 / 60 of 65 / 9004.
  - M3 and M4 each turn one row red. W09 and W05 are the rows that guard them.
- **Decision 68, `scripts/resources.py static e56e311 9f74728`:** flash +400 on the stand-in and +400 on Rev A;
  internal RAM +0, IRAM +0, calc_task's frame +0, no internal-RAM symbol changed.
- **The stand-in at 9f74728:** ver=0.78, core=9f74728.
  - repin-rc7.txt: 16 of 16 met, with the control (its line 678) red. From the keys:
    `rep=SOLVED, 2 CPLX: X=2 | row=SOLVED, 2 CPLX ROOTS NOT SHOWN`;
    `rep=PARTIAL, DEGREE 3 LEFT, 2 CPLX | row=PARTIAL, DEGREE 3 LEFT`; then, after SST, `row=2 CPLX ROOTS NOT SHOWN`.
    stack-free 23,732 B (floor 8,188).
  - Both new key sequences were replayed through app.c on the host first, after the step before them, to these rows.
  - casrows.txt: ALL MET. make clang-check: 66 files, 0 failing.
  - The port moved at the flash (ACM1 to ACM3); the rerun with no flash passed.
  - Claim #10755, released, peak 566.8 MiB.
- **Accepted (#10788, abacus bb17416)** from a clean clone at 685923d: make test rc 0, 079 17/17, 076 23/23,
  screen_test 9004/9004 and 21/21 frames, app_test 779/779. Its control (e56e311's code with these tests) gave 079 8/17
  and 076 22/23, as measured here.
  - **abacus's own plant**, SOLVED read as PARTIAL (`d == 8` → `d == 9`): 079 11/17 (W01-W05, W13), 076 21/23 (C21 and
    C22, W03's twin), 048 65/65. tally has the GO for the APK.
