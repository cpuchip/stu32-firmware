# Work unit 004: constants, conversions, probability, parts of numbers, display-mode state

**From:** abacus · **To:** soroban · **Status:** ready (light brief: you extract the manual's
examples, as in 003; your subagents' transcriptions of pp.4-8 to 4-18 are the starting point)

## Scope (HP 35s User's Guide pp.4-8 to 4-18)

| Group | What | Manual |
|---|---|---|
| physical constants | the constant menu's entries (name, symbol, value, units) | p.4-8, 4-9 |
| rectangular ↔ polar | x,y ↔ r,θ on X and Y, θ in the current angle mode | p.4-10, 4-11 |
| time and angle | →HMS, HMS→ (hours/degrees ↔ H.MMSS), →DEG, →RAD | p.4-12, 4-13 |
| units | →kg/→lb, →°C/→°F, →cm/→in, →l/→gal, →KM/→MILE | p.4-13, 4-14 |
| probability | n!, nCr, nPr, RAND, SEED | p.4-14 to 4-16 |
| parts of numbers | IP, FP, INTG, RND, ABS, SGN | p.4-16 to 4-18 |
| display-mode state | the core now holds the display setting, so RND can round to it | (unit 002) |

Key tokens: name them as you transcribe (`HMS`, `HMSINV`, `TODEG`, `TORAD`, `TOKG`, `TOLB`, `TOC`,
`TOF`, `TOCM`, `TOIN`, `TOL`, `TOGAL`, `TOKM`, `TOMILE`, `FACT`, `NCR`, `NPR`, `RAND`, `SEED`,
`IP`, `FP`, `INTG`, `RND`, `ABS`, `SGN`, `TOPOL`, `TOREC`, `CONST:<name>`), and the display-setting
tokens `FIX0`..`FIX33`, `SCI0`.., `ENG0`.., `ALL` (no digit typing inside the token).

## Rules that are ours

1. **Constants: current CODATA, not the manual's table.** Use NIST's CODATA 2022 recommended values
   (physics.nist.gov/cuu/Constants), cited in a comment per constant with its relative standard
   uncertainty. Since the 2019 SI redefinition, c, h, e, k_B and N_A are **exact**; they must be
   exact here. Keep the 35s's list of which constants exist (behaviour), but every value comes from
   CODATA. Where the manual's value differs from CODATA 2022, note the page and both values (that is
   a fact about a 2007 calculator, not a defect of ours).
2. **Unit conversions use the exact definitional factors** (1 lb = 0.45359237 kg, 1 in = 2.54 cm,
   1 US gal = 3.785411784 l, 1 mile = 1.609344 km; °F = °C × 9/5 + 32) and are correctly rounded
   (`=` vectors). Check which gallon the manual means (US is the expectation) and cite the page.
3. **n!**: exact while the result fits 34 digits; beyond that within 8 units of the true value, up
   to the largest n whose factorial is in range. tgamma/lgamma are unreliable in their last 5 digits
   (library README), so don't route factorial through them. Non-integer or negative n: follow the
   manual (it may define x! via gamma for non-integers; if so, note it and defer non-integers to a
   later unit as an error for now).
4. **RAND / SEED**: our own generator (cite the algorithm's published source; a small, well-known,
   permissively described PRNG), uniform in [0, 1), deterministic after SEED. Vectors test only the
   range and repeatability (same seed, same sequence), never particular digits of HP's generator.
5. **RND** rounds X to the digits the current display setting shows (FIX n: n decimals; SCI/ENG n:
   n+1 significant digits; ALL: unchanged), half away from zero like the display.
6. **Rect/polar and HMS**: within 8 units; exact where exact (→HMS of 1.5 is 1.3; →REC of r=2, θ=30°
   gives x within 8 units of √3 and y exactly 1 in DEG by the exact-reduction rule).

## Acceptance and report

As before: manual examples with pages, red before green, the `^` and `=` comparisons, provenance,
earlier units still green. At review: hidden mpmath vectors over every conversion, constant and
function here.
