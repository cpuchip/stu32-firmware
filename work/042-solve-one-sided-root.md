# Work unit 042: SOLVE, a root approached from one side

**From:** abacus · **To:** soroban · **Status:** next, ahead of 041 (a correctness defect in SOLVE, found by abacus's
hidden FCN set for 040, 2026-10-07)

## The defect

When both guesses lie on one side of a simple root, the secant search can close in on the root without ever
crossing it. The loop's "closing in on a minimum of |f| without a sign change" exit (calc.c, solve_core: |b − a| ≤
1E-30 max(|a|, |b|)) then goes to `nosign`. There sp_bracket finds no interior minimum, because |f| falls
monotonically toward the root. The result is NO ROOT FND, with X already holding the root.

**Measured at 734bb48** (abacus's builds; the vectors in docs/evidence/2026-10-07-040-fcn-review.md):

- **Keyboard SOLVE on a program,** f = c·e^(x/s) − k, a monotone function with one simple root:
  - 400 random cases, the guesses on one side, 1E-3 to 3 from the root and 1E-3 to 1 apart;
  - **68 of 400 report NO ROOT FND** (37 of 225 near, 31 of 175 far);
  - in all 400, X holds the root within 1E-29.
- **GRAPH's ROOT and EXTR** (040) use this engine with the cursor's column and the next one as guesses. In abacus's
  hidden FCN set (seed 40040, 3000 cases), **38 of 1042 ROOT/EXTR cases** fail the same way. In each, the cursor
  is within 3 columns of a simple root with |f'| ≥ 0.25 (or an extremum with |f''| ≥ 0.25), and the nearest other
  root is at least 20 columns away.
- An example:
  - SIN(X)+0.13×X−0.04 in RAD on −8..5;
  - the cursor at column 133 (x = −3.6667);
  - ROOT gives NO ROOT FND;
  - the root is −3.68775…, where f′ = −0.73.

## Rules

1. **A root approached from one side is found.** When the search closes in, and the secant still points past b
   toward a root (|f| has been falling along the steps), the search steps past the projected root to find a sign
   change. Then it goes to Brent's bracket as usual. Propose the step and its bounds (for example, the secant's
   step doubled, then doubled again, up to a few tries).
2. **The 024b path is unchanged.** Only a genuine interior minimum of |f| (sp_bracket) goes to the minimizer and
   its report (33s p.D-8, D-10).
3. **What counts as a root is unchanged:** a sign change, or f = 0.
4. **The evaluation count:** state the extra evaluations this costs in the cases above, and SOLVE_MAX still bounds
   everything.

## Acceptance

- **Red first:** abacus's 400-case sweep (in the evidence file) as a vector file; 68 red today.
- **Green:**
  - the sweep 400/400 with no error;
  - abacus's hidden FCN set: the 38 green, and nothing else changed;
  - every accepted SOLVE vector unchanged (013, 024, 024b, 035), the minimum cases reporting as before.
- **Mutants:**
  - the old exit back (the sweep goes red);
  - stepping past in the wrong direction;
  - a step past that ignores SOLVE_MAX.
- **The S3:** the evaluation counts before and after on the sweep.
- **tutor's lessons:** checked by their own harness at the repin.

## Accepted (abacus, 2026-10-07, a6d466c)

- **abacus's build:** a git-archive export with Babal staged. make test passes, with app_test 503/503 and the sweep
  401/401.
- **The sweeps, each red at 734bb48 and green here:**

  | Set | At 734bb48 | At a6d466c |
  |---|---|---|
  | abacus's sweep, c·e^(x/s) − k | 332/400 | 400/400 |
  | a fresh hidden one-sided sweep, sin x + kx + c (guesses 1E-4 to 0.1 from the root, |f′| ≥ 0.25; the seed kept) | 523/600 | 600/600 |

- **No change to accepted behaviour:** unit 013's hidden set (seed 1315), 10000/10000 before and after.
- **040's hidden FCN sets:**
  - seed 40040: 2962/3000 at 734bb48, 2992/3000 here;
  - a second seed: 2938/3000 at 734bb48, 2993/3000 here;
  - **every ROOT case is now green.**
  - The 15 that remain are all EXTR (sinlin and xexp), NO ROOT FND, and each failed the same way before 042. They
    are unit 040c's, not this unit's.
- soroban's two findings stand: the crossing that was checked too late (W0027), and the flat last digit (W0008).
