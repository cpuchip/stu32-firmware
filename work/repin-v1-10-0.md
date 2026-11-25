# Repin: Casimir 1.10.0 (casimir 2e62056, the v1.10.0 tag)

**From:** abacus #17447 (under Michael's #10856 rule: a repin is soroban's, no card; abacus replays it). **Status:**
built at 7edf8f8; results below.

## What changes

- **CASIM_PIN is 2e620567905e58b5752032f31651f5c4b6b22a52,** the commit of the v1.10.0 tag (tag object f693e9c).
  origin/main contains it in casim's clone, which was read, never fetched into.
- **1.10.0's source over cf11f9f:** 2 files, 146 insertions and 6 deletions. casim.h's change is the version text
  alone: "1.10.0", minor 10.
  - No firmware code reads CASIM_VERSION or casim_version(). The pin is a commit hash, cut to 7 characters for VER.
- **No firmware code changes.** VER stays 0.103.
- **What moves through the keys** (CAS 043, casim's work/release-1.10.0.md): with CAS3's CPLX on, x^n = c prints every
  root where the line holds them, else the line without the bit, never less. The PURE POWER steps page runs under the
  bit too.
- **This answers the tension the 1.8.0 repin raised** (work/repin-v180.md, "For abacus").
  - With CPLX off, 076's hint "NO REAL ROOTS. CAS3 CPLX ON" now leads to an answer.
  - X^3=2 under the bit now has its real root and the pair, where it had lost the root.

## Tests

- **The census** (`make -i test` on the new pin before any test was written, census.txt): two lines moved, and both
  are the 1.8.0 repin's controls K03 and K04, which pinned PARTIAL under CPLX.
  - Amended in place in tests/repin-v180-soroban.txt, each with a note naming this repin, to the new lines.
  - Nothing else moved: steps_test, tbook and every other file are unchanged.
- **tests/repin-v1-10-0-soroban.txt (new, in make test): 13 vectors, 16 expectations.** The expected texts are casim
  1.10.0's notes.
  - **R01-R06:** every root of X^3=2, X^4=-16, X^6=-1, X^5=32 (RAD's cosine form and DEG's COS(72)) and X^12=4096.
  - **R07:** EVAL on X^3=2's second root, a complex X.
    - Its value was computed apart: Python's decimal at 60 digits, 2^(1/3)×(−1−i√3)÷2, whose cube is 2 to 59
      digits.
    - It is compared within 8 units of the 34th digit, as 076 compares a complex X.
  - **R08:** a root's own row.
  - **R09:** X^9=64, whose roots don't fit, so it prints the line without the bit (1.9.0: PARTIAL).
  - **P01:** the PURE POWER page under the bit, with the three roots.
  - **K01-K03:** controls. X^3=1 and X^3=A as at 1.9.0, and 076's hint with CPLX off.
  - Every line was read on both pins before the file was written.
  - **On the old pin's build** (a scratch worktree at fde1720, pin cf11f9f): R01-R09 and P01 fail, 10 of 13, and
    K01-K03 pass. rc 1 (old-pin-run.txt).

## Results

**1. make test** at 7edf8f8 (make-test.txt): rc 0.
- 22,363/22,363 vectors and 61,621/61,621 expectations, summing every `n/m vectors` and `n/m expectations`. That's
  fde1720's 22,350 and 61,605 plus the new file's 13 and 16 (K03 and K04 amended, not added).
- screen_test 67/67 frames; the replay's 51 scripts; app_test 919/919.

**2. make clang-check** (clang-check.txt): rc 0, 76 files, 0 failing, the control refused.

**3. D68,** `scripts/resources.py static fde1720 7edf8f8` (d68.txt), the first repin with D68's object report
(work/d68-object-check.md):

| | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | 2,423,536 -> 2,426,928 (+3,392) | 2,351,200 -> 2,354,736 (+3,536) |
| DIRAM, IRAM code, calc_task's frame | +0 | +0 |
| libmain.a | +3,397 | +3,489 |

- **casim.c.obj's loaded sections** (work/repin-v190-evidence/casim-size.py):
  - Rev A: 256,207 -> 259,530 (+3,323).
  - Stand-in: 256,347 -> 259,746 (+3,399).
  - Both are under decision 94's 301,056 B line.
  - casim's notes give 297,742 B for its own S3 measure, **3,314 B under the line**: the next unit that needs more
    brings its size to Michael first.
- **Moved outside the changed sources,** by D68's report: Rev A's calc.c fn_first +24, gk15 +96 and integ_core +48, and
  the stand-in's rtc_init literal −4. Each reads **"object identical"**: layout, not code, now said by the gate
  itself.
- The containers peaked at 794.7 MiB (60 samples, soroban-repin-* through my shim, 4 GiB cap, --rm).

**4. The boards:** not run.
- No frame changed (67/67, the replay's 51).
- The answers that move are SOLVE's reports under CPLX, through the same core the device runs.
- casim's own S3 bench at the tag passes 1,516 cases, the first under the bit. Its worst stack is 11,744 of a 12 KiB
  task. The calc task's stack is 32 KiB on both boards.

**Claims:** none needed (the small-container rule, recorded in my lane).

## After acceptance (abacus #17461)

Accepted at 3a0a001. abacus's replay: make test rc 0, 22,363/22,363 and 61,621/61,621. With the pin back at cf11f9f,
R01-R09 and P01 are red and K01-K03 green (3/13). firmware/ and core/ are untouched. The K03/K04 amendment pins T113's
answer where the test pinned PARTIAL.
