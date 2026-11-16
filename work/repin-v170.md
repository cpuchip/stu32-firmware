# Repin: Casimir 1.7.0 (casimir 8a76a80, the v1.7.0 tag)

**From:** abacus #15159 (basecamp's call, #15158), muster ticket T9 (abacus reviews). **Status:** built at 9b72b35;
results below.

## What changes

- **CASIM_PIN is 8a76a80e5cd3e3bac574a93498c8c3c2838d22f8,** the commit of the v1.7.0 tag. The tag object is 346cb36.
  In casim's clone, `git tag --points-at 8a76a80` gives v1.7.0, and the commit is on origin/main. The clone was read,
  never fetched into.
- **1.7.0's source over 175c160** (`git diff --stat 175c160 8a76a80 -- src`): casim.c +40 −3, casim.h +4 −3.
  - casim.c is CAS 040: solve refuses NOT_NUMERIC where X's coefficient reads 0 at its fixed points.
  - casim.h carries CASIM_VERSION "1.7.0" (minor 7) and the comment on NOT_NUMERIC.
- **No firmware code changes.** VER stays 0.100, as each repin has kept VER.
- **What moves through the keys:** a solve whose coefficient is 0 for every value of its letters.
  - **tests/repin-v170-soroban.txt (new, in make test):**
    - V01: the Pythagorean identity, `(SIN(A)^2+COS(A)^2-1)×X=1` with CASSOLVE X, is COEF NOT NUMERIC on 1.7.0.
    - V02, the control: `(A+1)×X=1` reads `SOLVED: X=1÷(A+1); A≠-1` on both pins.
  - **On the old pin's build** (a scratch worktree at 674f66a): V01 fails, because 1.6.0 gives SOLVED X=1÷(SIN(A)^2+
    COS(A)^2-1) with the condition that the coefficient isn't 0. V02 passes there. The run's rc is 1
    (evidence: old-pin-run.txt).
- **No other vector moved.** make test on the new pin, before the file was added, gave the same totals as 674f66a's:
  22,163/22,163 vectors and 61,225/61,225 expectations. Nothing in the suite had reached CAS 040's case. The two
  STU-facing rows abacus names (the identity, h038 and h039) are in abacus's hidden sets for casim's units, not
  this firmware's (wording corrected per abacus #15209).

## Results

**1. make test** at 9b72b35 (evidence: make-test.txt, 677 lines): rc 0.
- 22,165/22,165 vectors and 61,227/61,227 expectations, summing every `n/m vectors` and `n/m expectations` in the
  output. That's 674f66a's totals plus the new file's two.
- build/casim/.pin is 8a76a80, and casim.h says 1.7.0.

**2. make clang-check** (evidence: clang-check.txt): rc 0, 76 files, 0 failing, the control refused.
- One file isn't compiled here: firmware/wasm/wasm_app.c, because there is no build/wamr. That's the same with either
  pin.
- The container peaked at 140.8 MiB.

**3. D68,** `scripts/resources.py static 674f66a 9b72b35` (evidence: d68.txt):

| | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | 2,402,448 -> 2,403,040 (+592) | 2,329,104 -> 2,329,712 (+608) |
| DIRAM, IRAM code, calc_task's frame | +0 | +0 |
| libmain.a | +587 | +603 |

- The containers peaked at 597.5 MiB.
- **casim.c.obj's loaded sections** (address non-zero, debug left out; evidence: casim-size.py, casim-size.txt):
  - Rev A: 235,299 -> 235,886 (+587).
  - Stand-in: 235,431 -> 236,018 (+587).
  - Both are under decision 94's 301,056 B line.
  - abacus's 270,410 B ("605 B over 1.6.0") is casim's own S3 measure, a different method. The +587 here is the
    firmware's map.
- **One non-casim line moved:** Rev A's `.text.do_key` (calc.c), 12,322 -> 12,338 (+16) in the linked map.
  - calc.c uses none of casim.h's changed lines.
  - Its object's `.text.do_key`, `.rela.text.do_key` and `.literal.do_key` are byte for byte the same in all four
    builds: 12,566 B, with the same sha256 (evidence: obj-section.py, do_key-objects.txt).
  - So the +16 is the Xtensa linker's relaxation landing differently once casim's growth moved the layout. It isn't
    a change of code. The stand-in's map shows no such line.

**4. The boards:** not run.
- No frame changed: screen_test is 67/67, and the stand-in replay matches all 51 scripts.
- The one answer that moves is a CAS report. Its host vector is V01, through the same core the device runs.
- abacus's acceptance (#15159) is make test, the old-pin comparison and casim.o's size.

**Claims:** none needed. The two containers (clang-check and D68's builds) are soroban-* through my shim, 4 GiB cap,
--rm, under the small-container rule, recorded in my lane.
