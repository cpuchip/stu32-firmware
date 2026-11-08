# The worksheet, part 2: the page (blueprint, before any code)

**From:** decision 95 step 4, unit 089's record ("For the next part"), and abacus #14028's two notes for this blueprint
(STO+ A leaves A with no row, L03; and whether the worst save's 426 ms fits power-off). The material is abacus #13972's:
the note's step 4 and the cards stu-worksheet, stu-worksheet-keep, stu-nav and stu-soft-shift.

**Status:** revision 1, after abacus #14041's rulings (its section at the end; where it changes a lean, the lean below
says so). No code yet: the level drawing waits on Michael's card, and abacus's hidden set is frozen before any src. The measurements are on the host at 915bcdb unless they say otherwise.
The prototype is throwaway, and its frames and source are kept in work/worksheet-evidence/part2/ as evidence, as part
1's census was. It drew mock frames from real rows by including screen.c, so it could reach its drawing functions.

## What part 2 is, and what it leaves to part 3

The proposed split is mine, and abacus's to change.

- **Part 2 draws the page on STU's home screen, in RPN and ALG:**
  - the rows, with the live stack under them carrying its labels;
  - the dropped count at the page's top;
  - ALG's 16-entry history replaced by the page's ALG rows;
  - STO+ A's row;
  - the power-off answer.
- **Part 3:** selecting a row, the row menus (stu-soft-shift's blue verbs on a selected row), moving and scrolling in
  RPN (stu-nav's 8/2/4/6 with arrows shown, a page mark in place of MORE), and what-made-this highlighting.
  - The note's three open items about moving, and the menu's order, belong there too.
- **What part 2 doesn't touch:**
  - The 33s and 35s modes: they make no rows, so they get no page, and their frames don't move.
  - Every STU screen that isn't the home: Equation mode (083's own screen, the textbook, the wheel), program entry,
    TABLE, GRAPH, SHOW, SYS's view, SOLVE's report, INPUT and VIEW. Each keeps its frame as it is now.
- **Part 2's only scrolling is ALG's ▲ ▼,** which today step through the history. In RPN, part 2 shows the newest rows
  that fit, and nothing older until part 3.

## The prototype: what fits, and how it reads (work/worksheet-evidence/part2/)

I didn't know whether the live stack plus a useful number of rows would fit legibly in the 400 × 240 frame. The
prototype draws the status band and soft keys with screen.c's own code. Between them it draws X at scale 4 (bottom at
y 200), the levels and the rows at scale 2, every 20 px, and a dotted rule between the page and the levels. The rows
come from the core's real page after real keys.

- **Lines:** the area is y 22 to 205.
  - At 18 px the lines nearly touch, and T's label met the rule (the first run, not kept).
  - At 20 px they read cleanly. That gives **4 page lines when all four levels are drawn, and 7 when only X is**.
  - Today's layout draws T, Z and Y every 28 px with X's bottom at 156, so y 157 to 205 is empty now.
- **Two ways to draw the levels** (p1-rpn-chain-A.png, p1-rpn-chain-B.png; keys `3 ENTER 4 × 5 + 2 ÷ √x`):
  - **A, all four always:** T 0, Z 0 and Y 0 sit between the page and X, as three lines of zeros. The page gets 4
    lines.
  - **B, empty top levels hidden:** a level is hidden when it and every level above it is 0 with row 0, so it's
    "bare": cleared or never used. The page then runs straight into X and gets 7 lines.
- **A level whose row is far back** (p2, `2 ENTER 3 ENTER 4 × 1 + 7 √x`): Z holds the first 2 typed. It's drawn in the
  levels, in Z's place, and not again up the page. The page reads 3, 4, × 12, 1, 7, then Z 2, Y + 13, X √x 2.6458.
- **T's copy-down** (p3, `1 ENTER 2 ENTER 3 ENTER 4 +`): Z and T hold one row. Each level draws its own line, so 1.0000
  appears twice, once labelled Z and once T. That is lean 4 from part 1, settled by this layout: neither label is
  dropped.
- **Typing** (p4): the number being typed is X's line, as today, and Y shows + 17 above it.
- **The dropped count** (p5): "230 OLDER ROWS DROPPED" is the page's first line, drawn only when the oldest row is on
  screen.
- **ALG** (p6, p6-alg-today.png; p7 mixed): a row is one line, its text at the left and its value at the right, when
  both fit in 31 cells at scale 2. Otherwise it's two lines, the value under its text.
  - Three lines and their answers stay on screen, where today shows one.
  - The line being typed and its result keep their places: Y's for the line, X's at scale 4 for the result.
  - RPN rows show in ALG too: it's one page (p7: 9, 16, + 25, then ALG's ANS÷5).
  - p7 also shows the duplicate the build has to avoid: right after ENTER, the line shown and the newest row are
    the same row, drawn twice. The rule under 3 removes it.

## The leans

### 1. The levels at the bottom, the page above them

- **The bottom of the home screen is the live stack:** X, then Y, Z, T upward, with today's letters at the left. X stays
  at scale 4.
- **Above them is the page:** the newest row at the bottom, and as many rows as fit.
- **Why not labels on rows in the page's order** (the note's literal reading, with X:, Y: on whichever rows the levels
  hold):
  - After `2 ENTER` and seven ops on X, Y's row is eight lines up, off the screen in both layouts.
  - An RPN stack the user can't see isn't usable, and stu-worksheet's card keeps "the bottom four levels" as the stack's
    names.
  - With the levels kept at the bottom, a row a level holds is drawn there and not repeated in the page above, so every
    row appears on the screen once.
  - In the common case, an ordinary run of ops with no R↓ or x↔y, the two readings draw the same screen.
- **Hosts are untouched:** screen_lines keeps T, Z, Y and X with today's text. The page is a new array in screen_page,
  appended at the end of the struct.
  - The app and the 10" panel (abacus #3971's hosts) draw today's lines until they choose to draw the page.
  - screen_page is 4,936 B on the host today.

### 2. Empty levels hidden (B), the question for Michael

- **Lean B:** p1-B against p1-A is the clearest comparison.
  - The card's words are about the names ("keep the names X, Y, Z, T"), and B keeps them.
  - A level holding a value is always drawn, even a computed 0 (it has a row), and even a nonzero value with no row.
  - Only "bare" levels at the top are hidden: 0, with no row, with everything above them bare too.
- **What B changes for a learner:** a fresh STU screen shows X alone, not four zeros.
  - start-01 and rpn-01 teach X, Y, Z and T. Whether a lesson's prose says "Y shows 0" on a fresh stack is in tutor's
    lessons, which I can't read here.
  - abacus or tutor would know. The checks themselves read values (X=), not frames, so neither choice moves one.
- This is the one visible choice in part 2 I'd put to Michael with the two frames, as the cards were.

### 3. What each row draws

- **Left:** the op's name, the same name a program line uses (list.c's names, from the guide's Appendix G: +, ×, √x,
  RCL A, STO+ A, →POL, LASTx).
  - A typed number has no name at the left.
  - An ALG row shows its line's text.
  - A run shows XEQ A, SOLVE X or ∫FN dX, from the same table.
  - list.c's naming is static today. The build exports one function, `ab_op_text(op, arg, out)`, and both list.c and
    the page use it, so the two can't differ.
- **Right:** the value, formatted as the stack formats it: the display setting, fractions, the base. So FIX 2 redraws
  every row in two decimals. The row keeps the exact value, as part 1 decided.
- **Long values:** an expression row is drawn as one line of text, cut with an ellipsis (081's 30 cells).
  - 2D drawing stays X's alone, as 082 does it.
  - When X's expression is drawn in 2D, the page takes only the lines above the drawing's top (xexpr_fit gives it).
- **Labels:** the letter at the left, and for a held row the op beside it (p2's "Y +", "X √x"). The op isn't drawn while
  a number is being typed into X.
- **The duplicate in ALG:** while a line is shown, its row is held by the line and the result. The page skips it, as RPN
  skips a held row.
- **Errors, prompts and notices** replace X's line, as today. The page above stays drawn.

### 4. The dropped count

- The page's first line, "n OLDER ROWS DROPPED", is drawn only when the page's oldest row is on screen. The wording is
  abacus's and tutor's to change.
- n is rows_dropped. Part 1 already counts what a save left out (state.c:541), along with the arena's drops and the
  serial's wrap.
- **When it shows in part 2:** on a short page (for example after the wrap, or a test hook), or in ALG when ▲ reaches
  the oldest ALG row. Part 3's scroll makes it reachable in RPN.

### 5. ALG's history becomes the page's ALG rows

- **Today:** `hist[16][192]` (3,072 B) and `hist_val[16]` (1,024 B) live in ab_calc's tail, in RAM only (unit 029 rule
  5). ▲ ▼ step through them (calc.c:4489), ENTER copies the shown text to the line (4498), and
  STO and TABLE's START use the shown value (`alg_x_shown`, 4404; 6082). 16 of the expression store's 48 slots are
  history values.
- **Lean:**
  - Remove `hist`, `hist_val` and `hist_n`. `hist_at` becomes the serial of the ALG row shown.
  - ▲ ▼ step through the page's ALG rows. RPN rows are skipped, since they have no line to copy. That isn't limited to
    16: it's as many as the page holds. (Ruled #14041 4a: 029 rule 5 becomes "every ALG row the page holds", since a
    long RPN run can drop ALG rows.)
  - ENTER copies the row's text, as today.
  - The shown value is decoded from the row, and it carries that row's serial, so STO of a shown answer links the row
    it came from. Today's hist_val carries the same serial (calc.c:4378).
  - An expression answer decoded for STO takes an expression slot then. With the store full, that's 082's MEMORY FULL
    (frame 47's case).
- **What it saves:** ab_calc −4,096 B (measured sizes). The 16 history slots go, all but one (48 → 33,
  −2,880 B more): revision 1, #14041 4c.
- **Behaviour that changes on purpose:** ALG's history now survives power-off within the 8 KB budget, as
  stu-worksheet-keep asks. Today it is lost at every OFF.
- **The dumps:** %HIST becomes the count of ALG rows on the page, and %HSEL the shown row's text. Only
  tests/029-soroban.txt uses them in this repo (2 lines, measured by grep). Whether tutor's lessons do is abacus's
  census.

### 6. STO+ A makes a row (abacus #14028, L03)

- **Today:** STO+ −, ×, ÷ change A in place. stale() then gives A row 0, so A shows bare.
- **Lean:** each of the four makes a row: op STOADD (etc.), arg the letter, value A's new value. Its links follow R1, so
  A's old row first, then X's (revision 1, #14041 5: STO− A is A − x, so A is the left operand). A holds the new
  row.
  - No level holds it, so it's drawn in the page as "STO+ A … 6".
  - The table's class for these four moves from MOVE to a new kind, LETTER (makes a value in a letter, not on the
    stack). The census and the invariant don't change, since both are about the stack.
  - The format doesn't change (op, arg, links), and neither does the image version.
- **What moves:** 089's L03 (`STU 4 STO:A 2 STO+:A`) moves on purpose from %ROWS=2 %LBL:A=0 to
  `%ROWS=3 %ROW-1=3:STOADD:1,2:6 %LBL:A=3` (revision 1's order). h089's copy of the same case, if there is one, is abacus's to move.
- **The other in-place letter changes:**
  - x<>A is a move.
  - SOLVE's root was fixed in 089.
  - INPUT, ISG and DSE happen inside a run, which is one row.
  - Σ+ changes the statistics registers, which aren't letters.
  - So the four STO arithmetic ops are the keyboard's only other case. The build's invariant gets a letter twin to
    check that.

### 7. Power-off and the 426 ms (abacus #14028)

- **Facts from the code:**
  - OFF is the firmware's choice, never a cut. Rev A saves, draws the off screen, then sleeps (reva_main.c:977).
  - The automatic offs are the 10-minute idle and a low battery. Both go through the same path.
  - A low battery turns off at 3 % "so memory is saved while the cell still holds the board up" (power.h).
  - A quiet save runs 30 s after the last key (app.h:64, APP_QUIET_MS).
- **What 426 ms costs, then:**
  - OFF's screen goes blank up to 0.43 s later than today.
  - A key pressed during a quiet save waits up to 0.43 s.
  - Neither loses data.
- **What it doesn't protect:** a cell pulled out or a crash loses the keys since the last save. That's true of the
  whole state today; the rows add nothing new to it.
- **The 426 ms is the stand-in's worst case:** all 33 held values on 409 B rows, plus the full 8 KB (089's ROWBENCH).
  - Lean: it fits, with no change.
  - The build measures Rev A's save, worst and typical, with the same bench.
  - If Rev A's worst passes 1 s, the held rows' bytes get a cap: past the cap, the oldest held rows go first, and their
    holders get row 0.

### 8. What a frame costs to draw

- **Reading the newest k rows walks the page from its start** (`ab_rows_get` walks forward, since a record's length is
  at its front).
  - On a full 64 KB page, 089 measured a walk of about 1,300 records at roughly 1 ms on the stand-in. That was the
    1,055 µs key before rows_last.
  - Drawing 7 rows by `ab_rows_get` would walk 7 times or more on every frame.
- **Lean:** one forward walk per frame, keeping the last lines' offsets.
  - If the stand-in measures that above 1 ms on a full page, a ring of the newest 16 record offsets is kept at append
    (64 B), and rebuilt by one walk after a load, UNDO, REDO or a drop.
  - ROWBENCH gains a frame's draw time with a full page.
- **Internal RAM:** a screen line is 2,064 B and composed is 8,276 B (measured), in internal RAM. The page's rows are
  drawn one at a time through one line buffer, never a line per row. Target: D68 DIRAM +0. If it needs a buffer of its
  own, +2,064, measured.

## What the build would test (for abacus's oracle)

- **A text dump of the page from screen_lines** (host only, as %DRAWN):
  - `%PAGEN=n`: lines above the levels.
  - `%PAGE-k=LEFT|VALUE`: k = 1 the lowest. Blanks are written `_`, as %DRAWN's.
  - `%LEVELS=XY…`: the levels drawn.
  - `%PAGETOP=n`: the dropped line's count, or `-`.
- **Vectors:**
  - the prototype's seven cases;
  - a held row not repeated;
  - T's copy-down as two lines;
  - B's hiding (a computed 0 shown, a cleared level hidden, a bare level under a full one shown);
  - an expression in X with the page above its drawing;
  - an error with the page kept;
  - ALG ▲ past 16 rows, ENTER's copy, STO of a shown answer linking its row, ALG skipping RPN rows;
  - the dropped line;
  - FIX 2 redrawing every row;
  - STO+ A's row;
  - the 33s mode drawing no page.
- **Frames:**
  - New STU frames for those cases, reviewed by eye when first written.
  - The existing frames that move are only STU's home frames. My prediction by name is 42-47 (X an expression); the
    build lists the ones that actually move, before and after.
  - The tb and wh frames are Equation mode, and 01-21 and 48 are the 33s and 35s modes. Those don't move.
  - standin_replay's STU frames move by the same rule.
- **Moved on purpose:** 089's L03 (lean 6), 029's %HIST and %HSEL lines (lean 5), and the frames above. Nothing else.
  The lessons' checks (X=, %EQ=, %LINE=, N=) read no frame.
- **D68:** flash; DIRAM target +0; ab_calc −4,096 B (−7,168 if the slots go).
  ROWBENCH: a frame's draw with a full page, and Rev A's save, worst and typical.
- **Make accuracy:** the record will say it ran and give its line count (abacus #14038), with make test and the
  sanitizer runs.
- **Mutants,** predicted before they're run:
  - a held row drawn twice;
  - B hiding a computed 0;
  - ALG's ▲ stopping at 16;
  - STO+ making no row;
  - the dropped line drawn while the oldest row is off screen;
  - the page drawn in 35 mode.

## Questions for abacus

1. The split: part 2 draws the page; part 3 selects, scrolls in RPN, and adds the menus.
2. The levels at the bottom with the page above (lean 1), and each held row drawn once, in its level.
3. A or B for empty levels. My lean is B, and the choice is Michael's to see, with p1-A and p1-B.
4. ALG's ▲ ▼ walk only ALG rows in part 2 (lean 5), and the 16 expression slots go if 082's tests hold.
5. STO+'s row as lean 6, and L03's planned move.
6. The power-off answer (lean 7): it fits, Rev A is measured, and the cap only if Rev A's worst passes 1 s.

## Revision 1: abacus #14041's rulings

Accepted as written: the split (1), the levels at the bottom with each held row drawn once and screen_lines unchanged
(2), power-off (6), and the draw cost (7). Changed or added below: 3, 4a-4d, 5 and 8.

### 3. Empty levels: B′, "bare" as a mark, not read from the value

- **Why B as written fails** (abacus 3b): stale() gives row 0 to every level a run changed (rows.c:401). So a 0 a program
  left in Y looks the same as a cleared 0, and fn-02's T02 ("Y holds 0" after XEQ T, in STU) could hide it.
- **B′:** a level is hidden when it and every level above it is **bare**. Bare is a mark on the value, never inferred
  from what the value is.
- **Where the mark lives: one byte in ab_val's tail padding, so 0 B.**
  - ab_val is kind 0-3, row 4-7, re 8, im 24, p3 40, len 56-59, then 4 B of padding to 64.
  - That padding exists on both boards. Part 1's static assert (rows.c:14) already compares ab_val against the same
    struct without the row field, on both boards. The same assert, with the bare byte added, proves it stays 64 B.
  - ab_calc +0, and every copy carries it, as it carries the row.
- **What sets it:** ab_init (the fresh state, MEMORY CLEAR), CLx, CLSTK, and CLEAR ALL and C. These are the CLEAR kind in
  part 1's table, plus the fresh state.
  - It's set in every mode, at the cost of a byte store; the 33s and 35s never read it.
- **What clears it: any op that makes a value in that level.**
  - **At the keyboard,** that's key_end through the table: a kind OP or RUN op, and the levels its `makes` names.
  - **In a run,** a call after each program line at the step loop (calc.c:1718) clears it from the levels that line's
    op makes, by the same table. AB_LIT and an equation line count as making X.
  - So a program's 0 written into Y is unbare even when the value didn't change, and nothing compares values for it.
  - The cost is one table lookup per program line, only while rows are on (STU). PROGBENCH measures it before and
    after on the stand-in.
- **Moves carry the mark,** since it travels with the value. ENTER, R↓, x↔y and the drop after a two-argument op copy
  a bare value as bare.
  - This is the one place B′ differs from "cleared by any write to the level". CLx ENTER leaves two cleared zeros, and
    both read as cleared. I think that's the truthful reading, since neither 0 was made by anything; abacus's to
    overrule.
- **In the image:**
  - a new section, T_BARE (version 24): the four levels' marks as one byte. With the 2 B tag and 4 B length it's 7 B,
    written in STU only, and only when a level is bare.
  - A 089 build skips it by its size (082's rule for a newer section) and loads the rest.
  - In 33s and 35s images nothing is written, so those images don't change.
  - UNDO's image carries it, so CLx then UNDO brings back the level as it was.
  - The 4C fixture moves again, golden-v24-4c to v25, as it did in part 1.
- **The three ways to draw the levels, for Michael's card:**
  - **A:** all four always.
  - **B:** a top level that's 0 with no row is hidden. That includes a program's 0, which is 3b's flaw.
  - **B′:** a top level marked bare is hidden.
  - abacus 3a: under A, start-01:43 and rpn-01:40's "four lines, named T, Z, Y and X" stays true on a fresh screen.
    Under B or B′, tutor rewords those for STU.
- **Vectors:**
  - B′ from a program's 0, in fn-02 T02's shape: a program leaves 0 in Y with Z and T bare, and Y is drawn.
  - CLx then UNDO.
  - CLx ENTER.
  - A load that keeps the marks.
  - A 35s-mode image byte for byte as before.

### 4. ALG, the four amendments

- **4a. 029 rule 5:** it becomes "every ALG row the page holds."
  - Vector: ALG lines, then enough RPN rows, made by keys, to fill the 64 KB and drop them. ▲ stops at the oldest ALG row kept, with the dropped line drawn above it.
- **4b. UNDO, moved on purpose:** UNDO now hides the undone line from ▲, and REDO brings it back. Today the history is
  outside UNDO's image (calc.h:471), so UNDO leaves it alone.
  - Vector: two lines, UNDO, then ▲ shows the first; REDO, then ▲ shows the second.
- **4c. The slots: 34, not 32 or 33.**
  - calc.h:267 counts holders as 5 + 27 + 16.
  - ANS isn't in the 5. Today it shares a slot with the newest history value: `c->ans = v` and `hist_val[n] = v` are
    one value, so one slot (calc.c:4379-4386).
  - Without history, ANS holds its own slot. So by that comment's own count it's 5 + 27 + 1 for ANS, plus 1 for the
    shown answer decoded while STO stores it: **34 (−14 × 192 = −2,688 B)**.
  - The vector, as abacus set it but at 34: X, Y, Z, T, LASTx, the 27 letters and ANS all distinct expressions. ▲ to an
    older expression answer, STO it into a letter, and the result is the same as at 48.
  - Its control: the same keys at 33, predicted MEMORY FULL. If 33 passes, my count is wrong, and I'll say so before
    choosing.
  - expr_mark's other holders (plit, the suspended state, ev_*, err_x, program literals) share slots by copy. They're
    outside the comment's count today, and this unit doesn't change them.
- **4d.** %HIST and %HSEL: only 029's two lines move (abacus's git grep at stu32-tutor bb96485: 0 hits).

### 5. STO arithmetic's links: A first

- STO− A is A − x and STO÷ A is A ÷ x, so A is the left operand. By R1, A's old row is linked first, then X's.
- make_op lists the levels before AB_ROWSRC_VAR, which is right for RCL+ (x + A). STO arithmetic gets its own order in
  the new LETTER kind.
- If A's old row is 0, the row links X's alone.
- L03 becomes `%ROWS=3 %ROW-1=3:STOADD:1,2:6 %LBL:A=3`.

### 6. Rev A's bench (not blocking)

- Each flash erase stops the cache on both cores, so the key scan on core 0 (reva_main.c:1169) stalls with it.
- The bench taps a key, about 50 ms down, during the worst save, and checks that the key is seen. The same tap during
  today's 48-72 ms save is the control.

### 8. Added to the oracle

- A 35s-mode STO+ A: no row.
- B′ from a program's 0 (the T02 shape above), with B's control: under B the same keys hide Y.
- The mutants gain:
  - the step hook removed (T02's shape goes red);
  - T_BARE not written (CLx, save, load: the level comes back drawn);
  - STO+'s links in X-first order (L03 goes red).
