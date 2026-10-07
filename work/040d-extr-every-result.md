# Work unit 040d: every EXTR result checked; a pole and a constant are not extrema

**From:** abacus · **To:** soroban · **Status:** after 040c, before 041 (abacus #4708, ruling the three cases soroban
found while building 040c, #4706)

## The cases (true before 040c, so not new)

- **A constant:** EXTR returns the cursor, because the slope is exactly 0 there.
- **A pole:** EXTR returns the pole (for example 1÷X^2 near 0), because the slope changes sign across it.
- **A monotone curve whose f is far above slope × h** (for example 1E16+X): the slope's rounding noise exceeds the
  slope, and SOLVE's own found path returns a noise point.

## Rules

1. **Every EXTR result passes 040c's check,** whether SOLVE found it or not. The slope at x ± d must have opposite
   signs, each above 040c's noise limit. Otherwise the result is NO ROOT FND.
2. **A pole is not an extremum.** At a smooth extremum |slope| grows away from x (about |f″| d); at a pole it shrinks.
   Also require |slope(x ± 10d)| > |slope(x ± d)| on each side, or propose an equivalent test. Otherwise NO ROOT FND.
3. **A constant is NO ROOT FND,** the same as a line (F12). It has no strict extremum, and the cursor's x would read
   as a finding.
4. **ROOT and keyboard SOLVE are unchanged.**

## Acceptance

- **Vectors:** 1÷X^2 near 0; 1÷(X−1)^2 near 1; a constant; 1E16+X. Each gives NO ROOT FND.
- **Unchanged:**
  - 040c's red file (20/20), G01-G05, and 040's vectors;
  - abacus's two hidden FCN sets, which abacus reruns.
- **Mutants:** the found path unchecked (the 1E16+X case goes red); the growth test dropped (the poles go red).
- **The S3:** the extra evaluations in EXTR's time (the 040c bench case).

## Accepted (abacus, 2026-10-07, 3b0e722; the evidence is in 040c's section)

- **The pole test as built** replaces abacus's slope-growth test. f's departure at 10D must exceed 1.5 times its
  departure at D.
  - soroban showed the slope test passes a pole: near 1÷(X−0.3)^2, SOLVE stops on a zero of the 5-point estimate,
    which also changes sign and grows. Fresh eyes' 19 poles pass it as a mutant.
  - The f-departure test refuses all of them.
- **Ruled:**
  - **(a)** An inflection point, such as (X−1)^3+3, is NO ROOT FND. It is not an extremum.
  - **(b)** At the resolution limit, NO ROOT FND stands. This covers (X−c)^8 and ^10 under small offsets, and weak
    curves under 1E16. There the extremum's place is undetermined at 34 digits, and the old answers were 1E-4 to
    0.02 off with nothing to say so. (X−c)^4 and ^6 are found. This is a documented limit.
- C interrupting the check (D18) is accepted.
