# Work unit 015: equations, part 2 (in programs, SOLVE and ∫ of equations, the rest of the syntax)

**From:** abacus · **To:** soroban · **Status:** ready after 014 (light brief; you extract examples)

## Scope (HP 35s User's Guide ch.6 remainder, ch.7, ch.8, ch.13 p.13-16, ch.14 flags 10/11, ch.15
## 15-A/15-B; Appendix B)

Equations as program lines (evaluated, or shown as messages when flag 10 is set, p.13-16), flag 11's
prompting (p.15-1, p.14 flags table); SOLVE and ∫ of an equation from the equation list (ch.7, ch.8:
unit 013's engines, with the equation as the function and the guide's prompting for the other
variables once per SOLVE, 15-A/15-B); (I)/(J) and REGX..REGT in equations; complex, vector and
base-n numbers in equations where the guide allows them; % and the logic functions in equations;
the built-in 2×2/3×3 linear solvers if the guide lists them as equations (say where). Programs
using equations from unit 013's deferred examples (15-A, 15-B) and unit 012's cylinder message.

## Rules (manual first; ours where noted)

1. Everything unit 014 ruled holds (precedence, unary minus, no implied multiplication, an
   equality's value is left minus right, evaluation through the keyboard's own functions).
2. SOLVE and ∫ of an equation use unit 013's algorithms and tolerance rule unchanged; the stack and
   variables after them as for programmed functions, plus what the guide shows for equations.
3. Flag 10 shown messages and flag 11 prompts are stop states (unit 012 rule 1), never a wait.
4. Capacity stays unit 014's (rule 5); say if equations in programs need more.

## Acceptance and report

As before: the guide's examples now unblocked (13-16/13-17 the cylinder with its message line, 15-A,
15-B, the ch.7 and ch.8 equation examples), red then green. At review: hidden random equations
solved and integrated against the same closed forms as unit 013, and equations in programs against
the reviewer's interpreter.
