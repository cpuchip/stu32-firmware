# Repin: casimir 2ba7217 (CAS 019, fractions) and the CAS arena 12,288 B

**From:** abacus #8796 (CAS 019 accepted at 2ba7217), #8809 (the arena with it, one repin unit; casim's bench
8d28f58, work/arena-12288.txt) · Michael: "bump the CAS memory up" (abacus #8785).

## What changed

- **CASIM_PIN 2ba7217.** CAS 019 prints a number casim makes as a fraction, and a typed decimal keeps its point.
- **AB_CAS_ARENA 8,800 to 12,288 B** (core/arena.h, with the reason). The arena is in PSRAM.
  - casim's device bench at 12,288: DEVICE PASS. The stack gate is unchanged at 11,744 of 12,288, since the depth
    guard, not the arena, bounds the stack.
  - SQRT(1+X)×EXP(X)'s order-11 Taylor needs 10,476 B under 019's fractions. DEG/GRAD X×ATAN(X) has 3,488 B to spare.
- **CASIM_DECIMAL_FORM (14) is an answer.** CAS 019 rule 5: when the fractions pass the cap or the arena, casim runs
  again and answers in 4f24e3e's decimal form.
  - core/calc.c's switch had no case for it, so it fell to INVALID DATA. The repin would have turned every such answer
    into an error: a scan below found 136 of 300 polynomial ∫AB answers come back this way.
  - cas_step now takes it as OK before the rows, the report and the insert. The student sees what 4f24e3e showed.
  - **The label is not built.** "The word is soroban's and Michael's" (casim's 019 doc, rule 5). It's proposed to
    abacus separately.
  - **→DEC** (CASIM_PRINT_DECIMAL) waits for Michael's keys.

## The rows that moved, each checked

**The 019 moves.** Each new text equals casim's SIMP of the row's hand answer with its decimals written as exact
fractions, at 2ba7217 (casim's own "rewrite judge"):
- 048 I10, I12, I14, I16, I17, I18, I23, I24;
- 057 Q04 (by hand X÷2-1÷4: X^2 by 2X+1);
- 070 S02 and S03, the textbook closed forms N(N+1)÷2 and N(N+1)(2N+1)÷6;
- device/standin/sum070.txt's Σ K^10, also checked against exact sums for N = 1..29.

**The arena moves:**
- **048 I19,** X×ATAN(X)+1÷(X^2-1), TOO COMPLEX at 8,800, answers at 12,288. Its expected text is the canonical form of
  the hand answer: X^2×ATAN(X)÷2-X÷2+ATAN(X)÷2 plus I10's.
- **070 S58,** casim 5ab9b59's closed form that ran out, answers. Its value equals the exact sum from Python's
  fractions over K = 1..50, 39928985231992/39720146275845.
- **070 S59 (new)** keeps S58's role: five partial-fraction terms from casim's golden corpus, NO_ROOM at 12,288 B and
  answering at 65,536 B.
- **056 D29** was NO_ROOM up to 10,023 B. At 12,288 it's RESULT TOO LONG, as from 10,024 B.
  - No polynomial ∫AB found runs out at 12,288 B. A seeded scan of 150 degree-11 products, each with bounds
    SQRT(2), SQRT(3) and 1÷3, SQRT(2), gave 299 answers (163 in fractions, 136 in decimal form) and 1 RESULT TOO LONG.
  - casim's ∫AB takes polynomials up to degree 11, and above that it's NOT_INTEGRABLE. The one ∫AB arena run-out on
    the keys is gone, as 070 recorded for I22's at 8,800.
- **056 D59 (new): DECIMAL_FORM on the keys.** One of the scan's decimal-form answers, inserted as an answer.
  - Its value was checked outside casim: the polynomial integrated exactly in Python, evaluated from SQRT(2) to
    SQRT(3), agrees to 6E-16.
  - Without the mapping it's INVALID DATA, and D59 goes red (2 of its expectations).

## Checks

- make test rc 0: 048 65/65, 056 43/43, 057 37/37, 070 58/58.
- casim-build: casim 2ba7217 built against core 894f066.
- **device/standin/repin2ba7217.txt** (the stand-in, below):
  - VER names the pin;
  - from the keys in DEG: X×ATAN(X) in fractions, I19 answering, S59's TOO COMPLEX;
  - the stack's low-water mark no lower than the deep paths' 8,188 B free.

## Decision 68 and the stand-in

**Static, `scripts/resources.py static 894f066 77d0241`:**
- Flash +4,560 on the stand-in and +4,576 on Rev A (casim's code).
- Internal RAM +0, IRAM +0, calc_task's frame +0. No internal-RAM symbol changed.
- The arena's +3,488 B is PSRAM (libmain's +8,029 counts it).

**The stand-in at 77d0241:**
- **repin2ba7217.txt:** ALL MET, the control red.
  - The pin is 2ba7217.
  - From the keys in DEG: X×ATAN(X) gives ATAN(X)÷2-90×X÷π+X^2×ATAN(X)÷2; I19 answers
    LN(ABS(X-1))÷2-LN(ABS(X+1))÷2+ATAN(X)÷2-90×X÷π+X^2×ATAN(X)÷2; S59's sum is TOO COMPLEX.
  - stack-free 23,828 B (floor 8,188); heap 62,747 B.
  - The port moved at the flash (ACM3 to ACM2). The rerun on the new port, with no flash, passed.
- **casrows.txt:** ALL MET.
- **sum070.txt:** ALL MET bar its control.
  - Σ K^10 reads in fractions.
  - S58's sum now answers 39928985231992÷39720146275845 where it said TOO COMPLEX. Its row moved, as 070's S58 did.
- make clang-check 66/0.
