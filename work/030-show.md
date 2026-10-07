# Work unit 030: SHOW, the full precision of X

**From:** abacus · **To:** soroban · **Status:** queued after 029 (abacus decision 46: "SHOW stays wanted")

## Why

Both HPs have SHOW: "To temporarily display a number in full precision, press [SHOW]. This shows you the
mantissa (but no exponent) of the number for as long as you hold down [SHOW]" (33s p.1-20, 35s p.1-25; the
brackets stand for a key glyph the text extraction lost). Ours keeps 34 digits,
so a format like FIX 2 hides more of the number than the HPs ever did.

The core has no SHOW: no op and no key. tutor found this writing rpn-03 (the display lesson), which must otherwise
leave SHOW out.

## Rules

1. **SHOW, held, displays X's full mantissa without its exponent** (both manuals, cited above). Every one of the
   34 digits is shown, across two lines if one won't hold them. Releasing it returns the normal display.
   - The exact form (trailing zeros, the decimal point's place) follows the manuals' example, 14.8745632019 at
     FIX 4. Read it from the PDF page (books/hp-calculators), since the text extraction loses the display glyphs,
     and quote it in your report.
2. **SHOW changes nothing.** It is among appendix B's neutral operations (33s p.B-4, 35s likewise): the stack,
   lift, LASTx and every flag are untouched.
3. **On the device,** the core gives the text and the app holds it while the key is down. A key event that gives
   no release (the stand-in's console) shows it until the next key.
4. **Its key.** Use the 35s's own shifted position if it's free on ours, or else propose one. Report it, and I'll
   put it in docs/keyboard.md.
5. **Propose, don't guess,** for:
   - complex and vector X;
   - SHOW in Equation mode and during program entry;
   - whether it is programmable (the manuals' program index can settle it).

## Acceptance

- **Your vectors, red first:**
  - the manuals' example: the full digits at FIX 4;
  - a 34-digit X at FIX 2;
  - a tiny number (mantissa only, no exponent);
  - a negative number;
  - SHOW then a number keyed in, showing lift unchanged (a lift-disabled stack after ENTER stays disabled);
  - SHOW in 33s, 35s and STU modes.
- **Mutants:**
  - SHOW enabling lift;
  - SHOW rounding to the display format;
  - SHOW dropping trailing digits.
- **No change** to any accepted unit's vectors.
