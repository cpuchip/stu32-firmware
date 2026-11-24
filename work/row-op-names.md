# Blueprint: every op that leaves a row has a name (unit 100)

**From:** abacus #16928 (tally's find on the phone, #16924), read at 4237c2e. **Status:** a blueprint for abacus's
acceptance. No src is written. A throwaway prototype in a scratch worktree answered what moves
(work/row-op-names-evidence/prototype.diff).

## The defect

A worksheet row whose op has no entry in core/list.c's NAMES draws "?" as its page line's left text. ab_op_text writes
"?" and returns −1, and pg_texts (firmware/screen.c:437) ignores the −1.
- **The repro:** abacus's case. STU, `2×X+5` stored, EQN MORE's →X, then 2 ×. The page line for the →X row reads
  `?|'2×X+5'` (evidence: probe-run-4237c2e.txt, A01).
- **The sibling in the same frame:** unit 098's level op (screen.c:1569) blanks the same case. A level holding a SIMP or
  CAS-SOLVE row shows no name where every other op's row shows one (frames below, "before").
- **The third reader:** the row's INFO view (screen.c:1023) writes "?" on the same −1.

## The census: which ops can leave a row

97 of the 250 ops have no NAMES entry. That's abacus's count, and mine by a set difference of calc.h's enum and the
table. A row is written only by core/rows.c's append, and its op comes from seven routes:

| route | its ops | unnamed among them |
|---|---|---|
| ab_rows_num | a typed number (NUM: no left text) | — |
| ab_rows_alg | an ALG line (its own text) | — |
| make_op | ab_rows_op's AB_ROWK_OP kinds | **CASSIMP, CASEXP** (CONST and TEST have their own text) |
| make_letter | STO+ − × ÷ | — |
| end_run | AB_ROWK_RUN: XEQ, R/S, SST, SOLVE, ∫ | **SST** |
| the report (ENTER or EVAL on SOLVE's report) | AB_CASSOLVE | **CASSOLVE** |
| the list's →X | AB_EQDROP | **EQDROP** |
| ab_rows_copy | AB_COPY | — |

So five ops can leave a row with no name.

**The second measure, from the opposite direction** (evidence: census.txt). A throwaway build logged every op row the
writer wrote over the whole of `make -i test` at 4237c2e, with no step failing.
- It logged 129,842 op rows over 116 distinct ops.
- Exactly five had no name, and they are the table's five:

| op | rows logged |
|---|---|
| EQDROP | 2,654 |
| SST | 244 |
| CASSIMP | 71 |
| CASSOLVE | 66 |
| CASEXP | 44 |

- abacus's other candidates leave no row of their own:
  - EQSTORE is a move.
  - ANS does nothing on RPN's stack; ALG's line reads it as an ALG row.
  - CASEVAL's row is written as CASSOLVE.
  - The other CAS ops answer into the list or a report: neither the table nor the logged census has a row of theirs.

**None of the five can be a program line.** Each returns NONPROGRAMMABLE in program entry (calc.c's list), and SST
moves the pointer there. So a name added to NAMES changes no program listing. That table is one table on purpose:
unit 090's rule is that a row's name is the line's.

## The change

1. **Five names in NAMES,** each the soft key's label for the op:

   | op | name | where the key reads it |
   |---|---|---|
   | AB_EQDROP | →X | EQN MORE |
   | AB_CASSIMP | SIMP | CAS, X EXPR, ROW EXPR |
   | AB_CASEXP | EXPND | CAS, X EXPR, ROW EXPR |
   | AB_CASSOLVE | SOLVE | CAS2, X EXPR |
   | AB_SST | SST | the R/S key's shift |

   - CASSOLVE's row is written with arg 0, so it reads SOLVE alone. The numeric solver's row reads SOLVE X, its
     letter. (Ask 1.)
2. **pg_texts blanks the left on ≤ 0, as level_op_text does.** With the five named, this happens only for an op this
   build doesn't name. That comes only from a damaged image, since a newer image is refused whole.
3. **INFO keeps its "?"** for that same case. (Ask 2.)
4. **No state version.** Names are display only, and the row records are unchanged. VER 0.101.

## What moves (the prototype, `make -i test`: nothing else)

- **Vectors, screen_test (67/67 frames), app_test (915):** nothing. No vector asserted the "?".
- **The stand-in replay:** device/standin/arith084.txt's two frames and cas088.txt's frames 2 and 4. They are board
  scripts whose CRCs are the host's. The unit updates the four CRCs in place with notes, and a board run on the
  stand-in (claimed) shows the device drawing them.
- **The frames** (evidence: frames/, before and after, as the replay draws them):
  - **arith084:** the page line `?` becomes `→X`.
  - **cas088 after SIMP:** the page line `?` becomes `→X`.
  - **cas088 after the root:** the page line becomes `→X`. Y now shows SIMP and X shows SOLVE beside their levels,
    where both were blank.
- **The screen-reader label** (unit 098's export) gains the same names beside the levels: SIMP, EXPND, SOLVE, SST, →X.

## The oracle (written before src; h100 frozen by abacus before src)

- **tests/100-soroban.txt** (vectors):
  - abacus's case: %PAGE-k's left is →X;
  - each of the five made and read on the page (%PAGE-k) and beside its level (%LEVELOP-k, 098's grammar);
  - a named op beside each as a control (×, √x);
  - the 33s and 35s modes, where the page is off: no change.
  - Every line run on 4237c2e first: the name lines fail there, and the controls pass.
- **rows_test, a name census by construction,** for every op 0 to AB_OP_LAST:
  - an op the table makes a row for (OP, RUN, LETTER, with each CMPLX arg and the census's own arg), plus CASSOLVE,
    EQDROP and COPY, must give ab_op_text > 0;
  - and in the existing census loop, every row any op makes on any base must have a name.
  - A sixth unnamed row op, added later, fails here before any frame shows it.
- **app_test:**
  - a row record with an op past AB_OP_LAST, written into the page as a damaged image would hold it, draws an empty
    left on the page;
  - INFO keeps "?";
  - the five rows' INFO lines read their names.
- **Mutants, predicted before the run:**
  - M0, the control;
  - M1-M5, each name removed: its vectors and the census go red;
  - M6, pg_texts' fallback reverted: app_test's damaged-row check;
  - M7, the census's route list without CASSOLVE: the census stays green but 100's SOLVE lines go red. That shows why
    the vectors stand beside the census.
- **D68** (flash, RAM), and **the board:** the two replay scripts on the stand-in, claimed.

## Asks for abacus

1. **CAS SOLVE's row:** `SOLVE` (the key's label, the lean), or `SOLVE X`. The letter would need the report's variable
   in the row's arg, a small core change.
2. **INFO's "?" for an op this build doesn't know:** keep it (the lean, since it says something is wrong), or blank it
   as the page does.
3. **The names beside the levels are new pixels on STU's home.** They complete unit 098's rule for these five ops, so
   I'd treat them as within it. Tell me if Michael should see the frames first.
4. **h100's hash before src.** Then the unit is built, mutants first.
