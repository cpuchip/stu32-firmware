# Work unit 003: one-argument functions, powers, trigonometry, percent

**From:** abacus (fermion), 2026-10-03 · **To:** soroban (lepton) · **Status:** ready

Michael asked both seats to keep pushing autonomously, so this brief is lighter than 001 and 002:
it fixes the scope, the rules that are **ours**, and the key-token names; **you extract the manual's
own worked examples** into the vectors file (cite each page), as you have been doing well. The
reviewer adds hidden accuracy tests from mpmath at review.

## Scope (HP 35s User's Guide, chapter 4, pp.4-1 to 4-7; Appendix B p.B-6 for LAST X)

| Group | Operations (key tokens) | Manual |
|---|---|---|
| exponential and log | `EXP` (eˣ), `LN`, `TENX` (10ˣ), `LOG` | p.4-1 |
| quotient and remainder | `IDIV` (INT÷), `RMDR` | p.4-2 |
| powers | `SQ` (x²), `SQRT`, `INV` (1/x), `POW` (yˣ), `XROOT` (ˣ√y) | p.4-2 |
| π and angle modes | `PI`, `DEG`, `RAD`, `GRAD` | p.4-3, 4-4 |
| trigonometry | `SIN` `COS` `TAN` `ASIN` `ACOS` `ATAN` | p.4-4, 4-5 |
| hyperbolic | `SINH` `COSH` `TANH` `ASINH` `ACOSH` `ATANH` | p.4-6 |
| percent | `PCT` (%), `PCTCHG` (%CHG) | p.4-6, 4-7 |

Stack behaviour follows unit 001: one-argument functions replace X; two-argument ones (POW, XROOT,
IDIV, RMDR, and whatever the manual says for PCT and PCTCHG; read p.4-6 closely, the 35s's percent
does not drop the stack the way + does) combine Y and X; every one of these saves LAST X where B-6
lists it; PI lifts like a recalled number. Angle-mode keys are neutral for stack lift (B-5). Default
angle mode: DEG (check the manual's statement and note it).

## Rules that are ours

1. **Accuracy.** Every result within **one unit in the 34th significant digit** of the true value of
   the function at the exact decimal argument (the reviewer measures against mpmath at 60 digits).
   Results that are exactly representable must be exact (√16 = 4, 10^3 = 1000, LOG 1000 = 3,
   2^10 = 1024, eˣ of 0 = 1).
2. **Exact angles in DEG and GRAD** (a teaching calculator must not show tan 45° as 0.999…98):
   when the argument is an exact multiple of 30° or 45° (in GRAD, of 50 and of 100/3 where exact),
   SIN/COS/TAN return the exact value when it is rational: 0, ±0.5, ±1. Irrational values (sin 45°,
   sin 60°, tan 30°) are rounded correctly as usual. TAN at an odd multiple of 90° (100 gon) is an
   error. The inverses agree: ASIN 0.5 = 30 and ATAN 1 = 45 exactly in DEG. **RAD gets no special
   cases**: SIN of the 34-digit π is the true sine of that number, about 1E-34, not zero.
3. **Domain errors** leave the stack and LAST X unchanged and report an error, as unit 001's ruling
   5 (√ of a negative, LN or LOG of 0 or a negative, 1/0, ASIN/ACOS outside [-1, 1], ACOSH below 1,
   ATANH at or beyond ±1, non-integer power of a negative base, 0 to a negative power). Complex
   results come in a later unit; for now they are errors. Use the manual's error names where it
   gives them (Appendix F) in a comment; the vector format only checks `ERR`.
4. **Range.** Results outside the normal decimal128 range are errors (unit 001 ruling 5).

## Vector format

As unit 001 (`ID | source | keys | expectations`), with the key tokens above added, and one new
comparison: **`X^value`** means within one unit in the 34th significant digit of `value` (use it
for transcendental results; give `value` to at least 36 significant digits from a computation you
name, e.g. Python `decimal` or mpmath). `=` stays exact.

Starter vectors (reviewer-written; checked with mpmath):

```
T01 | ours: exact angle   | DEG 45 TAN      | X=1
T02 | ours: exact angle   | DEG 30 SIN      | X=0.5
T03 | ours: exact angle   | DEG 60 COS      | X=0.5
T04 | ours: exact angle   | DEG 180 SIN     | X=0
T05 | ours: exact angle   | DEG 90 COS      | X=0
T06 | ours: tan 90 errors | DEG 90 TAN      | ERR X=90
T07 | ours: exact inverse | DEG 0.5 ASIN    | X=30
T08 | ours: exact inverse | DEG 1 ATAN      | X=45
T09 | ours: exact in GRAD | GRAD 50 TAN     | X=1
T10 | ours: RAD no cases  | RAD PI SIN      | X^-1.158028306006248941790250554076921836E-34
T11 | ours: exact results | 16 SQRT         | X=4
T12 | ours: exact results | 2 ENTER 10 POW  | X=1024
T13 | ours: exact results | 1000 LOG        | X=3
T14 | ours: domain error  | 2 CHS SQRT      | ERR X=-2
T15 | LAST X (B-6)        | 9 SQRT          | X=3 L=9
```

T10's value is the sine of π rounded to 34 significant digits (3.141592653589793238462643383279503,
which exceeds π by about 1.158E-34, so its sine is negative), computed with mpmath at 80 digits.

## Acceptance

All vectors pass (the starter set plus the manual's examples you add, each citing its page); red
before green as always; provenance comments; formatting unaffected (unit 002 vectors stay green).
At review: hidden mpmath accuracy vectors over every function and each angle mode.

## Report back (DM to abacus)

Commits, red and green, pages, ambiguities with pages.
