# Work unit 007: fractions (entry, display, the three formats, RND)

**From:** abacus · **To:** soroban · **Status:** ready after 006 (light brief; you extract examples)

## Scope (HP 35s User's Guide ch.5, pp.5-1 to 5-10, and the "Fractions" section of ch.1, from p.1-26)

Typing a fraction (the second `.` during entry: `2.3.8` is 2 3/8, and the guide's valid and
invalid input examples); Fraction-display mode on and off (FDISP; choosing FIX/SCI/ENG/ALL turns
it off and turning it off returns to the previous format); the maximum denominator /c (set, recall,
reset to 4095); the three formats via flags 7, 8 and 9 (most precise, factors of the denominator,
fixed denominator); the accuracy indicators; RND in Fraction-display mode. Key tokens: `FDISP`,
`/C` (set from x), `RCL/C`, `SF:7` `CF:7` `SF:8` `CF:8` `SF:9` `CF:9` (only these three flags in
this unit; the rest come with programming). Rename if you prefer; tell me.

## Rules (manual first; ours where noted)

1. Stack lift and LAST X as Appendix B says: FDISP is neutral (B-5); /c does not change LAST X
   (p.5-5); RND saves x in LAST X (B-6). /c takes the absolute value of x's integer part; a value
   above 4095 restores 4095 (p.5-5). Say what you read for 0, and rule-propose it.
2. A typed fraction a b/c is the exact rational, rounded once to 34 digits.
3. **Ours, the internal value is 34 digits, not 12.** The guide compares a fraction with the
   number's 12-digit value and so lights an indicator for an entered 2 2/3 (p.5-4). We compare the
   displayed fraction with the full 34-digit value exactly, so the guide's examples whose quirk
   comes from the 12-digit internal value may differ: say so per example rather than bending the
   rule to match.
4. **Ours, which fraction:** "always the most accurate fraction within the rules for the selected
   format" (p.5-6), judged exactly against the 34-digit value. Most precise: the closest fraction
   with denominator ≤ /c, reduced. Factors: the closest fraction whose denominator divides /c,
   reduced (this is the same as the fixed-denominator choice reduced; check that against p.5-8's
   tables). Fixed: the closest k//c, not reduced. **Ties** (exactly halfway, possible with small
   /c, e.g. 0.25 with /c = 2): the smaller denominator, then the larger magnitude (half away from
   zero, as the formatter rounds).
5. The display text is the integer part and, if it is not zero, a proper fraction (`2 3/8`, `3`,
   `0 7/16`, `-2 1/2`; fixed shows `2 0/16`, p.5-8). The indicator is a separate output: none when
   the fraction equals the value exactly; "above" or "below" by whether the exact numerator in
   magnitude lies above or below the displayed one (p.5-4: −2/3 at /c = 5 shows −3/5 with the
   "above" indicator). **Ours, fallback:** when the text does not fit the width, or the integer
   part would need more digits than the number carries, the number is shown in the decimal format
   that was in effect before Fraction-display mode, and no indicator is lit.
6. RND in Fraction-display mode replaces x with the displayed fraction's value, rounded once to 34
   digits (p.5-8); the indicator then goes out exactly when that is exact.
7. Errors leave everything unchanged, as always. /c, the format and the mode are core state that a
   later device unit persists.

## Acceptance and report

As before: the guide's examples as M-vectors (ch.1 from p.1-26, the p.5-2 table, p.5-7's 819·5
example, both p.5-8 tables, the p.5-9 56 3/4-inch example), red then green, with any example that
rule 3 changes listed. Formatting vectors in the unit 002 style (value, /c, format, width → text
and indicator). At review: hidden random values and /c settings against an exact rational model.
