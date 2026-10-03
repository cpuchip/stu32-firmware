# Work unit 002: number formatting (FIX, SCI, ENG, ALL)

**From:** abacus (fermion), 2026-10-03 · **To:** soroban (lepton) · **Status:** ready

How a number becomes the text on one line of the display. This is a pure function: no stack, no
keys, no screen. Later units call it for every register they show. It matters more than its size:
Michael's chief complaint about the 35s was that its display was too narrow to show a full-
precision result without switching to FIX. Our display is wider and our numbers have 34 digits,
so some 35s rules carry over and some are replaced by rules of our own, marked **ours** below and
"ours" in the vectors.

## Sources

HP 35s User's Guide: pp.1-21 to 1-25 (PDF pages 37 to 41) for FIX, SCI, ENG, ALL, the radix and
the thousands separator; p.1-16 (PDF 32) for FIX falling back to scientific. Read keystrokes and
displays from the rendered pages; the text extraction loses the key glyphs.

## The function

`format(value, mode, n, width, radix, separator) -> text`

- `value`: a number from the engine (decimal128). `mode`: FIX, SCI, ENG or ALL. `n`: the digit
  setting, 0 to 33 (**ours**; the 35s allows 0 to 11 because it holds 12 digits; we hold 34).
- `width`: the most characters the line can hold. Every character counts: sign, digits, radix,
  separators, `E`, the exponent's sign and digits. The text never exceeds `width`.
- `radix`: `.` (default) or `,`. `separator`: on (default) or off. The thousands separator is `,`
  when the radix is `.`, and `.` when the radix is `,` (p.1-24). Separators group the integer part
  only, in threes, and never appear in a scientific mantissa (**ours**: the manual shows only an
  integer part grouped).
- Exponents are written `E` then the exponent: `E5`, `E-5`, `E6144`. No `+`, no leading zeros.
- A minus sign leads a negative number. Negative zero is shown as `0` (the engine never yields it).

## Rules

**FIX n** (p.1-21): round to n decimal places and write the number in plain notation with exactly
n decimals (no radix when n = 0). It falls back to SCI n when: the value is not zero but rounds to
zero at n places (p.1-16: 0.000042 in FIX 4 is `4.2000E-5`); or the plain form would show more
than 34 significant digits (**ours**, our counterpart of the 35s's 10¹¹ limit on p.1-21); or the
plain form is wider than `width` (**ours**).

**SCI n** (p.1-22): one digit before the radix, n after, then the exponent. Round the value to n+1
significant digits first; if rounding carries (9.996 in SCI 2), the exponent moves (`1.00E1`).

**ENG n** (p.1-22, 1-23): like SCI, but the exponent is a multiple of three, so one to three digits
come before the radix; n is the number of digits after the first significant digit (n+1 significant
digits in all). A carry can move to the next group of three (999.6 in ENG 2 is `1.00E3`). ENG 0
of 123456 is `100E3` (one significant digit). Zero in ENG n is `0.` and n zeros, then `E0` (p.1-23).

**ALL** (p.1-23, then **ours**): show the value with all its significant digits and no trailing
zeros (no radix if it is an integer), in plain notation if that fits `width`. If it does not fit,
the 35s goes scientific (p.1-23), which on a 34-digit machine would turn 1/3 into scientific
notation every time. So, **ours**: round the plain form to the decimals that fit and remove trailing
zeros, *provided it still shows at least min(the value's own significant digits, 12) significant
digits* (counting the digits rounded to, so 9.999... rounded to `10.00000000000` qualifies and shows
`10`). Otherwise write it in scientific notation with as many significant digits as fit, trailing
zeros removed (`1E-30`, `1.23456789E-4`).

**The exponent is never cut off (ours; Michael's rule from the 35s's narrow display).** In SCI and
ENG, and wherever FIX or ALL fall back to them, if the text is wider than `width`, reduce the digits
after the radix one at a time (rounding again from the value, not from the shorter text) until it
fits. The exponent always appears whole.

**Display rounding: half away from zero (provisional).** 0.125 in FIX 2 is `0.13`; -2.5 in FIX 0 is
`-3`. This is the usual calculator convention and **will be checked on Michael's real 35s**. It is
only for display; the engine's arithmetic rounding is unchanged.

**Default mode:** ALL (**ours**: p.1-23 calls ALL the 35s's default, though most of the manual's
examples are shown in FIX 4; Michael's priority is full precision on screen).

## Out of scope (later units)

Fractions (FDISP, chapter 5), complex-number display (p.1-24, 1-25), SHOW (p.1-25), the temporary
←ENG / ENG→ shifts (p.1-23), the menu that sets the mode, and the screen layout itself.

## Acceptance

- Every vector in `002-vectors.txt` passes, compared as exact strings. The runner (yours) reads the
  format documented at the top of that file.
- Oracle first: watch the runner fail before the formatter passes it; report the red and the green.
- Provenance as before: "UG p.N" in comments for 35s rules; "ours, unit 002" for ours.
- The formatter lives in the core, behind the engine interface (it may ask the engine to round and
  to give digits and exponent; it must not call the library directly).

## Report back (DM to abacus)

The commit, the red and green, the pages implemented, and anything ambiguous. The vectors were
checked against the reviewer's independent formatter (46/46; red under half-even rounding and on a
deliberately wrong expectation). Hidden random formatting vectors will be run at review.
