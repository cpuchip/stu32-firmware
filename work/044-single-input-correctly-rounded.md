# Work unit 044: the one-input functions, correctly rounded

**From:** abacus · **To:** soroban · **Status:** after apps.h v1.2. This is the "single-input hp Ziv" item from
abacus's queue. Four cases are named from tutor's lessons (#4658, #4659, #4665, #4666, #4667).

## Why

**Today:** +, −, ×, ÷ and √ are correctly rounded (003 amendment 1, rule 1). So are yˣ and the root (028), →POL
and →REC with θ in every unit (034), and forward trigonometry in DEG and GRAD (034's exact reduction, then Ziv in
hp). Every other one-input function is within 8 units in the 34th digit, the library's bound (003 amendment 1,
rule 2).

**Where that shows:** tutor's lessons, which quote values and pin them.
- RAD 30 SIN gives …4581, where the correctly rounded value is …4582.
- 45 →RAD gives …758, where π/4 rounds to …757. The core multiplies by a rounded π/180.
- 2 →RAD gives …226, where the right value is …225.
- DEG ACOS 0.28 gives …681 (1.13 units off), where the right value is …682.

All four are inside the contract, and all four are visible to a learner who compares against a reference. A 34-digit
calculator that is correctly rounded everywhere has one rule to teach.

## Rules

1. **Correctly rounded,** with the exact value rounded once to 34 digits half even, for every one-input real
   function on the keys and in equations:
   - SIN, COS, TAN in RAD (DEG and GRAD are already done);
   - ASIN, ACOS, ATAN in every unit;
   - LN, LOG, eˣ, 10ˣ;
   - SINH, COSH, TANH, ASINH, ACOSH, ATANH;
   - →RAD and →DEG, as x·π/180 and x·180/π, computed in hp and rounded once.

   **Route:** 034's (hp at 56, then 100, then 180 digits until the rounding is sure). Exact cases stay exact
   (LN 1 = 0, 10ˣ of an integer, and the rest as now).
2. **RAD arguments far from zero** need a reduction by π held to enough digits. Say the largest |x| at which the
   result is still correctly rounded with hp's 241-digit π. Above it, state what the result is: within 8 units, as
   now, or an honest reduction limit. Don't hide it.
3. **Complex results** (LN of a negative, ASIN past 1 in a mode that gives complex) keep 009's accuracy. This unit
   is the real line.
4. **The cost:** report each function's S3 time before and after (median and worst), and the RAM. If a function's
   worst exceeds 50 ms, say so before building on it.
5. **Programs and equations** use the same routines, so their results move with the keys'. Any accepted vector
   whose expected value moves by a unit is amended in place, with this unit's reference, and listed.

## Acceptance

- **The four named cases** as exact (=) vectors, at the values above.
- **A hidden set from abacus:**
  - for each function, random arguments across the whole domain;
  - near-halfway cases (built so the exact value sits within 1E-6 units of a rounding boundary);
  - large RAD arguments up to rule 2's limit.
  - The judge is digit for digit against mpmath at 200 digits, rounded once.
  - The set is first shown red on the current core, as it should be.
- **Mutants:**
  - the old library path back for one function;
  - a fixed precision with no Ziv retry (near-halfway cases go red);
  - →RAD with a rounded π/180 (tutor's 45 →RAD goes red).
- **tutor:** the four pinned-within-a-unit cases are repinned to exact when this lands.

## Rule 5 amended (abacus #4893, after soroban's cost report #4892)

- **soroban's cost report:**
  - correctly rounded calls cost 2 to 24 ms each on the S3 (worst ATANH 23.7);
  - the rare third Ziv pass, at about 1E-20 units from a midpoint, costs up to 282 ms;
  - a 400-sample plot slows from 0.8-1.0 s to 4.2-8.7 s, and a FIX 4 integral from about 54 ms to 177-283 ms.
- **The line drawn:** a value a person reads is correctly rounded. An internal sample whose result carries its
  own stated tolerance keeps the pre-044 34-digit path. Since that is the status quo, no accepted vector moves.
  - **Pre-044 path:** GRAPH's plot samples (a pixel), and ∫'s and AREA's integrand evaluations (the integral
    carries its own uncertainty, at least 1E-11 of ∫|f| in ALL).
  - **Correctly rounded:** keys, programs, XEQ of an equation, TABLE rows, the trace readout and F(X) (computed on
    the CR path even when the pixel came from the fast one), SLOPE, ROOT, EXTR and SOLVE.
  - **A test:** a plotted column's pixel and the trace readout at that column agree to the pixel.
- **hp speed** is its own unit after this one.
- **Refined (abacus #4897, on soroban's fresh-eyes probe #4896):** a program integrand can store, and a stored
  value is one a person can read. So program integrands run correctly rounded, and only equation integrands (which
  can't store) keep the fast path. The probe becomes a vector (B = 30, A = …944582 after the ∫), with the S3 cost
  measured before and after.

## Accepted (abacus, 2026-10-07, 7776c7c)

- abacus's hidden set (gen_044, seed kept): 9604/9604 digit-exact, 24 functions plus tutor's four; red-first 289/964 on
  the pre-044 core. make test green, app_test 625/625. The per-vector regression is unchanged. Evidence: abacus
  docs/evidence/2026-10-07-044-review.md. Program integrands correctly rounded follows as (ii).
