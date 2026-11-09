# Unit 095 built: STEPS, Casimir's show-steps on the calculator

**From:** the blueprint work/cas-steps-device.md (measured cost fb69d72, revision 1 ab2796d/479eef1); abacus #14234 (unit
095, accepted with the view opening on STEP 1), #14240 (h095 frozen at abacus a756e81 before any src, 12 vectors; %STEP's
and %STEPDEPTH's grammar), #14242 (two-word rules with `_`, the angle as the op ran).
- **The build:**
  - e82416c: the source.
  - 6b67fcb: the mutants' predictions, before any run.
  - ab438ea: the mutant run.
  - 4809cf7: the board script's C timing.
- **VER 0.96:** 095 plus the out-of-order 092 (work/version-extra.txt).

**The 47.8 KB isn't Casimir growing** (abacus #14234). casim's own size measure (scripts/size-s3.sh) compiles the objects,
which already hold the steps code. Only the firmware's link dropped it, because until now no key reached `casim_step`.

## What was built

- **The request** (core, the RAM tail after the selection): the last D/DX, ∫DX or SOLVE answer's op, letter, model and
  angle (with CPLX's bit), as the op ran; the equation's text; and for D/DX and ∫DX the answer inserted.
  - **Replaced** by a new CAS op; a refused op leaves none.
  - **Cleared** by MEMORY CLEAR, CLEAR ALL and an image's load.
  - **Live** for SOLVE while its report is open; for D/DX and ∫DX while an entry of the list holds the answer, so an
    edit, a deletion or the list cleared ends it.
- **STEPS** (AB_CASSTEPS, nonprogrammable): CAS3's fifth key, acting when the D/DX or ∫DX answer's entry is the one shown,
  and the report's fourth key (▲ ▼ EVAL STEPS) for SOLVE. Anywhere else it does nothing.
- **The view** opens on STEP 1, top-down. Only the page shown is computed: one `casim_step` call, CASIM_STEPS_ONE_LINE, in
  the CAS arena, giving the count, the page and its rule and depth together.
  - **Status line:** `STEP k OF n`.
  - **The page:** `RULE: text` as casim's CLI prints it, after a `·` for each level of depth. It's wrapped in rows of 31
    cells at scale 2 under the status, eight rows at most, each broken after a blank or an operator in its second half,
    else where the row ends.
  - **Keys:** ▲ ▼ (the bar CAS STEPS) page, wrapping. C or ← leaves, to the report when it came from there. Any other key
    leaves and acts.
  - **Busy:** a page is one of the app's long ops (the opening and ▲ are marked, ▼ is SST, already a run). A C during
    it discards the page and leaves the view: INTERRUPTED.
- **HELP:** STEPS' entry. help_test reaches it from CAS3 and the report's bar: 316 of 316 entries.
- **Tests:**
  - **Vectors:** tests/095-soroban.txt, 27 vectors and 68 expectations. The derive pages are casim's
    docs/language.md table read one-line; the solve's and the integral's counts are the bench's.
  - **Tokens and dumps:** CASSTEPS; %STEPREQ, %STEPN, %STEPK, %STEP, %STEPRULE, %STEPDEPTH (#14239, #14240).
  - **The census, kept** (abacus #14234): tests/steps_test.c over tests/steps-corpus.txt. 75 cases: 63 paged, 12
    refused just as their ops refuse, the most arena 10 KiB of 16, 28 steps, the longest page 123 cells, 0 bad.
    It runs in RAD, the firmware's way; the blueprint's census ran in DEG (30 steps, 129 cells).
  - **app_test:** CAS3's STEPS, the report's STEPS, the view's bar, ▼, C back to the report.
  - **screen_test:** frames 64-66 (one row, three rows, the longest page) with checks.
- **STEPBENCH** (device/dev_bench.h, both consoles): every case through the keys' path, timed.
- **Board scripts:** device/standin/steps095.txt (replayed on the host) and stepbench095.txt (a field session).

## What moved

- **094's bar check in app_test:** the report's fourth key is STEPS now, where it was empty.
- **The blueprint's frame "at 5 rows"** is six rows. Breaking after operators, not mid-name, takes a row more.
- **Nothing else:** no other vector, frame or board script moved.

## Results

- **make test** at e82416c: rc 0; 217 lines, 21,539 vectors, 58,352 expectations (every `n/n vectors` line, repeats
  included). That's 092's 215 / 21,485 / 58,216 plus 095's 27 and 68, plain and sanitized.
  - Within it: steps_test 0 bad; app_test 900/900; help_test 316/316 entries; screen_test 66/66 frames, 9,035 checks;
    standin_replay 48 scripts.
  - work/095-evidence/make-test.txt.
- **make accuracy:** rc 0, 3:51, peak RSS 1.23 GiB (work/095-evidence/make-accuracy.txt).
- **clang-check:** 75 files (steps_test.c new), 0 failing, the control refused.
- **Mutants** (predictions at 6b67fcb before the run; work/095-evidence/mutants-run1.txt): 7 of 10 exactly.
  - **Survived, as predicted:**
    - T1 (MEMORY CLEAR's explicit clear removed): its init zeroes the whole calculator, the tail with it.
    - T2 (every page computed on opening): only the time differs, which STEPBENCH measures.
  - **Red exactly as predicted:**
    - T4 (a new CAS op not replacing the request): R03 and R04.
    - T5 (SOLVE's request outliving its report): S03.
    - T7 (no interrupt): R11.
    - T9 (STEPS on any entry): R08.
  - **My misses:**
    - T3 (template lines shown) also reddened R09 (the count is 4, not 3) and frame 66 with its check.
    - T6 (C closing the report too) also reddened 094's EVAL check, which follows my 095 lines and needs the report
      the fault closes.
    - T8 (▲ not wrapping) also reddened frame 66: I made that frame with ▲ from STEP 1, and forgot it.
- **D68** (`scripts/resources.py static 479eef1 4809cf7`, the commit before the src):

  | | flash | DIRAM | IRAM | calc_task's frame |
  |---|---|---|---|---|
  | the stand-in | +50,096 | +0 | +0 | +0 |
  | Rev A | +49,216 | +8 | +0 | +0 |

  - The blueprint measured 47.8 KB for `casim_step` alone. The rest is the view, the request, STEPBENCH and HELP.
  - The DIRAM is the composed screen: `comp` +4 for its steps flag, aligned to 8 on Rev A.
- **STEPBENCH**, ms (stand-in / Rev A head-less; RAD, one line; work/095-evidence/board/):

  | case | steps | opening (STEP 1) | the slowest page | every page |
  |---|---|---|---|---|
  | solve X^2-5×X+6=0 | 4 | 2.1 / 1.4 | 2.2 / 1.4 | 8.4 / 5.5 |
  | solve V=I×R for R | 3 | 2.8 / 1.6 | 2.9 / 1.6 | 8.4 / 4.6 |
  | derive X^3×SIN(X)×EXP(X)×LN(X) | 5 | 1.9 / 1.0 | 1.9 / 1.0 | 7.4 / 3.7 |
  | derive (X^2+1)^5×(X-3)^4÷(X+2)^3 | 13 | 1.8 / 1.0 | 2.6 / 1.2 | 24.8 / 11.0 |
  | integrate X^2 | 2 | 2.1 / 1.1 | 2.1 / 1.1 | 4.2 / 1.9 |
  | integrate EXP(X)×SIN(X) | 7 | 10.9 / 5.6 | 13.5 / 6.7 | 77.9 / 39.0 |
  | solve (X-1)×...×(X-6) | 13 | 61.7 / 61.1 | 61.8 / 61.1 | 801 / 793 |
  | integrate ATAN(X) | 7 | 34.0 / 23.8 | 54.0 / 38.6 | 294 / 208 |
  | integrate X×ATAN(X) | 13 | 45.9 / 31.7 | 76.5 / 54.5 | 822 / 574 |
  | integrate X×ATAN(X)+1÷(X^2-1) | 28 | 141 / 95 | 187 / 129 | 4,354 / 2,955 |
  | integrate 1÷((X-1)×(X-2)×(X-3)×(X-4)) | 22 | 322 / 310 | 385 / 372 | 6,440 / 6,156 |

  - **Rev A's 32 KB instruction cache** halves the light cases. The heavy ones barely move: the four-factor integral's
    worst page is 372 ms on Rev A against 385. Their time is in the arithmetic, not the code's fetch.
  - The stack's low-water mark stayed at least 24,668 B free, against the deep paths' floor of 8,188.
- **The stand-in** (image 4809cf7; work/095-evidence/board/standin-steps095.out): steps095, 9 PASS.
  - STEP 1's and STEP 2's frames (1468449E, 9ACB255E) are the host's.
  - **C during the four-factor integral's 384 ms page gave INTERRUPTED**, and the view left. The calc task's stack had
    25,084 B free. Only the control is red.
  - **A miss of my script's:** the first board run sent C straight after ▼. It reached the key queue before the calc task
    took ▼, so it was a key, not an interrupt, and INTERRUPTED didn't come. Sent 0.1 s into the page (4809cf7), it
    interrupts. That first run's output was overwritten by the second's (standin-steps095-try2-ab438ea.out, which
    passed).
- **Claims:** lepton docker soroban-* 4 GiB and the stand-in board 04:43:49-04:51:55Z (the flashes, both scripts, Rev A
  head-less, clang-check, D68); peak 596.2 MiB (docker stats every 2 s, soroban-* containers, 100 samples). Again
  04:52:04-04:54:05Z, to restore the stand-in's image at 4809cf7 and run steps095 on it; not sampled (the sampler had
  stopped), one build and flash as the first window's. The board is left on the stand-in's image at 4809cf7.

## For the next unit

- abacus's h095 run.
- tutor may weigh in on the view's opening page (abacus #14234): STEP 1 as built.
- Decision 95's two (the inexact mark and erf), blueprints first, as queued.
