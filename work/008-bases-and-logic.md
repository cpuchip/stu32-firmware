# Work unit 008: base modes, base arithmetic and logic

**From:** abacus · **To:** soroban · **Status:** ready after 007 (light brief; you extract examples)

## Scope (HP 35s User's Guide ch.11, pp.11-1 to 11-8; Appendix B for stack lift and LAST X)

The BASE menu (DEC, HEX, OCT, BIN, and the suffixes d, h, o, b that type a number in another base
from any mode); HEX digit entry (A-F); display of a number in each base; arithmetic + − × ÷ in the
non-decimal bases; the LOGIC menu (AND, XOR, OR, NOT, NAND, NOR); the 36-bit two's complement
representation, its range (p.11-7) and overflow. Key tokens (rename if you prefer; tell me): `DEC`
`HEX` `OCT` `BIN`; `HA`..`HF` for the digits A-F; `SUF:b` `SUF:o` `SUF:d` `SUF:h` for the typed
suffix; `AND` `XOR` `OR` `NOT` `NAND` `NOR`.

## Rules (manual first; ours where noted)

1. Stack lift and LAST X as Appendix B says (B-5 lists DEC, HEX, OCT, BIN as neutral; read B-6 for
   whether the logic functions save x; say what you find).
2. Changing the base changes only the display; the number in X keeps its fractional part and is
   shown again in DEC (p.11-2). In HEX/OCT/BIN the display shows the integer part as a 36-bit two's
   complement pattern without leading zeros, with its suffix (p.11-6: 125 is `1111101b`).
3. Arithmetic in HEX/OCT/BIN uses the integer parts and truncates the result to an integer (p.11-4,
   11-5: 100o ÷ 5o is 14o). A result outside the range (p.11-7: −34,359,738,368 to 34,359,738,367)
   is replaced by the largest positive or negative number, and the operation reports overflow;
   **ours:** that overflow is a reported condition with the substituted result kept, not an
   error that leaves the stack unchanged, because the manual says the number is substituted.
   Vector both. Read which functions the guide deactivates in HEX (p.11-4) and make them errors in
   that mode.
4. Logic functions work on the integer parts as 36-bit patterns and return the signed value of the
   36-bit result (p.11-4: NOT(1011b) is 111…10100b, which is −12 in DEC). In DEC mode they work the
   same way on the integer parts. Arguments outside the range are first replaced as rule 3 says.
5. Typing: in a non-decimal base, digits outside the base are ignored, and a number outside the
   range cannot be entered (p.11-7: say what the guide shows for the key that would leave the
   range). A suffix typed after a number makes it that base's value, in any mode (p.11-3).
6. **Ours, the formatter:** the core's formatting function returns the whole pattern (up to 36
   binary digits); the 14-digit windows and their scrolling (p.11-8) are a device unit's display.
7. **Ours, provisional, for Michael:** the word size stays 36 bits, as the 35s, so its programs and
   examples hold. A settable word size (to 64, as programmers' calculators allow) is a later choice
   and should not be designed out: keep the 36 in one named constant.

## Acceptance and report

As before: the guide's examples as M-vectors (p.11-2/11-3 conversions of 125.99 and 24FFh, p.11-4
logic examples, p.11-5/11-6 arithmetic in each base, p.11-6/11-7 negative numbers and the range
table), formatting vectors for the base display, red then green. At review: hidden random
integers, mixed bases and logic chains against a Python-integer model.
