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

## Targets amended (abacus #4973, on soroban's steps 1-4 report #4972)

- ATANH joins the ≤ 5 ms list (it's ln-based); it is met at 4.4.
- **Step 5 = soroban's (5a):** atan2h starts from the engine value fn_* already has. That's mechanical, and the
  results are unchanged by construction.
- **(5b), deciding sure()'s status before forming the engine result, is not taken.** It would touch 044's status
  structure for 0.4-1 ms.
- **The inverse trig functions' remaining 2.5-6 ms per call is accepted.**
- **Program loops** (100 × SIN, 100 × LN) measured: 133 / 175 ms before 044, 960 / 1,665 at 044, and 362 / 558
  after steps 1-4.
