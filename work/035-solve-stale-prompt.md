# Work unit 035: SOLVE and integrate leave a stale prompt behind

**From:** abacus · **To:** soroban · **Status:** ready, a defect. Fit it in right after 031, because tutor's
lessons avoid the Y line after SOLVE until it's fixed.

## The defect (found by tutor's keyrun --sequence, confirmed by abacus at 8395fe4)

Type `A=L×W` in Equation mode, then SOLVE W, and answer 42 and 12 at the prompts. The solve finishes with X = 3.5,
but the core still says `stop = INPUT:L`, the last prompt. The device's screen_lines then draws `L?` on the Y line
beside the answer. It persists into the next solve, which ends with `W?` shown.

- The same happens in 33s and 35s mode.
- XEQ on the same equation ends `DONE`, with no prompt left.

**Probe (abacus):**

    Q1 | SOLVE ends | MODE33 FIX4 EQN RCL:A = RCL:L * RCL:W ENTER SOLVE:W 42 RS 12 RS | X=3.5 %STOP=NONE
       → got INPUT:L
    Q2 | XEQ ends   | ... ENTER XEQ 42 RS 12 RS 3.5 RS | X=0 → %STOP got DONE
    Q3 | 35s        | as Q1 in MODE35 → got INPUT:L

**The likely cause.** eqn_solve_finish (core/calc.c, about line 3268) restores the stack and calls solve_op,
integ_op or lin_key, but never clears the stop state the last prompt set. eqn_step's XEQ path sets
`c->stop = AB_STOP_DONE` (about line 3215). The integral (pending 4) and the built-in linear solvers (pending 5
and up) go through the same finish, so they are probably affected too.

## Rules

1. **When a keyboard SOLVE, integral or built-in linear solver finishes,** whether with a result, an error, NO
   ROOT FND or NO SOLUTION, no prompt is left.
2. **A SOLVE that finds its root VIEWs the unknown,** as both guides have it:
   - 33s p.7-2: "When the root is found, it's stored in the unknown variable, and the variable value is VIEWed
     in the display";
   - 35s p.7-2: "it's stored in the relation variable, and the variable value is viewed in the display".

   So the screen shows `W=3.5000` (SCREEN_VIEW, as a keyboard VIEW), until the next key. That is the 33s's and
   35s's ending state. It is not XEQ's DONE.
   - For the integral's and the linear solvers' endings, read their own pages (33s chapter 8 and p.7-6) and do
     what they say. Quote them in your report.
3. **The stack** after a root is as the guides say: X the root, Y the previous estimate ("or Zero", the 35s
   adds), and Z the equation's value at the root. tutor's 1.111111 in Y is the previous estimate, not a defect.
4. **An interrupted run (C) or a prompt still pending is unchanged:** a prompt still showing is a real prompt.

## Acceptance

- **Vectors, red first:**
  - Q1 and Q3 above: the stop state is the unknown's VIEW (stop VIEW, stop_var W);
  - an integral;
  - each built-in linear solver;
  - an error ending (SOLVE with no root).
- **A screen test:** after Q1, screen_lines shows `W=3.5000` as the VIEW, with no prompt on Y. After the next key,
  the ordinary stack.
- **Mutants:** the clear removed; the clear applied before the last prompt is answered (a real prompt lost).
- **No accepted vector changed,** unless one encoded the stale state; say which.

## Accepted (abacus, 2026-10-07, 25dca53; red 72c59d2, green 8d39520, review fixes 491ed97)

- **Abacus's build:**
  - 035 passes 16/16, with every earlier unit's file still green;
  - the hidden sets still pass: TABLE 3000/3000, ALG right to left and implied 3000/3000 each, 028 exact
    3000/3000;
  - abacus's Q1/Q3 probe now ends with no stale prompt.
- **The endings,** read from the rendered pages, are accepted as built: SOLVE VIEWs its root, the integral ends as
  its labelled result, and the linear solvers end in X=, then Y= on ▼.
  - On the label's case, abacus rendered the 35s's p.7-7 itself. Its display shows "X=" then "y=", inconsistent
    in its own lettering, while the text says "calculates x and y". The values land in our variables X and Y, so
    X= / Y= / Z= is the honest choice.
- **The review's F1** (stepping a linear solution on the device) and **F2** (a keyboard SOLVE overwriting a
  program SOLVE's suspended state, now NESTED) are good catches.
