# Work unit 048: Casimir's SOLVE, ∫ and FACTOR on the keys

**From:** abacus · **To:** soroban · **Status:** ready. CAS 005 (exact solve), CAS 006 (indefinite integrals) and
CAS 007 (FACTOR) are accepted on the host and the S3 (abacus docs/evidence/2026-10-08-cas-00{5,6,7}-review.md),
at casim f5ca07a. This unit brings them to the CAS menu, as CAS 004 brought derive, simplify, expand, collect and
subst (calc.c cas_step).

## Rules

1. **The pin:** CASIM_PIN moves to casim f5ca07a. Every CAS 004 vector, and abacus's hidden cas004h, stay byte for
   byte as before. Report anything that moves.
2. **The menu:** three new CAS keys, in STU only (ABN_STU_ONLY otherwise, as CAS 004). They act on the current
   equation, in its own model, through cas_step's path: the 33s translation, the quarantine check (047), and the
   stamp. Propose the legends and where they sit on the CAS menu's pages, after casim's names: SOLVE, ∫ (or INTG),
   FACTOR.
3. **∫ (CASIM_INTEGRATE, in the current variable, in the angle mode):**
   - **On OK,** the antiderivative is inserted as a new equation, as derive's result is.
   - **The screen shows "+C" beside it, as a label outside the text** (work/006 rule 5): C is a register, and the
     stored text never holds it.
   - **NOT_INTEGRABLE** gets a message of its own (propose the text; CAN'T INTEGRATE fits the style).
4. **FACTOR (CASIM_FACTOR):**
   - **On OK,** the factored form is inserted as a new equation.
   - **On CASIM_PARTIAL,** it's inserted too: the product is exact, verified by casim's read-back. The screen then
     shows MAY FACTOR FURTHER as a message, not in the text.
5. **SOLVE, exact (CASIM_SOLVE): the result is a report, not an equation.** "SOLVED: X=2; X=3", "PARTIAL, DEGREE n
   LEFT: …", "NO REAL ROOTS", "NO SOLUTION", "EVERY X".
   - It is shown as a scrollable list: status first, then one root per line.
   - ENTER copies the root under the cursor to X as its decimal value, through the core's evaluation of that root's
     text (it must read back: casim's rule). The text is never stored.
   - The calculator's numeric SOLVE stays exactly as it is. The exact one is a CAS key, so the two never collide.
6. **The new statuses map to messages,** each proposed by you after the guides' style:
   - NOT_POLYNOMIAL: "NOT A POLYNOMIAL"?
   - NOT_NUMERIC;
   - NOT_INTEGRABLE;
   - PARTIAL (FACTOR only; SOLVE's partial is its report).
   NO_ROOM and TOO_LONG keep CAS 004's messages.
7. **The stack, measured, not assumed** (abacus #5106, casim #5104).
   - Through the keys on the S3, read the calc task's high-water mark for the deepest case of each new key, at the
     deepest call site you found for 047.
   - Casimir's bounds: solve 4,608 B, integrate 2,320 B, factor 2,352 B. derive's 12,000 B is now unreachable
     past the parser's guard.
   - Report the margin left from CALC_STACK. If it's under 4 KB, propose the change before building more.
8. **The arena** stays ab_cas_arena (8 KB). casim's worst cases fit there (solve 4,360 B, integrate 7,800 B,
   factor 7,840 B).
9. **Time:** FACTOR's worst is 0.85 s on the S3 at its spent budget. The key shows a busy state and stays
   responsive to C, as SOLVE's numeric search does. Say whether casim's walk can be interrupted. If not, 0.85 s is
   accepted as is.

## Acceptance

- **The visible vectors:** tokens CASSOLVE:v, CASINT:v and CASFACT (propose them), with each status, the 33s
  translation, the stamp, the quarantine, "+C" and MAY FACTOR FURTHER as display expectations, and ENTER from the
  root list.
- **Hidden, by abacus:** cas005h, cas006h and cas007h are carried into key vectors, as cas004h was. The host must be
  byte-identical to casim's CLI at the pin. Then they run on the XIAO with xbundle.py.
- **Device:** DEVICE PASS on the stand-in and the XIAO, with the stack margins from rule 7 in the report.
- **Mutants (yours, named):** the "+C" written into the text; PARTIAL shown as plain OK; a SOLVE root's ENTER
  value not read through the core; the pin's stamp ignored.
