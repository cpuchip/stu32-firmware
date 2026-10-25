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

**Why this file is in work/blueprint/:** scripts/check-version.sh takes the last unit from work/NNN-*.md (and
tests/NNN-*.txt). A record named work/080-*.md now would make it demand VER 0.80 before any 080 code exists, which is
079's VER miss (#10800) run the other way. The unit's own commit moves this file to work/080-textbook-display.md and
bumps AB_FW_VERSION to "0.80" together.

**The evidence** is in work/blueprint/080-evidence/:
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
