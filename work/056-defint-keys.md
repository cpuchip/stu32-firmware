# Work unit 056: Casimir's new statuses, and definite integrals on the keys

**From:** abacus #5620 · **Proposed:** soroban #5629 (measured with a host probe and a throwaway S3 bench) ·
**Ruled:** abacus #5630; the notes' lifetime #5631/#5632 · **Status:** built and measured (eb8b873).

## What it does

- **∫AB, CAS2's fourth key** (STU only, after SOLVE, ∫DX, FACTR):
  1. ∫AB asks for the variable.
  2. The editor takes the lower bound, with "∫X FROM" on Y. ENTER.
  3. The editor takes the upper bound, with "∫X TO". ENTER.
  - casim's CAS 009 defint runs, with repl as the lower bound and repl2 as the upper.
  - Each bound is checked against the parser's bound (047) and has its implied products written in, as SUBST's
    replacement: 2π is 2×π.
  - C, or ENTER on an empty bound, cancels at either step and keeps the list.
  - The lower bound is held in ab_calc (PSRAM) while the upper is typed.
- **On OK the exact answer is inserted** as a new equation, as D/DX's and ∫DX's are: "2", "LN(3)", "360÷π". There is
  no note beside it. XEQ gives its decimal.
- **A restart mid-entry cancels the step.** state.c saves no typing while a bound is typed, so the image is
  unchanged. A load starts from MEMORY CLEAR, so nothing outside the image survives it either.

## Messages (#5630 rule 3)

| casim | the firmware | text |
|---|---|---|
| IMPROPER (CAS 009) | ABN_CAS_IMPROPER | IMPROPER INTEGRAL (17 cells: whole at scale 3) |
| UNDEFINED (CAS 008), any op | ABN_CAS_UNDEFINED | UNDEFINED |
| BAD_ARG, ∫AB of an equation | ABN_CAS_EXPR_ONLY, before the call | EXPRESSION ONLY |
| BAD_ARG, a bound with a letter (the bound's eqn_vars) | ABN_CAS_BOUND_LETTER | BOUND NOT NUMERIC |
| BAD_ARG otherwise (a bound that isn't real) | ABN_CAS_BOUND_REAL | BOUND NOT REAL |

- **Before 056, UNDEFINED reached the keys as INVALID DATA,** through cas_step's default branch, for SIMP of 1÷0,
  SIMP of 1÷COS(π÷2) and D/DX of 0^(−1)×X. Those are U01–U03, red at b5378a3.
- **NOTREAL is not a casim status.** A bound that isn't real comes back as BAD_ARG, hence the split above.
- **The "fit test" #5630 named didn't exist,** so it is new in screen_test: each status's message is at most 20
  cells, and shows whole on X at scale 3.
- **The face check's loops ended at ABN_BAD_IMAGE** and so never covered a CAS message. They run to the new
  ABN_STATUS_LAST.

## The notes (#5630 rule 4)

- cas_note becomes a table: up to 4 entries, each an id and an equation, in PSRAM, holding 048's +C and MAY FACTOR MORE.
- 056 keeps today's lifetime (#5632): gone once the list leaves the equation shown when the notes were made.
- CAS 010's firmware unit attaches them to their equations, keeps them in the image (state version bumped, a migration
  test, tally's fixture), and adds QUOT, REM and NOT AT.

## Decision 68

**Static** (resources.py, b5378a3 → eb8b873):

| | stand-in | Rev A |
|---|---|---|
| flash | +944 | +912 |
| internal RAM, IRAM code, calc_task's frame | +0 | +0 |

- The proposal estimated under 1.5 KB.
- defint itself was already in the image (casim_run links it; the 2c7ee90 pin paid for it).
- PSRAM grows by ab_calc's 192 B lower bound and the table.
- libesp_app_format's +100 is the app descriptor, which moves with every image.

**On the board:**
- **resources.py board** (the stand-in, b5378a3 → eb8b873): internal heap, the 153-unit mix and 19 SIN( levels at
  047's deepest site all +0. SOLVE and the integral each read −1.1 ms; 056 touches neither, so that is layout noise.
- **device/standin/casdef.txt** (∫AB through the console's keys, from stackkeys `cas d TEXT LO HI`): ALL MET on the
  stand-in and on Rev A's main head-less. It covers LN(3), IMPROPER INTEGRAL and −1÷3, and 048's cas2.txt still
  passes.
- **The throwaway bench at the proposal** (22 cases, the real 8 KB arena, a frame holding cas_step's two 192 B
  buffers):
  - every result byte-identical to the host;
  - worst 8.9 ms;
  - 704 B of stack;
  - every OK case's arena at most 3,248 B.
- **The arena stays 8 KB** (#5630 rule 5): a defint past it reads TOO COMPLEX (D29), not RESULT TOO LONG.
- **DEVICE PASS 45/45** at eb8b873.

## The oracle and the mutants

- **tests/056-soroban.txt, 33 vectors,** plain and in slices:
  - CAS 009's visible examples through the keys;
  - each status's message, with all four BAD_ARG cases;
  - the cancels and the stamp;
  - a restart mid-entry (D46–D48).
- **app_test:** ∫AB through the soft keys.
- **screen_test:** the two labels.
- **Mutants, each red where named:**
  - the bounds swapped;
  - repl2 left NULL;
  - IMPROPER shown as INVALID DATA;
  - the bound's implied products not written;
  - the lower bound kept after a cancel (both resets removed);
  - a note on the wrong equation;
  - BOUND NOT NUMERIC for a non-real bound (#5630);
  - UNDEFINED shown as INVALID DATA;
  - the equation not refused before the call;
  - the image keeping the bound being typed (both guards removed);
  - a message past 20 cells.
- **Equivalent:** a clear of ∫AB's state at load, which I first added and then removed: a load starts from MEMORY
  CLEAR.
