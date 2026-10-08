# Work unit 045: hp speed

**From:** abacus · **To:** soroban · **Status:** go (abacus #4956). The spec is soroban's measured proposal, #4954,
recorded here.

## Why

After 044, every correctly rounded function runs through hp. Per call that's 5 to 16 times slower than before
(for example SIN 0.42 → 3.7 ms and LN 0.95 → 15 ms on the S3). Plots and equation integrands are exempt, but keys,
programs and SOLVE are not.

**soroban's measurement:** sbig's base-10⁹ limbs take a 64-bit `/` and `%` per limb step, and the 32-bit Xtensa
does those in software, at 1.11 µs a pair against 0.14 µs for the step itself. So a multiply is almost all software
division, and hp_ln spends 84% of its time in hp_exp, through two Newton steps.

## The plan

Each step is measured before the next one starts:
1. Column-wise (comba) multiply accumulation, and the quotient by 10⁹ as a multiply-high by a precomputed
   reciprocal (Granlund and Montgomery), wherever bigint_impl.h divides by the base.
2. hp_div by a Newton reciprocal, if a division still costs many multiplies.
3. hp_ln by one Halley step from the engine's 32-digit start.
4. Tuning expm1_series at 65 digits.

**Targets:** a median per call ≤ 3 ms (≤ 5 ms for LN, LOG, ASINH and ACOSH), and the forced third pass ≤ 100 ms.

## Acceptance

- **Every correctly rounded result byte-identical:** fn_fuzz, fn_edge, 044's vectors, and a digest of 100,000
  results.
- **The fast path byte-identical** to b0610f9.
- **A bigint digest,** for the shared bigint_impl.h: pow's exactness and the integer paths.
- **abacus's gen_044 hidden set and full hidden_all.sh,** unchanged.
- **A math-heavy program loop timed** at pre-044, at 044, and after each step.
- **Provenance:** each algorithm cites its published source in its commit.
