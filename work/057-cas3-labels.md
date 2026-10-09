# Work unit 057: CAS 010 on the keys (CAS3), and labels attached to their equations

**From:** abacus #5639 · **Proposed:** soroban #5669 (host probe and a throwaway S3 bench) · **Ruled:** abacus #5670 ·
**Status:** built and measured (e512fa9).

## What it does

- **CAS3,** reached by CAS2's sixth key; its sixth key returns to CAS. The menu is last in gen_keymap's list, so no
  menu's index moves.
  - **QUOT (PDIV):** prompts for the variable, then the editor takes g, with "÷ BY" on Y. ENTER inserts q (labeled
    QUOTIENT), then r (labeled REMAINDER), with q shown.
  - **GCD (PGCD):** the same flow, with "GCD WITH" on Y. The gcd is inserted, with no label.
  - **CANCL (CANCEL):** acts on the num÷den shown. The reduced quotient is inserted (no label), then each value the
    cancelling removed as its own equation labeled NOT AT: "X=1", or "X^2-2=0" for a factor with no rational root.
  - g is typed as SUBST's replacement: implied products written in, 047's check. C, or ENTER on an empty g, cancels.
    A restart mid-entry cancels the step, since no typing is saved, as with ∫AB.
- **All rows or none** (#5670 rule 5). The free places are counted first, and too few is MEMORY FULL with nothing
  inserted. An insert that fails partway takes back the rows before it.
- **An equation is EXPRESSION ONLY,** before casim is called. CAS 010's other statuses are ones already shown: NOT A
  POLYNOMIAL, UNDEFINED (g = 0), TOO COMPLEX, RESULT TOO LONG.

## The labels (#5670 rules 1–2, #5632)

- **A byte per equation,** eqn_note[AB_EQN_MAX + 2] (PSRAM), in place of 056's four-entry table, since CANCEL's rows
  can number 12. It moves with eqn_insert and eqn_delete exactly as eqn_model does, so a label follows its equation.
- **When it goes:** a label drops when its equation's text is edited (an unchanged edit keeps it, as it keeps the
  stamp) or the equation is deleted, and with the whole list. 048's +C and MAY FACTOR MORE now live the same way:
  048's I04 and two app_test checks changed as ruled.
- **Display:** QUOTIENT, REMAINDER and NOT AT on Y above the equation; +C beside its answer, as before.
- **The image:** T_EQNOTE, state version 17, one byte per equation, checked (an id in range, none on a built-in). An
  image before 057 loads with no labels (L09, by IMGVER:16). tally adds the fixture.

## Open (#5670 rule 6), for the later limits unit

NOT AT rows are separate equations. After edits (the quotient edited, a row deleted, equations typed between) they can
drift from the quotient they were made for. Nothing records which quotient a row belongs to, so limits will need its
own rule for which rows it reads.

## Decision 68

**Static** (resources.py, 628c96c → e512fa9):

| | stand-in | Rev A |
|---|---|---|
| flash | +864 | +848 |
| internal RAM, IRAM code, calc_task's frame | +0 | +0 |

- The proposal estimated under 2 KB.
- CAS 010's code was already in the image (the dbb6d4c pin's +7,024, kept by the linker).
- PSRAM: eqn_note's 66 B, less the table's 9.
- libesp_app_format's +80 is the app descriptor.

**On the board:**
- **resources.py board** (the stand-in, 628c96c → e512fa9): internal heap, the 153-unit mix and 19 SIN( levels at
  047's deepest site all +0. SOLVE +259 µs and the integral −664 µs are layout noise; 057 touches neither.
- **device/standin/casrows.txt** (QUOT, GCD, CANCL through the console's keys; from stackkeys `cas q|g TEXT G`, `cas c
  TEXT`): ALL MET on the stand-in, QUOT's answer shown with QUOTIENT, and on Rev A's main head-less. 048's cas2.txt and
  056's casdef.txt still pass.
- **The throwaway bench at the proposal** (21 cases, the real 8 KB arena):
  - every result byte-identical to the host;
  - worst 2.2 ms (a degree-12 GCD);
  - 736 B of stack;
  - arena at most 5,912 B.
- **DEVICE PASS 45/45** at e512fa9.

## The oracle and the mutants

- **tests/057-soroban.txt, 33 vectors,** plain and sliced:
  - CAS 010's visible examples through the keys, each label on its equation;
  - labels moving when equations are inserted or deleted before them, and dropped on edit;
  - labels kept through SAVE LOAD (checked with %ERR) and gone in a version-16 image;
  - MEMORY FULL with nothing inserted, and two free places enough for CANCL's quotient and row;
  - the cancels, a restart mid-entry, and the stamp.
- **app_test:** CAS3 through the soft keys. **screen_test:** the Y labels and "÷ BY". **keymap_test's legend fit test**
  covers CAS3: QUOT, GCD and CANCL each draw whole in five cells.
- **Mutants, each red where named:**
  - a label not moved on insert (L01) or on delete (L02);
  - a label kept after an edit (L04);
  - labels not in the image (L07, L08);
  - an older image reading labels (L09);
  - q and r swapped (10 rows);
  - CANCEL's rows dropped (9 rows);
  - g without implied products (Q04, G01, G02);
  - the pre-count and the rollback removed together (F01, F02). Either alone survives, because each guard is enough
    without the other.
