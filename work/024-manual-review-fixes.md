# Work unit 024: fixes from the 33s/32SII manual review

**From:** abacus · **To:** soroban · **Status:** ready. It is independent of 023; do whichever you like first.

## Why

Michael asked for the core to be reviewed against the HP 33s User's Guide and the 32SII Owner's Manual, which the
units had not been written from. Five reviewers wrote 799 vectors from the two manuals; 680 pass. The full findings
and every vector are in abacus docs/evidence/2026-10-06-manual-review-33s-32sii/. Most failures are divergences
between the machines, which wait for Michael's rulings. This unit holds only the ones that are bugs on any reading.
Each was reproduced by abacus on the core at 02ddee9.

## Rules

1. **33s mode: C and ← after an error message only clear it** (an amendment to unit 017 rule 4).
   - 33s p.1-23 and F-1, and 32SII p.1-19 and E-1, say C and ← clear the message and do nothing else; any other key
     clears it and then acts. Rule 4 said "any key" and asked to be told if the guide read otherwise, and it does,
     for these two keys.
   - Today `ab_do_arg` (calc.c:3110-3113) clears the message and passes the key on, so
     `7 ENTER 2 CHS SQRT C` leaves X = 0 where it should be −2.
   - Treat C and ← there the way `ab_view_key` treats them after VIEW.
   - The 35s mode is already right (every key only clears).
   - Michael's digit test (`1 ENTER 0 ÷ 5` gives X 5, Y 0, Z 1) must still pass.
2. **SOLVE's NO ROOT after a math error while bracketing reports the last defined point.**
   - 33s p.7-6 and p.D-12: X is the final estimate, and Z is f there.
   - Today, at calc.c:1955 (`if (bad) goto noroot_bracket`), `b` has already moved to the point where f failed.
     The report then puts that point in X (−0.145 for √(x/(x+0.3)) − 0.5 from 0 and −10, where the 33s shows 0),
     and Z belongs to another point.
   - Restore `b` and `fb` to the last defined point before reporting. Check the other `bad` exits for the same
     shape.
   - Make sure "f at the reported X is Z" holds on every NO ROOT path.
3. **A base-mode overflow says OVERFLOW.**
   - All three manuals show OVERFLOW briefly, with the largest number kept (33s p.10-3, 32SII p.10-3, 35s p.11-5).
     TOO BIG is their word for a decimal too big to show in a base, and that one is still ours to decide (unit 008).
   - Today ABN_CLAMPED maps to "TOO BIG" (msg.c:16).
   - Also check that the device shows the clamped status at all: ABN_CLAMPED sets no message (calc.c:3102-3125),
     and a reviewer suspects the screen never shows it. If it doesn't, make it show the way the guide's "briefly"
     reads, and say how long.
4. **Equation syntax is checked after the prompts are answered.**
   - 33s p.6-18 and 32SII p.6-20 say so; the 35s is silent.
   - Today `EQ:A×(B+ XEQ` gives SYNTAX ERROR at once (calc.c:2845, 2930), without asking for A.
   - If you find a reason the early check matters (a prompt for a variable that a broken equation never reaches),
     say so instead of changing it.
5. **Investigate, then report:** NO ROOT for a minimum well above zero.
   - x² − 6x + 13 from 0 and 10 reports X 2.83, Y −3.50, Z 4.03. The 33s (p.D-10) and 32SII (p.C-10) show X and Y
     about 3, and Z 4.
   - Unit 013 made part of the report's content ours, so this may be by design.
   - Tell me what our search does on a function that never crosses zero, and whether X and Y should bracket the
     minimum. Change nothing until we agree.

## Acceptance

- The reviewers' failing vectors for these become passing vectors in work/024-vectors.txt:
  - rule 1: K01, K02, K05, K06;
  - rule 2: Q16, Q16b;
  - rule 3: B17;
  - rule 4: B14.
- Copy them from abacus docs/evidence/2026-10-06-manual-review-33s-32sii/{stack,eqnsolve,cxbasestat}/vectors.txt.
  They carry their manual pages.
- Every vector the reviewers wrote that passes today still passes. Their files are the regression set: run all five
  folders before and after, and report the counts.
- Red first, then green. Mutants, each caught:
  - C still acting in 33s mode;
  - the NO ROOT point taken one step late;
  - the overflow text reverting to TOO BIG.
