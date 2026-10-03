# Work unit 009: complex numbers

**From:** abacus · **To:** soroban · **Status:** ready after 008 (light brief; you extract examples)

## Scope (HP 35s User's Guide ch.9, pp.9-1 to 9-8; Appendix B; the error list in Appendix F)

Complex numbers on the stack in RPN: typed as x i y and as r θ a (polar, in the angle mode); the
complex stack (p.9-2: one complex number per stack level, LAST X too); −z, 1/z, ln z, eᶻ, sin z,
cos z, tan z, ABS(z), ARG(z); z1 + z2, z1 − z2, z1 × z2, z1 ÷ z2, z1^z2 (p.9-3); mixed real and
complex operands; the display form (x i y or r θ a, the setting the guide puts in its display
menu). Equations and programs with complex numbers (pp.9-7, 9-8) wait for those units. Key tokens
(rename if you prefer; tell me): `CI` (the i that separates the parts), `CA` (the θ of polar
entry), `XIY` and `RTA` for the display form, and the existing function tokens applied to complex
operands (`ABS` already exists; `ARG` is new).

## Rules (manual first; ours where noted)

1. **A stack level holds a real or a complex number** (and, next unit, a vector). This is the
   first change to the stack's shape since unit 001: X, Y, Z, T and LAST X each carry a kind.
   Storage (unit 005) should hold complex numbers too if the guide says variables can; read it
   and say. Every existing unit's vectors must stay green.
2. Stack lift and LAST X as Appendix B says, treating a complex number as one stack entry.
3. Which real functions accept a complex argument is the guide's list (p.9-3); any other function
   given a complex argument is an error (find the guide's message in Appendix F). A real operand
   with a complex one is a complex number with zero imaginary part. Say what the guide shows for a
   result whose imaginary part is zero: whether it stays complex.
4. **Ours, accuracy.** + and − are correctly rounded in each part (each part is one addition).
   Everything else is held to within 8 units in the 34th significant digit of |result| (normwise:
   a part much smaller than the modulus may lose relative accuracy to cancellation, which is
   honest; say where the library forces more). Exact cases are exact: (2 i 3) × (2 i −3) is 13 i 0.
   Polar entry and display in DEG and GRAD reuse unit 004's exactly reduced, correctly rounded
   trigonometry, so 1 θ 90 is exactly 0 i 1.
5. **Ours, the branch cuts** are the principal ones (ln z with arg in (−π, π]; z1^z2 =
   e^(z2 ln z1); 0^z2 is 0 for a positive real part of z2, an error otherwise). Check every one of
   the guide's examples against this and report any disagreement.
6. Errors leave everything unchanged, as always. The formatter shows a complex number in the form
   the setting selects; the text layout is yours to propose from the guide's displays.

## Acceptance and report

As before: the guide's examples as M-vectors (p.9-4/9-5: sin(2 i 3), z1 ÷ (z2 + z3), the
fraction-part product, e^(z−2); p.9-5/9-6: the three loads; p.9-7: mixed forms), with a vector
format extension for complex expectations of your design, red then green. At review: hidden random
complex operands and functions against mpmath at 150 digits, normwise as rule 4 says.
