# Unit 094: SOLVE's report, ENTER copies and EVAL evaluates (blueprint, before any code)

**From:** decision 98 (abacus 08af29b; Michael 2026-11-09, "Hold eq-04 until the change is done"), abacus #14144 (the
five items), #14150 (tutor's correction, #14148: eq-04's four places). Read at stu32-firmware 2367efb.

**Status:** draft. No code. A throwaway prototype in a scratch worktree (ENTER changed alone, then a trap on every answer
row's ENTER) measured what moves; its numbers are below. The prototype isn't kept.

## Today (99a62eb's rule, measured at 2367efb)

- The report (048) opens on SOLVE's status row, with ▲ ▼ on the bar "CAS REPORT" (soft keys 1 and 2; 3-6 empty).
- ENTER on an answer row (`X=…`; its `(MULT k)` not part of it) first checks every condition row at the stored letters
  (038's repin, abacus #13771). The first row that fails gives CONDITION FAILS with the cursor on it, and the view stays.
  Otherwise it evaluates the right side (STU's parse, the angle mode), puts the number in X with lift as RCL's, and closes
  the report. ENTER on the status row, a note row or a condition row (no `=`) does nothing (calc.c:5309-5345).
- The row it makes (rows.c:563): SOLVE, linked to Y's row after the lift.

## The change

1. **ENTER copies.** On an answer row, ENTER puts the answer's right side in X, the same meaning as COPY on a selected
   093 row: the exact value, lift as RCL's, and the report closes.
   - The right side is the text after `=`, with ` (MULT k)` dropped.
   - It's put in X by 088's rule for a CAS result on X (calc.c:3968): a plain decimal (`cas_plain_number`) becomes a
     number, anything else an expression (`ab_expr_new`). So `X=1 (MULT 2)` gives the number 1, `R=V÷I` the expression
     `V÷I`, and `X=(1+SQRT(5))÷4` the expression `(1+SQRT(5))÷4`.
   - The rows aren't read. They're reached only through EVAL (abacus #14144).
   - MEMORY FULL (no free expression slot): nothing changes and the view stays, as 088's verbs do.
   - ENTER on the status, a note or a condition row: nothing, as today.
2. **EVAL evaluates.** A new op, AB_CASEVAL, nonprogrammable, does exactly what ENTER does at 99a62eb: the rows checked,
   CONDITION FAILS with the cursor on the failing row, else the right side evaluated to a number in X. Outside the report
   it does nothing.
3. **EVAL's place: soft key 3 of CAS REPORT,** the first empty one. ▲ ▼ stay on 1 and 2, so no vector or frame with
   the arrows moves. The bar becomes ▲ ▼ EVAL _ _ _.
4. **The rows the copy makes (R4).** ENTER and EVAL each make one row, SOLVE (its left as today), holding what went
   to X (an expression or a number). Its link is the row the solve read:
   - **Solved on the stack** (088: SOLVE on X's expression): X's row when SOLVE ran. That's what today's link to Y's row
     reaches after the lift, so nothing changes there.
   - **Solved from the equation list** (048): no row (the list's equation isn't a row), so no link.
   - **A bug this fixes (089's):** today's link to "Y after the lift" on the list path is whatever X held before ENTER,
     which the solve never read. Measured at 2367efb: `STU 7 ENTER EQ:X^2-5×X+6=0 CASSOLVE:X SST ENTER` makes row 2,
     SOLVE, linked to row 1, the typed 7. The fix: the solve records the row it read (X's on the stack, 0 from the list)
     beside the report (RAM only, as the report is), and ENTER and EVAL link to that.

## What moves (the prototype's census)

**27 vectors press ENTER on an answer row** (found by trapping that ENTER as an error in the prototype and running every
`./build` line of make test): 048's C01-C13, L06, R01, R02, R03, R09, R10 (19); 088's S02-S05 (4); 076's C03 and C22
(2); 079's W03 and W11 (2). With ENTER copying and nothing else changed, **17 of them go red**: 048's C01, C03-C13, R02
and L06; 088's S04 and S05; 076's C03.

The rule: a vector whose point is the evaluated value or a condition changes ENTER to EVAL and keeps every expectation.
A vector whose root is a plain number keeps ENTER, and from now on tests that a plain-number answer copies as a number.

| file | to EVAL (expectations unchanged) | keeps ENTER (green under the copy) |
|---|---|---|
| 048 | C01-C13 (the repin's set), L06, R02 | R01, R03, R09, R10 |
| 088 | S04, S05 | S02, S03 |
| 076 | C03 | C22 |
| 079 | (none) | W03, W11 |

- **C02 goes to EVAL although it stays green:** its point is "the row holds, 0 placed". Under the copy it would pass
  without the row being read.
- **038's repin and its M1 set:** C01, C03, C05-C10, C12 and C13 are all in "to EVAL". The rule they test (the
  condition rows) moves with them unchanged, so the M1 mutant ("no row fails") still turns exactly those red.
- **app_test:** 5 checks press ENTER on a root (088's four after `ENTER on the root: 3 in X`, 048's `ENTER on X=3`).
  All are plain numbers, so they stay green and keep ENTER.
- **The stand-in's replay:** cas088.txt's frame after ENTER (line 99, X=3) stays A98892E5 under the copy. The
  repin-eedbf16 script doesn't press ENTER on an answer row. No screen_test frame moves.
- **New vectors** (the build's, worked out by hand first): ENTER copying an expression (`R=V÷I` gives `%XEXPR=V÷I`),
  a long answer copied whole (L06's 37 cells), a complex root, MULT dropped, the rows not read by ENTER (C01's case
  copies 0 without CONDITION FAILS), EVAL on the status and condition rows doing nothing, EVAL outside the report doing
  nothing, the rows' links on both paths (the 089 bug's case among them), and MEMORY FULL.
- **HELP:** a new entry for EVAL; the report's ENTER text says it copies. help_test's sweep reaches the report's bar.

## eq-04 (stu32-tutor 56c910f; tutor #14148, through abacus #14150)

eq-04 presses ENTER on an answer row in four places. All four become EVAL (soft key 3 on the report's bar), with their
vectors and fmt-vectors:

| item | lines | X after |
|---|---|---|
| K02 | 95-102 | 4.0000 |
| K03 | 106-112 | CONDITION FAILS |
| F03 | 176-183 | 100.0000 |
| E02B | 236-242 | 3.5000 |

report-enter is a TOPICS row (TOPICS:99), not an item. What it says about ENTER changes to the copy, with EVAL for the
evaluation, in tutor's own words. I haven't read stu32-tutor; the lines and values are tutor's, as relayed.

## A tension to rule on, before the build

**C01's and C05's answers are plain numbers that the rows say aren't solutions at the stored values.** Under the copy,
`X×Y÷Y=3` at Y=0 puts 3 in X with no message: the case 038's repin was built to stop (`3` isn't a solution where Y=0).
The copy is right in the symbolic sense (it's the answer wherever its rows hold), and decision 98 reaches the rows only
through EVAL. I'm building it as ruled unless abacus or Michael says otherwise. The alternatives, for the record:
- (b) ENTER refuses with CONDITION FAILS when the answer is a plain number and a row fails at the stored values;
- (c) the copied row's INFO names its condition rows.

**Complex roots** copy as the report shows them: casim's `(-1+SQRT(3)×0i1)÷2` (measured in the prototype). A student
reads that text in the report already, so the copy doesn't add a new form.

## What the build would test (for abacus's oracle)

- The vectors above; the 18 moved to EVAL with their expectations as they are.
- **Mutants, predicted before they're run:**
  - EVAL as a copy;
  - ENTER evaluating;
  - ENTER reading the rows;
  - the copy as an expression always (no plain-number rule);
  - the link back to "Y after the lift" (the 089 bug);
  - EVAL's rows check removed (M1's set goes red, now under EVAL).
- **D68:** an op, a soft key, a help entry, the link's 4 B beside the report. Flash small, DIRAM +0 (the report's fields
  are in ab_calc, in PSRAM).
- **The stand-in:** cas088 replayed, and a script that copies `V÷I` and evaluates it, each frame against the host.

## Questions for abacus

1. EVAL on soft key 3.
2. The copied value by 088's rule (a plain decimal a number, else an expression).
3. The SOLVE row linked to the row the solve read, none from the list (fixing 089's link).
4. The rule for the 27 vectors (EVAL where the point is the value or a condition; ENTER kept on plain-number roots).
5. The tension: build as ruled (rows only through EVAL), or (b) or (c).
