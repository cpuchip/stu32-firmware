# Work unit 034: the polar form correctly rounded (→POL, ARG, and the r∠θ display)

**From:** abacus · **To:** soroban · **Status:** queued after 036b (found by your 030b red, confirmed by abacus)

## Why

Your 030b red found it, and abacus confirmed it with mpmath. For 3i4, →POL's angle is one unit low in the 34th
digit:

| | core | mpmath, rounded to 34 digits |
|---|---|---|
| degrees | 53.13010235415597870314438744090658 | …0659 |
| radians | …629224287 | …4288 |

That's inside the 8-unit bar. But SHOW (030) now shows every digit, in r∠θ too (030b), and y^x is already correctly
rounded (028). So the polar form should be correct to the last digit as well, the same promise.

## Rules

1. **These are correctly rounded** (at worst half a unit in the 34th digit) in DEG, RAD and GRAD, for every real
   pair in range:
   - **θ:** atan2(y, x) in the angle unit in force (→POL, ARG, the r∠θ display, and the equation engine's ARG);
   - **r:** √(x² + y²) (→POL's magnitude, ABS of a complex number), with no intermediate overflow for x and y near
     the range's ends.
2. **The quadrants and the axes are exact:**
   - θ = 0, 90, 180 and −90 (or their RAD/GRAD values, each correctly rounded) on the axes, by the guides' sign
     conventions;
   - −180 versus 180 on the negative real axis follows today's (accepted) behaviour, and says so.
3. **Route:** 028's hp path (atan at 50 digits, rounded once) or your choice. Measure the S3 cost before and after.
4. **→REC's inverse** (r, θ to x, y) gets the same treatment if it's cheap. Say whether it already is.

## Acceptance

- **A hidden set from abacus:**
  - random complex numbers across magnitudes from 1E-6000 to 1E6000 and every quadrant;
  - each angle mode;
  - X= exact, digit for digit, against mpmath at 120 digits, as 028's was judged.
- **Your own fuzzer** in `make accuracy`.
- **Mutants:** the old atan2 back (it must fail the exact set); a quadrant sign flipped.
- **No change** to accepted vectors, except where one encoded the one-unit-low value. Amend it in place, and say
  which.
- **The S3:** →POL's time before and after.
