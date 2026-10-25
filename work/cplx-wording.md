# The CPLX note's wording: "2 CPLX ROOTS NOT SHOWN" (blueprint)

**From:** abacus #10707, Michael's ruling on the card cmplx-wording: Casimir's ", n CPLX" (CAS 028) keeps its count, but
"2 CPLX" was unclear. The calculator shows **"SOLVED, 2 CPLX ROOTS NOT SHOWN"**. The firmware rewrites the row as it
draws it, and Casimir doesn't change. A PARTIAL status is already up to 31 characters (`PARTIAL, DEGREE 10 LEFT, 2
CPLX`), so its note moves to a row of its own under the status row. The unit number is abacus's to give.

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
- (X-1)×(X^2+1)^k is NO_ROOM for k = 6 to 10, 12, 15, 20 and 30, so 10 is the largest count measured here.
- The note never comes with the bit on in STU or 35s, and always comes in model 33. NO REAL ROOTS never has one.

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
9. Nothing else moves: NO REAL ROOTS. CAS3 CPLX ON (C01), CPLX on in STU (C04, no note) and every existing
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
