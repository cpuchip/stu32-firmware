# Work unit 050: where SOLVE's and ∫'s time goes on the S3, and yˣ's rounding without a division

**From:** soroban (proposed #5302, the profile #5304) · **Ruled by:** abacus #5303 (step 1, the profile), #5305
(step 2, (a) only) · **Status:** built and measured.

## Step 1: the profile (measurement only)

**The question:** on the S3, SOLVE of X^3-2×X-5 takes 62 ms against 128 µs on the host (490×), and the integral of
SIN(X)^2+X×EXP(-X) over 0..1 takes 90 ms against 115 µs (780×).

**The instrument** is device/standin/prof.sh. The core is built with -finstrument-functions in its own build directory;
the stand-in's hooks count each function's calls and cycles, inclusive and exclusive, on the calc task while
EQNBENCH runs. EQNBENCH also times each equation alone, a parse and an evaluation, at X = 1.234 and at a 34-digit X.

**Measured, uninstrumented:**
- **Parsing is 3–6% of an evaluation** (187 µs and 323 µs), so "parse once" was struck.
- **The cubic alone** took 892 µs at X = 1.234 but 3,100 µs at 34 digits; inside SOLVE an evaluation took 5,190 µs.

**The profile's ranks** (instrumented, so about twice the time):
- **SOLVE:** a big-integer division, from yˣ's exact power rounded by dividing it by 1; then decimal128 arithmetic in
  the BID library (~30%: abn_mul about 60 µs a call, uninstrumented).
- **The integral:** the library's transcendentals (~21%), decimal128 arithmetic, and the NAMES scan (~5%).

## Step 2: the exact power, rounded without dividing by 1

**What already existed:** unit 038's pow_xnum gives yⁿ for 2 ≤ |n| ≤ 64 exactly. It squares y's coefficient as a big
integer and rounds once; a negative n is one exact division. **The bound:** a 34-digit y to the 64th has at most 2,176
digits, inside the bigint's 6,840, and a power that doesn't fit falls back to the high-precision route (ABN_RANGE). So
the exact path, its bound and its fallback were all in place.

**What cost the time:** for a positive n, pow_xnum rounded through xn_div_to_abn(power, 1). That function scales the
numerator by 10^45 and divides it by the divisor through the general big-integer division, here a division by 1.

**The change** (core/xnum_impl.h): when the divisor is 1, the quotient is the scaled numerator, exactly, with no
remainder. So the rounding gets those same arguments without the division.
- **Byte-identical by construction:** the same function (BI_(to_abn)) gets the same value, exponent and sticky flag.
- **The scaling is kept on purpose.** Rounding the unscaled numerator changes the cohort of a short result: 2.5
  becomes +25E-1, not the +2500…0E-33 the division gave.
- **yˣ never reaches that case:** its exact path runs only past 34 digits. But the other callers that can divide by 1
  do: a mean of one data point, the linear solver's determinant of 1, complex division, calc.c's sum.

## The oracle

- **tests/pow_cohort.py:** yˣ on two fn_drive builds, compared as engine text (coefficient and exponent).
  - The cases are aimed at the edges: n from −66 to 66, bases of 1 to 34 digits and with trailing zeros, 1, −1, ±0,
    negative bases, 0 to negative powers, and the overflow, underflow and subnormal edges.
  - Against the core before 050, over five seeds of 20,000 (100,550 cases, about 18% errors on both): **0 differing**.
- **tests/050-soroban.txt:** a new %XRAW expectation, X's raw engine text. It pins yˣ's results and the cohort of a mean
  of one point, as the core before 050 gave them. It runs in make test, with a save/load pass.
- **Mutants:**
  - yˣ's x³ as decimal128 x·x·x (two roundings): pow_cohort 101 differing; W01 fails.
  - The shortcut for every divisor: pow_cohort 5,801 differing.
  - The unscaled numerator: pow_cohort 0 differing (as argued above), but W05 and W06 fail. Before 050 no vector
    pinned that cohort.
- **every vector and fuzz set:** unchanged (make test, make accuracy).

## Measured on the S3 (EQNBENCH, best of five, three runs; PROGBENCH the control)

| bench | before (dbe1cdc) | after |
|---|---|---|
| SOLVE of X^3-2×X-5, µs | 62,282–62,388 | 56,954–57,716 (−8.5%) |
| the cubic alone at a 34-digit X, µs | 3,100 | 2,596 (−16%) |
| the cubic alone at X = 1.234, µs | 893 | 875 |
| the integral, µs | 89,921–90,284 | 90,589–90,770 (+0.6%: no integer power of 3 or more in it) |
| PROGBENCH SIN / LN, ms | 359 / 546 | 358 / 544 |

**Smaller than the profile promised.** The division was about 500 µs a cube, not the 2.2 ms its instrumented rank
suggested. Instrumentation inflates call-dense code (14,530 calls of bi_qr9_ in one SOLVE), so even the profile's
order overstated it. A bench before and after is the measure; the profile only points.

**What remains at a full-length X:** 1.7 ms of the cubic's 2.6 ms, unmeasured as yet. Likely decimal128 arithmetic
at 34 digits; possibly the exact power's conversion, since BI_(to_abn) builds its digits as text (not timed). That is
the next profile's question; (b) and (c) stay parked until it answers.
