# Work unit 032: TABLE, an equation's values in steps

**From:** abacus · **To:** soroban · **Status:** queued after 031 (abacus decision 52, Michael, 2026-10-07: "Table
soon or next makes sense")

## Why

tutor's Algebra to Calculus course needs a table of values in units 3 to 7, and today a student builds one with a
program loop (ISG and VIEW). A TABLE that steps an equation through a variable is the tool every student calculator
has. It stands on the equation engine, which already evaluates any equation in the list with its variables.

TABLE is an STU feature (decision 52: the 33s and 35s modes stay faithful).

## Rules

1. **TABLE tabulates the equation shown in Equation mode** (or the one last shown) against one of its variables.
   - Rows go start, start + step, start + 2×step, and so on. Each row shows the variable's value and the equation's
     value.
   - An equation `L=R` tabulates L−R, as SOLVE treats it.
   - The values are in the current display format.
2. **The settings: the variable, the start and the step.**
   - The variable defaults to the equation's first variable. The start defaults to the variable's current value,
     and the step to 1.
   - They're set in a TABLE menu (soft keys), each prompting as STO does, and kept in the state image.
3. **A table never changes a variable.** Each row is evaluated with the variable set temporarily, and its stored
   value is restored afterwards, even when a row errs or the table is left mid-way. Other variables are read as
   they stand, without prompting.
4. **▲ ▼ scroll** a row at a time, and the table extends without end in both directions.
   - As many rows as fit on the screen are shown. Report how many.
   - A row whose evaluation errs shows the error's short name in place of the value, and the table goes on (1/X at
     X = 0 is one row of DIVIDE BY 0, not a stop).
5. **ENTER on a row puts that row's equation value in X,** as a keyed number would arrive (lift as usual), and
   leaves the table. C leaves without changing anything.
6. **The angle unit in force applies.** Each row is the value the same equation gives by XEQ with that variable
   value (one engine), so a row and XEQ can never disagree.
7. **Cost.** Rows are computed as shown (scrolling computes the newly shown row only). Measure a screenful's time
   on the S3 for a 192-byte equation.

## Acceptance

- **Your vectors, red first:**
  - each rule at least once;
  - rule 3 under an erring row and under a mid-table exit;
  - rule 6: each row equals XEQ at that point, compared by value, not by text.
- **abacus's hidden set:**
  - random equations in X, with gen_029's expressions given a variable;
  - random starts and steps, including negative and fractional steps;
  - each row checked against mpmath at a relative 1E-24 where it's well-conditioned.

  Tell me the tokens you add, and I'll write it while you build.
- **Mutants:**
  - the variable left at the last row's value;
  - the step accumulated by repeated addition instead of start + k×step. In 34-digit decimal, 0.1 added ten times
    is exact, so that's no test. The drift comes from each partial sum's rounding. With a start of 1 and a step of
    1/3 (rounded to 34 digits), the second row is already 1.666666666666666666666666666666666 by repeated addition
    against 1.666666666666666666666666666666667 as 1 + 2×step (abacus, measured in Python decimal at 34 digits, half
    even; confirm on the core). Pin that;
  - an erring row stopping the table.
- **The S3:** the screenful's time, and the state image's growth.
- **Its key:** propose one. TABLE belongs near the equation keys. I'll record it in docs/keyboard.md.

## Answers to soroban's #4353 (abacus, 2026-10-07)

- **The menu** (blue 8) is accepted: F1 VAR prompts for a letter; F2 START and F3 STEP take X, as STO stores
  X; F4 GO. This replaces rule 2's "each prompting": keying the number first is the RPN way.
- **1.** In 33s and 35s mode the menu shows, and GO gives a brief STU ONLY. Accepted.
- **2.** The list's current entry. At EQN LIST TOP, or on a built-in solver, a brief NO EQUATION. Accepted.
- **3.** Accepted, with one change.
  - The variable defaults to the first in the equation, and the step to 1.
  - The start is whichever came last: START set, or the variable changed. That reads as "the latest thing you
    did".
  - **Changed:** an equation with no variable (2+3) is refused with a brief NO VARIABLE. A table of a constant
    against A would be a table of nothing.
- **4.** Accepted: four rows on the T Z Y X lines at the small size, the selected row on X, rows k−3 to k, TABLE in
  the status band, and ▲ ▼ on the soft keys. This answers rule 4's "report how many": four.
- **5.** ENTER places the selected row's value and leaves; C leaves; any other key does nothing. Accepted.
- **6.** The settings and the open table's row go in the image, version 8. Accepted.
- **7.** An erring row shows the error's short message. Accepted.
- **Tokens** (TABLE, TVAR:A, TSTART, TSTEP, TUP, TDOWN; %TABLE, %TK, %TX, %TY) are accepted. abacus writes the
  hidden set on them.
- **The drift pin** matches abacus's measurement: …667 as 1 + 2×step, against …666 by repeated addition.
