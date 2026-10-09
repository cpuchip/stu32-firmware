# Work unit 054: yˣ at full length: the doomed exact path skipped, small_int without text

**From:** soroban (proposed #5544) · **Ruled by:** abacus #5546 (the bound n·(d−1)+1 > 34, the named vectors, the
mutants) · **Status:** built and measured.

## Where the time went (054's brackets, before the change)

On the S3, 1,200 pow_real calls through OPBENCH, 600 of them reaching pow_xnum (a 34-digit X cubed):
- **pow_exact, about 143 µs a call,** always failing at 34 digits: the decimal128 squaring runs until a product
  rounds;
- **small_int, about 33 µs a call:** abn_digits formats the number as text and reads it back, and pow_real called it
  three times;
- pow_xnum itself: setup about 74 µs, squarings 41, rounding 157 (left alone here).

## The change

1. **The skip** (core/fn.c pow_real; abacus #5546):
   - y with d significant digits is c × 10^k, where c has d digits and is not a multiple of 10;
   - so y^|n|'s coefficient is c^|n| ≥ 10^(|n|(d−1)), and c^|n| is never a multiple of 10 either;
   - so the exact result has at least |n|(d−1)+1 significant digits;
   - past 34 the exact path must fail, so it is not tried.
2. **small_int without text** (core/abn_intel.c abn_small_int, abn_sig_digits):
   - both functions give abn_digits' answers by reading the decimal128's bits (IEEE 754-2008 3.5.2, the binary
     encoding: sign, a 14-bit exponent biased by 6176, a 113-bit coefficient);
   - the other form, which no finite canonical value takes, falls back to the text;
   - pow_real now reads small_int once, not three times (it is false for any fraction, so the abn_is_integer
     beside it was redundant).
3. **Without software division** (99891c4): the first cut (3840317) made a short X³ slower, 22.9 → 26.4 µs.
   - **Why:** its limb arithmetic divided 64 bits at every step, which the S3 does in software (about 1.1 µs a
     call, unit 045).
   - **The fix, three parts:**
     - the division runs in 32 bits while the remainder is 0;
     - a coefficient that is odd or not a multiple of 5 has no trailing zero to strip, and that is known at once
       (2³² is 1 mod 5, so c mod 5 is the limbs' sum mod 5);
     - the digit count compares with tables of the powers of ten instead of dividing.

## The oracles

- **tests/abn_test.c** (host, and on the chip through tests/device):
  - what it checks: both readings against abn_digits' text, over 1,599,800 values: edges, zeros with exponents,
    generated texts with trailing zeros across the whole range, and products, sums and quotients;
  - it did not build before the functions existed;
  - on the S3 (tests/device at e89504a): 16,104 of 16,104 readings agree, and DEVICE PASS 45/45;
  - **mutants killed:**
    - a 16-bit coefficient mask;
    - the sign dropped;
    - the integer test without e < 0;
    - the 1E9 limit at 2E9;
    - a 1E9 block counted as 8 zeros;
    - the mod-5 test as mod 3;
    - the 32-bit path's quotient taken before its remainder (it hangs; the timeout catches it);
    - the high table read one place on.
  - **Equivalent mutants:** `r > 1E8` for `r >= 1E8`, and `>` for `>=` in the low table. In both, the value
    compared has had its trailing zeros removed, so it is never exactly a power of ten.
- **tests/pow_cohort.py** compares the old core with the new:
  - the comparison is by the engine's text, so it sees the cohort as well as the value;
  - first cut: 0 differing over 520,139 cases (seeds 1–5 at 100,000, seed 54 at 20,000); the rework: 0 over seeds 1
    and 2 at 100,000;
  - the edge set gains abacus's named vectors (1.00000000001 cubed, and its d = 12, n = 3 neighbours up to
    2.15443469003, the greatest exact cube of a 12-digit base), and powers of a one-digit base past POW_XNUM_MAX.
- **tests/054-soroban.txt** runs X01–X08 through the keys:
  - the values are exact integer arithmetic;
  - an exact power's cohort is IEEE 754's ideal exponent for an exact product (the exponents summed).

**The d·n mutant, measured, not as predicted.** #5546 expected it to turn the cohort red "on exactly" 1.00000000001
cubed and its neighbours. It doesn't:
- **Why those stay green:** an exact 34-digit result has one encoding. The route the mutant sends it to (pow_xnum)
  rounds an exact value once, which changes nothing.
- **Where it is red:** exact powers shorter than 34 digits, where the exact path keeps the ideal exponent and the
  other routes return a full 34-digit coefficient. That is d = 1 with |n| ≥ 35: 2³⁵ to 2¹⁰⁰, 3⁶⁵, 0.2¹⁰⁰, (−2)¹⁰¹.
- **Counts:** the cohort shows 585 differing; X05–X08 red, X01–X04 green.
- **Consequence:** the bound's exact edge (|n|(d−1)+1 = 34) is invisible in bytes, only in time. abacus's vectors
  stay as named proofs that the edge's results are exact, and X05–X08 are what catch a bound that skips too eagerly.

## Decision 68

**Static** (resources.py static), both targets:

| commits | stand-in flash | Rev A flash | internal RAM | IRAM code | calc_task's frame |
|---|---|---|---|---|---|
| 054 first cut, 20692ce → 3840317 | +608 | +624 | +0 | +0 | +0 |
| the rework, cebee20 → 99891c4 | +448 | +448 | +0 | +0 | +0 |
| **054 in all** | **+1,056** | **+1,072** | +0 | +0 | +0 |

The flash is the two readings and, in the rework, the tables (19 × 8 + 15 × 16 = 392 B, const, in flash). No
internal-RAM symbol changed.

**On the board** (resources.py board, the stand-in):

| resource | 20692ce (before) | 3840317 (first cut) | 99891c4 (054) |
|---|---|---|---|
| internal heap free after boot, B | 62,827 | 62,827 | 62,827 |
| calc task stack free, 047's deepest site (153-unit mix / 19 SIN( levels), B | 8,180 / 8,916 | 8,180 / 8,916 | 8,180 / 8,916 |
| OPBENCH X³, X = 1.234, µs | 22.9 | 26.4 | **21.4** |
| OPBENCH X³, X of 34 digits, µs | 607.2 | 193.2 | **155.7** (−74%) |
| EQNBENCH SOLVE X^3-2×X-5, µs (best) | 34,087 | 30,600 | 31,038 (−8.9%) |
| EQNBENCH the integral, µs (best) | 54,737 | 52,656 | 52,615 (−3.9%) |
| EQNBENCH one evaluation of the cubic, X = 1.234 / 34 digits, µs | 597 / 1,602 | 509 / 1,229 | 582 / 1,246 |
| PROGBENCH SIN / LN (the control: no yˣ), ms | 270 / 376 | 267 / 377 | 270 / 385 |

**What the figures support:**
- **The X³ components** (OPBENCH, the same call timed alone) are the change's effect. A full-length cube costs a
  quarter of what it did, and a short one a little less than before.
- **The whole-evaluation totals swing between images by more than X³ can explain.** At X = 1.234 the evaluation read
  509 with the first cut and 582 with 054, while its cube moved 5 µs.
- **The control moved too:** PROGBENCH LN, which never reaches yˣ, read +9 ms. So a few percent of each total is
  the image's layout (the S3's time is mostly instruction fetch, unit 051), not this code.
- **SOLVE's −3.0 ms is less than its cubes predict.** Its 12 evaluations at about 450 µs saved each would give about
  5.4 ms. Some of the gap may be iterates that are short at the start, and some the layout noise above. That is not
  measured further here.
