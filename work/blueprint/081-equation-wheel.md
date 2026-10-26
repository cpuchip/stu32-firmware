# Unit 081: the equation list as a wheel (blueprint)

**From:** abacus #11114, on Michael's answers of 26 Oct (04:34-04:38Z): "080, then the wheel (neighbours at size 2),
then step 3". His note: "Maybe we can draw main equation at size 3 and the 1 or 2 above / below at size 2." The wheel
came out of his first look at 080 on the phone (abacus #11101, point 3). **Blueprint first, no code;** abacus checks it.

**What it is:** in STU's Equation mode, the list is shown as a wheel instead of one entry.
- The centre is the entry shown, marked ▶, laid out by 080's rule: 3 when it fits, else 2, else wrapped, else scrolled.
- Above and below it are the entries BST and SST would bring, at size 2, one line each in 1D, cut with an ellipsis.
- It uses 080's area (rows 21 to 205) and 080's tree for the centre, and adds no state.

**Why this file is in work/blueprint/:** as for 080. A record named work/081-*.md would make check-version ask for VER 0.81
before the code. The unit's commit moves it to work/081-equation-wheel.md and bumps VER together.

## 1. How many neighbours fit

Measured on the host with tbook (each of casim's 4,884 distinct corpus answers at 2a9c354 as the centre, fitted by 080's
rule in the whole area of 183 px). A neighbour takes 18 px: a line at size 2 is 14 px, plus a 4 px gap.

| the centre | answers | its height, p50 / p90 / max | neighbours that fit around it, at p50 / p90 / max |
|---|---|---|---|
| whole at 3 | 3,528 | 51 / 87 / 153 px | 7 / 5 / 1 |
| whole at 2 | 734 | 35 / 82 / 84 px | 8 / 5 / 5 |
| wrapped at 2 | 551 | 85 / 158 / 319 px | 5 / 1 / 0 |
| drawn as text | 71 | (its own lines at 2) | |

- **4,437 of 4,813 centres drawn in 2D leave room for 2 above and 2 below.** 4,740 leave room for 1 and 1.
- **The rule, from Michael's "1 or 2":** 2 above and 2 below when they fit; else 1 and 1; else none. With none, a centre
  taller than the area scrolls as 080's does.
- A size-3 centre and a size-2 one differ little here. A size-3 answer is usually taller (its median is 51 px against
  35), but both leave room for 2 and 2 at their p90.
- **The placement:** the centre sits in the middle of the area, the neighbours stacked above and below it. The ▶ is in
  the left margin at the centre's axis. A neighbour is cut to 30 cells and an ellipsis (31 cells at size 2, as
  079 measured).

## 2. SST and BST, and 080's tall scroll

- **SST and BST move the wheel:** the next or previous entry becomes the centre, as they step the list today. No new
  key, and the list's order and wrap are unchanged.
- **A centre taller than its window keeps 080's rule, as Michael ruled it:** SST shows its next line until its end, then
  moves the wheel; BST mirrors it. Its window is the area less the neighbours' rows. So a centre that would need more
  room drops to 1 and 1, then to none, before it scrolls.
- **While it scrolls, ▲ and ▼ mark its hidden lines,** as in 080, and its neighbours stay where they are.

## 3. EQN LIST TOP, the built-in solvers, and other stamps

- **The wheel shows the list as SST and BST walk it.** EQN LIST TOP is an entry like the others. At the centre it's 080's
  clean text at size 3. As a neighbour it's a size-2 line: the line below the last entry, and the line above the first.
- A built-in solver's entry ("2*2 lin. solve") is the same: text, at 3 in the centre and 2 as a neighbour.
- **An entry is shown once.** A list shorter than the wheel's slots leaves them empty rather than repeating.
- **An equation stamped 33 or 35, at the centre in STU mode,** is drawn as its text with its mark, inside the wheel. It
  isn't drawn in 2D, since STU's grammar would group it wrongly. **That changes 080's rule** (such an equation kept the
  stack) so the wheel holds every entry. **This one is Michael's to confirm.**
- **Typing stays one line, with no wheel** (080's rule); the wheel comes back when the entry is stored. 33s and 35s modes
  have no wheel.

## 4. The phone and the web

- **No new contract.** The wheel lives in 080's area, rows 21-205, so `textbook_on` and the frame carry it as they carry
  080 (tally's stu_frame; primer paints the whole frame).
- screen_lines' x keeps the centre's text and `drawn` the centre's drawn text, as in 080.
- **The neighbours are only in the frame.** If tutor or keyrun want them as text, screen_page could add above[2] and
  below[2] (the host's struct only, not the device's RAM). **The question goes to tally, primer and tutor:** the
  default is no new field.
- tally's and primer's checks: one wheel frame named by its rows 21-205 CRC, as 22-tb-stu3 is.

## 5. The oracle, and RAM +0

**Frames (screen_test), each looked at before it's kept:**
1. 2 and 2 around a size-3 centre;
2. 2 and 2 around a size-2 one;
3. 1 and 1 around a tall centre;
4. none, with a scrolled centre and its ▲▼;
5. EQN LIST TOP at the centre;
6. the last entry, with EQN LIST TOP below it;
7. a 33-stamped centre in the wheel;
8. a neighbour cut with its ellipsis;
9. a list shorter than the slots;
10. STU typing (one line, no wheel).

**The frames that must not move:** 14-equation and 15-equation-typing (33 mode), and 33-tb-typing-stu. 080's single-entry
frames (22-36) move, since each becomes a wheel with its list's neighbours. That's ruled, and each is looked at again.

**app_test:** SST and BST move the centre; a tall centre scrolls, then moves the wheel; EQN LIST TOP's neighbours; the
33s mode shows no wheel; screen_lines' x is the centre's text.

**Mutants:**
- neighbours at size 1;
- a neighbour not cut (it runs off the edge);
- an entry shown twice in a short list;
- the tall centre's scroll lost;
- the wheel in 33s mode;
- a 33-stamped centre drawn in 2D.

**The device:** the stand-in shows a wheel from the keys, with its CRC the host's; and stack-free, with the probe's
reading through render(), against 080's 4,428 B.

**RAM and flash:**
- Neighbours are drawn straight from the list's text with screen_text, through a stack slice of 64 B. There's no new
  buffer, the pool is 080's (the centre only), and nothing is new in app_state: SST and BST already step the list, and
  tb_line already scrolls. **Internal RAM +0**, measured by D68.
- Flash: about 1.5-3 KB expected (the wheel's layout in screen.c), measured at the gate.

**The saved state:** nothing. The wheel is a view of the list; the image doesn't change.

**The readback** is 080's, unchanged: the centre is drawn by 080's code, and the neighbours are casim's text.

## Not in this unit

Expressions as values on the stack, EQN folders and groups (step 3, after this; its blueprint starts from the saved
state), and typing in 2D.
