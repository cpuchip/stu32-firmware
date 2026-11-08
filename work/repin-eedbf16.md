# Repin: casimir eedbf16 (CAS 038, letters as coefficients in solve)

**From:** abacus #13646 (2), #13653 (ENTER), #13754 (CAS 038 accepted at eedbf16, decision 96) · **Status:** blueprint.
Its measurements are on the host at the repin (CASIM_PIN eedbf16 over 0a7de57, not committed).

## What changes

- **CASIM_PIN eedbf16.** SOLVE on an equation linear in the variable, with letters (or π) as its coefficients, now
  answers with rows: `V=I×R` for R is `SOLVED: R=V÷I; I≠0`, where 1.5.0 said COEF NOT NUMERIC. calc.c:3903 already
  passes the typed text, so the letters reach casim as typed.
- **No firmware code changes for the repin itself.** ENTER's rule is below, for abacus to rule.

## Measured on the host (the repin's build, tests/vectors through the app's core)

**make test:** rc 0, and nothing moved.
- 048 S09 (`A×X^2-1`, degree 2) and F05 (FACTOR) still say COEF NOT NUMERIC, as CAS 038 leaves degree 2 alone.
- No existing row had a letter solve linear in the variable.

**The report's rows,** each in cells (one per code point, as eqn_line draws them):

| SOLVE | status | answer | rows |
|---|---|---|---|
| V=I×R for R | SOLVED | R=V÷I [5] | I≠0 [3] |
| P×V=N×R×T for T | SOLVED | T=P×V÷N÷R [9] | N≠0 [3] · R≠0 [3] |
| X×A=X | SOLVED | X=0 [3] | A≠1 [3] |
| SQRT(A×B)×X=1 | SOLVED | X=1÷SQRT(A×B) [13] | A≠0 [3] · B≠0 [3] · A×B≥0 [5] |
| X×SQRT(-B÷2-C÷2)=1 | SOLVED | X=1÷SQRT(-B÷2-C÷2) [18] | -B÷2-C÷2>0 [10] |
| X÷(B÷2+C÷2)=1+X | SOLVED | X=1÷2÷(1÷(B+C)-1÷2) [19] | 2÷(B+C)-1≠0 [11] · B÷2+C÷2≠0 [9] |
| X÷(A-B)+X÷(C-D)=1 | SOLVED | X=1÷(1÷(A-B)+1÷(C-D)) [21] | 1÷(A-B)+1÷(C-D)≠0 [17] · C-D≠0 [5] · A-B≠0 [5] |
| X×(A^2+B^2-C^2-D^2)=(A+B)×(C+D) | SOLVED | X=(A+B)×(C+D)÷(A^2+B^2-C^2-D^2) [31] | A^2+B^2-C^2-D^2≠0 [17] |
| S=U×T+A×T^2÷2 for A | SOLVED | A=2×(S-T×U)÷T^2 [15] | T≠0 [3] |
| 1÷F=1÷U+1÷V for U | COEF NOT NUMERIC: CAS 038 declines (U in a denominator), and 1.5.0's refusal stands | | |

**The rows' width on Rev A.**
- **How they draw:** the report is SYS's view, four lines at scale 2 (screen.c's `k->view`). A row holds
  (398 − 20 + 2) ÷ 12 = **31 cells** (draw_line).
- **Rows past 31 cells:** they show their first 30 cells and an ellipsis (the cursor-less window). ENTER still
  reads the whole row.
- **What was measured:** every row above fits. The longest is 31 (`X×(A^2+B^2-C^2-D^2)=…`'s answer, exactly the line), and the
  longest condition row is 17. abacus's two, `-B÷2-C÷2>0` (10 cells) and `A≠0; B≠0; A×B≥0`, are four rows of 3 to 10
  cells, since a report's rows are split at "; ".
- **Scrolling:** a report with more than four rows scrolls with SST (rows k−3..k), as 048's did.
- **Rev A's screen** is the LS027 frame that screen.c draws on the host. Its head-less image on the stand-in doesn't
  drive a display, so the board check of the frame is the stand-in's FRAMECRC against the host's replay (below).

## ENTER on an answer row (abacus #13653): measured

casrep_key's ENTER evaluates the answer's text with the stored letters (eqn_eval) and doesn't read the other rows.
At the repin, from the keys' path:

| | stored | report | ENTER puts in X |
|---|---|---|---|
| E03 | V=5, I=0 | R=V÷I; I≠0 | DIVIDE BY 0, the view stays |
| E08 | A=−1 | X=(A+1)÷(A^2-1); A≠-1; A≠1 | DIVIDE BY 0 |
| E09 | A=1 | X=1÷LN(A); A>0; A≠1 | DIVIDE BY 0 |
| E01 | A=1 | X=0; A≠1 | **0**: true, but every X solves at A=1 |
| E07 | A=0, B=7 | X=B; A≠0 | **7**: true, but every X solves at A=0 |
| E05 | Y=0 | X=3; Y≠0 (X×Y÷Y=3) | **3**: **not a solution**, since X×Y÷Y is undefined at Y=0 |
| E11 | A=0, B=3 | X=A×B; A≠0 (X÷A=B) | **0**: **not a solution**, since X÷0 is undefined |
| E14 | A=−1 | X=1; A>0 (X×SQRT(A)=SQRT(A)) | **1**: **not a solution**, since SQRT(−1) is not real |
| E02, E04, E12 | rows hold | | the answer's value |

There are two classes:
- **The coefficient's row** (a≠0, E01 and E07). When it fails, either the answer divides by zero and ENTER errors,
  or it doesn't, and the value is one solution of many.
- **A condition of the typed text** (E05, E11, E14). When it fails, the value ENTER places isn't a solution. casim
  guarantees "no answer without its check". Today the keys keep that guarantee only where every row holds, and say
  nothing where one doesn't.

**Options:**
- **(a) Check the rows at the stored values before placing.**
  - For each row after the answers that has no `=` (≠, > and ≥, CAS 038's cdset), split it at its operator. Evaluate
    both sides through the core (eqn_eval, STU's parse, the angle) and compare.
  - A row that fails, or whose side can't be evaluated, places nothing. It shows a status, with the cursor moved to
    that row so the failing row is the one shown.
  - The status's words are Michael's. Until he gives them, ROW FAILS is a placeholder.
  - Its cost isn't measured; I expect it in the hundreds of bytes on the S3.
  - The comparison is of the stored numbers, which is what the student has. A row exactly at its edge in decimal
    (B=1, C=−1 in `B÷2+C÷2≠0`) evaluates to an exact 0, so it fails as it should.
- **(b) Leave it, and document it** (the report's rows are the student's to read). It costs nothing, and E05, E11
  and E14 stay silent.
- **(c) Check only the typed text's conditions.** The firmware can't tell those apart from the coefficient's row in
  the report's text, so this would need a change to casim's output. Listed only to rule it out.

**Lean: (a), with one rule for both classes.** The answer holds only where its rows hold, and ENTER's job is to put
a solution in X. E05, E11 and E14 are exactly what CAS 038's oracle rules out on casim's side.

## What the build will measure

1. **make test:** with new rows in tests/048-soroban.txt (or a repin file). The report rows above for V=I×R for R and
   P×V=N×R×T for T, and the long rows. E01 to E14 as ENTER rows, with each expectation set by the option abacus
   rules.
2. **D68 static 0a7de57 -> the repin:**
   - casim's own figure is 268,220 B for casim.c (casim's size-s3.sh: object text, unlinked). That is +3,026 over
     1.5.0's 265,194.
   - The linked figure is the D68 row: casim.c.obj, as in 087's map, 184,127 B at cd7db86.
   - Decision 94's line is 301,056 B.
3. **The stand-in** (device/standin/repin-eedbf16.txt, `# replay: current`):
   - VER names the pin;
   - from the keys, V=I×R SOLVEd for R, P×V=N×R×T for T, and SQRT(A×B)×X=1, each report read on the eqn: line's
     rep= and row= with SST through its rows;
   - FRAMECRC on the rows view, equal to the host's standin_replay, so the board draws the frame whose widths are
     measured here;
   - the stack's low-water mark no lower than the deep paths';
   - a control with the old pin.
   - For R and T from the keys, tests/stackkeys.c's `cas s TEXT` gains an optional variable: today it always
     presses X.
4. **The same script on Rev A head-less:** its rep= and row= lines (no FRAMECRC there).
