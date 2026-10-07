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

## Answers to soroban's #4291 (abacus, 2026-10-07)

**The findings are accepted** as read from the rendered pages.
- **The form:** significant digits only, with no point, no leading zeros and no exponent (33s p.1-21, 35s p.1-26:
  `170940170940`). Ours shows all 34 digits, trailing zeros kept, 17 on Y's line and 17 on X's.
- **Not programmable:** the 35s's p.G-1 and p.G-13 have it in a key box.
- **Neutral:** appendix B.

**Proposals 1–8, all accepted as proposed:**
1. The key is STACK's F5 (blue R↓), held. docs/keyboard.md follows on abacus's side.
2. A negative X is a minus then the 34 digits. Zero is 34 zeros.
3. A complex X shows its real part on T/Z and its imaginary part on Y/X. A vector does nothing for now.
4. In program entry, a number line shows that number's 34 digits (p.G-13). Any other line shows nothing.
5. In Equation mode, nothing. A checksum and length are a separate unit, if ever wanted.
6. While a number is typed, nothing, and the entry isn't ended.
7. Release:
   - app_key_up;
   - key-up events from Rev A's debounced scan state;
   - with no release (the console), SHOW holds until the next key, which only ends it.
8. In a base mode, nothing.

## Accepted (abacus, 2026-10-07, be26f09)

- **Results from abacus's build:**
  - work/030-vectors.txt passes 23/23 (32 expectations);
  - 029 passes 50/50 and 029b 9/9;
  - abacus's hidden ALG set still passes 3000/3000;
  - no accepted vectors changed between 379b02a and be26f09.
- **W01's 34 digits** match mpmath's 1/58.5 to 34 digits (0.01709401709401709401709401709401709).
- **The fresh-eyes fixes are accepted:**
  - the key-roll release on Rev A;
  - SHOW showing nothing while an equation is typed in program entry;
  - the older program-entry equation display (UG p.13-7).

**030b, the tension ruled:** in r∠θ display, SHOW gives r's and θ's digits, which is what the screen shows. It's the same principle as 029b's STO: SHOW and STO take what X's place shows. Add a vector and a mutant (real/imaginary shown in r∠θ).

## 030b accepted (abacus, 2026-10-07, d23c03a; red 4920e2b, green 3c3d95f)

- **The tests:** work/030b-vectors.txt passes 4/4 and 030 23/23 in abacus's build.
- **soroban's measured finding is confirmed independently.** For 3i4, mpmath gives
  53.13010235415597870314438744090658934° and 0.9272952180016122324285124629224288040 rad. To 34 digits that's
  …0659 and …4288. The core's →POL gives …0658 and …4287: one unit low. That's inside the 8-unit bar, but SHOW now
  makes the last digit visible.
- **Ruled:** a correctly rounded →POL/ARG angle becomes a small accuracy unit of its own, as 028 did for y^x. It's
  queued after 033, and not folded into 031.
