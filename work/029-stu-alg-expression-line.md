# Work unit 029: STU mode, and its ALG as an expression line

**From:** abacus · **To:** soroban · **Status:** ready after 028 (abacus decision 51, Michael, 2026-10-06)

## Why

Michael chose STU's ALG (decision 51): "Option one and your side question with last ans lands in x is good."

- **Option one** is an expression line, typed whole the way a textbook writes it (`sin(30)+2×3²`), edited with the
  equation editor's cursor, and finished with `=`. Students coming from TI calculators and Desmos already work this
  way. tutor's course can then show the same calculation in ALG and in RPN side by side.
- **The side question:** switching from ALG to RPN puts the last answer in X.

The equation engine already does the parsing, precedence and every function. This unit is entry, display, `ANS`
and history. The HPs' own ALGs are later units: the 35s's next (function first, as here), then the 33s's (value
first).

## Rules

1. **STU becomes a personality.**
   - MODE's F3 is `STU`, beside 33s and 35s. It is saved in the state image, and the status line shows it.
   - Until STU's own calls are ruled, STU behaves as 35s mode in everything but entry (abacus's call in
     decision 51).
   - A label written in STU mode is stamped STU (decision 45), and runs as 35s mode does.
2. **ALG (MODE F5) is offered in STU mode only, in this unit.**
   - It is blank in 33s and 35s modes until their own ALG units.
   - Switching personality to 33s or 35s while in ALG returns to RPN, by rule 6.
   - RPN/ALG is saved in the state image. The status line shows `ALG` or `RPN`.
3. **The expression line.** In ALG, the keys that type in the equation editor type into the line, with the editor's
   cursor, insert, ◀ ▶ and ← (`eqn_type`, unit 014). A function key inserts `SIN()` with the cursor inside it.
   - **`=` is ENTER.** It parses and evaluates the line by the equation engine's precedence (35s p.6-14), in the
     current angle unit.
   - **Variables** A–Z (and i) read their stored values without prompting, as ENTER on an expression does.
   - **The display:** the line stays on screen with the result below it, right-aligned in the current display
     format.
   - **Mistakes:**
     - A syntax error puts the cursor at `eqn_errpos`, and the line stays editable.
     - A math error (DIVIDE BY 0 and the rest) shows its message. The next key clears it and leaves the line
       editable.
   - **After a result,** typing a digit or a function starts a new line. Typing a two-number operator (+ − × ÷ ^)
     starts the new line with `ANS` before it, so `+ 3 =` adds 3 to the last answer.
   - **C clears the line,** and the history is kept.
4. **ANS** is a name the parser knows, for the last result.
   - It starts as 0.
   - It is set by each successful `=`, and kept in the state image.
   - A key types it in ALG. Choose that key, and say which in your report (LASTx's place is the natural one, since
     ALG has no LASTx); I'll put it in docs/keyboard.md.
   - `ANS` in a stored equation or a program's equation line is SYNTAX outside ALG. It is not a variable.
5. **History.** ▲ and ▼ step through earlier lines and their results, at least the last 16.
   - ENTER on a history entry copies its text into the edit line. ◀ or ▶ on one starts editing a copy.
   - The history is in RAM only and lost at a power cut. It is not in the state image (ours: the image stays
     small).
6. **ALG → RPN puts ANS in X** (Michael's ruling), arriving as a keyed number would: the stack lifts unless lift is
   disabled.
   - **RPN → ALG** sets ANS to X. That is ours, the mirror of the ruling, so a calculation started in RPN carries on
     in ALG.
   - The stack is otherwise untouched by ALG. A result is not pushed.
7. **Keys with no meaning on an expression line show `RPN ONLY`** briefly and do nothing: R↓, x↔y, LASTx's RPN
   role, the stack-review keys, and program entry and XEQ of programs. Decision 46 keeps programs RPN.
8. **STO and the other commands that act on X:**
   - **`STO A` after a result** stores ANS.
   - **`STO A` with a line being typed** evaluates the line as `=` does, then stores the result (ours: the 35s's
     appendix C doesn't describe STO in ALG, so this is a choice, not a copy).
   - Implement STO, and list any other X-acting command you meet in the report rather than guessing at it.
9. **Numbers and modes.** Precision, rounding, overflow (unit 026) and the base modes are the engine's, unchanged.
   - In a base mode the line accepts that base's digits, as the equation editor does.
   - Complex and vector values work as the equation engine already evaluates them.

## Acceptance

- **Your vectors,** red first:
  - a token or tokens to select STU and ALG, and the line's text and the result as expectations;
  - every rule above at least once, with rules 3, 6 and 7 at their edges: ANS at its first use, lift disabled
    before the switch, ALG → RPN → ALG round trips.
- **A hidden set from abacus:**
  - expressions evaluated by mpmath at 120 digits, rounded once to 34, the way units 028 and CAS 002 were judged;
  - typed as key sequences: function keys insert `F()`, and ▶ steps out of a closing parenthesis;
  - judged within 8 units in the 34th digit;
  - plus a precedence set: ^ right-associative, unary minus against ^ (−2² is −4: the 35s's table, p.6-14, puts power before
    unary minus), and implicit multiplication if the engine has it, stated either way.
- **No change to any accepted unit's vectors.** The equation engine's behaviour in Equation mode is untouched.
- **Mutants, each caught:**
  - ALG → RPN without the lift;
  - an operator after a result not inserting ANS;
  - the history kept in the state image (a state-size test should see it);
  - ALG offered in 33s mode.
- **The S3:** the state image's size before and after, and the history's RAM.
