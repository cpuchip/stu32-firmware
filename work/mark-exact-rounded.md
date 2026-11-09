# The exact-or-rounded mark (decision 95; blueprint, before any code)

**From:** decision 95 (abacus docs/decisions.md; Michael 2026-11-08 ~07:00Z: "Pull in your 3 recommendation for intel
decimal. When things are winding down."), its item "an exact-or-rounded mark, from the library's inexact flag carried
through a calculation (abn_intel.c reads the flags only for overflow and underflow today)", and the recommendation he
answered (abacus, 06:48Z): STU marks an answer exact (`=`) or rounded (`≈`); "a good teaching feature, and it's mostly
display work". Wording as abacus relayed it from the sources (#14260). Where it shows, what counts as exact and which
modes are this blueprint's to propose; STU only unless argued otherwise.

**Status:** draft. No code. Measured first with a throwaway (work/mark-evidence/: the patch, census.c, judge.py and
their outputs).

## The risk, and the measurement

Calling an exact answer rounded is a nuisance. Calling a rounded answer exact is a false statement on the screen. So
the mark is designed around the false-exact count, which must be 0, and the false-rounded count is its cost.

**The prototype:** a sticky flag in abn_intel.c, set whenever a library call reports INEXACT in `accept` (every library
result but fmod's passes through it).
- The core's own exact and high-precision paths (xnum, hp, the big integers) finish in `BI_(to_abn)`. That writes 39
  digits and a sticky 40th through `abn_from_text`, so their final rounding is seen by the same flag.
- trunc, floor and idiv mask INEXACT on purpose (an integer part is exact).
- `abn_mul_exact` reports it to its caller.

**The census** (census.c): each op run once through the core in STU, X and Y set exactly, the flag cleared first.
- **The judge** (judge.py): mpmath at 300 digits says whether the decimal128 result is the true value (relative
  difference under 1e-250).
- **The first judge was wrong twice, both fixed before these numbers:** it used an approximate π for degrees (sinpi
  and cospi now), and at 1e-70 its tolerance called sin(1E-40) = 1E-40 exact.
- **Two runs:**
  - the chosen arguments: integers, halves, the special angles, powers of ten, 32 values;
  - random arguments (seed 95): integers, short decimals, 10- to 34-digit decimals, 1E-k; 120 of them.

| run | cases | exact, called exact | rounded, called rounded | false exact | false rounded | clamped | errors |
|---|---|---|---|---|---|---|---|
| chosen | 5,379 | 3,186 | 1,521 | 4 | 111 | 29 | 528 |
| random | 20,640 | 8,553 | 8,520 | 298 | 11 | 603 | 2,633 |

("errors": the op refused the arguments, a domain error or the like, and there's nothing to mark.)

**The false exacts, every one, in three classes:**
1. **An overflow clamped** to the largest number with a notice (SINH, COSH, FACT, POW, XROOT past the range). The op
   reports OK and its value is replaced, so no library call saw the rounding. Counted apart above as "clamped" once
   seen; in the first raw count they were false exacts.
2. **An underflow flushed to 0** (286 of the random run's 298: POW 106, XROOT 180). The true value is nonzero, the
   result exactly 0, no flag.
3. **The core's own shortcuts**, which return a constant or x itself without a rounding call:
   - fn_tanh at |x| ≥ 100 gives ±1: 15 cases, 100, 123.456, 132, 150, 180...
   - COS of 1E-35 gives 1 (once).
   - Reading fn.c finds more of the same shape: 10^x of a tiny x gives 1, ln(1+x) of a tiny x gives x. There are 8
     uses of `tiny()` and the saturations beside them.

So **the flag alone gives false exacts, and the three classes close them.**

**The false rounded, per op** (the chosen run; exact results the flag calls rounded / every exact result of that op):
- **XROOT 89/107.** The root's index goes through a rounded 1/x, so the flag is nearly useless for roots: 8 XROOT 3
  is exactly 2 and comes out flagged.
- **POW 12/266:** 4^0.5 = 2.
- **Inverse trig in DEG at the special angles:** ASIN 3/5, ACOS 3/5, ATAN 2/4 (asin 0.5 = 30° goes through radians
  and ×180/π).
- COS 1/5 and COSH 1/1.

The random run has 11, all XROOT.

## The leans

1. **What a value carries:** one bit, "rounded", in ab_val's padding as B′'s bare mark is (090), so every copy carries
   it and it costs no RAM.
   - **An op's result is rounded when any input was, or when the op itself rounded:** the flag; a clamp; a flush to 0;
     a shortcut that returns a constant (each marked where it returns); a numeric SOLVE or ∫, whose answers are
     approximations by method whatever their digits.
   - **A typed number is exact,** unless it has more digits than the engine keeps (the parse rounds: the flag sees it).
   - **Constants:** π, e and the physical constants that are measured are rounded. The defined ones (c, g_n,
     conversion factors set by definition) are exact.
   - **STO, RCL, the stack's moves and the worksheet's rows carry it.**
   - **In the image:** a T_ROUNDED tag in STU, as T_BARE (090), the state version bumped. A reloaded value keeps its
     mark.
2. **What counts as exact, beyond the flag** (to cut false rounded; each a check that can only prove exactness):
   - **XROOT, and POW whose exponent is the reciprocal of a whole number up to 99:** the result r is exact when rⁿ
     equals the base exactly (abn_mul_exact chain, n multiplications at most). 8 XROOT 3 = 2: 2³ = 8 exactly.
   - **DEG inverse trig:** a result that's a whole number of degrees is checked by the exact sine and cosine of that
     angle (the table the forward trig already uses for exact angles: 30°'s ½ and the like). asin ½ = 30 exactly.
   - **Not attempted:** a general "is this decimal the true value" test. It would be a second implementation of each
     function.
3. **Where it shows:** STU's home, X's line: `≈` before a rounded value, and nothing before an exact one.
   - Exact is the default a student expects, so the mark is on the exception. The recommendation's `=` for exact would
     put a sign on every typed number.
   - The worksheet's rows, the levels above X, the 33s and 35s modes, programs' displays and ALG's history: no mark in
     this unit.
   - INFO on a selected row (093) says EXACT or ROUNDED.
4. **The display's own rounding is separate.** FIX 4 shows 1.2346 for the exact 1.23456. The mark speaks of the value,
   as SHOW's full digits do, not of the digits drawn. That's the one thing a student could misread, so HELP's entry for
   the mark says it.
5. **Complex and base modes:** a complex value is rounded if either part is. Base modes are integers, and a clamp there
   (ABN_CLAMPED) marks it rounded.
6. **CAS:** an expression is exact by construction (rationals). A plain number from SIMP or EXPND is exact, unless
   Casimir answered in its decimal form (CASIM_DECIMAL_FORM).

## What the build would test (for abacus's oracle)

- **The census as a test** (tests/mark_test, as steps_test is), over both argument sets, against a table of mpmath's
  verdicts made once and checked in, since mpmath isn't in make test: **false exact = 0** (a hard fail), and false
  rounded under a stated bound per op.
- **Vectors:** a dump `%XMARK=` (`≈` or `-`) and `%MARK-L=` for each level.
  - typed numbers;
  - the four arithmetic ops on exact and inexact inputs;
  - carry through STO, RCL, the stack and a program;
  - each class (a clamp, a flush to 0, tanh(100), a shortcut);
  - the back-checks (8 XROOT 3, 4 POW 0.5, asin 0.5 in DEG);
  - constants;
  - the image (a rounded value saved and loaded keeps its mark);
  - the 33s and 35s modes untouched.
- **Frames:** X with and without ≈.
- **Mutants,** predicted first:
  - the flag not carried from an input;
  - the clamp not marked;
  - the flush not marked;
  - tanh's shortcut not marked;
  - a back-check that passes without checking;
  - the mark shown outside STU.
- **D68,** measured: the flag is one store a call; the bit is in padding; the back-checks are code.

## Questions for abacus

1. The mark on the exception only (`≈` for rounded, nothing for exact), on X's line, with INFO saying which.
2. The back-checks as scoped (roots and reciprocal powers, whole degrees), and nothing more general.
3. Constants split by definition (measured ones rounded, defined ones exact). This needs a list from const.c.
4. The bit in the image (a state version bump), or RAM only, where a reload would mark everything exact.
5. The census's mpmath table checked in and regenerated by a script outside make test (with mpmath), as accuracy's
   references are.
