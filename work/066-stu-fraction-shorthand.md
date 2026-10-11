# Work unit 066: STU takes the 33s's fraction shorthand

**From:** Michael's decision 83, item 1 (abacus #6765: "the stu32 mode should borrow the short hand . . From the 33s
for 5 . . 8") · **Gate:** soroban · **Status:** built and measured; abacus's unit001 model (MODE33_FRACTION) gains STU
for the hidden sets.

## What it is

In 33s mode a fraction typed with no numerator digit takes its integer part as the numerator: 5 . . 8 is 5/8, as
Michael's 33s keys it (FC2, abacus #5550). STU now reads it the same way. The 35s still reads 5 . . 8 as 5 and no
fraction (the 35s keys 5/8 as 0 . 5 . 8: work/024-review/functions/vectors.txt C05).

What does not move:
- FC3, the 33s's four-digit denominator, stays 33s-only: STU and the 35s keep taking a fifth digit.
- The 33s's E with nothing typed (it starts as 1 E) stays 33s-only.

The change is one predicate, `short_frac(c)`, which is `!m35 || stu`. It is passed to `entry_value` at all four call
sites: the entry, a complex number's first part, a vector's elements, and a base suffix's decimal. Before, all four
passed `!m35`, and STU sets m35.

## The checks

- **Oracle first,** in tests/fc-soroban.txt:
  - F05 flips from X=5 to X=0.625.
  - F14-F17 are new: 16 . . 8 = 2; 5 . . 8 +/- = -0.625; 0 . 5 . 8 = 0.625; and 5 . . 12345 = 5/12345, where STU
    keeps the fifth digit. The exact values are from Python's decimal at 34 digits.
  - Before the change, F05, F14, F15 and F17 were red and F16 was green, as predicted.
- **A mutant** with the shorthand in every mode turned F02 (the 35s's 5 . . 8 = 5) red. Restored, green.
- `make test` green (app_test 668/668; the vector files with save/load and the store), and `make clang-check` 62
  files, 0 failing.

## Decision 68

**Static, `scripts/resources.py static 7e0951d 9aee6d3`:**

| | stand-in | Rev A |
|---|---|---|
| flash | +0 | +16 |
| static internal RAM (DIRAM) | +0 | +0 |
| IRAM code | +0 | +0 |
| calc_task's frame | +0 | +0 |

libmain is +8 on both; no internal-RAM symbol moved.

**At run time, `scripts/resources.py board 7e0951d 9aee6d3 --targets standin,reva`:** heap and its largest block +0 on
both boards. Rev A head-less masked stack +0 on every row: the figure is 8,196 (the mix in all three states), SIN in RAD
8,916. The stand-in's unmasked rows are +0 (8,180, and 8,900 for SIN).

Rev A's unmasked rows moved with the new binary's timing, as the gate's footnote says they do: SIN in RAD 8,724 to
8,916 (factory and full), the full mix 8,052 to 8,132. The interrupt's largest cost on a row was 192 before and 64
after, under the 384 allowance, and the run exited 0. That makes it the masked gate's first green run on the board
(abacus #6723; the red at a planted 32 was 7e0951d's own check).

Speed (best of the bench), stand-in / Rev A:

| | stand-in | Rev A |
|---|---|---|
| SOLVE | 30,034 to 30,349 µs (+1.0%) | 17,005 to 16,899 µs (-0.6%) |
| integral | 51,296 to 50,985 µs (-0.6%) | 28,807 to 29,118 µs (+1.1%) |
| PROGBENCH SIN | 270 to 270 ms | 206 to 207 ms |
| PROGBENCH LN | 384 to 384 ms | 255 to 253 ms |

The moves go both ways, within a few percent, and none of these paths types a fraction: layout and timing, not the
change.

The version bump (AB_FW_VERSION "0.66", the second commit) is a string of the same length. Its static diff against
the first commit is below.

**The bump, `scripts/resources.py static` from the first commit to the second:** +0 on both boards in flash, DIRAM, IRAM
code and calc_task's frame, and no internal-RAM symbol changed. So the unit's static cost is the first commit's (+0 /
+16 flash), and the run-time readings above stand for the unit.
