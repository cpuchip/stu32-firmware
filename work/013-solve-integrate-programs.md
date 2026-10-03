# Work unit 013: SOLVE and ∫ on programmed functions

**From:** abacus · **To:** soroban · **Status:** ready after 012 (light brief; you extract examples)

## Scope (HP 35s User's Guide ch.15 pp.15-1 to 15-12; ch.7 and ch.8 for what SOLVE and ∫ return,
## how to read their results and how to choose guesses; Appendix B)

FN= label (select the programmed function); SOLVE variable (the unknown's stored value and X are
the two initial guesses, ch.7); ∫FN d variable (lower limit in Y, upper in X); what each leaves on
the stack and in the variable, and the messages when SOLVE finds no root or ∫ cannot proceed (read
ch.7's "Interpreting Results" and ch.8's "Accuracy of Integration"); INPUT for the unknown or the
variable of integration is skipped during SOLVE/∫ (pp.15-1, 15-8), other INPUTs read the stored
value; SOLVE and ∫ used inside a program (pp.15-6, 15-10); the restrictions (p.15-11, e.g. no
recursive SOLVE). Solving and integrating equations wait for the equations unit; the 35s's
equation list is not in this unit.

## Rules (manual first; ours where noted)

1. **Ours, the algorithms.** HP's root finder and integrator are theirs; ours are published
   mathematics, named in the code with their sources. SOLVE: a bracketing-safeguarded method (for
   example Brent's, 1973) that returns a root when the function changes sign or reaches 0, a
   minimum-of-|f| report otherwise, with the guide's stack contents and messages. ∫: an adaptive
   rule of your choice (for example Gauss–Kronrod 7-15) that returns the integral in X and an error
   estimate in Y, as the guide's ∫ does.
2. **Tolerance from the display setting**, as the guide says ∫'s accuracy is controlled (ch.8): the
   uncertainty requested follows FIX/SCI/ENG/ALL. Say exactly how you map each setting to a
   tolerance, and what ALL means (the guide's answer, if it gives one).
3. Each function evaluation is one program run under unit 011's step budget; an error inside the
   function during SOLVE follows the guide (read whether SOLVE treats it as a bad estimate or stops).
   Count evaluations and report typical counts: the device pays for each one.
4. Stack lift and LAST X around SOLVE and ∫ as the guide and Appendix B say.

## Acceptance and report

As before: the guide's ch.15 examples in RPN form (the ALG ones rewritten as RPN programs), the
ch.7/ch.8 examples that apply to programmed functions, red then green, with evaluation counts. At
review: hidden polynomials and transcendental functions with known roots, and integrals with
closed forms, judged against the tolerance rule 2 states (and the error estimate in Y checked to
cover the true error).
