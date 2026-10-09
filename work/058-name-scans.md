# Work unit 058: the equation evaluator's per-token cost (the parser's name scans)

**From:** soroban (proposed #5783, from a throwaway S3 bench) · **Ruled:** abacus #5784 · **Status:** built and
measured (f57c35f, 87fc03b).

## What cost the time

On the S3, 200 evaluations each, X = 1.234 (µs per evaluation):

| expression | STU | 35s |
|---|---|---|
| "5" | 43.0 | 9.0 |
| "X" | 74.3 | 39.9 |
| 8 × X | 671.7 | 402.8 |
| X^3−2×X−5 | 559.3 | 435.1 |

- **Every STU operand paid about 34 µs:** parse_mul called implied_here after it, which scanned all 84 NAMES (a strlen
  and a strncmp each) for a function name to multiply by.
- **Every variable paid about 31 µs in each model:** parse_prim scanned them before deciding X was a variable.

## The change

- **name_could(k, s)** compares a name's first byte before its strlen and strncmp, in parse_prim, implied_here and
  the variable scan.
  - A name is never empty, so it only skips what strncmp would refuse: each scan matches what it matched before,
    first and longest match unchanged.
  - eqn_name_before is left as it was: it matches backwards from a place set by each name's length, and its only
    caller is the editor's backspace over a name.
  - No index table: no RAM, and nothing in flash beyond the code.
- **name_at** (87fc03b) holds parse_prim's longest-match loop, noinline.
  - Inlined, the first-byte test grew parse_post's frame from 240 to 256 B (xtensa objdump, `entry a1`). Every level
    of a nested equation holds that frame, so 047's deepest site lost 288 B and 19 SIN( levels lost 304.
  - Out of line, the frames are as before (name_at has its own 64 B, off the recursive path), as 049 moved
    parse_prim's rare branches out.

## The oracle

- **tests/namedrive.c** prints every public reading of names:
  - eqn_eval, eqn_vars and eqn_has_chain in models 33, 35 and STU;
  - eqn_write_implied;
  - eqn_name_before at each position.
- **tests/name_cohort.py** runs two builds on abacus's set, and the old eqn.c against the new gave 181,625 inputs,
  0 differing (also at 87fc03b). The set:
  - every name and every prefix of one;
  - each name followed by each byte 1–255;
  - each byte before a name, alone, after X, and between 2 and a call;
  - operands (2, X, (X), 2.5, ), π) right before each name, bare, called, and in Y=;
  - 100,000 random strings over the names' bytes, with digits and operators.
- **Positive controls,** each caught: a first byte missed for the arrow names (3,570 lines differ), and for S (1,976).
- **Equivalent:** name_in skipped in implied_here. Even with inputs aimed at it there are 0 differences: a wrongly
  implied product then fails in parse_prim at the same place, with the same status.
- **all-checks passes:** eqn_fuzz 2000/2000; eqn_depth_fuzz and eqn_stack_fuzz unchanged.

## Decision 68

| | stand-in | Rev A |
|---|---|---|
| flash (e127909 → 87fc03b) | +96 | +96 |
| internal RAM, IRAM code, calc_task's frame | +0 | +0 |

**On the board** (resources.py board, the stand-in, e127909 → 87fc03b):

| resource | before | after |
|---|---|---|
| internal heap free after boot, B | 62,827 | 62,827 |
| stack free, 047's deepest site / 19 SIN( levels, B | 8,180 / 8,916 | 8,180 / 8,916 |
| OPBENCH evaluating "X" / "5", µs | 73.8 / 42.8 | 15.7 / 14.5 |
| OPBENCH the cubic's evaluation, X = 1.234, µs | 539.7 | 335.8 (at f57c35f) |
| EQNBENCH SOLVE X^3-2×X-5, µs (best) | 31,011 | 29,308 (−5.5%) |
| EQNBENCH the integral, µs (best) | 52,218 | 50,359 (−3.6%) |
| PROGBENCH SIN / LN (the control), ms | 269 / 383 | 269 / 380 |

f57c35f, before name_at, read SOLVE −6.9% and the integral −8.0%. The difference is within this bench's layout noise
plus name_at's call.

## 049's question re-measured (abacus #5784: a measurement only)

The parse share of one evaluation (EQNBENCH's parse against its evaluation):

| | e127909 | 87fc03b |
|---|---|---|
| X^3−2×X−5, X = 1.234 | 187 of 518 µs (36%) | 42 of 364 (11.5%) |
| X^3−2×X−5, X of 34 digits (SOLVE's iterates) | 187 of 1,250 (15%) | 42 of 1,104 (3.8%) |
| SIN(X)^2+X×EXP(−X), 1.234 / 34 digits | 323 of 5,708 (5.7%) / 323 of 6,657 (4.9%) | 71 of 5,493 (1.3%) / 71 of 6,428 (1.1%) |

After 058, parsing once would save at most about 4% of SOLVE and about 1% of the integral. The number doesn't argue for
it.
