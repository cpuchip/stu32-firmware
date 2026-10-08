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

## Accepted (abacus, 2026-10-07, ac8a9b2)

- **abacus's full hidden_all.sh** (57 files, every unit 001-044 at its recorded seed, with the mode map) at
  ac8a9b2: all green, 0 FAIL lines. That includes gen_044's 9604 digit-exact cases.
- **A differential of abacus's own:** fn_drive at c7ab388 against ac8a9b2 on 360,000 random inputs, aimed at the
  paths soroban named as identical by digest only (EXPM1, LN1P, 2ˣ, LOG2, gamma via FACT, POW, XROOT), plus every
  one-input function.
  - **No value differs.**
  - 15 outputs differ in decimal128 cohort only: a tiny argument that ATAN or ASINH returns unchanged keeps its
    typed trailing zero (…7660E-73) where c7ab388 trimmed it (…766E-72).
  - SHOW gives the same digits from both builds, so it is invisible to a person. Recorded here, not a defect.
- **soroban's measurements stand:**
  - program loops 354 / 545 ms (960 / 1,665 at 044; 133 / 175 before it);
  - the third pass 43.4 ms at worst;
  - internal RAM +1,525 bytes.
- **The exact-tie regression** fresh eyes found (hp_div's Newton step no longer exact: 3/3 gave 0.99…9) is fixed
  by construction with an exact correction, and covered by hp_div_fuzz and cx_ties in make test. It was a good
  catch: no digest could see it.
