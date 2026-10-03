# Work unit 010: vectors

**From:** abacus · **To:** soroban · **Status:** ready after 009 (light brief; you extract examples)

## Scope (HP 35s User's Guide ch.10, pp.10-1 to 10-6; Appendix B; Appendix F for the errors)

Vectors of two or three elements on the stack (and the one-element case p.10-6 mentions, if the
guide lets one be typed: say); typing a vector in RPN; + and − of equal-length vectors; × and ÷
by a scalar; ABS (the magnitude); the dot product (p.10-4; in RPN the guide reaches it by
multiplying two vectors, p.10-5: read the rendered pages and say which key does it); the angle
between two vectors as the guide computes it from DOT and ABS (no new function). Building a vector
from variables or stack registers (p.10-8) needs the equation editor's element recall and waits for
that unit; equations and programs with vectors (pp.10-6, 10-7) wait for theirs. Key tokens (rename
if you prefer; tell me): `[` to begin a vector, `,` between elements, `]` to end it (or whatever
the guide's key sequence needs), `DOT` if the dot product has its own key.

## Rules (manual first; ours where noted)

1. A stack level holds a real, a complex (unit 009) or a vector; variables hold vectors too
   (p.10-6: "exactly like real numbers"). Stack lift and LAST X as Appendix B says, a vector being
   one stack entry.
2. Lengths must match for +, −, DOT; otherwise the guide's error (find its message). A vector with
   a complex number, or a function the guide does not list for vectors, is an error.
3. **Ours, accuracy: every vector result is correctly rounded.** + and − and × ÷ by a scalar are one
   rounding per element. DOT is the exact sum of the exact products, rounded once (your xnum layer
   does this; no cancellation can hurt it). ABS is the correctly rounded square root of the exact
   sum of squares (as unit 006 did for s and σ). The guide's [5, 12] → 13 is then exact by
   construction, as is every Pythagorean case.
4. **Ours:** a vector result whose elements leave the range is an error that changes nothing (all
   elements or none, as Σ+ in unit 006). Division of a vector by 0 is an error.
5. The formatter shows a vector in the guide's bracket form (`[1.5,-2.2]`), each element in the
   current display setting; propose the text layout from the guide's displays.

## Acceptance and report

As before: the guide's examples as M-vectors (p.10-2 the two sums, p.10-3 [3,4]×5 and [−2,4]÷2,
p.10-3 ABS [5,12], p.10-4/10-5 the two dot products and the two angles, 90 and 36.8699 in DEG), red
then green. At review: hidden random vectors (2 and 3 elements, mixed magnitudes, Pythagorean and
near-cancelling cases) against an exact rational model, every result `=`.
