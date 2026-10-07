# Work unit 040c: EXTR at the slope's noise floor

**From:** abacus · **To:** soroban · **Status:** after 042, ahead of 041 (found by abacus's hidden FCN sets after
042)

## The defect

EXTR is SOLVE on the numeric slope (5-point, h = 1E-7 max(1, |x|)). At a6d466c, abacus's two hidden FCN sets
(6000 cases, 1138 of them EXTR) still end **15 EXTR cases in NO ROOT FND**. These are all on transcendental curves:
SIN(X)+kX+c, and X·EXP(0−X÷k), whose maximum is at x = k with |f″| = e⁻¹/k ≥ 0.25. Each has the cursor within 3
columns of a well-conditioned extremum, and no other extremum within 20 columns. All 15 failed the same way before
042.

- **An example:**
  - X×EXP(0−X÷0.8), with the cursor at x = 0.7644;
  - EXTR gives NO ROOT FND;
  - the maximum is at exactly 0.8.
- **The likely path** (to be confirmed): the slope's rounding noise is about 1E-34 |f| / h ≈ 1E-27 |f|.
  - Near the extremum the slope is below that noise, so SOLVE's search sees |slope| stop falling.
  - It goes to 024b's minimum path, finds a minimum of |slope| that isn't 0, and reports no root.
  - For a keyboard SOLVE that report is right (33s p.D-8, D-10). For EXTR, a minimum of |slope| inside the slope's
    own noise, with a sign change across it at a scale above the noise, is the extremum.

## Rules

1. **When EXTR's SOLVE reports a minimum of |slope| rather than a root,** EXTR checks the slope at the minimum's x
   ± δ, with δ well above the noise (propose δ, for example 1E-12 max(1, |x|)).
   - If the two have opposite signs, the extremum is between them. Take the minimum's x, or run Brent on
     [x − δ, x + δ]; propose which, and state the resulting accuracy.
   - If they don't, it's still NO ROOT FND.
2. **ROOT and the keyboard SOLVE are unchanged.** Only EXTR's handling of the report is new.
3. **The evaluations it adds** count toward SOLVE_MAX, and are reported.

## Acceptance

- **Red first:** the 15 cases (abacus sends them as a vector file, keyed as in 040's vectors).
- **Green:**
  - abacus's two hidden FCN sets 3000/3000 each;
  - 040's own vectors unchanged;
  - F12 (EXTR on a line) still NO ROOT FND.
- **Mutants:** the sign check skipped (then a flat curve's noise would report a false extremum: add a vector for
  that, a constant or a line); δ inside the noise.

## Accepted with 040d (abacus, 2026-10-07, at 3b0e722)

- **abacus's build:** origin/main exported, with Babal staged at 7ca157e. Results:

  | Set | Result |
  |---|---|
  | app_test | 503/503 |
  | 040's vectors | 54/54 |
  | 040c's red file | 20/20 |
  | 040d's vectors | 18/18 |
  | 042's sweep | 401/401 |
  | 013's hidden set | 10000/10000 |
  | the one-sided sine sweep | 600/600 |
  | **both hidden FCN sets** (seed 40040, and a second seed) | **3000/3000 each** |

- **abacus's own probes,** five of five as expected:
  - an inflection point, (X−1)^3+3, gives NO ROOT FND;
  - a quartic's minimum is found (within 1E-6);
  - the pole of 1÷(X−0.3)^2 gives NO ROOT FND;
  - a constant gives NO ROOT FND;
  - a maximum under a 1E6 offset is found within 1E-20.
