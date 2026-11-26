# Repin: Casimir 1.11.0 (casimir 84da80f, the v1.11.0 tag)

**From:** abacus #17715 (under Michael's #10856 rule: a repin is soroban's, no card; abacus replays it). **Status:**
built at c9af29d; results below.

## What changes

- **CASIM_PIN is 84da80f4a3dd47ea38dd3f1337f524ed24a40a88,** the commit of the v1.11.0 tag (tag object c5ca367).
  origin/main contains it in casim's clone, which was read, never fetched into.
- **1.11.0's source over 2e62056** (`git diff --numstat`): src/casim.c 65 insertions and 1 deletion, CAS 044; casim.h
  2 and 2, the version text.
- **No firmware code changes.** VER stays 0.105.
  - The status mapping already reads what moved: casim's BAD_ARG on a defint becomes BOUND NOT NUMERIC when a bound
    holds a letter (core/calc.c, unit 056), so 1.11.0's new refusal for a letter bound needs nothing here.
- **What moves through the keys** (CAS 044, casim's work/release-1.11.0.md):
  - **∫AB answers powers and products of SIN and COS of multiples of one angle exactly,** where 1.10.0 said CAN'T
    INTEGRATE. For example, SIN(X)^2 from 0 to π is π÷2, and in DEG from 0 to 45 it is -45÷π+45÷2.
  - **Two refusals change:**
    - such an integrand with a letter bound is BOUND NOT NUMERIC, as every ∫AB with a letter bound is;
    - `COS(LN(EXP(X+1)))` from 1 to 2 is IMPROPER INTEGRAL. casim's notes list that as a Not yet: the right refusal is
      1.10.0's CAN'T INTEGRATE.

## Tests

- **The census** (`make -i test` on the new pin before any test was written, census.txt): **nothing moved.**
  - 22,483/22,483 vectors and 61,789/61,789 expectations, 05ccd9f's totals, with no ignored error.
  - Every other gate passed as before: screen_test 73/73 frames, tbook_test 39/39, steps_test 0 bad, app_test
    919/919, the replay's scripts.
  - **Why nothing moved:** no earlier vector sends a SIN and COS polynomial through ∫AB.
    - The one vector that pins CAN'T INTEGRATE through ∫AB is 056 D28, X×LN(X).
    - The one in 048 is EXP(X^2) through ∫, not ∫AB.
- **tests/repin-v1-11-0-soroban.txt (new, in make test): 18 vectors, 29 expectations.** The expected texts are casim
  1.11.0's notes (work/044-examples.txt).
  - **R01-R12:** the answers:
    - SIN^2, COS^4, SIN^3 and SIN^5 at the table's bounds;
    - SIN×COS, and SIN(X)×SIN(3×X)'s 0;
    - the bounds reversed;
    - a bound off the table (0 to 1);
    - DEG and GRAD;
    - a letter in the integrand;
    - casim's Not yet for an odd function's 0 over -1 to 1, which prints as its four COS terms.
  - **B01:** a letter bound, BOUND NOT NUMERIC.
  - **B02:** the IMPROPER Not yet, pinned with a note naming the right refusal, so casim's fix moves it visibly.
  - **K01-K04:** controls:
    - X×SIN(X)^2 and SIN(X)^2×X÷X, CAN'T INTEGRATE;
    - SIN(X) from 0 to π, 2;
    - X^2 with a letter bound.
  - **Each R value checked apart** (quad.py, quad.txt): a composite Simpson quadrature, 200,000 panels in floats.
    - All twelve printed forms agree within 1E-10. The largest difference is 5.5E-13, GRAD's 100.
    - R11's form is exactly 0, since COS(-1) = COS(1).
    - R10 was read at A = 1.7.
  - **Every line was read on both pins before the file was written.**
  - **On the old pin's build** (a scratch worktree at 05ccd9f, pin 2e62056): R01-R12, B01 and B02 fail, 14 of 18, and
    K01-K04 pass. rc 1 (old-pin-run.txt).

## Results

**1. make test** at c9af29d (make-test.txt): rc 0.
- 22,501/22,501 vectors and 61,818/61,818 expectations, summing every `n/m vectors` and `n/m expectations`. That's
  05ccd9f's 22,483 and 61,789 plus the new file's 18 and 29.

**2. make clang-check** (clang-check.txt): rc 0, 76 files, 0 failing, the control refused.

**3. D68,** `scripts/resources.py static 05ccd9f c9af29d` (d68.txt):

| | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | 2,428,096 -> 2,429,056 (+960) | 2,355,808 -> 2,356,784 (+976) |
| DIRAM, IRAM code, calc_task's frame | +0 | +0 |
| libmain.a | +980 | +972 |

- **The S3 size** (abacus's ask): casim.c.obj's loaded sections (work/repin-v190-evidence/casim-size.py):

  | | 05ccd9f | c9af29d | change |
  |---|---|---|---|
  | Rev A | 259,514 | 260,482 | +968 |
  | Stand-in | 259,750 | 260,730 | +980 |

  - Nearly all of it is CAS 044's code: do_dtrig (+833 on Rev A), dt_defint (+86) and td_mixed (+60). The rest are
    ±2-4 B moves in casim's own functions.
  - Both are under decision 94's 301,056 B line, 40,574 B under on Rev A.
  - That line is judged on casim's own S3 measure, a different build: 298,895 B by its notes, **2,161 B under**. The
    firmware's figure is not that measure and doesn't replace it. The next casim unit that needs more than 2,161 B
    brings its size to Michael first.
  - The apps fit their partitions with room: Rev A 2,356,784 of 7,340,032 B, the stand-in 2,429,056 of 5,242,880.
- **Moved outside the changed sources,** by D68's report:
  - Rev A's calc.c eqn_step, +4, **"object identical"**.
  - None on the stand-in.
- **The stand-in's libbid.a reads −32,** and this repin doesn't touch libbid. The gate's object check finds no loaded
  section outside the changed sources that moved there. Earlier runs with libbid untouched read the same kind of
  drift: 1.10.0's −4 and +40, and 104's −4.
- The containers peaked at 1,034.2 MiB (45 samples, soroban-repin-* through my shim, 4 GiB cap, --rm;
  container-peak.txt). None was left.

**4. The boards:** not run.
- No frame changed (73/73, the replay's scripts).
- The answers that move are ∫AB's, through the same core the device runs.
- casim's own S3 bench at the tag passes 1,521 cases byte-identical to the host, five of them definite SIN and COS
  integrals. Its worst stack is 11,744 of a 12 KiB task, unchanged. The calc task's stack is 32 KiB on both boards.

**Claims:** none needed (the small-container rule, recorded in my lane).

## For abacus

- **The census's lines:** none moved. No existing vector pins NOT_INTEGRABLE on a SIN and COS polynomial through ∫AB.
- **The moves through the keys** (all new lines in the new file, none amended):
  - R01-R12 answer, where 1.10.0 said CAN'T INTEGRATE.
  - B01 is BOUND NOT NUMERIC, where 1.10.0 said CAN'T INTEGRATE.
  - **B02 is IMPROPER INTEGRAL, where 1.10.0 said CAN'T INTEGRATE.** It's casim's own Not yet: the interval check
    reads LN(EXP(u)) as typed. On the calculator it is a message that names the wrong reason, never a wrong value.
    Pinned so the fix shows up.
- **To replay:** make test at this record's head. Then put the pin back at 2e62056: R01-R12, B01 and B02 go red and
  K01-K04 stay green, 4/18.

## After acceptance (abacus #17727)

Accepted at ddecbc5 (abacus c963fde). abacus's replay: make test rc 0, 22,501/22,501 and 61,818/61,818; the repin file
18/18. With only the pin back at 2e62056, 4/18: R01-R12 and B01-B02 red, K01-K04 green. Its peak was 510 MiB. B02,
pinned with its note naming the right refusal, stands as is.
