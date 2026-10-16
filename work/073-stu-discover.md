# Work unit 073: stu-discover, a key does what its legend says or the screen says what it does

**From:** Michael's stu-discover ruling ("adopt the rule and all these fixes", 2026-10-15 19:45Z), through abacus #8714,
from the hidden-key audit (abacus #8495; its case ids in brackets) · **Status:** built on the host; Decision 68 to
follow.

The rule for every item: a key does what its legend says, or the screen says what it does now.

## What it does

1. **[H41] STU's features say where they are.** Every STU-only refusal reads **STU ONLY: MODE, STU** (MODE is blue
   ENTER, STU its third key).
   - Blue TABLE and blue GRAPH in 33s mode still don't open (041 A14), but no longer silently.
   - The CAS key in 33s and 35s modes refuses the same way.
   - In 35s mode TABLE's and GRAPH's GO already refused with STU ONLY; the text is the new one.
   - 041 notes A14 as superseded.
2. **[H27] UNDO and REDO say which.** A brief notice, as OVERFLOW's: UNDO, or REDO when the last UNDO undid. A new
   change clears it, so its UNDO says UNDO again.
   - With nothing to undo: NOTHING TO UNDO.
   - A refused key (any brief notice) changed nothing, so its UNDO mark is taken back. UNDO then still undoes, or
     redoes, what came before it.
3. **[H43, H37] Equation mode.**
   - **A setting acts** while the list is shown: the angle, the display (with the complex format, xiy and rθa), the
     base, the fraction display, the model, the beeper, the flags. So do the clearings that don't touch the list
     (variables, statistics), as ALG lets them act. It went silently nowhere before: RAD with the list shown left the
     angle in degrees. While an equation is typed, the setting stores it first. That's the 35s's rule, UG p.6-8:
     "selecting a setting menu (such as …) will end the equation edit status".
     - **Not the entry (RPN or ALG):** ALG with Equation mode on is no state the image holds, so it says NOT IN EQN.
     - **In a CAS step's editor** (SUBST's replacement, ∫AB's or Σ's bound, QUOT's or GCD's g), a setting drops the
       step and never runs it.
     - **33s from MODE on a 35s built-in** leaves the list at its top, as EQN does (041 A4).
   - **Every other key that did nothing says NOT IN EQN,** a brief notice as ALG's RPN ONLY is, and changes nothing.
     That covers R↓, x↔y, STO and its arithmetic, LASTx, Σ+, XEQ of a label, and the rest, while the list is shown or
     an equation is typed. A key that types still starts an equation.
     - A prompted key refused whatever its letter (STO, LBL, VIEW, …) is refused before its prompt, as ALG's are.
       RCL still prompts, since its letter types.
     - A sweep found them: every op through Equation mode with the list shown, OK and the state image unchanged.
     - **Named, not changed:** these still return OK with nothing done (fresh eyes): the CAS keys and BST/SST while an
       equation is typed, ∞ outside Σ's bound, EDIT on TOP or a built-in.
   - **The list's bar has EXIT on F6 again** (unit 020's: Equation mode left, the list kept). **( ) moves to the
     list's GOLD bank, GOLD then F6**, beside REGX..REGT, and still begins an equation with the pair (unit 046).
4. **[H12] GRAPH's digit pad on screen.** While a graph is open, the band after the readout shows the pad's legend,
   right-aligned to x 328 (clear of STU), never over the plot:
   - "8▲2▼4◀6▶ 5HOME G×10" (19 cells) where the readout leaves room (to 22 cells);
   - else "8▲2▼4◀6▶5" (to 32 cells);
   - else "8▲2▼4◀6▶", which fits after the longest readout (33 cells, TANL's).
   - It's in screen_lines' text, and a cell always separates it from the readout.
   - My first build had two forms, and the short didn't fit after the longest readout. The boundary test caught it,
     and the third form went in.
5. **[H18] HYP is the prefix in all three modes,** STU too, with the menu's soft keys kept as in 33s and 35s modes.
   - While HYP waits, **HYP_** is the X line's prompt (the 35s index: "Displays the HYP_ prefix", 4-6). It never
     covers a line being typed (an equation, ALG's line); the annunciator says it there.
   - **HYP** is the band's annunciator (35s p.1-13), in the base's place, free in decimal; the prompt shows it in any
     base.
   - ab_recstu, the STU gate of e5d54af, is removed. 041 notes B10's change.

**A new status, ABN_NOT_IN_EQN,** is a brief notice (STATUS_SINCE 0: never the message an image holds). No image
version change: 056 D58 checks that its code is refused in an image, and D56 moves to 46.

**What the bar's change moves elsewhere:**
- tests/stackkeys.c types a leading ( in Equation mode as GOLD then F6.
- device/standin/casrows.txt's CANCL row, "(X^2-1)÷(X-1)", gains its GOLD.
- In program entry the editor is already typing, so stackkeys' prog keys (resources.py's stack rows) don't change.

## Checks

- **tests/073-soroban.txt,** 21 rows:
  - settings acting with the list shown and while typing; each refusal; a typing key and XEQ still working; a brief
    notice not kept in an image;
  - fresh eyes' cases: ALG refused and the image loading; rθa; a setting in ∫AB's bound; 33s mode on a built-in;
    CLVARS.
  - No save/load pass: E18 types a bound, which isn't in the image.
- **app_test:**
  - the STU-only notices: blue 7 and 8 in 33s mode, the list's CAS in 33s and 35s modes, STU unchanged;
  - UNDO, REDO, UNDO, and UNDO after a new change;
  - EXIT on F6 in each mode, and ( ) from GOLD's F6 (046's checks);
  - the pad legend in the frame and in screen_lines, its geometry, and its forms at 0, 22, 23, 32 and 33 cells;
  - HYP in each mode: the prefix (STU: COSH; SINH on 35s and STU labels), HYP_ and the annunciator while it waits,
    both gone after.
  - 728 → 747 checks.
- **Mutants, each red on its own checks:**

| mutant | item | what it breaks | failed |
|---|---|---|---|
| T1 | 1 | the old text | 6 |
| T2 | 1 | GRAPH, TABLE, CAS silent again | 4 |
| T3 | 2 | no notice | 4 |
| T4 | 2 | never REDO | 1 |
| T5 | 3 | settings silent again | 4 rows |
| T6 | 3 | refusals silent again | 7 rows |
| T7 | 3 | ( ) back on F6 | 2 |
| T8 | 4 | no legend | 1 |
| T9 | 4 | the long legend whatever the readout | 1 |
| T10 | 5 | STU's old gate | 3 |
| T11 | 5 | no prompt | 3 |
| T12 | 5 | no annunciator | 3 |
| F1 | 3 | ALG allowed in the list | E16 |
| F2 | 3 | the CAS step runs on a setting | E18 |
| F3 | 3 | the built-in kept in 33s mode | E19 |
| U1 | 2 | a refused key keeps its UNDO mark | 1 |

- **Fresh eyes (a subagent) on the first build found four real defects, each verified by a run:**
  - ALG from the list wrote an image the loader refuses (the device would clear memory at boot);
  - xiy and rθa were refused;
  - a setting in a CAS editor ran the step;
  - 33s mode kept a 35s built-in shown.
  - And smaller ones: HYP_ over a typed equation, prompted keys refused only after their letter, UNDO's two edge
    cases, and two rows that overclaimed (E04, E06).
  - Each is fixed, with a row or check that goes red without the fix.

## Not in this unit

- (I) and (J) printed on the 3 and 0 keys [H28]: a legend change, routed to draftsman.
- HELP (blue 9, then any key): its own unit.
- H02, H14, H51 stay as they are.
