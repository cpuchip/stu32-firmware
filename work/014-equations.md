# Work unit 014: equations, part 1 (the equation list, syntax, evaluation)

**From:** abacus · **To:** soroban · **Status:** ready after 013 (light brief; you extract examples)

## Scope (HP 35s User's Guide ch.6, pp.6-1 to 6-20; Appendix B; Appendix F for the messages)

The equation list (entering, displaying, selecting, editing and clearing equations, pp.6-4 to 6-8);
the three types of equation (equality, assignment, expression, p.6-9); evaluating with ENTER and
with XEQ, and responding to prompts for variable values (pp.6-10 to 6-13); the syntax: variables,
numbers, functions, parentheses, operator precedence (pp.6-14 to 6-16) and the functions allowed in
equations (p.6-16 on); syntax errors, reported when an equation is evaluated (p.6-19). Equations as
program lines and as messages (flag 10, p.13-16), flag 11's prompting, and SOLVE and ∫ of
equations come next (unit 015). Not in this unit: the checksum and length (p.6-19), which is HP's
algorithm; ours, if any, comes later.

## Rules (manual first; ours where noted)

1. **An equation is text** in the guide's notation, stored as typed (the guide displays equations as
   entered, p.13-16); it is parsed when evaluated, as the guide says. Define the token stream the
   keys produce and the grammar you parse, from pp.6-14 to 6-16, and say where the guide is silent
   (for example unary minus against ^, implied multiplication, if the guide allows it).
2. **Evaluation uses the same core functions as the keyboard** (so every accuracy rule of units
   003-010 holds inside equations) and the same variables; an assignment stores its result.
3. Stack lift and LAST X when an equation is evaluated, as Appendix B and the guide's examples show.
4. Errors in an equation (syntax and domain) are reported as the guide names them and change
   nothing; say which line of the guide pins each.
5. **Ours, capacity:** the equation list's size is a named constant; equations share memory with
   programs on the 35s (p.13-21): say how you budget it.

## Acceptance and report

As before: the guide's ch.6 examples as M-vectors (the equation entries, the three types, ENTER
and XEQ evaluation, the precedence table's cases, the syntax-error examples), with a vector format
for typing an equation (your design), red then green. At review: hidden random expressions, built
and evaluated by an independent reviewer parser written from the guide's precedence table, checked
against the keyboard path for the same operations.
