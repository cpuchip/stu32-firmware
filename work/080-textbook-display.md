# Unit 080: the textbook display (blueprint)

**From:** abacus #10848. Michael is taking up the new STU home screen from the prototypes ("They looked good and is
what I think other calculators are moving towards"). The design note's order is step 2, the textbook display; step 3,
expressions as values; step 4, the worksheet, which reuses step 2's layout tree. This is step 2. **Blueprint first, no
code.** The start is the throwaway prototype on branch proto-textbook (8f98ea5, abacus #8391).

**The card rulings it holds** (as abacus relayed them):
- stu-textbook-size: the largest scale that fits, 3 then 2; then a wrap at the top-level + and −; then a scroll inside
  the entry. Never scale 1, and never a sideways scroll.
- Typing stays one line.
- STU only: the 33s and 35s modes don't move.

**Where this file was:** it was written in work/blueprint/ (c98b931). scripts/check-version.sh takes the last unit
from work/NNN-*.md (and tests/NNN-*.txt), so a record named work/080-*.md would have made it demand VER 0.80 before any
080 code existed, which is 079's VER miss (#10800) run the other way. The unit's commit moved it here and bumped
AB_FW_VERSION to "0.80" in the same commit. The blueprint below is as abacus approved it (#10907). Where the code
measured differently, the gate section at the end says so.

**The evidence** is in work/080-evidence/:
- tbprobe.patch: the S3 probe, applied on 8d663c7 with the prototype's tb.c and tb.h;
- tbprobe-run.txt: the probe's output;
- depth-census.c and size-census.c: the host censuses, run against the prototype's parser;
- census-results.txt: their output.

## 1. Stack: a depth cap, with the 1D text past it

**Measured on the S3** (the stand-in, interrupts live; the TBPROBE command, a throwaway, never on main). The
prototype's parse, layout, wrap and draw run inside render(), the calc task's draw site. The stack below the call point
is painted and scanned afterwards.

| sample | depth (binary / structural) | the renderer's stack | the S3's time |
|---|---|---|---|
| control: no work | — | 512 B (the paint margin, as designed) | 5 µs |
| control: 4,000 B touched | — | 4,080 B | 88 µs |
| `X×LN(X)-X` | 4 / 3 | 800 B | 0.25 ms |
| a Taylor answer, 123 nodes (134 wrapped) | 11 / 6 | 2,160 B | 1.69 ms |
| a 13-term polynomial (factor) | 16 / 4 | 3,120 B | 1.13 ms |
| a 20-term polynomial (expand) | 21 / 3 | 3,456 B | 1.40 ms |
| `EXP(X^2)×SIN(SIN(…))`, casim's stress nest | 33 / 32 | 6,112 B | 1.43 ms |
| `X^(X^(…))`, casim's stress nest | 44 / 43 | 11,168 B | 0.61 ms |

- **What's live when render() is called: 796 B** of the calc task's 32,768 B (CALC_STACK).
- Two runs agree to the byte. After the probe, the board's own reading (`stack-free=20804`) equals 32,768 − 11,964,
  the probe's worst total. That's an independent instrument agreeing.
- **The worst level costs about 254 B on the S3** (11,168 ÷ 44). The prototype's static estimate was 176 B: the xtensa
  frames read by -fstack-usage leave out the windowed ABI's spills.

**The renderer never runs on top of a deep CAS call.** render() is called from the calc task's loop and from the app's
busy hook. The hook (firmware/app.c, `a->busy`) runs before a long call, not during one. So the 8,188 B floor, which
was measured with CAS frames live, isn't what the renderer shares.

**Two design choices bound it anyway:**
- **The tree is n-ary in + − and ×.** The prototype's binary tree makes a 20-term polynomial recurse 21 deep with no
  nesting at all. Walking each chain of terms in a loop makes the recursion follow only brackets, fractions, powers,
  radicals and functions: the structural depth.
- **A cap of 12 structural levels, with casim's linear text drawn past it.** Over casim's golden corpus at 4625452 (every answer
  with status OK, PARTIAL or DECIMAL_FORM, of 191 B or less, solve's roots split into their own texts: 5,853 parsed), 99.6% of answers are 10 levels deep or less, and no real answer is deeper than 10. **No answer falls
  between 11 and 31.** The 21 answers past the cap (32 to 43) are casim's own stress nests (`SIN(SIN(…))`,
  `X^(X^(…))`, `IP(IP(…))`).

**Expected worst:** about 12 × 254 B plus the wrap's one frame (464 B, the prototype's static figure), about 3.5 KB,
plus 796 B live, about 4.3 KB of 32,768 B.
Even if a later change drew inside the deepest CAS path (24,580 B in use), that's under the task's size. The gate
measures it on the S3: the deepest real answer, a synthetic 12-deep, and a 43-deep drawn through the fallback.

**The other two options, not chosen:**
- **An explicit stack:** more code, for every answer, to save stack that the cap already bounds.
- **A task of its own:** another stack in internal RAM, and a hand-off of the frame buffer. Decision 68 resists both.

## 2. Code and nodes

- **Nodes:** 18 B each (the prototype's tb_node). The most in any answer is **123** after parsing, and **139 after the
  wrap** adds its row nodes (the 20-term expand). The pool is **160 nodes, 2,880 B**, static in PSRAM, so internal RAM
  stays +0. An answer that runs out of nodes is drawn as its 1D text, never cut.
- **Code:** the prototype measured 8,728 B of text at -O2 on the S3 (6,833 at -Os; its README at 8f98ea5). The unit
  adds the n-ary walk, the cap, ∞, the scroll, the minus's spacing and the implied-product rule. **Expected: 10 to 12 KB
  of flash, IRAM +0, internal RAM +0.** Decision 68 is measured at the gate.
- **Time:** at most 1.7 ms an answer on the S3 (the Taylor answer, above), against a frame's send of about 50 ms (the prototype's figure, 2026-10-15). The
  tree is rebuilt from the text at each draw and never cached.

## 3. Where it draws, and what keeps casim's text untouched

Every place screen.c draws an equation or answer (`eqn_line`):

| screen | 080 |
|---|---|
| Equation mode, the equation shown (screen.c's eqn_mode branch): typed equations and every CAS answer put into the list | **drawn textbook in STU mode, when the equation is STU-stamped** |
| Equation mode, an equation being typed | stays one line, with its cursor (ruled) |
| program entry, an equation line being typed | stays one line |
| SOLVE's report rows (048, 079): the status, the note, each root | **stays as it is**: four rows of 28 px at scale 2, and a stacked fraction is about 35 px. The proposal is to revisit with the worksheet |
| STU's ALG line and its history (Y and T) | **stays as it is**: drawing the history is the worksheet (step 4) |
| the stack, VIEW, SHOW, the graph and table | numbers, unchanged |

- **Only STU-stamped equations.** An equation stamped 33 or 35 shown in STU mode keeps today's line and its mark
  (`2^3^2 ·35`). The layout parses with STU's grammar (powers right to left, implied products, unit 031), which would
  group another mode's text wrongly.
- **The area:** an answer drawn textbook takes rows 21 to 205, between the band's rule and the soft keys, with T, Z and
  Y not drawn while it's shown. It sits at the bottom of the area, as X does. A label (WHERE, MAY FACTOR MORE, QUOTIENT,
  REMAINDER, NOT AT) goes at the area's top left at scale 2. ∫'s +C sits after the answer's last line.
- **Visible change:** a short STU equation is drawn at scale 3 where today's X line uses 4, because the ruling's
  largest scale is 3.
- **casim's text is never touched.** The renderer reads `eqn[]` and never writes it. Its pool is separate. Wherever it
  doesn't draw (a parse failure, past the cap, the pool full, a term too wide), it draws the same text as 1D. SAVE,
  LOAD, ENTER, RCL, the vectors' %EQ, the board's `eqn:` line and screen_lines all keep the text, as 079 keeps
  cas_rep.

**One thing to settle with tally:** screen_lines gives a host each line's text, and memory says the app draws its
screen from screen_lines. If so, the app shows the 1D text until it draws the layout itself. If the app shows
screen_render's frame, it gets 080 as the device does. abacus found stu32-app's Dart tests reading "the LCD, which
screen.c draws". That's one question for tally before the gate.

## 4. The open details

**The size rule, over the corpus** (size-census.c; 5,853 answers of 191 B or less, the line the calculator holds):

| class | answers | share |
|---|---|---|
| whole at scale 3 | 4,455 | 76.1% |
| whole at scale 2 | 783 | 13.4% |
| wrapped at 2, within 180 px | 519 | 8.9% |
| wrapped at 2, taller than 180 px (the tallest 256 px) | 35 | 0.6% |
| a term alone wider than the line | 61 | 1.0% |

- **The scroll, for the 35 tall ones.** The answer shows its top, with ▼ at the area's bottom right. SST scrolls a line
  at a time until the end is shown, then the next SST goes to the next equation; BST mirrors it. Nothing scrolls
  sideways. **This changes what SST does on a tall answer, so it's Michael's to confirm.** The other way is to show
  the top with ▼ and no scroll until the worksheet (step 4) brings its own.
- **A term wider than the line, the 61:** e.g. a fraction whose denominator is a long polynomial. Wrapping inside a
  fraction isn't in the ruling. Proposal: casim's text, wrapped at its operators across lines at scale 2. That's still
  never sideways, and the scroll above covers it if tall.
- **The minus between two fractions** sits on the bars' row, as a textbook sets it, and in this face reads as an
  underscore (the prototype's `s2-cycle` frame; a + there reads fine). Two tries didn't help: a bar inset half a
  stroke, and a bar 1 px thinner. **Proposal:** a full cell of space each side of a binary minus that touches a
  fraction, in two variants for the glass check in front of Michael: (a) the minus on the bar row; (b) the minus 1 px
  above it. The gate takes his pick.
- **Dropping ×:** the display drops × exactly where unit 031's parser reads the product back:
  - after a number, a ")" or a letter, and before a letter-start operand (a variable, a function name with its "(", a
    constant name) or a "(": `3X`, `2(X+1)`, `(X+1)(X-1)`, `X(Y+1)`;
  - **kept before a number** (`X×2`, `2×3`: 031's rule D refuses `A2` and `2 3`) and **before b, o, d and h** (rule G);
  - a function word gets a full cell's space before it (`X ATAN(X)`, from the prototype), so a name can't seem to swallow
    a letter (rule E's `AASIN(`);
  - **two letters side by side:** `XY`, as 031 reads it (rule 5). **Question:** draw them touching, as a textbook does,
    or with a thin gap? The proposal is a 1 px gap, because a multi-letter constant name could otherwise seem to appear;
  - EXP(u) is drawn e^u (the prototype's), and `2e^X` is a drawn form, not typed text; 031's rule F (2EXP(1) is
    SYNTAX) is about typing.
- **∞** must parse: the prototype refuses it, and the census's 5 failures are all ∞ (`∞`, `-∞`, `∞; ABS(X)<1`).

## 5. The tests

- **The frame oracle (screen_test).** New reference frames in tests/frames: an STU equation at 3; an answer at 2; a
  wrapped one; a tall one showing its top and its ▼; a scroll step; a term too wide, drawn as text; the 43-deep stress
  nest drawn as text past the cap; a label; +C. Each frame is written once at the unit and reviewed by eye before
  it's kept. **Red at the head before the unit** (the new frames don't match today's 1D lines), green after.
- **The frames that must not move:** 14-equation (33 mode) and 15-equation-typing (typing stays one line) stay
  byte-identical. That's the oracle for "33s and 35s don't move" and "typing stays one line".
- **A readback oracle for dropping ×.** Each corpus answer's drawn token stream is written back as text, with × only
  where it's drawn. It goes through STU's parser and must evaluate equal to casim's text at seeded points. That's 5,853
  answers on the host, in make test.
- **Mutants:** the STU-only rule dropped (14-equation moves); typing drawn textbook (15 moves); the cap removed (the
  43-deep frame isn't text); scale 1 allowed (the wrapped frame changes); × dropped before a number (the readback
  fails); a 33-stamped equation parsed as STU (a `2^3^2 ·35` frame); the pool's overflow not handled (a synthetic answer
  past 160 nodes must fall back).
- **The device:** the stand-in shows the new frames from the keys, with the frame's CRC equal to the host's (as
  tests/frames/standin.crc does now). The stack is read on the S3, as in section 1, for the deepest real answer, the
  12-deep and the fallback. stack-free stays above the deep paths' floor.
- **tutor:** the lessons that describe STU's Equation mode screen change. That's the equation shown, CAS answers, and
  scale 3 for short equations. The report, ALG and the 33s/35s modes don't. abacus asks tutor for the count.

## The saved state

**Nothing.** Step 2 is a view: the tree is rebuilt from the stored text at each draw, and nothing is cached in the image.
The scroll position is transient: a new equation, a key that leaves, or SAVE/LOAD start it at the top. AB_STATE_VERSION
doesn't move. Steps 3 and 4 are where the image changes (expressions as values, worksheet entries). Keeping step 2
stateless means those steps start from today's image, as Michael's note asks.

## Not in this unit

The report rows, ALG's history, the worksheet, expressions as values, typing in 2D, and any 33s or 35s screen.

## The gate (soroban, 2026-10-26; abacus OK'd the blueprint at c98b931, #10907, with four additions)

**The commits, on d3354ac:** 1b6e0fc (firmware/tbook and tests/tbook_test), 5ebebe2 (the screen, the scroll, screen_page,
the board line), c334e98 (the pool's limit, a 33s-stamped equation), 50df5c0 (VER 0.80, this record moved here, FRAMECRC),
b82820e (a line taller than the area), ed7ecf1 (tbook080.txt), fbc5b35 (internal RAM +0), 71bd6f3 (Michael's look),
aa738bb (look080.txt).

**Where the code measured differently from the blueprint:**
- **TB_DEPTH is 16, not 12.** The unit's own tree counts layout levels: no corpus answer is past 12, and casim's stress
  nests start at 33. Nothing falls between 13 and 32.
- **TB_NODES is 200, not 160.** Lists cost an ITEM node a term: 166 is the most an answer needs, wrapped.
  `1+1+...+1` at 191 B reaches the pool and stops cleanly (err 2), drawn as text.
- **The parse counts a bracketed exponent twice** (the exponent and its bracket), so 8 levels of `^(...)` are the most it
  takes. That's honest about the parse's own stack, and no corpus answer reaches it.
- **tb_fit takes the area's height** (b82820e). A seven-deep continued fraction fit the width at 3 but stood 231 px in
  one line, which can't scroll, so the area would have drawn nothing. Now it draws at 2, 154 px.
- **The minus beside a fraction has a full cell (6s) either side,** as Michael's look (a) says. My first code had 4s.
  The wrap now measures with that gap too. Its estimate had used the plain gap, so some lines came out too wide and fell
  to the text.

**The four additions (abacus #10907):**
- **The E rule:** after a number, × stays before an operand starting with E (`3·E`). Measured through eqn_eval:
  `3E+1` is SYNTAX where `3×E+1` is 19, and `2EXP(1)` is SYNTAX. `i` isn't an STU variable.
- **The synthetic sweep:** A-Z and 25 function words after 3, (1+2), B and X^2. All 204 pairs read back equal.
- **e^u reads back as EXP(u),** with × before it after a number or a letter (rule F, and rule E's longest name).
- **census-results.txt is labelled:** structural, binary, size, and the unit's own.

**make test** rc 0, peak RSS 173,512 KB: app_test 788/788, standin_test 349/349, screen_test 9005/9005 with 33/33 frames,
tbook_test 27/27, check-version `0.80, unit 080`.
- **tbook_test's census** (casim 2a9c354, each of 4,884 distinct texts once): 3,528 whole at 3, 730 at 2, 529 wrapped,
  27 taller than 180 px, 54 with a term too wide, 16 past TB_DEPTH (every one a stress nest), none past the pool, none
  unparsed. The most nodes is 166.
- **The readback:** all 4,884 corpus answers and all 204 sweep pairs evaluate equal to casim's text at three seeded
  points.

**The frames (screen_test):**
- **New, each looked at and kept:** 22 (an STU equation at 3), 23 (fractions), 24 (wrapped), 25 and 26 (tall, then
  scrolled), 27 (a term too wide, drawn as text), 28 (the 43-deep nest, as text), 29 (a label), 30 (+C), 31 (the minus
  between fractions), 32 (a radical and LN|u|), 33 (STU typing, one line).
- **The 21 frames before are byte-identical.** Two QUOT checks moved: in STU the label is at the area's top left, at 2.
- **For the app (tally #10916):** 22-tb-stu3's rows 21-205 have CRC **9A4A7D8C**, pinned in screen_test.

**The control: the screen change switched off at the head** (textbook_shown false).
- screen_test: frames 22-32 and the CRC check are red (9002/9005, 22/33 frames). 33 passes either way: typing is one line.
- app_test: 782/788, all six 080 checks red.
- Restored, everything is green.

**The mutants, at the final head aa738bb, each red:**

| mutant | caught by |
|---|---|
| M1, the STU-only rule dropped | app_test 787/788 (33s mode) |
| M2, typing drawn in 2D | frame 33 (8998/9005) |
| M3, the cap removed | tbook_test 26/27: no answer is past the depth |
| M4, scale 1 allowed | frame 27 |
| M5, × dropped before a number | tbook_test 24/27 |
| M6, a 33s-stamped equation parsed as STU | app_test 787/788 |
| M7, the pool's overflow not handled | tbook_test 26/27 |
| M8, the E rule dropped | tbook_test 25/27 |

- **M1 doesn't move frame 14:** that equation is 33-stamped, so the stamp rule alone keeps it as text. The 33s-mode check
  in app_test is M1's guard.
- **M3 no longer moves frame 28:** since b82820e, the nest is too big at any scale and falls back to text either way.

**Decision 68, `scripts/resources.py static d3354ac aa738bb`:** flash +12,928 on the stand-in and +13,024 on Rev A;
internal RAM +0, IRAM +0, calc_task's frame +0, no internal-RAM symbol changed.
- That's a little over the blueprint's 10-12 KB.
- The first reading was internal RAM +232 (the area's struct static, tb_looks, an int in app_state). fbc5b35 brought it
  to +0: the struct is on render's stack, tb_looks is PSRAM's zeroed BSS, and the scroll is a byte in app_state's padding.

**The stack and the time on the S3** (a throwaway probe, work/080-evidence/gate-probe.patch, with interrupts live):
- **The method:** each sample is put in the calc as the shown STU equation, the stack below the calc task's loop is
  painted, and render() runs. That covers screen_render, the area and tbook.
- **The controls:** 512 B with no work, and 4,032 B with 4,000 touched. Every sample reads textbook_on=1.
- **The stack:** render() peaks at 3,248 B for `3×X^2-4`, the Taylor answer, the 15-deep continued fraction and the
  43-deep nest drawn as text, and at 3,584 B for the deepest real answer (12). With 844 B live, that's 4,428 B of
  32,768 at most.
- **The time:** a frame's own time (a second render, the state unchanged) is 1.6 to 2.8 ms. FRAMEBENCH agrees: 1.1 ms
  a frame for the stack screen, and 1.7 ms for `3×X^2-4` typed from the keys.
- **The probe's first render after its paint takes about 19 ms for every sample.** Painting about 27 KB just before it
  plausibly evicts the cache, so that time belongs to the probe, not the frame.
- **A first run of the probe, after RESTART,** still had the stand-in's MEMORY CLEAR prompt up, which takes X ahead of
  Equation mode. Its times weren't all of the textbook path. The run above clears it and prints textbook_on for each
  sample.

**The stand-in, at aa738bb** (read: `ver=0.80 core=aa738bb`):
- **tbook080.txt:** every row met, with the control red. From the keys, `drawn=3X^2-4` and `framecrc: rows 21-205
  9A4A7D8C`, the host's CRC. stack-free 29,044.
- **look080.txt, for Michael:** 3×X×Y+X×Y^2 (`drawn=3XY+XY^2`, CRC 51AA78F5) and the minus between fractions (CRC
  94F84FBB), each the host's. EQN LIST's ▲ and ▼ move between them. Every row met, with the control red. The board is
  left showing the minus between fractions.
- **On fbc5b35:** repin-rc7.txt 18 of 18 with the control red, and casrows.txt ALL MET.
- Each key sequence was replayed through app.c on the host first, to the same CRC.
- make clang-check: 68 files, 0 failing.
- The port moved at some flashes (ACM1 to ACM3); each rerun with no flash passed.
- Claim #10941, released (#10962), peak 658.4 MiB.

**For the app and the web (their steps, after this unit):**
- Add firmware/tbook.c to their builds.
- When screen_page's textbook_on is set, draw rows 21-205 from screen_render's frame, as for a graph (tally's
  stu_frame; primer paints the whole frame).
- x keeps casim's text, Y a label's text, and `drawn` is what's drawn.
- tally's AVD check is CRC 9A4A7D8C for 22-tb-stu3's state.

**For tutor:** screen_page's `drawn` and the board's ` | drawn=` give the 2D form as text, e.g. `3X^2-4`,
`(e^X SIN(X))÷2-(e^X COS(X))÷2`, `LN|3X-2|÷729+√(X+1)`. The 18 quotes in 10 lessons (#10917) check against it.

## Accepted on the code (abacus #10992, abacus 92e26d1), and the gap's reach

- abacus replayed 85503e9 from a clean clone: VER 0.80, the 33 frames, CRC 9A4A7D8C, the 21 older frames byte-identical.
  Its plant (the minus's gap off, letter_touch forced) turned 8 frames red, 22's CRC among them.
- **The 1 px gap reaches every pair drawn side by side, not only two letters.** tbook.c's sep_w gives it to every
  SEP_NONE pair: `3X`, `XY`, `(X+1)(X-1)`, `X(Y+1)`, `X^2Y`. Michael's card (tb-look) named "XY with a gap". The code
  wasn't a choice made for 3X: the gap went on the side-by-side separator, and every pair takes it.
  - **To match the card,** sep_w would give the gap only where both sides are letters ('a' or 'E'). That moves every
    frame with a digit or a bracket beside a factor, 22's CRC among them, which the app's and the web's checks pin.
    So it waits for Michael's word at the look (abacus puts it on the card), rather than a change after acceptance.
  - look080's first screen, `3XY+XY^2`, shows both cases on the glass: 3 to X and X to Y.
- The unit's acceptance waits on Michael's look (card tb-exponent, v170: the gap's reach and exponents at scale 1). The
  stand-in stays on look080.

## +C on its line (abacus #11101; Michael's first look on the phone, tally #11099)

- **The fault:** draw_textbook centred +C on the last line's box. An exponent raises that box's top, so +C sat high:
  tally measured ∫(5+6X)'s + of +C at rows 183-197 against the line's 186-200. The placement also assumed the last
  line sat at the area's bottom, which a scrolled view doesn't.
- **The fix:** +C goes on the last line's axis, the row the line's glyphs sit on. The line's top is found by walking the
  lines drawn, so a scrolled view places it too.
- **The oracle:** frame 34-tb-plus-c-power (5X+3X^2 +C) and two pixel checks: the 5 on the line's main row (the
  check's own footing), and +C on that row beside the last glyph. Before the fix the +C check was red (9006/9007) and
  the footing check green; after it, 9007/9007.
- **What didn't move:** frame 30 (+C on a line with no exponent) is unchanged, since the axis and the old centring agree
  there. 22's CRC 9A4A7D8C is unchanged too. make test rc 0.
- **Not on the board:** the stand-in stays on look080 (aa738bb) for Michael's look, as abacus asked, so this fix wasn't
  flashed. Neither look screen has a +C.

## EQN LIST TOP and the built-in solvers on a clean screen (abacus #11109, Michael's ask)

- **The change:** in STU, textbook_shown also takes EQN LIST TOP and a built-in solver's entry ("2*2 lin. solve"). Each
  is drawn through the area's text path (shown_text: never parsed as math), with T, Z and Y gone and textbook_on set,
  so the phone and the web draw them from the frame too.
- The shown equation was indexed directly before, at an eqn_cur of -1 at the list's top. Now one helper reads it.
- 33s and 35s are untouched: EQN LIST TOP keeps its stack there. So does an equation stamped 33 or 35 in STU mode, as
  before.
- **The oracle:** frames 35-tb-list-top and 36-tb-builtin, and two app_test checks (STU's top: textbook_on, X and drawn
  "EQN LIST TOP", T empty; 33s's top: the stack, as before).
  - Against the screen.c before it, both frames are red, and so is the STU check (789/790). The 33s check passes
    either way, as the guard it is.
  - With it: screen_test 9008/9008 and 36/36 frames, app_test 790/790, make test rc 0. 22's CRC 9A4A7D8C is unchanged.
- Not flashed: the stand-in stays on look080 for Michael's look.

## A sum inside a sum keeps its brackets after a sign (abacus #11116, #11119)

- **The fault:** the n-ary rewrite kept only half the prototype's bracket rule for a sum's items, brackets for a minus
  term after a sign. It dropped the other half: a sum after a sign. So a typed `A-(B+C)` drew as `A-B+C`, another value
  on screen. `+(-43Y-192)` drew as `+-43Y-192`.
- It was found by replaying the glass equation for card tb-exponent. Casim's canon never prints the shape, which is why
  the corpus readback passed.
- **The fix:** item_br. After a sign, a sum or a minus takes brackets, in the layout, the drawing, both texts and the
  wrap's width estimate.
- **The oracle:**
  - drawn-text checks: `A-(B+C)`, `A-(B-C)`, `A+(-B-C)`, the glass polynomial, and `(A+B)+C` (the first term needs none);
  - readback checks for six typed shapes;
  - abacus's widening: a seeded family of 400 typed equations, sums, differences, products and quotients of bracketed
    sums and differences, two or three deep, with a unary minus inside and outside. Each is drawn, read back and
    evaluated equal at three seeds.
- **Red, then green:** against the old rule, tbook_test is 31/39, and the family has 102 of 400 differing. With the fix:
  39/39, the family 400 of 400, none undrawn.
- All 36 frames and 22's CRC are unchanged; app_test 790/790; make test rc 0.
- **The corpus census moves slightly,** since the brackets add width: 3,526 whole at 3, 732 at 2, 528 wrapped, 27 taller,
  55 too wide, 16 past the depth.
- **tb-exponent (#11114):** exponents stay at size 1 in a scale-2 answer, as built ("tiny but readable"), and so does
  the 3X gap. The exp2 side-by-side branch (tb-exp2) is deleted, moot.
