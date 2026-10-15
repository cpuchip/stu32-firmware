# Work unit 070: CAS 018's sums on the keys (Σ), with casim 4f24e3e

**From:** abacus #8507 (the CASIM_PIN card) · **Proposed:** soroban #8544 · **Ruled:** abacus #8545 ·
**Words:** NO CLOSED FORM and "X≠1" shown are Michael's (#8294); BAD BOUND and A CONDITION are provisional (#8545) ·
**Status:** built on the host; Decision 68 and the device bench to follow.

## What it does

- **CASIM_PIN 4f24e3e** (abacus #8556), which contains 65369cb, where CAS 018 was accepted. The repin is its own commit,
  before this unit's. Two changes in it reach the keys, with rows that are red at 65369cb:
  - CAS 013 rule 8 B (a78c7e5, 4f24e3e): the reduced fraction first, where nothing removed is a hole.
    - 048 I24: (X^3+X)÷(X^2+1) answers 0.5×X^2. At 65369cb, 32e5ddd's rule made it TOO COMPLEX at 8,800 B; a4dd723
      said CAN'T INTEGRATE.
    - 048 I22: (X^2+3×X-1)×(3×X+2)÷(3×X+2)^2 answers at 8,800 B, where it ran out before partfrac (a4dd723's own
      fix). No row now reaches that run-out through the keys at 8,800 B: casim's example of it, (8×X-7)×(2×X+5)÷(2×X+5)^2,
      answers too. I19 still holds TOO COMPLEX.
  - 5ab9b59, sums: a closed form that runs out of room is NO_ROOM, not TOO_LONG. 070 S58 is casim's own row.
- **Σ, CAS2's fifth key** (beside ∫AB). It prompts for the variable and then follows ∫AB's flow (056):
  - Y shows "ΣK FROM" while the lower bound is typed, then "ΣK TO" for the upper. ENTER on the upper runs casim_sum,
    with lo in repl and hi in repl2.
  - An equation (L=R) is EXPRESSION ONLY. An empty bound, or C, cancels the step and keeps the list.
  - Like cas_def, the step is not in the image.
- **∞.** While Σ's upper bound is typed, the typing bar is EQN TYPE SUM, whose fourth key reads ∞ and types U+221E. It
  takes the place of =, which means nothing in a bound. Everywhere else the key does nothing: Σ's lower bound, ∫AB,
  an equation typed, ALG's line (RPN ONLY), program entry (no line).
  - The parser has no ∞, and casim takes exactly two texts holding one: ∞, the sum to infinity, and -∞, which is its
    BAD_ARG (BAD BOUND). Those two go to casim as typed. Any other text holding ∞ (∞+1, a blank after it) fails the
    parser: SYNTAX, still typed, as any bound that doesn't parse.
  - **Corrected from the ruling's wording (fresh eyes):** #8544 said casim's BAD_ARG decides every other ∞ text. casim
    says SYNTAX for them, after the step had closed, so the typed bound was lost. Only exactly ∞ and -∞ are sent.
- **The answer and its conditions.** casim gives "answer; X≠1; ...". The answer is inserted and shown, then each
  condition is a row of its own after it, in order, labeled **WHERE** on Y, in casim's text (X≠1, ABS(X)<1). It is
  all or none, as cas_rows is for CANCEL (057).
  - A row is stored as typed: the list keeps text it can't parse, as the 35s keeps a message.
  - **A WHERE row is no equation.** Every op on it says **A CONDITION**, before any editor or prompt opens: each CAS op
    (Σ too), SOLVE, ∫, XEQ, ENTER, TABLE, GRAPH.
  - Editing it is allowed: the edited text loses the label, as 057's rows do.
- **Statuses:**
  - NO_CLOSED_FORM: NO CLOSED FORM (ABN_CAS_NO_CLOSED).
  - BAD_ARG on a sum: BAD BOUND (ABN_CAS_BAD_BOUND). Sums take letters in a bound, so ∫AB's two words don't fit.
  - UNDEFINED, UNDECIDED (CAN'T DECIDE), TOO_LONG (RESULT TOO LONG), NO_ROOM (TOO COMPLEX): as before.
- **State image version 19:** no new field. The label may be WHERE, and NO CLOSED FORM, BAD BOUND or A CONDITION may
  be the message showing. An older image holding any of them is refused.

## Checks

- **tests/070-soroban.txt,** 57 rows through the core op. Each answer is casim's CLI at 4f24e3e with the 8,800 B
  arena, and every sum answers the same at 65369cb except S58. They cover:
  - answers with and without rows, two letter groups, telescoping, the empty sum, terms written out;
  - ∞ sums, every status, each place ∞ does nothing, ∞ in other text kept typed;
  - each op on a WHERE row, an edited row;
  - the image round trips and the version-18 refusals.
- **056 D56** moves to 45, one past the new last status.
- **app_test:** Σ through the keys, the variable prompt, both Y labels, the bar's = then ∞, the answer with its WHERE
  row, and ∫AB's upper bound keeping =.
- **Mutants:** 17, each red on its own rows (scratch mut070.py; M17 is the reviewed ∞ defect, red on S55 and S56). They cover the WHERE label, each WHERE check (CAS,
  SOLVE/∫, evaluate, TABLE, GRAPH), ∞ sent as typed, ∞ typed anywhere, the two status mappings, the image's version
  gates (label, messages), the bar, the prompt's name, Σ run as ∫AB, and the rows not split.

## Not in this unit

- Steps: casim_step on CASIM_SUM is BAD_ARG (CAS 018 rule 10).
- X = 1's value as a row ("X=1: N+1"): CAS 018 leaves it to Michael. The student gets it with one more sum.
