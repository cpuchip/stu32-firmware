# Blueprint: the CAS on the stack (SIMP, EXPND and SOLVE on X's expression)

**From:** soroban · **To:** abacus · **Asked:** decision 95 item 1 (abacus #13664: "simplify, expand and solve on the
expression in X (082's step 3, the one 084 pointed to). Blueprint first.") · **Unit:** 088 if abacus agrees (087 was
the last). **Base:** 99a62eb.

**How it was measured.** A throwaway prototype (a scratch worktree on 99a62eb, never committed): about 30 lines in
core/calc.c. cas_step reads X's text when Equation mode is off and X is an expression, and on SIMP and EXPND it
replaces X. The list path is untouched. Every behaviour below is from it through tests/vectors, or from the list path
at 99a62eb (casim eedbf16).

## What a student can do today, and what's missing

- **The path:** 082 and 083 drop an equation to X (EQN, MORE, →X), and 084 does arithmetic on it. `2×X+5=11`, then
  2 ×, gives `(2×X+5)×2=11×2` in X.
- **What's missing:** a CAS key outside Equation mode is NO EQN, and the CAS menu is reached only from EQN LIST's soft
  key. So the expression can be built on the stack, but not simplified or solved there.

## 1. The rule

In STU, outside Equation mode and program entry, with an expression or a name in X:
- **SIMP and EXPND** replace X with casim's answer, as an expression. LASTx gets the old X; nothing lifts or drops;
  the result enables the lift, as a one-argument function's does (084's class).
- **SOLVE (in a variable)** opens the report on X's text, as on the list. X stays while the report shows.
  - ENTER on a root puts its value in X with the stack lifted, as RCL does today, so the expression goes to Y.
  - The repin's rule holds: a row that fails at the stored values is CONDITION FAILS.
- **Errors leave everything as it was:** casim's refusals (TOO LONG, TOO COMPLEX, COEF NOT NUMERIC, …) as on the list,
  MEMORY FULL when the store has no free slot, and C during a SOLVE.
- **The text is X's own, read as STU's** (082 stamps a dropped expression STU). The angle mode and CAS3's CPLX apply,
  as on the list.
- **Unchanged:**
  - A number in X stays NO EQN.
  - 33s and 35s modes are STU ONLY.
  - ALG is unchanged: 084 found its keys type into the line and never reach the dispatch with an expression.
  - The list's CAS keys still work on the list.
  - Every other CAS key outside the list stays NO EQN.

**Measured (the prototype):**

| keys (STU) | X after | Y after |
|---|---|---|
| 7, EQN `2×X+5=11` →X, 2 ×, SIMP | `4×X+10=22` | 7 |
| … SIMP, LASTx | `(2×X+5)×2=11×2` | `4×X+10=22` |
| `(X+3)^2` →X, EXPND | `6×X+X^2+9` | |
| 7, `2×X+5=11` →X, SOLVE X | the report `SOLVED: X=3`, with X still `2×X+5=11` | |
| … SST, ENTER | 3 | `2×X+5=11` |
| `3×X+2×X` →X, SIMP, 3 | 3 | `5×X` (the lift) |
| `V=I×R` →X, SOLVE R, SST, ENTER (MEMORY CLEAR, I=0) | CONDITION FAILS, cursor on `I≠0` | |
| `A×X^2-1` →X, SOLVE X | COEF NOT NUMERIC, X unchanged | |
| 5, SIMP | NO EQN (as today) | |
| the list: `3×X+2×X` SIMP | the list gains `5×X`, as today | |

**What casim gives on typical stack texts** (the list path, the same call):
- **SIMP:**
  - `(2×X+5)×2=11×2` → `4×X+10=22`
  - `3×X+2×X-X` → `4×X`
  - `1÷3+1÷6` → `1÷2`
  - `2+3×4` → `14`
  - `2×π÷π` → `2`
  - `A×B+B×A` → `2×A×B`
  - It leaves `(X^2-1)÷(X-1)` alone (CANCL reduces it) and `SIN(X)^2+COS(X)^2` too.
- **EXPND:**
  - `(X+1)×(X-1)=0` → `X^2-1=0`
  - `(X+3)^2` → `6×X+X^2+9`
- **SOLVE:**
  - `X^2-5×X+6` → `SOLVED: X=2; X=3`

## 2. The keys: how a student reaches them (Michael's to choose)

Today nothing on the stack reaches SIMP, EXPND or SOLVE. Options:
- **(a) A bar that follows X.** In STU, outside Equation mode and program entry, with no menu open and an expression or
  a name in X, the soft keys show **SIMP · EXPND · SOLVE · →EQN · (empty) · ▸** (▸ to the CAS menu).
  - It is the CAS REPORT bar's pattern (app.c's eqn_bar): the bar follows the state.
  - Today those keys show nothing on a plain stack, so nothing is displaced.
  - →EQN stores X back without first going into the list. Today it's only in EQN MORE.
  - When X holds a number, the bar is gone.
- **(b) The CAS menu on a shifted key everywhere.** It acts on the list in Equation mode and on X outside it. That needs
  a free shifted position; I haven't surveyed which are free.
- **(c) Both.**

**Lean: (a).** It shows only when it can act, and it uses no new key. The legends and the empty fifth key are Michael's.

## 3. Questions for abacus (and Michael)

1. **The keys:** (a), (b) or (c) above.
2. **A result that is a plain number** (`2+3×4` SIMP gives `14`). Today the prototype leaves the expression "14" in X,
   and 084's arithmetic would then build `14+1` from it.
   - (i) Keep it an expression; EVAL, a later unit, makes numbers.
   - (ii) A result that is a decimal number alone (digits, a point, a sign, an exponent; no operator, letter or π)
     becomes a number in X. Anything else, such as `1÷2` or `2×π`, stays an exact expression.

   **Lean: (ii).** A student who simplifies to 14 expects 14. (ii) loses nothing, because it applies only where the
   text is already a decimal number.
3. **SOLVE's ENTER keeps the expression in Y** (as measured: RCL's lift). Or the root replaces X, with the expression
   in LASTx. **Lean: keep it in Y**, as the list's ENTER does, so the work isn't lost.
4. **More ops on the stack later** (D/DX, ∫DX, FACTR, CANCL). The prototype's path takes any op; this unit is the
   three that decision 95 names.

## 4. What changes in the code (expected)

- **core/calc.c:**
  - cas_step reads X's text and the STU model when the CAS acts on the stack, and on SIMP and EXPND it replaces X
    (ab_expr_new, LASTx, the lift).
  - The dispatch sends SIMP, EXPND and SOLVE there when X holds an expression or a name.
  - Answer 2 (ii) is one check of the text.
- **firmware/app.c:**
  - The bar (answer 1).
  - SOLVE's `computing` mark (the interrupt's delivery, unit 048) is today `c->eqn_mode`-only, and covers the stack
    SOLVE too.
- **firmware/keymap.c:** the bar's entry.
- **Unchanged:** casim, the image (an expression in X is image 21 already; the report isn't saved, as on the list),
  and the list's CAS.

## 5. The oracle

1. **make test:**
   - a new tests/088-soroban.txt with every row in §1's table, each casim answer as at eedbf16;
   - LASTx;
   - errors leaving X;
   - MEMORY FULL with the store full;
   - the number rule (answer 2);
   - C during SOLVE;
   - the list unchanged (048 and 057 rows);
   - app_test for the bar (shown with an expression in X; not shown with a number, in Equation mode, in program
     entry, in ALG, or in 35s mode).
2. **Mutants, predicted before running:**
   - the list's text used on the stack;
   - LASTx not set;
   - the stack lifted;
   - a number in X sent to casim;
   - the bar shown with a number;
   - (ii)'s check accepting `1÷2`.
3. **D68:** flash expected in the hundreds of bytes; RAM +0.
4. **The stand-in** (a replayed script): `2×X+5=11` from the keys, →X, 2 ×, then the bar's SIMP and SOLVE, ENTER.
   Its frames are the host's (FRAMECRC), and the stack low-water stays above the deep paths'.
5. **clang-check.**
