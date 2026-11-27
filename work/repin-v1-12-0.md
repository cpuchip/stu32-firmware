# Repin: Casimir 1.12.0 (casim e7b2c51, the v1.12.0 tag)

**From:** abacus #18300 (under Michael's #10856 rule: a repin is soroban's, no card; abacus replays it). **Status:**
built at dfa2733; results below.

## What changes

- **CASIM_PIN is e7b2c516c1aa8b190268f02e14fec7d9e3c5c7a6,** the commit of the v1.12.0 tag (tag object 7a2a646).
  origin/main contains it in casim's clone, which was read, never fetched into.
- **1.12.0's source over 84da80f** (`git diff --numstat`): src/casim.c 18 insertions, CAS 045's td_pos and td_droppos
  and the call in td_defint; casim.h 2 and 2, the version text.
- **No firmware code changes.** VER stays 0.107.
- **What moves through the keys** (CAS 045, casim's work/045-typed-domain-positive.md and work/release-1.12.0.md):
  - **∫AB no longer refuses on a typed condition that always holds.** Typed LN(u) asks u > 0 and SQRT(u) u ≥ 0. Where
    u is EXP, ALOG or COSH of anything, or a positive number to a power that isn't a number, u > 0 wherever it has a
    value. So `LN(EXP(SIN(X)))` from 0 to 2 is `-COS(2)+1`, where 1.11.0 said IMPROPER INTEGRAL.
  - **The same for a letter:** `LN(EXP(K))×SIN(X)` from 0 to 1 is `K-K×COS(1)`. 1.11.0 gave BAD_ARG there, and the
    firmware showed it as BOUND NOT REAL (below).
  - The true refusals stay: a pole is IMPROPER INTEGRAL, a letter bound BOUND NOT NUMERIC, a negative number's root
    UNDEFINED.

## Tests

- **The census** (`make -i test` on the new pin before any test was written, census.txt): **one line moved,** and it
  is the one pinned to move.
  - The 1.11.0 repin's B02, `COS(LN(EXP(X+1)))` from 1 to 2: IMPROPER INTEGRAL at 1.11.0, `-SIN(2)+SIN(3)` now. That
    file pinned it with a note naming casim's Not yet, "so casim's fix moves it visibly".
  - **Amended in place** in tests/repin-v1-11-0-soroban.txt with a note naming this repin. Its note said CAN'T
    INTEGRATE was the right refusal. casim 1.12.0's notes correct that line of their own: the right result is the
    answer (integrate already answers `SIN(X+1)`), and the amendment says so.
  - Nothing else moved: 22,656/22,657 vectors and 62,162/62,164 expectations, B02's alone failing. screen_test 73/73
    frames, tbook_test 39/39, steps_test 0 bad, app_test 919/919, the replay's scripts all current.
- **tests/repin-v1-12-0-soroban.txt (new, in make test): 19 vectors, 27 expectations.** The expected texts are casim's
  CAS 045 tables.
  - **R01-R10:** the answers. COS(LN(EXP(X+1))) from -1 to 1 and in DEG; LN(EXP(SIN(X))) forwards, reversed, in DEG
    and GRAD; its cube; LOG(ALOG(SIN(X))); SIN(X)×LN(EXP(COS(X))); a bound of 0.5.
  - **L01-L03:** a letter's condition that always holds: LN(EXP(K)) and SQRT(EXP(K))^2 times SIN(X), and LN(EXP(K))×X
    through the table's path.
  - **K01-K06:** controls:
    - SQRT(K)^2×SIN(X), casim's true BAD_ARG;
    - 1÷(X-1) and TAN(X) over their poles, IMPROPER INTEGRAL;
    - LN(EXP(X)) from -1 to 1, casim's Not yet after 1.12.0, CAN'T INTEGRATE;
    - a letter bound, BOUND NOT NUMERIC;
    - SQRT(-1)×X, UNDEFINED.
  - **Each R and L value checked apart** (quad.py, quad.txt): a composite Simpson quadrature, 200,000 panels in
    floats, over the integrand as typed (LN(EXP(u)) computed as written), K at 1.37.
    - All 14 printed forms agree within 1E-10. The largest difference is 2.6E-13, DEG's 180÷π and GRAD's 200÷π.
    - The quadrature also covers B02's amended `-SIN(2)+SIN(3)`, as its R01 row.
  - **Every line was read on both pins before the file was written** (a probe in scratch).
  - **On the old pin's build** (a scratch worktree at 1b7b706, pin 84da80f): R01-R10 and L01-L03 fail, 13 of 19, and
    K01-K06 pass. rc 1 (old-pin-run.txt). On 1.11.0, R01-R10 are IMPROPER INTEGRAL and L01-L03 BOUND NOT REAL.

## A message that names the wrong reason (for abacus)

- **K01, `SQRT(K)^2×SIN(X)` from 0 to 1, shows BOUND NOT REAL on both pins, though both bounds are real.**
  - casim's refusal is right: it is BAD_ARG because K < 0 has no value, and the answer doesn't show it.
  - The message is the firmware's. Unit 056 (core/calc.c, abn_from_casim) reads every defint BAD_ARG as a bound's:
    BOUND NOT NUMERIC when a bound's text holds a letter, else BOUND NOT REAL (#5630 rule 3).
  - casim's defint now also gives BAD_ARG for a letter's condition in the integrand that the answer can't show, and
    that path reaches BOUND NOT REAL with real bounds.
  - On 1.11.0, L01-L03 reached it too. 1.12.0 answers them, so K01's kind is what's left.
- **Never a wrong value:** it's a refusal with a message naming the wrong reason.
- **Not changed here:** a new message, or a test of the bounds before the mapping, is a firmware change and so a unit.
  K01 pins today's message with a note, so a fix moves it visibly.
- **Pinned by nothing before this file:** the suite's BOUND_NOT_REAL lines are 056's D26 and D53, both a SQRT(-1)
  bound.

## Results

**1. make test** at dfa2733 (make-test.txt): rc 0.
- 22,676/22,676 vectors and 62,192/62,192 expectations, summing every `n/m vectors` and `n/m expectations`.
  - That's 1b7b706's 22,657 and 62,164, plus the new file's 19 and 27.
  - Plus 1 more expectation from B02's amendment: its error check doesn't count as an expectation, and its %EQ does.
- screen_test 9,948/9,948 checks, 73/73 frames; app_test 919/919; check-version 0.107.

**2. make clang-check** (clang-check.txt): rc 0, 76 files, 0 failing, the control refused.

**3. D68,** `scripts/resources.py static 1b7b706 dfa2733` (d68.txt):

| | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | 2,428,800 -> 2,428,912 (+112) | 2,356,944 -> 2,357,056 (+112) |
| DIRAM, IRAM code, calc_task's frame | +0 | +0 |
| libmain.a | +129 | +129 |

- **casim.c.obj's loaded sections** (work/repin-v190-evidence/casim-size.py; casim-size.txt):

  | | 1b7b706 | dfa2733 | change |
  |---|---|---|---|
  | Rev A | 260,486 | 260,615 | +129 |
  | Stand-in | 260,726 | 260,855 | +129 |

  - **td_droppos is +124 of it,** a new section. td_pos has no section of its own in either map, so it is inlined
    there. td_defint is −1. The rest are ±1 to ±12 B moves across casim's own functions, +6 in sum.
  - Both are under decision 105's 331,776 B line: Rev A is 71,161 B under.
  - That line is judged on casim's own S3 measure, a different build: 299,032 B by its notes, 32,744 B under. The
    firmware's figure doesn't replace it.
  - The apps fit their partitions with room: Rev A 2,357,056 of 7,340,032 B, the stand-in 2,428,912 of 5,242,880.
- **Moved outside the changed sources,** by D68's report:
  - the stand-in's state.c `.literal.ab_state_load` −8 and `.literal.parse` +8, each **"object identical"**;
  - none on Rev A.
- The containers peaked at 937.4 MiB (35 samples, soroban-repin-* through my shim, 4 GiB cap, --rm;
  container-peak.txt). None was left.

**4. The boards:** not run.
- No frame changed (73/73, the replay's scripts).
- The answers that move are ∫AB's, through the same core the device runs.
- casim's own S3 bench at e7b2c51 passes 1,521 cases byte-identical to the host. Its worst stack is 11,760 of a
  12 KiB task, by its notes. The calc task's stack is 32 KiB on both boards.

**Claims:** none needed (the small-container rule, recorded in my lane).

## For abacus

- **The census's line:** the 1.11.0 repin's B02, amended to the answer.
- **The moves through the keys** (all new lines in the new file, none amended but B02):
  - R01-R10 answer, where 1.11.0 said IMPROPER INTEGRAL.
  - L01-L03 answer, where 1.11.0 said BOUND NOT REAL.
- **The question:** K01's BOUND NOT REAL with real bounds (above). Is that a unit you want, or is it pinned as it
  stands?
- **To replay:** make test at this record's head. Then put the pin back at 84da80f:
  - R01-R10 and L01-L03 go red, and K01-K06 stay green: 6/19.
  - The 1.11.0 file's B02 goes red, 17/18.

## After acceptance (abacus #18306)

- **Accepted at 8dc08cf** (abacus c991b1a).
  - **abacus's replay** (in its container): make test rc 0, 22,676/22,676 and 62,192/62,192, 0 FAIL lines; the new
    file 19/19.
  - **With only the pin back at 84da80f:** the new file 6/19 (R01-R10 and L01-L03 red), and the 1.11.0 file 17/18
    with B02 alone red.
  - Read by hand: nothing changes under firmware/ or core/. Host casim.o +184.
- **K01 stays pinned as it stands.**
  - The fix is a later firmware unit: when both bounds are real numbers, a defint BAD_ARG is the integrand's, not a
    bound's.
  - It needs a new message text, and that text is Michael's, so it waits for a card. No work on it now.
