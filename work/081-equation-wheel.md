# Unit 081: the equation list as a wheel (blueprint)

**From:** abacus #11114, on Michael's answers of 26 Oct (04:34-04:38Z): "080, then the wheel (neighbours at size 2),
then step 3". His note: "Maybe we can draw main equation at size 3 and the 1 or 2 above / below at size 2." The wheel
came out of his first look at 080 on the phone (abacus #11101, point 3). **Blueprint first, no code;** abacus checks it.

**What it is:** in STU's Equation mode, the list is shown as a wheel instead of one entry.
- The centre is the entry shown, marked ▶, laid out by 080's rule: 3 when it fits, else 2, else wrapped, else scrolled.
- Above and below it are the entries BST and SST would bring, at size 2, one line each in 1D, cut with an ellipsis.
- It uses 080's area (rows 21 to 205) and 080's tree for the centre, and adds no state.

**Where this file was:** work/blueprint/ (ae71530), as for 080, so check-version wouldn't ask for 0.81 before the code. The
unit's commit moved it here and bumped AB_FW_VERSION to "0.81" in the same commit. The blueprint below is as abacus
approved it (#11125). The gate section at the end says where the code differs.

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

## The gate (soroban, 2026-10-26; abacus OK'd the blueprint at ae71530, #11125, with two additions)

**The commits, on ae71530:** 3044782 (the wheel), ca6677d (080's acceptance noted), 18b155a (VER 0.81, this record moved
here), 9ce1b5f (wheel081.txt).

**What the code does, and where it differs from the blueprint:**
- **ab_eqn_step (core/calc.c):** the list's step, factored out of SST/BST. The keys and the wheel share one source, so the
  wheel's neighbours can't drift from what the keys bring.
- **The centre is fitted alone,** keeping its largest scale (Michael's "main equation at size 3"). The neighbours each
  side are then the most of 2, 1 and 0 that leave it room. A centre taller than the area is alone, and scrolls as 080's.
- **Content starts at x 16, after the ▶,** and the area's width is 380.
- **Additions the frames asked for, each looked at:**
  - a centre's label is right-aligned, so it doesn't read as a neighbour's line;
  - a dotted rule in each gap between the centre and its neighbours. Without it, a centre drawn as text and wrapped
    read as its neighbours' lines (frame 27).
- **A fault found in frame 40, before the commit:** shown_text skipped the first byte of any entry not drawn in 2D. That
  was meant for a built-in's leading 0x01, but a 35-stamped `2^3^2+X` read `^3^2+X`. Now only that byte is skipped.
- **The 33/35-stamped centre** is inside the wheel as its text and mark (frame 40, `2^3^2+X ·35`). This changes 080's
  rule, and it's Michael's card; the unit's acceptance waits on it.

**make test** rc 0 at 9ce1b5f, peak RSS 176,540 KB: app_test 793/793, standin_test 349/349, screen_test 9008/9008 with 41/41
frames, tbook_test 39/39, check-version `0.81, unit 081`.

**The frames (screen_test):**
- 080's 22-32 and 34-36 move as ruled (each becomes a wheel with its list's neighbours), each looked at again.
- New: 37 (2 and 2 around a size-3 centre, five distinct entries), 38 (2 and 2 around size 2), 39 (1 and 1 around a
  112 px centre), 40 (a 35-stamped centre), 41 (a neighbour cut with its ellipsis).
- The 21 frames before 080 and 33-tb-typing-stu are byte-identical.
- **22's rows 21-205 CRC is re-pinned 43FC116C** (080's was 9A4A7D8C). tally and primer re-pin in their steps.
- The QUOT label checks and the +C pixel checks now compute the wheel's geometry rather than fixing 080's positions.

**app_test (abacus's second addition):**
- For every centre in a list of five entries and the two built-ins, TOP included, each neighbour the wheel shows is
  ab_eqn_step's entry. Its line is eqn[i]'s text cut to 30 cells and an ellipsis, across the wrap through EQN LIST TOP.
- Around entry 2, the order is 1 then 0 above (nearest first) and 3 then 4 below.
- No wheel in 33s mode.
- The 33-stamped check now expects its text in the wheel.

**The mutants, each red:**

| mutant | caught by |
|---|---|
| W1, neighbours at size 1 | 16 frames |
| W2, a neighbour not cut | frame 41, app_test 792/793 |
| W3, an entry shown twice | 11 frames (the index check passes, since a repeat names the right entry: the frames guard it) |
| W4, the tall centre's scroll lost | frame 26-tb-tall-scrolled |
| W5, the wheel in 33s mode | frame 14, app_test 790/793 |
| W6, a 33-stamped centre drawn in 2D | frame 40, app_test 792/793 |
| W7, the neighbours' order reversed (abacus's seventh) | 16 frames including **37-wh-size3**, where above and below differ; app_test 791/793 |

- **W4's first try didn't build** (the mutant left `ui` unused under -Werror). It was rewritten to build and then caught.
  A mutant counts only when it builds.

**Decision 68, `scripts/resources.py static ae71530 9ce1b5f`:** flash +1,552 on the stand-in and +1,552 on Rev A; internal
RAM +0, IRAM +0, calc_task's frame +0, no internal-RAM symbol changed.

**The stack and the time on the S3** (080's gate probe re-applied, work/081-evidence/):
- **The stack:** render() peaks at 4,000 B with the wheel (the 43-deep nest drawn as text, with its neighbour), against
  080's 3,584. With 844 B live, that's 4,844 of 32,768 at most. Each probe sample is a one-entry list, so the wheel
  shows TOP as its one neighbour; a neighbour is drawn at render's top level.
- **The time:** a frame's own time (a second render) is 2.0 to 2.9 ms for every sample but the nest. The nest's second
  render read 20.3 ms in both runs, which isn't traced. FRAMEBENCH on the same screen, 50 frames, gives 3.2 ms a frame,
  the steady state.

**The stand-in, at 9ce1b5f** (read: `ver=0.81 core=9ce1b5f`):
- **wheel081.txt:** X^2+1, 3×X^2-4 and SIN(X)+1 typed in STU, then ▲. `drawn=3X^2-4` and `framecrc: rows 21-205 85B6EE37`,
  the host's (the same keys replayed through app.c). stack-free 28,596. Every row met, with the control red.
- **look080.txt is 080's look,** expecting ver 0.80 and 080's single-entry CRCs. It fails on 081's image, as it should,
  and is kept as 080's record.
- make clang-check: 68 files, 0 failing. Claim #11131, released (#11139), peak 669.1 MiB.

**For tally and primer:** no new contract. The wheel is inside rows 21-205 under textbook_on. x is the centre's text and
drawn the centre's drawn text. Re-pin 22-tb-stu3's CRC to 43FC116C.

## Accepted on the code (abacus #11152, at de499c9)
- abacus's clean clone matched the gate: the moved frames are exactly 22-32 and 34-36, and 01-21 and 33 are byte for
  byte. Its plant (never 2 and 2) turned the index check red, app_test 792/793.
- The 33s-stamped centre inside the wheel waits on Michael's card (wheel-33-entry).

## Accepted (abacus #11224)
- Michael's wheel-33-entry: "Plain text with its mark, inside the wheel". That is frame 40 as built, so 081 is accepted
  with no change.
