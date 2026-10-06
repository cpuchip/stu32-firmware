# Work unit 028: y^x correctly rounded everywhere

**From:** abacus · **To:** soroban · **Status:** ready, after 027

## Why

Unit 026's review found that the engine's pow is off by millions of units in the 34th digit for a base near 1 with
a large exponent (soroban #4219: `1.000…001^1E35`). Near the range edges, 026 already routes y^x through the hp
path, which was correctly rounded on all 500 cases a reviewer sent through it. Everywhere else the engine's pow is
still used. Accuracy is the core's promise (units 003 and 021: within 8 units in the 34th digit, judged by mpmath), so
a function that can miss by millions somewhere must be fixed everywhere, not only where one case was found.

## Rules

1. **y^x within 8 units in the 34th significant digit for every real y and x where the result is defined and in
   range.** The same holds for every function built on pow: x√y (XROOT), 10^x and e^x if any route through it, and
   the equation engine's `^`.
2. **Choose the route** (the hp path everywhere, or the engine's pow with a guard that detects the bad region and
   switches). Measure the cost of each on the S3 and say why you chose.
3. **Integer exponents** keep exact results where they're exact today (2^10 = 1024 exactly), and negative bases with
   integer exponents keep their sign.

## Acceptance

- **A hidden set from abacus** (mpmath at 120 digits): bases within 1E-30 to 1E-3 of 1 with exponents up to 1E40;
  bases near the range edges; integer and half-integer exponents; negative bases with integer exponents; random pairs.
- **Your own fuzzer** (extend tests/range_fuzz.py, or a new pow_fuzz.py, in `make accuracy`): at least 20000 cases
  across the same regions.
- **No change** to any accepted unit's vectors except where they encoded the old error (say which, amended in place).
- **Mutants:** the bad-region guard removed (if you choose the guard), and an exact integer power returning
  inexactly.
- **The S3 cost:** y^x's time before and after, typical and worst.
