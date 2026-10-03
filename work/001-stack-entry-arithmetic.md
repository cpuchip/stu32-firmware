# Work unit 001: the stack, number entry, and the four operations

**From:** abacus (fermion), 2026-10-03 · **To:** soroban (lepton) · **Status:** ready

This is the first slice of the calculator: a classic four-level RPN stack, number entry, the
four arithmetic operations, and the stack keys around ENTER. Everything else (display formats,
one-argument functions, storage, programs) comes in later units. The spec below is written in
our own words from HP's manual; the test vectors in `001-vectors.txt` are the acceptance test.

## Sources (read these yourself)

HP 35s User's Guide, `books/hp-calculators/hp35s-ug-en.pdf`. **The text extraction (`.txt`) loses
every key glyph**, so keystroke columns come out blank and some operators become `?`. Read the
keystrokes from the rendered pages (e.g. `pdftoppm -r 110 -f N -l N -png`). Manual page → PDF page:

| Manual pages | PDF pages | What |
|---|---|---|
| 1-15 to 1-17 | 31 to 33 | keying in numbers, CHS, the E key, the entry cursor, backspace |
| 2-1 to 2-8 | 47 to 54 | the stack, R↓, R↑, x↔y, lift and drop, ENTER, clearing |
| 2-8 to 2-12 | 54 to 58 | LAST X, correcting mistakes, reusing a number |
| 2-12 to 2-17 | 58 to 63 | chain calculations and the exercises |
| B-4 to B-6 | 304 to 306 | which operations disable stack lift, which are neutral, which save LAST X |

## Behaviour (our words)

1. **The stack** is four registers, X (newest), Y, Z, T (oldest), each holding one real number
   in this unit. A fresh calculator has all four at zero. X is the number being entered or the
   last result.
2. **Stack lift** is a flag, always either enabled or disabled; it decides what happens when the
   next number starts in X. If enabled, the stack lifts first (T is lost, Z→T, Y→Z, X→Y) and the
   new number goes into X. If disabled, the new number overwrites X and nothing moves (B-4, B-5).
3. **Which operations disable lift** (B-5): ENTER, CLx (clear X), clear-stack, Σ+ and Σ−
   (Σ± are not in this unit), and backspace when it acts as clear-X. **Neutral operations**
   (B-5) leave the flag alone; in this unit that means digit entry itself. **Every other
   operation enables lift** (B-4), including x↔y, R↓, R↑, LAST X and the four arithmetic
   operations.
4. **ENTER** ends entry, lifts the stack so X is copied into Y (T is lost), and disables lift,
   so the next number typed overwrites the copy left in X (2-6).
5. **Two-argument operations** (+ − × ÷) combine Y and X into one result in X and **drop** the
   stack: Z→Y, T→Z, and **T keeps its value** (T replicates). The operand order is Y op X, so
   `9 ENTER 2 −` is 7 and `10 ENTER 4 ÷` is 2.5 (2-5).
6. **R↓** rolls the contents down: Y→X, Z→Y, T→Z, X→T. **R↑** rolls up: X→Y, Y→Z, Z→T, T→X.
   **x↔y** swaps X and Y only (2-3, 2-4).
7. **LAST X** holds the X value from just before the most recent numeric operation; in this unit
   the four arithmetic operations save it (B-6). The LAST X command lifts the stack (if lift is
   enabled) and puts that value into X (2-8, and the stack diagram on 2-11). It is not in B-6's
   list of operations that save x, so it leaves the LAST X register as it was (our reading of
   B-6; the 2-11 diagram agrees: LAST X still holds 52.3947 after it is recalled and used).
8. **Number entry** (1-15 to 1-17): digits and one decimal point build a number with an entry
   cursor present. **E** starts the exponent of ten; digits after it are the exponent
   (`1 E 6` is one million; 1-16). What E does when no mantissa has been typed is not stated on
   these pages: out of scope for this unit, and not tested.
   **CHS** during mantissa entry negates the mantissa; during exponent entry it negates the
   exponent; on a completed X it negates X (the mantissa sign, if X has an exponent; 1-15).
   **Backspace** during entry removes the last character typed; when entry has ended it acts as
   clear-X (sets X to 0, disables lift) (1-17, B-5). ENTER and any operation end entry (1-17).
9. **Clearing.** CLx sets X to 0 and disables lift. Clear-stack sets X, Y, Z, T to 0 and
   disables lift (2-7, B-5). Their effect on LAST X is not stated on these pages; not tested.

## Numbers in this unit

~~The number engine (a permissive decimal library or our own) is **Michael's decision, pending**;
your charter says to ask him before adding any library.~~ **Ruled 2026-10-03 by Michael: "Lets do
A intel decimal libs."** The engine is the **Intel Decimal Floating-Point Math Library**:

- Fetch it from Intel's own distribution at netlib, and nowhere else (not from any calculator
  project that bundles it): `https://netlib.org/misc/intel/IntelRDFPMathLib20U5.tar.gz`
  (v2.5, dated 2026-09-28, 5,817,987 bytes, sha256
  `85dafd70f0fe2a8da218ade4233fca9d3228b0b04cd6d8527f7499926be01037`, checked on fermion
  2026-10-03). Verify the hash before use. Licence: BSD-3-Clause ("Copyright (c) 2007-2026, Intel
  Corp."); keep its notice with the source and in our eventual licence file.
- Use the 128-bit decimal format (34 significant digits). Rounding mode for arithmetic: the
  library's default (round-half-even) unless a later unit says otherwise.
- Still write the stack against a small number interface of your own design (make, add, subtract,
  multiply, divide, negate, parse from entry, compare, to-text), with the library behind it, so
  the core never calls the library directly and the engine stays swappable.
- No binary floating point anywhere in results (0.1 + 0.2 must be 0.3).
- If you already wrote a provisional decimal of your own, keep it only if it helps as a test
  oracle; the shipped path is the library.
- The 35s's own limits (12 displayed digits, exponents to ±499; p.1-15) are **not** binding on
  us; range and overflow behaviour come in a later unit with the engine.

## Acceptance

- Every vector in `001-vectors.txt` passes. Comparison rules are at the top of that file.
- Oracle first: the vector runner exists and fails before the stack code makes it pass; report
  the red run and the green run.
- Provenance: each behaviour above cites its manual page in a comment or `docs/spec/`, in our own
  words; commit messages carry the pages ("from 35s UG p.2-6").
- Builds and runs on lepton with the standard toolchain; portable C11, no platform calls in the
  core.

## Things in the manual to be careful with

- **A typo in the manual:** the Planck's-constant example on p.1-16 shows `6.621E-34` after keying
  6.6261. The keys typed give 6.6261×10⁻³⁴; the vector expects that, not the misprint.
- The 2-11 stack diagram uses symbolic `t` and `z` for whatever was in T and Z; vector V09 makes
  them concrete (t = 1, z = 2) by typing `1 ENTER 2 ENTER` first.
- The 35s has R↑ as a shifted key; here it is just an operation named R↑. Physical key placement
  is a separate decision (ours, not the 35s's).

## Amendment 2 (2026-10-03): rulings on soroban's questions

Review of 21ece9f: soroban's `make test` passes in a fresh Ubuntu container on fermion (library
fetched from netlib and verified); 39/39, 91/91. On 10,000 hidden random key sequences generated
by the reviewer's independent model, the core matched **all 50,000 register checks** under the
literal reading of this unit; the only vector failures were sequences that hit an error, which the
vector format could not yet express. No defect found. The rulings below change behaviour in one
place (ruling 1) and add one token to the vector format (ERR).

1. **Completing a typed number enables stack lift.** Digit entry stays neutral while you type, but
   when an operation ends the entry, lift is enabled before that operation acts. So
   `1 ENTER 2 LASTX` keeps the 2: X = LAST X, Y = 2, Z = 1 (vectors V40, V41). The manual does not
   show this case; HP practice such as keying a number and then recalling a register to multiply
   it depends on it. **Provisional until checked on Michael's own HP 35s.**
2. **Backspace removes keys, not CHS presses;** erasing every key ends entry with X = 0 and lift
   disabled, and the sign is discarded (V42, V43). Provisional until checked on his 35s.
3. **E with no mantissa digit typed is ignored** (V44). Provisional until checked on his 35s.
4. **CHS on a completed X enables lift** (B-4; it is not in B-5's neutral list) (V45).
5. **Entry limits and errors, as soroban proposed:** 34 mantissa digits and 4 exponent digits,
   further keys ignored; a key that would take the value outside the normal decimal128 range is
   ignored; a failed operation (÷0, result out of range) leaves X, Y, Z, T and LAST X unchanged and
   reports an error, after completing any number being typed (V46 to V49).
6. **Vector format: the `ERR` token.** In the expectations column, `ERR` means at least one
   operation in the vector returned an error. Without `ERR`, any error fails the vector (the
   runner's current behaviour). V49 uses it.

On the manual: credit where due, keep naming it only in commit messages; code comments citing
"UG p.N" are right. Not vendoring the library is right too: the fetch script and its pin are the
provenance.

## Report back (DM to abacus)

The commit, the runner command with its red and green output, the manual pages implemented, and
anything in the manual you found ambiguous (with page).
