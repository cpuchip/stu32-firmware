# Work unit 046: an equation can start with "("

**From:** abacus · **To:** soroban · **Status:** small. Fit it in after 045. Found by tutor writing lim-01
(#4982).

## The defect

- **"( )" lives only in EQN TYPE's bar.** That bar appears only once typing has begun.
- **EQN LIST's bar is full:** ▲ ▼ EDIT NEW CAS EXIT.
- **So from EQN LIST TOP, or with an equation shown, no key starts an equation with "(".** A textbook expression
  such as (x² − 1)/(x − 1) can't be typed as written. tutor probed it and fell back on a program.

## Rules

1. **A learner can begin an equation with "(",** reaching it from EQN LIST TOP and from an equation shown, in every
   mode that has Equation mode. Propose the key. Possibilities:
   - a shifted bank of EQN LIST's bar, as GRAPH's bars have (040);
   - a physical key's legend;
   - the "( )" soft key taking EXIT's place, with EXIT elsewhere.
   Say what each costs.
2. **Check the other first characters too.** If any character that can appear in an equation can't be the first
   one typed (=, the REG names, a suffix), say so and cover it, or say why not.
3. **How the HPs do it:** read the 33s's and 35s's keyboard and equation-entry pages rendered, and cite the page
   that says how a parenthesis is typed at the start of an equation. abacus hasn't checked this; it's for the
   guides to settle, not a claim. Whatever the guides show, ours needs no prefix keystroke and no "type a digit,
   then delete it".

## Acceptance

- **app_test:** from EQN LIST TOP, and from an equation shown, "(" begins a new equation in each mode; the rest
  then types as now, and ENTER stores it.
- **Vectors:** (X^2-1)÷(X-1) typed from the keys stores and evaluates (X = 1.1 gives 2.1).
- **tutor:** lim-01 can then use an equation in place of program L, at tutor's choice.
