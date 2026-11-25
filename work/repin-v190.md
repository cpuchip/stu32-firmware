# Repin: Casimir 1.9.0 (casimir cf11f9f, the v1.9.0 tag)

**From:** abacus #17261 (under Michael's #10856 rule: a repin is soroban's, no card; abacus replays it). **Status:**
built at e5cf3f1; results below.

## What changes

- **CASIM_PIN is cf11f9fd801f390cd74076f27dc865f52d32871f,** the commit of the v1.9.0 tag (tag object acbe993).
  origin/main contains it in casim's clone, which was read, never fetched into.
- **1.9.0's source over c564e41** (`git diff --stat c564e41 cf11f9f -- src`): 2 files, 498 insertions and 4
  deletions. casim.h's change is CASIM_VERSION "1.9.0" (minor 9) alone: no API, no new steps rule.
- **No firmware code changes.** VER stays 0.103, as each repin has kept VER.
- **What moves through the keys** (CAS 042, casim's work/release-1.9.0.md):
  - integrate (∫DX) answers powers and products of SIN and COS of multiples of one angle, where 1.8.0 refused them
    (CAN'T INTEGRATE here);
  - their steps are refused (casim's BAD_ARG), shown as UNSUPPORTED with the request open.

## Tests

- **The census** (`make -i test` on the new pin before any test was written, census.txt): nothing moved. No step
  failed; steps_test is unchanged (its corpus has no SIN or COS power).
  - tbook_test reads 4,873 answers from the golden corpus, where it read 4,871 at 1.8.0: two of casim's moved lines
    now have answers, and they draw.
  - So no vector of ours reached CAS 042, as with 1.7.0.
- **tests/repin-v190-soroban.txt (new, in make test): 14 vectors, 16 expectations.**
  - The expected texts are casim 1.9.0's notes. Each was checked by hand by its derivative:
    - X÷2-SIN(2×X)÷4 gives 1÷2-COS(2×X)÷2 = SIN(X)^2;
    - COS(X)^4's 3×X÷8+SIN(2×X)÷4+SIN(4×X)÷32 gives (3+4×COS(2×X)+COS(4×X))÷8;
    - DEG's and GRAD's 45÷π and 50÷π are 180÷(4π) and 200÷(4π).
  - **R01-R10:** the answers in RAD, DEG and GRAD, with a letter divisor.
    - R10 reads the original above the answer. It fails on 1.8.0 only because the ∫ errors there, so it adds little
      beyond R02.
  - **P01:** the steps refused.
  - **K01-K03:** controls. SIN(X)×COS(X) keeps 1.8.0's SIN(X)^2÷2; TAN(X)^2 and X×SIN(X)^2 are still refused.
  - Every line was read on both pins before the file was written.
  - **On the old pin's build** (a scratch worktree at d638ef6, pin c564e41): R01-R10 and P01 fail, 11 of 14, and
    K01-K03 pass. The run's rc is 1 (old-pin-run.txt).

## Results

**1. make test** at e5cf3f1 (make-test.txt): rc 0.
- 22,350/22,350 vectors and 61,605/61,605 expectations, summing every `n/m vectors` and `n/m expectations`. That's
  d638ef6's 22,336 and 61,589 plus the new file's 14 and 16.
- screen_test 67/67 frames; the replay's 51 scripts; app_test 919/919.

**2. make clang-check** (clang-check.txt): rc 0, 76 files, 0 failing, the control refused. wasm_app.c isn't compiled
(no build/wamr), as before.

**3. D68,** `scripts/resources.py static d638ef6 e5cf3f1` (d68.txt):

| | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | 2,414,688 -> 2,423,536 (+8,848) | 2,341,600 -> 2,351,200 (+9,600) |
| DIRAM, IRAM code, calc_task's frame | +0 | +0 |
| libmain.a | +8,792 | +9,520 |

- **casim.c.obj's loaded sections** (casim-size.py, casim-size.txt):
  - Rev A: 247,355 -> 256,207 (+8,852).
  - Stand-in: 247,555 -> 256,347 (+8,792).
  - Both are under decision 94's 301,056 B line. casim's notes give 293,725 B (+9,887) for its own S3 measure, a
    different method.
- **Non-casim lines that moved in the linked maps,** with no source changed:
  - Rev A's calc.c: solve_core +612, gk15 +48, integ_core +8, and their literals;
  - list.c's ab_prog_line literal +4;
  - the stand-in's state.c literal pools, +16 and −16;
  - a few 4 B literals in the IDF's and the games' objects.
- **Their objects are the same.** solve_core, gk15 and integ_core's .text, .literal and .rela, ab_prog_line's, and
  state.c's two literal pools are byte for byte the same in all four builds: 11 sections, each with one size and one
  sha256 (obj-section.py, object-sections.txt).
  - So, as at 1.7.0 and 1.8.0, these are the Xtensa linker's relaxation and layout once casim grew, not changes of
    code.
- The containers peaked at 785 MiB (42 samples, soroban-repin-* through my shim, 4 GiB cap, --rm).

**4. The boards:** not run.
- No frame changed (67/67, the replay's 51).
- The answers that move are ∫'s, through the same core the device runs; their host vectors are R01-R10.
- casim's own S3 bench at the tag passes 1,510 cases byte-identical to the host. Its worst stack is 11,744 of a 12 KiB
  task, the masked reading unchanged from 1.8.0 by its notes. The calc task's stack is 32 KiB on both boards.

**Claims:** none needed. The containers are under the small-container rule, recorded in my lane.
