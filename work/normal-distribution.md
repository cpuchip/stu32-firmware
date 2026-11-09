# The normal distribution from erf and erfc (decision 95; blueprint, before any code)

**From:** decision 95 (abacus docs/decisions.md; Michael 2026-11-08 ~07:00Z), its item "the normal distribution from
erf/erfc, beside Σ statistics, each function checked against mpmath first, since the library's transcendentals go
through binary128", and the recommendation he answered (abacus, 06:48Z): "erf/erfc give probabilities from z-scores,
on top of the statistics we already have (Σ+). It's a new feature for stats students. The bytes should be small but
aren't measured." Plus: "Any library function we adopt gets an accuracy check against a high-precision reference
first." Wording as abacus relayed it (#14260). The keys and whether the inverse comes too are this blueprint's to
propose; STU only.

**Status:** draft. No code. Measured first: work/erf-evidence/ (the host checks erfcheck.c and phicheck.c with their
judges and outputs; ERFBENCH's patch and the stand-in's run; D68).

## The measured cost

**The library's erf and erfc** (bid128_erf, bid128_erfc; Intel DFP, the engine's library, BSD-3-Clause, already linked)
against mpmath at 60 digits. The arguments:
- 1,201 on [−6, 6], every 0.01;
- 2,000 random 34-digit ones on [−10, 10];
- 201 in erfc's tail, 6 to 106 by halves, where erfc falls to about 1E-4900;
- 118 tiny ones, ±1E-k.

| function | correctly rounded | under 1 unit of the 34th digit | under 2 | the worst |
|---|---|---|---|---|
| erf | 2,811 | 704 | 5 | 1.22 units, x = −0.5570957714… |
| erfc | 3,138 | 359 | 23 | 1.69 units, x = 2.33 |

Faithful: never off by 2 units in the 34th digit, often exact to the last. That's the library's binary128 route, as
the decision expected, and it's adequate for a 12-digit display with 22 digits to spare.

**The normal distribution in the engine's own arithmetic:** Φ(z) = erfc(−z/√2)/2 and Q(z) = erfc(z/√2)/2, each step
in decimal128. Measured over 1,601 z on [−8, 8], 1,500 random, and the tail 9 to 39, against mpmath's erfc(∓z/√2)/2.
- **Φ:** within 77.7 units of the 34th digit, the worst at z = −7.04.
- **Q:** within 1,680 units, at z = 27.
- **Why:** this is the argument's rounding, not erf's. z/√2 rounded to 34 digits carries a relative error near
  1E-34, and in the tail the result's relative sensitivity to its argument grows like z². So Φ and Q are right to at
  least 30 significant digits for |z| ≤ 39 (where Q is about 1E-333).
- **Doing better** would mean computing erfc's argument at higher precision (the core's hp) and a series of our own.
  I don't propose it: 30 digits under a 12-digit display.
- **My judge was wrong first:** it took Q as 1 − Φ at 80 digits, which cancels to 0 for z ≥ 18. Fixed to erfc
  before these numbers.

**The S3** (ERFBENCH on the stand-in, 20 calls each, microseconds a call):

| x | erf | erfc |
|---|---|---|
| 0.5 | 144 | 507 |
| 1 | 335 | 581 |
| 2.33 | 310 | 652 |
| 5 | 340 | 611 |
| 13.4 | 20 | 920 |
| 27 | 20 | 779 |
| 100 | 20 | 759 |

So under a millisecond a call. The stack stayed at 26,812 B free.

**Flash:** +4,173 B in libbid (bid128_erf and bid128_erfc and what they bring), +4,864 B with the throwaway bench
(`resources.py static 3a8248a 97593ce`). RAM +0. The decision's "small" holds.

## The leans

1. **The functions:**
   - **Φ(z), lower tail:** P(Z ≤ z).
   - **Q(z), upper tail:** P(Z > z), computed as erfc(z/√2)/2, not 1 − Φ, so the tail keeps its digits (Φ(27) is 1 to
     34 digits; Q(27) is 7.39E-161).
   - **ERF and ERFC** themselves.
   - **The inverse, Φ⁻¹(p):** the z with Φ(z) = p, for 0 < p < 1. It's in the lean because the decision's students need
     it most (a critical value from a confidence level). The library has no inverse, so it's Newton's method on Φ in
     the core, started from a rational approximation's z, with Φ's derivative, the density e^(−z²/2)/√(2π). Its
     accuracy is measured in the build against mpmath before it's kept; the target is 30 digits, as Φ's. If the
     measurement misses it, it comes out of the unit, not in at a worse accuracy.
2. **The keys:** PROB's sixth key, empty today (Cn,r Pn,r n! RAND SEED _), opens a NORM menu: Φ, Q, Φ⁻¹, ERF, ERFC and
   ▸ back to PROB.
   - PROB is the statistics' neighbour.
   - In the 33s and 35s modes the sixth key stays empty.
   - One-argument ops on X: LASTx kept, lift as any function's.
3. **In expressions:** STU's parser gets ERF( ERFC( NCDF( NCDFINV( (the names a reader of the line can read), so ALG,
   EQN and the CAS's numeric evaluation reach them. Casimir treats them as functions it doesn't differentiate, as it
   does other non-elementary names. That needs checking against casim's language, and it's the question below.
4. **z only, not x with μ and σ.** Standardizing (x − μ) ÷ σ is the student's own step, and the stack makes it short.
   A version taking x̄ and sx from the Σ registers would hide the step being taught.
5. **Programmable:** yes, in STU programs, as the other functions.
6. **The mark (the other blueprint's rule 2):** their results are rounded by default, an untracked path, until a unit
   proves otherwise (erf(0) = 0 and Φ(0) = 0.5 are exact; the build may mark those two).

## What the build would test (for abacus's oracle)

- **An accuracy test with a checked-in mpmath table** (pinned 1.3.0, its own hash, as the mark's): ERF and ERFC within
  2 units of the 34th digit; Φ and Q within 30 significant digits for |z| ≤ 39; Φ⁻¹ within its measured bound.
- **Vectors:**
  - textbook values: Φ(1.96) = 0.975002…, Q(1.645) = 0.04998…, Φ⁻¹(0.975) = 1.95996…, ERF(1) = 0.842700…;
  - the tails (Q(10), Q(27), Φ(−27));
  - domain errors (Φ⁻¹ of 0, 1, −0.1);
  - the keys, the parser's names, a program line;
  - the 33s and 35s modes untouched.
- **Mutants,** predicted first:
  - Q as 1 − Φ (the tail's digits lost);
  - erfc's sign wrong in Φ;
  - Newton's last step not taken;
  - Φ⁻¹ outside (0, 1) not refused.
- **D68 and ERFBENCH** in the build, against the measured +4.2 KB and under 1 ms.

## Questions for abacus

1. The scope: Φ, Q, ERF, ERFC and the inverse Φ⁻¹, with Φ⁻¹ kept only if it meets 30 digits when measured.
2. The keys: PROB's sixth key opening NORM.
3. The parser's names (ERF( ERFC( NCDF( NCDFINV( ) for ALG, EQN and the CAS's numeric evaluation, given Casimir's
   language. Or the keys only, in this unit.
4. z only (standardizing left to the student), not μ and σ from the Σ registers.
