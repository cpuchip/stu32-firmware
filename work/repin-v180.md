# Repin: Casimir 1.8.0 (casimir c564e41, the v1.8.0 tag)

**From:** abacus #16902 (under Michael's #10856 rule: a repin is soroban's, no card; abacus replays it). **Status:**
built at 0a9b505; results below.

## What changes

- **CASIM_PIN is c564e411d208a2960ee222590c4dd90bcee1c5bf,** the commit of the v1.8.0 tag. The tag object is d189385.
  In casim's clone, `git tag --points-at c564e41` gives v1.8.0, and origin/main contains it. The clone was read, never
  fetched into.
- **1.8.0's source over 8a76a80** (`git diff --stat 8a76a80 c564e41 -- src`): 2 files, 550 insertions and 12
  deletions, casim.c 555 lines and casim.h 7. casim.h's are CASIM_VERSION "1.8.0" (minor 8) and one new rule,
  CASIM_RULE_PURE_POWER, after CYCLE.
- **No firmware code changes.** VER stays 0.100, as each repin has kept VER. The steps page reads the rule's name from
  casim_rule_name (core/calc.c:5443, tests/vectors.c:1121), so PURE POWER needs no table (casim #16840).
- **What moves through the keys** (CAS 041, casim's work/release-1.8.0.md):
  - x^n = c with numbers gives its real roots exactly, where 1.7.0 printed PARTIAL;
  - a solve with letters in a quadratic or a pure power is answered with its rows, where 1.7.0 refused it COEF NOT
    NUMERIC;
  - a PURE POWER steps page comes just before the answer;
  - a letter solve's steps are refused (casim's BAD_ARG), which the firmware shows as UNSUPPORTED, the report still
    open.

## Tests

**tests/repin-v180-soroban.txt (new, in make test): 19 vectors, 31 expectations.**
- The expected texts are casim 1.8.0's notes: x^n = c, X^2=A, X^3=A, the letter quadratic, and the three PURE POWER
  pages. The values are by hand: 64^(1/9) = 4^(1/3) = 1.587401051968199474751705639272308 to 34 digits (Python's decimal
  at 50 digits), 4096^(1/12) = 2, and SQRT(9) = 3.
- **R01-R11:** the answers, EVAL on a root (R02, R07), and 048's two moved equations.
- **P01-P04:** the three PURE POWER pages and a letter solve's refused steps.
- **K01-K04:** controls, read the same on both pins.
- Every line was read on both pins before the file was written.
- **On the old pin's build** (a scratch worktree at 4708e3f, pin 8a76a80): R01-R11 and P01-P04 fail, 15 of 19, and
  K01-K04 pass. The run's rc is 1 (evidence: old-pin-run.txt).

**Amended in place, each with a note naming this repin:**
- **tests/048-soroban.txt S09 and S12:** casim's own examples V0017 (`A×X^2-1`) and V0014 (`X^3^2-64`). They moved in
  casim's work/005-examples.txt at c564e41, and the vectors now read as those examples do.
- **tests/088-soroban.txt R01:** "SOLVE refused, a letter, on the stack" used `A×X^2-1`, which solves now. Its
  refusal is `A×X^3+X=1` (degree 3 with a letter, still NOT NUMERIC by casim's notes), checked on both pins.
- **tests/steps_test.c:** unit 095's census says an answered op's pages must all come. CAS 041 refuses a letter
  solve's steps BAD_ARG by contract. Only that case (a solve, a letter other than the variable standing alone in the
  text, BAD_ARG) is counted apart. Any other refusal is still bad.
  - The census without the allowance: 1 bad, `s X A×X^2-1` (census-before-amends.txt). With it: 0 bad, 1 counted
    apart.
  - A five-line plant counted `X^2=A` and `X^3=A` apart, paged `X^9=64` and `d X SIN(A×X)`, and kept `SQRT(X)=2` as
    an op refusal.
  - The allowance has no mutant run of its own. No case in the census or the plant has its pages refused by something
    other than BAD_ARG.

**The census** (`make -i test` on the new pin before any amendment, census-before-amends.txt): three steps failed,
and nothing else moved:
- 048 (S09, S12);
- 088 (R01);
- steps_test (1 bad).

tbook_test read the new pin's golden corpus with nothing left unparsed: 4,871 answers, 39/39 checks. I didn't check
which of casim's 10 moved lines are among them.

## Results

**1. make test** at 0a9b505 (evidence: make-test.txt, 678 lines): rc 0.
- 22,184/22,184 vectors and 61,257/61,257 expectations, summing every `n/m vectors` and `n/m expectations` in the
  output.
  - That's 4708e3f's 22,165 and 61,227 plus the new file's 19 and 31, less 1 from 048 (185 to 184 expectations:
    S09's error fields became one %CASREP).
- screen_test: 9,529/9,529 checks, 67/67 frames. The stand-in replay matches all 51 scripts.

**2. make clang-check** (evidence: clang-check.txt): rc 0, 76 files, 0 failing, the control refused. firmware/wasm/
wasm_app.c isn't compiled, because there is no build/wamr, the same as at 1.7.0.

**3. D68,** `scripts/resources.py static 4708e3f 0a9b505` (evidence: d68.txt):

| | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | 2,403,040 -> 2,414,528 (+11,488) | 2,329,712 -> 2,341,536 (+11,824) |
| DIRAM, IRAM code, calc_task's frame | +0 | +0 |
| libmain.a | +11,478 | +11,782 |

- **Room:** the factory partitions are 0x500000 (5,242,880 B, stand-in) and 0x700000 (7,340,032 B, Rev A).
- **casim.c.obj's loaded sections** (address non-zero, debug left out; evidence: casim-size.py, casim-size.txt):
  - Rev A: 235,886 -> 247,355 (+11,469).
  - Stand-in: 236,018 -> 247,539 (+11,521).
  - Both are under decision 94's 301,056 B line. casim's notes give 283,838 B (+13,428) for its own S3 measure, a
    different method.
- **Non-casim lines that moved in the linked maps:**
  - Rev A's calc.c: solve_core +308, do_key +40, ev +4, prog_run_lines +4.
  - The stand-in's state.c: two literal pools, +16 and −16.
  - Both maps: a merged-string section booked to esp_app_desc.c.obj, +48.
  - A few 4 B literals in the IDF's own objects.
- **The objects' own sections are the same.** No source changed (the commit touches tests only).
  - calc.c.obj's .text, .literal and .rela of solve_core and do_key, and the .text of ev and prog_run_lines, are byte
    for byte the same in all four builds.
  - So are state.c.obj's ab_state_load and parse sections on the stand-in.
  - (evidence: obj-section.py, calc-objects.txt)
  - So, as at 1.7.0, these moves are the Xtensa linker's relaxation and layout once casim grew, not changes of code.
  - The +48 string section isn't traced. A merged-string section is booked to the first object that holds one, and
    casim's new texts are a likely tenant. That is unverified.
- The containers peaked at 460.8 MiB (41 samples through soroban's shim, 4 GiB cap, --rm; container-peak.txt).

**4. The boards:** not run.
- No frame changed: 67/67, and the replay matches.
- The answers that move are CAS reports and steps pages, and their host vectors run through the same core the device
  runs.
- casim's own S3 bench at the tag passes 1,499 cases byte-identical to the host. Its worst stack is 11,760 of a 12 KiB
  task, unchanged from 1.7.0 by its notes. The calc task's stack is 32 KiB on both boards.

## For abacus: one tension, not a fault

With CAS3's CPLX off, `X^4=-16` is now `NO REAL ROOTS. CAS3 CPLX ON`. Unit 076's hint names where the complex roots
are turned on. With CPLX on, the same solve is `PARTIAL, DEGREE 4 LEFT` (K03, on both pins), so the hint leads to less.
- The same holds for odd powers. `X^3=2` with CPLX off is `SOLVED, 2 CPLX: X=2^(1÷3)` (R05). With CPLX on, it's
  `PARTIAL, DEGREE 3 LEFT` (K04), and the real root is lost.
- 1.7.0 printed PARTIAL for all of these, on or off, so the hint only now reaches a degree-4 NO REAL ROOTS.
- No value is wrong.
- **The choices:**
  - leave it;
  - the firmware gives the hint only where CPLX would answer, which costs a second solve;
  - casim answers x^n = c under its complex bit.
- **Who decides:** abacus and casim. Nothing here changes the firmware's behaviour.

**Claims:** none needed. The containers (D68's builds and clang-check) are soroban-repin-* through my shim, under the
small-container rule, and recorded in my lane.
