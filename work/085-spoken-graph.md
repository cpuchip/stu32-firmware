# Blueprint: the spoken graph (words for a drawn graph)

**Unit 085.** **From:** soroban · **To:** abacus · **Asked:** abacus #13460 (Michael, 2026-11-07: "Let's do the two open items")
· **Status:** accepted to build as unit 085 (abacus #13489); the build and its record are below.

## The question

When GRAPH draws a curve, what does a screen reader hear? Today it hears nothing. learn (primer) labels the frame
from the core's own lines, and the APK (tally) does the same, but the core makes no text for a graph. This spec
covers three things: the words, where they live, and how they're judged.

## What was measured first

The rules below were measured on a throwaway probe: a copy of tests/vectors.c in my scratch directory, calling only
the public graph model (ab_graph_sample, ab_graph_window, ab_graph_row, ab_graph_axes). It was never committed, and
the firmware tree is untouched.

**The results:**
- **33 graphs.** Each output is quoted in the appendix.
- **Length:** 101 to 242 characters, the function's name included.
- **Time:** all 33 in 0.06 s on lepton's host, peaking at 4.3 MB.

**What the probe changed.** It failed twice, and both failures shaped the rules:
- **The pole.** The first version heard 1÷X's pole as two turning points and a crossing. That gave rules 4 and 6:
  a jump splits the curve, and a jump isn't a crossing.
- **The y axis.** The first version read the y axis off column 200, whose x is 0.0250626…, not 0. It said "near
  40" for 1÷X. That gave rule 5: f(0) is evaluated at exactly 0.

**One difference from the build.** The probe evaluated f(0) as a fast-path sample from a shifted window. The build
evaluates it as F(X) does, correctly rounded. Every f(0) in the appendix is exact, so the two paths agree there.

## (a) The words

### How the text is built

There are four sentences, in a fixed order. Only the fourth varies in shape.

1. **The name:** `Graph of <the equation's text>.` The text is the equation as the list holds it, the same text
   that screen_lines gives a host for the equation line. For 033's L−R form, `, left side minus right side` is
   added before the period. That form is the one whose screen note says L−R.
2. **The window:** `x from A to B, y from C to D.` A and B are XMIN and XMAX. C and D are the window as drawn:
   the set values, or the fit (ab_graph_window).
3. **The axes:** `Meets the x axis …; the y axis near V.` (rules 4 and 5).
4. **The shape:** what the curve does, left to right (rules 6 to 9). When it has more than four events, a summary
   replaces it (rule 10).

**When there are no values.** If no column has a value, the text is the name, the window, and `No values in the
window.`, with nothing more.

### What the words are made of

Every fact comes from three things the core already computes, and nothing else:
- **The 400 samples,** as drawn: 033's pinned column x and ab_graph_sample's value. A column whose status isn't OK
  (or whose value isn't real) has no value.
- **Their rows,** by ab_graph_row in the drawn window.
- **One value at x = 0,** F(X)'s correctly rounded path, pure.

**Words only.** Every number is rounded as in rule 2, and every claim is about the samples. There's no SOLVE, no
EXTR, and no claim between columns finer than a column.

### Rule 1: each column has a state

- **GAP:** the column has no value.
- **ABOVE:** its row is −1.
- **BELOW:** its row is H (185).
- **IN:** its row is 0 to 184.

### Rule 2: how numbers are said

- **The x unit (ux)** is the least power of ten ≥ |(XMAX − XMIN) ÷ 399|. A root is only known to within a column,
  so x is never said finer than a column.
- **The y unit (uy)** is the greatest power of ten ≤ |(YMAX − YMIN) ÷ 184|. A y that's said is a sample's exact
  value, so it's shown to a row.
- **Rounding:** a value v is said as r × u, where r = v ÷ u rounded to an integer, half even. Each step is rounded
  once at 34 digits.
- **Writing:** the result is written by ab_format in ALL, at 12 characters, with '.' and no separator. −0 is
  written 0.
- **Why ALL:** not the display mode, so FIX 4 can't add zeros the words don't know. Not the device's radix either,
  because the words are an English sentence that uses commas.
- **The window's ends** are said with the same units: x ends at ux, y ends at uy.

**Measured.** With the default −10..10, ux is 0.1. With S24's fitted −267..89, uy is 1 (a row is about 1.9), so f(0) =
−13 is said "−13". The probe's first version used the power of ten at or above a row and said "−10" there; that's
why uy rounds down.

### Rule 3: a jump

A jump between columns c−1 and c needs all of the following:
- 2 ≤ c ≤ 398;
- columns c−2 to c+1 all have values;
- y(c−1) and y(c) are nonzero, with opposite signs;
- |y(c−1)| > |y(c−2)| and |y(c)| > |y(c+1)|. That is, |y| grows toward the change from both sides.

This is a pole's signature, and the samples hold it. A root has |y| shrinking toward the change.

### Rule 4: where it meets the x axis

These meetings come from the samples, whether y = 0 is inside the drawn window or not.
- **A zero sample** whose neighbours aren't zero is a meeting at its column.
- **A run of two or more zero samples** is said as `Lies on the x axis from x near a to b`, using the run's ends.
- **A sign change between adjacent nonzero samples** c and c+1 that isn't a jump is a meeting at whichever column
  has the smaller |y|. On a tie, it's at c.
- **A zero at x = 0.** If f(0) (rule 5) is exactly 0, and no meeting or zero sample is already at column ycol−1 or
  ycol, a meeting at 0 is added in its place. This catches √X starting at 0 and X² touching
  at 0, which the columns never land on.

**How it's said:**
- 1 to 4 meetings: `Meets the x axis near a`, `near a and b`, `near a, b and c`, `near a, b, c and d`.
- More than 4: `Meets the x axis N times, from near a to z`.
- None: `Doesn't meet the x axis`.
- After a run of zeros: `; meets it near …`.

### Rule 5: the y axis

This applies when 0 lies in the x window, that is, when ab_graph_axes gives a y axis column (ycol ≥ 0).
- **The value:** f(0), evaluated at x = 0 exactly, by F(X)'s correctly rounded path, with the variable restored
  (033 rule 7).
- **How it's said:** `; the y axis near V` at uy, even when V lies outside the drawn window. If f(0) has no value,
  it's `; no value at x 0`.
- **When 0 isn't in the x window,** nothing is said about the y axis.

Sentence 3 ends with a period.

### Rule 6: the events, left to right

Walk the columns from 0 to 399. Each event has the columns on its left and right where the stretches beside it end
and start.

| Event | When | Said | Left, right |
|---|---|---|---|
| leaves at the top | IN at c−1, ABOVE at c | `leaves at the top near x x(c−1)` | c−1, c |
| leaves at the bottom | IN, then BELOW | `leaves at the bottom near x x(c−1)` | c−1, c |
| enters at the top | ABOVE, then IN at c | `enters at the top near x x(c)` | c−1, c |
| enters at the bottom | BELOW, then IN | `enters at the bottom near x x(c)` | c−1, c |
| passes up or down | BELOW then ABOVE (or the reverse), with no jump | `passes up through the window near x x(c)` (or `down`) | c−1, c |
| jump | rule 3 between c−1 and c | `jumps near x x(c)` | c−1, c |
| gap | a maximal run of GAP columns, a to b | `no values from near x a to b`, `from the left edge to near x b`, `from near x a to the right edge` | a−1, b+1 |
| high or low | rule 7 | `a high near x X, y Y` (or `a low`) | k, k |

**Order.** Events are taken in column order. At one column the order is: leaves, passes, jump, gap, enters, turn.

### Rule 7: a turn

**A segment** is a maximal run of consecutive columns that have values, share one row, and have no jump inside.

A segment from i to j is a **turn** when all of these hold:
- its row is 0 to 184;
- i > 0 and j < 399;
- columns i−1 and j+1 have values;
- there's no jump at i or at j+1;
- the rows of i−1 and j+1 are both greater than the segment's row (a **high**: the curve is higher there) or both
  less (a **low**).

**Where it's said:** at the segment's sample with the greatest y for a high, or the least for a low. On a tie, the
leftmost. x is said at ux and y at uy.

**Why rows.** Turns use rows, not values, so the reader hears the turns the eye can see. S28, X + SIN(50X) ÷ 1000,
wiggles by 0.001 under a row of 0.12. It's said `Rises across the window`. By values it would count every turn of SIN(50X): about 318 on −10..10, from 159 periods.

### Rule 8: the direction of a stretch

A stretch runs between two events, or from an edge to an event. It takes the columns from the previous event's
right column to the next one's left column.

**Its direction** comes from its first and last IN columns, when it has at least two:
- `rises` when the row decreased;
- `falls` when it increased;
- `is level` when it's equal.

**An implied direction isn't said.**
- A direction is left out when the events on both sides imply it.
- What each event implies after it: a high, falls; a low, rises; entering at the top, falls; entering at the
  bottom, rises.
- What each event implies before it: a high, rises; a low, falls; leaving at the top, rises; leaving at the bottom,
  falls.
- Jumps, gaps and passes imply nothing.
- **Exception:** a direction just before a turn is always said, joined to it: `falls to a low near x 0, y −4`.

### Rule 9: the narrative

The phrases are joined with `, `, and the last one with `, then `. The first is capitalised, and the narrative ends
with a period.

**With no events:**
- If the curve has IN columns, it's the stretch's direction plus `across the window` (`Rises across the window.`).
- If it has none, it's `Above the window throughout.` or `Below the window throughout.`

### Rule 10: the breath cap

With more than four events, the narrative is replaced by a summary. Each part is said only when its count is
nonzero:
- `Turns N times, from x near a to z`, adding `, highs near y H` when every high is said the same, and `, lows
  near y L` likewise, then a period;
- `Leaves the window N times.`;
- `Passes through it N times.`;
- `Jumps N times.`;
- `No values in N places.`

Four events with their directions came to 231 characters for S22, a quartic. The cap is set there.

## (b) Where the text lives

**The call:** `int ab_graph_words(ab_calc *c, char *out, int size)` in core/calc.c, declared in calc.h beside 033's
model.
- It returns the length.
- With no open graph (`!c->gr_on`), it writes "" and returns 0.
- It is pure: no variable or setting changes. That's 033 rule 7, the same pure evaluation as the samples.
- **The only write** is the fit memo 040's review already keeps (gr_fit_ok), when AUTO hasn't been fitted yet.
  That's RAM only, and exactly what ab_graph_window does today.

**Who calls it.** primer and tally call it whenever the plot is complete:
- after GO;
- after every zoom, and after AUTO or a window key;
- when the reader asks.

They link the core already. The device app doesn't call it: the S3 has no reader.

**Nothing on the screen changes.**
- screen_page, screen_ui and the frame are untouched.
- No state is added to ab_calc.
- The image is unchanged.
- The trace's readout is already text (033b's screen_graph_readout), and is outside these words.

**Memory.**
- It works in one streaming pass, holding four samples, the current segment, the counts, and the first five
  events' phrases. So statics stay at +0, as D68 measures.
- Its stack holds a few abn and the phrase buffers. I'll report the measured stack and flash at the build.
- The probe's 400-sample arrays were for measuring only.

**The cost.**
- 400 fast samples, plus 400 more when AUTO's fit isn't known yet, plus one correctly rounded f(0).
- That's one plot's evaluations. On a host it's nothing: the probe's 33 graphs took 0.06 s in all.

## (c) Vectors and mutants

**The token.** `%GSAY=<text>` is new in tests/vectors.c. It takes the rest of the line, spaces included, so it
comes last in the expectations.

**The vector file.** tests/0NN-soroban.txt (abacus numbers the unit) carries the appendix's graphs, each red first
against the empty call. They are:
- each rule's row;
- abacus's four first rows (S01, S03/S02, S15, S05);
- the purity row, S31 (`@X=3` after the call).

**For abacus's hidden set.** It can compute the expected text from mpmath samples at the pinned column x, using
rules 1 to 10. As in 033's set, skip any case where a sample lies within 1E-20 of a decision point:
- zero, for a sign;
- a row boundary;
- a half-unit tie in rule 2;
- an equal |y| in rule 4's nearer column, or between a segment's samples.

**Mutants.** Four were run on the probe (marked *probe*), and those results are its output. The rest are predictions. My hand-worked guesses for the four were wrong in each case (I had S01 as "2.03", S27's sides swapped, S06 as "40", and M11 on S15), so none of the predictions should be read as measured.

| # | Mutant | Red |
|---|---|---|
| M1 | ux the power of ten below a column (claims more than a column) | *probe:* 26 of 33 differ; S01 "near -1.98 and 1.98", "a low near x -0.03" |
| M2 | uy the power of ten above a row | S24: the y axis "-10" |
| M3 | no jump test: every sign change meets the axis | S06, S07: "Meets the x axis near 0" |
| M4 | a jump doesn't split a segment or a stretch | S06: a high and a low at the pole |
| M5 | the meeting at the left column, not the nearer | *probe:* 2 of 33 differ; S27 "near -2.1 and 2" for roots ±2.07 |
| M6 | turns by values instead of rows | S28: hundreds of turns |
| M7 | an edge segment counted as a turn | S29: "a low near x 0" on 0..10 |
| M8 | f(0) as column ycol's sample | *probe:* 14 of 33 differ; S06 "the y axis near 39.9" |
| M9 | f(0) = 0 added beside a meeting already at ycol | S15, S09: "near 0" twice, and 8 meetings |
| M10 | implied directions said | S18: "falls" and "rises" back |
| M11 | the cap at five events | *probe:* 1 of 33 differs; S19, five turns, narrated at 320 characters |
| M12 | a zero run said as one meeting | S25: "Meets the x axis near -10" |
| M13 | the samples not pure (the variable kept at the last x) | S31: @X |
| M14 | the display mode used for numbers | S01 under FIX 4: "-2.0000" (a vector in FIX 4) |

## Tensions and questions for abacus

1. **A pole of even order is said as a high.** S14, 1÷X², gives `Rises to a high near x 0, y 1592, then falls`
   beside `no value at x 0`.
   - From samples alone, a pole and a narrow peak look the same.
   - 040d's EXTR test tells them apart, at a few evaluations per turn. I'd keep it out of this unit and document
     the limit. Say if you want it in.
2. **The words and the pixels differ at a pole on purpose.** With AUTO, 1÷X's samples ±40 are both inside the
   window, so 033 rule 4 joins them and the screen draws a wall at 0. The words say `jumps near x 0`. I think
   the words are right and the wall is 033's documented behaviour. A sighted reader and a listener get different
   facts there.
3. **The numbers are English: '.' and ALL,** whatever the device's radix or display mode. The rest of the screen a
   reader hears (screen_lines) follows the device. A comma-radix user hears "1.7" in the graph and "1,7" on the
   stack.
4. **The caps:** four listed meetings and four narrated events. They're set by S22, a quartic, at 231 characters.
   I'll report the longest over your hidden set at the build.
5. **Your SIN row needs RAD.** The default angle is DEG, so SIN(X) on −10..10 is a tenth of a period (S04).
   "Several periods" is RAD on −10..10 (S15), or 0..1080 in DEG.
6. **The name is the equation's own text,** symbols and all (`X^2-4`, `1÷X`), as screen_lines gives it. Spelling
   ^ or ÷ as words is the hosts' job, as it is for the equation line today. If the hosts want words from the core,
   that's one table, and a separate question.
7. **Where my runs happen.** Your #13460 says to claim fermion for any run. I build and test on lepton, my own box,
   as for every unit so far. I need nothing on fermion.

## Appendix: the probe's 33 graphs

The keys are in the vectors' form. Every number is the probe's output, unedited.

```
S01 parabola, two roots     STU EQ:X^2-4 C GRAPH
  Graph of X^2-4. x from -10 to 10, y from -9 to 101. Meets the x axis near -2 and 2; the y axis near -4. Falls to a low near x 0, y -4, then rises.
S02 line, no roots, fitted  STU EQ:X+20 C GRAPH
  Graph of X+20. x from -10 to 10, y from 9 to 31. Doesn't meet the x axis; the y axis near 20. Rises across the window.
S03 line, set window        STU EQ:X+20 C 5 CHS YMIN 5 YMAX GRAPH
  Graph of X+20. x from -10 to 10, y from -5 to 5. Doesn't meet the x axis; the y axis near 20. Above the window throughout.
S04 SIN, DEG default        STU EQ:SIN(X) C GRAPH
  Graph of SIN(X). x from -10 to 10, y from -0.191 to 0.191. Meets the x axis near 0; the y axis near 0. Rises across the window.
S05 leaves at the top       STU EQ:X^2 C 10 CHS YMIN 50 YMAX GRAPH
  Graph of X^2. x from -10 to 10, y from -10 to 50. Meets the x axis near 0; the y axis near 0. Enters at the top near x -7, falls to a low near x 0, y 0, then leaves at the top near x 7.
S06 1/X fitted              STU EQ:1÷X C GRAPH
  Graph of 1÷X. x from -10 to 10, y from -43.9 to 43.9. Doesn't meet the x axis; no value at x 0. Falls, jumps near x 0, then falls.
S07 1/X set                 STU EQ:1÷X C 5 CHS YMIN 5 YMAX GRAPH
  Graph of 1÷X. x from -10 to 10, y from -5 to 5. Doesn't meet the x axis; no value at x 0. Falls, leaves at the bottom near x -0.2, jumps near x 0, enters at the top near x 0.2, then falls.
S08 square root             STU EQ:SQRT(X) C GRAPH
  Graph of SQRT(X). x from -10 to 10, y from 0.01 to 3.31. Meets the x axis near 0; the y axis near 0. No values from the left edge to near x 0, then rises.
S09 SIN, DEG 0..360         STU DEG EQ:SIN(X) C 0 XMIN 360 XMAX GRAPH
  Graph of SIN(X). x from 0 to 360, y from -1.1 to 1.1. Meets the x axis near 0, 180 and 360; the y axis near 0. Rises to a high near x 90, y 1, falls to a low near x 270, y -1, then rises.
S10 constant                STU EQ:X×0+5 C GRAPH
  Graph of X×0+5. x from -10 to 10, y from 4 to 6. Doesn't meet the x axis; the y axis near 5. Is level across the window.
S11 cubic, two turns        STU EQ:X^3-3×X C 3 CHS XMIN 3 XMAX GRAPH
  Graph of X^3-3×X. x from -3 to 3, y from -19.8 to 19.8. Meets the x axis near -1.7, 0 and 1.7; the y axis near 0. Rises to a high near x -1, y 2, falls to a low near x 1, y -2, then rises.
S12 L-R                     STU EQ:X^2=4 C GRAPH
  Graph of X^2=4, left side minus right side. x from -10 to 10, y from -9 to 101. Meets the x axis near -2 and 2; the y axis near -4. Falls to a low near x 0, y -4, then rises.
S13 V=expr                  STU EQ:Y=X^2-4 C GRAPH
  Graph of Y=X^2-4. x from -10 to 10, y from -9 to 101. Meets the x axis near -2 and 2; the y axis near -4. Falls to a low near x 0, y -4, then rises.
S14 1/X^2 (limit 1)         STU EQ:1÷X^2 C GRAPH
  Graph of 1÷X^2. x from -10 to 10, y from -80 to 1672. Doesn't meet the x axis; no value at x 0. Rises to a high near x 0, y 1592, then falls.
S15 SIN, RAD, periods       STU RAD EQ:SIN(X) C GRAPH
  Graph of SIN(X). x from -10 to 10, y from -1.1 to 1.1. Meets the x axis 7 times, from near -9.4 to 9.4; the y axis near 0. Turns 6 times, from x near -7.8 to 7.8, highs near y 1, lows near y -1.
S16 TAN, RAD, set           STU RAD EQ:TAN(X) C 5 CHS YMIN 5 YMAX GRAPH
  Graph of TAN(X). x from -10 to 10, y from -5 to 5. Meets the x axis 7 times, from near -9.4 to 9.4; the y axis near 0. Leaves the window 6 times. Jumps 6 times.
S17 X^2 touches             STU EQ:X^2 C GRAPH
  Graph of X^2. x from -10 to 10, y from -5 to 105. Meets the x axis near 0; the y axis near 0. Falls to a low near x 0, y 0, then rises.
S18 off both sides          STU EQ:X^2-50 C 10 CHS YMIN 10 YMAX GRAPH
  Graph of X^2-50. x from -10 to 10, y from -10 to 10. Meets the x axis near -7.1 and 7.1; the y axis near -50. Enters at the top near x -7.7, leaves at the bottom near x -6.3, enters at the bottom near x 6.3, then leaves at the top near x 7.7.
S19 SIN(X)/X, RAD           STU RAD EQ:SIN(X)÷X C GRAPH
  Graph of SIN(X)÷X. x from -10 to 10, y from -0.278 to 1.061. Meets the x axis 6 times, from near -9.4 to 9.4; no value at x 0. Turns 5 times, from x near -7.7 to 7.7, lows near y -0.217.
S20 below throughout        STU EQ:X-100 C 5 CHS YMIN 5 YMAX GRAPH
  Graph of X-100. x from -10 to 10, y from -5 to 5. Doesn't meet the x axis; the y axis near -100. Below the window throughout.
S21 X^3 through a window    STU EQ:X^3 C 5 CHS YMIN 5 YMAX GRAPH
  Graph of X^3. x from -10 to 10, y from -5 to 5. Meets the x axis near 0; the y axis near 0. Enters at the bottom near x -1.7, then leaves at the top near x 1.7.
S22 quartic, three turns    STU EQ:X^4-5×X^2+4 C 3 CHS XMIN 3 XMAX GRAPH
  Graph of X^4-5×X^2+4. x from -3 to 3, y from -4.4 to 42.1. Meets the x axis near -2, -1, 1 and 2; the y axis near 4. Falls to a low near x -1.6, y -2.2, rises to a high near x 0, y 4, falls to a low near x 1.6, y -2.2, then rises.
S23 LN                      STU EQ:LN(X) C GRAPH
  Graph of LN(X). x from -10 to 10, y from -3.99 to 2.6. Meets the x axis near 1; no value at x 0. No values from the left edge to near x 0, then rises.
S24 a long name             STU EQ:3×X^5-2×X^4+7×X^3-11×X^2+5×X-13 C 2 CHS XMIN 2 XMAX GRAPH
  Graph of 3×X^5-2×X^4+7×X^3-11×X^2+5×X-13. x from -2 to 2, y from -267 to 89. Meets the x axis near 1.4; the y axis near -13. Rises across the window.
S25 zero everywhere         STU EQ:X×0 C GRAPH
  Graph of X×0. x from -10 to 10, y from -1 to 1. Lies on the x axis from x near -10 to 10; the y axis near 0. Is level across the window.
S26 0 not in the x window   STU EQ:X^2-9 C 1 XMIN 5 XMAX GRAPH
  Graph of X^2-9. x from 1 to 5, y from -9.2 to 17.2. Meets the x axis near 3. Rises across the window.
S27 the nearer column       STU EQ:X^2-4.285 C GRAPH
  Graph of X^2-4.285. x from -10 to 10, y from -9.3 to 100.7. Meets the x axis near -2.1 and 2.1; the y axis near -4.3. Falls to a low near x 0, y -4.3, then rises.
S28 a wiggle under a row    STU RAD EQ:X+SIN(50×X)÷1000 C GRAPH
  Graph of X+SIN(50×X)÷1000. x from -10 to 10, y from -11 to 11. Meets the x axis near 0; the y axis near 0. Rises across the window.
S29 a minimum at the edge   STU EQ:X^2 C 0 XMIN 10 XMAX GRAPH
  Graph of X^2. x from 0 to 10, y from -5 to 105. Meets the x axis near 0; the y axis near 0. Rises across the window.
S30 four roots              STU EQ:(X^2-1)×(X^2-4) C 3 CHS XMIN 3 XMAX GRAPH
  Graph of (X^2-1)×(X^2-4). x from -3 to 3, y from -4.4 to 42.1. Meets the x axis near -2, -1, 1 and 2; the y axis near 4. Falls to a low near x -1.6, y -2.2, rises to a high near x 0, y 4, falls to a low near x 1.6, y -2.2, then rises.
S31 the variable kept       STU EQ:X^2-4 C 3 STO:X GRAPH      (then @X=3)
  Graph of X^2-4. x from -10 to 10, y from -9 to 101. Meets the x axis near -2 and 2; the y axis near -4. Falls to a low near x 0, y -4, then rises.
S32 passes up               STU EQ:1000×X C 5 CHS YMIN 5 YMAX GRAPH
  Graph of 1000×X. x from -10 to 10, y from -5 to 5. Meets the x axis near 0; the y axis near 0. Passes up through the window near x 0.
S33 passes down             STU EQ:-1000×X C 5 CHS YMIN 5 YMAX GRAPH
  Graph of -1000×X. x from -10 to 10, y from -5 to 5. Meets the x axis near 0; the y axis near 0. Passes down through the window near x 0.
```

## Accepted to build as unit 085 (abacus #13489), with a touch rule (abacus #13501)

**abacus #13489:**
- **(a) One word order** for both counted forms: `Meets the x axis N times, from x near a to z`, and
  `Turns N times, from x near a to z`. The gap phrases take the same order: `no values from x near a to b`, and
  `from the left edge to x near b`.
- **(b) A fresh listener** reads a sample of the texts before acceptance. abacus runs it.
- **Tensions 1 to 7** are ruled as proposed:
  - 1÷X² is kept as a high, a documented limit ("the listener hears what the eye sees");
  - the words, not the wall;
  - '.' and ALL;
  - the caps;
  - SIN in RAD;
  - the symbols are the hosts' job;
  - lepton.

**abacus #13501, a spec finding.** A curve that touches the x axis away from 0 has no sample on the touch.
- (X−1)² said "Doesn't meet the x axis" beside "a low near x 1, y 0".
- **Rule 4 gains:**
  - **A touch.** A turn whose said y is 0 at uy, with no zero sample and no sign change from column i−1 to j+1 of
    its segment, is a meeting at the turn's column.
  - **A gap's edge.** A sample beside a gap, said at y 0 at uy, with no meeting beside it, is a meeting there.
  - **What "Doesn't meet" means:** no sample meets the axis.
- **The cost, as abacus says:** (X−1)²+0.01 has the same rows, and is heard as touching, which is what the eye
  sees.
- Rows S38 to S42.

**Two forms the spec left open, built as follows:**
- **Two or more runs of zeros:** `Lies on the x axis in N stretches, from x near a to z`.
- **In the summary, a count of 1:** `once` (`Turns once, near x a`, `Jumps once.`), and `in one place`.

## Mutants: predictions, committed before any run

Each mutant is a text change to core/calc.c in a scratch copy, built with the same flags. A plant whose build fails
counts as a failure, not a pass. The runner prints the red vectors of tests/085-soroban.txt.
- **"Exactly"** means those rows are red and no others.
- **"At least"** means a broad change: those rows must be among the red.

| # | Mutant | Predicted red |
|---|---|---|
| M1 | ux a power of ten lower | at least S01, S27 (the probe: 26 of 33) |
| M2 | uy the power above a row, not below | at least S24 |
| M3 | no jump test | exactly S06, S07, S16 |
| M4 | a jump doesn't split a segment or end a turn's neighbour | exactly S06 |
| M5 | the meeting at the left column of a sign change | at least S27 (the probe had a second row it didn't name) |
| M6 | segments of one column (a plateau never turns) | at least S01, S09, S22 |
| M7 | an edge segment can turn | at least S29, S01 |
| M8 | f(0) as column ycol's sample | at least S06 (the probe: 14 of 33) |
| M9 | f(0) = 0 added beside a meeting already there | exactly S04, S05, S09, S11, S15, S16, S17, S21, S25, S28, S29, S32, S33 |
| M10 | implied directions said | exactly S18, S21, S39 |
| M11 | the cap at five events | exactly S19 |
| M12 | a run of zeros said as one meeting | exactly S25 |
| M13 | the words leave the variable at the last column's x | exactly S31 |
| M14 | the display mode's format (FIX 4 by default) for the numbers | at least S01, S34 |
| M15 | no touch at a turn (#13501) | exactly S38, S39, S42 |
| M16 | no touch at a gap's edge (#13501) | exactly S41 |

**Two departures from the blueprint's table:**
- **M6** was "turns by values". As planted it is "segments of one column": a value-by-value extremum test needs the
  values kept, which the build doesn't keep. S28 isn't its row.
- **M13** sets the variable directly. eval_at restores the variable whether or not it is pure, so an impure
  evaluation can't show in X.

## The build (c27e146 the code, a94807d a fix from unit 084), measured 2026-11-07 18:0x-18:28Z

**What was built:**
- core/calc.c's `ab_graph_words` follows rules 1 to 10 and #13501.
- `%GSAY` is in tests/vectors.c.
- tests/085-soroban.txt has 42 rows and runs in make test, plain and with save and load.
- It is in KEYV, so it also runs under the sanitizers and in slices of 1, 7 and 1000.
- VER is 0.85.

**Red first:**
- Against the stub that wrote "", 1 of the 37 rows then written passed: S37, the empty text.
- With both touch rules planted out, S38, S39, S41 and S42 went red and S40 didn't move. The plant's build
  succeeded.

**Green:**
- 42/42 rows, plain, with save and load, under the sanitizers, and in each slice mode.
- make test exits 0 (141 s, 402 MB peak, on lepton).

**The longest text** over the 42 rows is 242 characters, S18. The shortest non-empty one is 78.

**Mutants** (the predictions are in the section above, committed first at ff0c9c9):
- **15 of 16 held as predicted.** That includes every "exactly" set except M10.
  - M1: 33 red.
  - M2: 15.
  - M3: S06 S07 S16.
  - M4: S06.
  - M5: S18 S27.
  - M6: 19.
  - M7: 29.
  - M8: 13.
  - M9: exactly its 13.
  - M11: S19.
  - M12: S25.
  - M13: S31.
  - M14: 41.
  - M15: S38 S39 S42.
  - M16: S41.
- **M10 missed its prediction.** I predicted S18, S21 and S39. S05 went red too. In S05 the stretch from the low to
  leaving at the top is implied twice: a low implies rising after it, and leaving at the top implies rising before
  it. I didn't count that stretch. The rows caught the mutant; it was my prediction that missed a case.
- **M15 and M16's first plants didn't build.** `nm < 0` made gcc see `mcol[-1]` under -Werror=array-bounds, and a
  failed build counts as a failure. Re-planted to compare against a string that is never said, they went as
  predicted.

**The device (Decision 68,** `resources.py static b9bbce6 c27e146`, 18:24-18:27Z, in a soroban-* container capped at
4 GiB, peaking at 918 MiB sampled every 5 s):

| Build | Flash | Internal RAM | IRAM |
|---|---|---|---|
| stand-in | +64 | +0 | +0 |
| Rev A | +80 | +0 | +0 |

- calc_task's frame is unchanged.
- libmain grows by 13 bytes and libesp_app_format by 64, which is the version string and the app header.
  `ab_graph_words` isn't linked into the device image, because the app never calls it.

**The host's stack:** ab_graph_words's frame is 5,904 B (x86-64, gcc -O2, -fstack-usage). It holds the 400 columns'
rows and bits, the meeting spans (3 × 401 int16), and the phrase buffers. Its callees, graph_sample and the
evaluator, sit on top of that. The blueprint's (b) said "four samples": the build keeps one sample at a time, plus
the columns' rows and bits. Statics are +0.

**clang-check:** 70 files, 0 failing, after a94807d.
- Before it, the check failed at main on tests/expr_test.c:1066. That line has been there since unit 084's
  97357b9.
- clang -Wstring-concatenation refuses `"X\xc3\xb7" "2"` written as two literals side by side. They're split so
  the 2 isn't read into the escape.
- 084's record carries no clang-check result, so the check wasn't run at 084. It is now in parentheses, and
  expr_test still passes 368/368.

**Waiting:**
- abacus's hidden set (it was ruled private, as h084's was);
- abacus's fresh listener (#13489 (b)).

## The fresh listener's three asks (abacus #13531), and "breaks"

abacus's hidden set h085 was green at 8a5102a: 23/23, run on fermion. Its reference agrees with the 41 non-empty
rows word for word. The listener heard 10 texts once and asked for three changes:
1. **Meetings said the same are said once.** −(X+4)²+0.001 said "near −4 and −4". Equal said meetings sit next to
   each other, because the columns are in order, so a repeat is dropped. The counted form counts the said values.
   0.001−SIN(X)² in RAD has 14 meetings and 11 said values, so it says "11 times".
2. **A y-axis value outside the drawn window says so,** by its row: `(above the window)` or `(below the window)`.
   A turn can't be outside: rule 7 makes every turn IN.
3. **One turn in the summary is named:** `Turns once, a high near x 0, y −0.25.`, with no "highs" clause.

**"Breaks" for "jumps"** (abacus left this to me). The listener heard "jumps" as maybe a vertical line, which is
exactly what 033's wall at an AUTO pole looks like. The narrative now says `breaks near x 0`, and the summary
`Breaks N times.`

**Rows.**
- Changed: S03, S18, S20 and S39 gain the window note; S06, S07 and S16 say "breaks".
- New: S43 to S45. S45's count came from a Python model.
- Red first: exactly those 10 rows failed at 8a5102a (37/47 expectations). Then the change: 45/45.
- On the way, gcc's -Werror=format-truncation refused the count buffer. The binary that ran was stale, and its
  date showed it, so that run isn't counted.

**Mutants for the three, predicted before any ran:**

| # | Mutant | Predicted red |
|---|---|---|
| M17 | no dropping of a repeated said meeting | exactly S43, S45 |
| M18 | no window note on the y axis | exactly S03, S18, S20, S39, S43 |
| M19 | one turn in the summary in the old form ("near x a, highs near y …") | exactly S44 |

**M1 to M16 are rerun on the new code against the same predictions,** with these exceptions:
- M10 now includes S05 (already measured);
- M3, M4, M8 and M14 keep their sets with "breaks" in the texts;
- M9 and M18 overlap on no row.

**What the rerun showed, and one more rule.**
- M17, M18 and M19 went exactly as predicted.
- The rerun's new rows added reds I hadn't listed for the old mutants: S44 under M3, S45 under M5, S43 under M10.
  Each is a new row in the mutant's own class.
- **M9 fell from 13 red rows to 1 (S25).** Dropping a repeat now swallows the "0" that M9 adds beside a meeting
  already said 0.
- That pointed at a case no row covered. When f(0) is exactly 0, the meeting at x = 0 can still be said ±ux if
  its pair's nearer column is the far one. X×(X−0.09) on −17.92..17.99 (a step of 0.09) said "Meets the x axis
  near 0.1" for its roots at 0 and 0.09.
- **Rule 4 now reads:** when f(0) is exactly 0, a meeting whose columns cover ycol−1 or ycol is said 0. The value
  is exact, so the column's rounding doesn't apply. That gives "near 0 and 0.1". The span insertion carries its
  columns along.
- S46 was red at d603223's code and is green now. No other row moved: 46/46.

**Predicted before it ran:**

| # | Mutant | Predicted red |
|---|---|---|
| M20 | the covering meeting said by its column, not 0 | exactly S46 |

M9 stays in the table. It's now caught by S25 alone (predicted: exactly S25), and is nearly equivalent: the drop
of a repeat covers it.

**The rerun at f7e9e64 (2026-11-07 18:47-18:51Z):**
- **Mutants M1 to M20.** Every "exactly" set held, and every "at least" set was among the reds:
  - M3: S06 S07 S16 S44.
  - M4: S06.
  - M5: S18 S27 S45.
  - M9: S25.
  - M10: S05 S18 S21 S39 S43.
  - M11: S19.
  - M12: S25.
  - M13: S31.
  - M15: S38 S39 S42.
  - M16: S41.
  - M17: S43 S45.
  - M18: S03 S18 S20 S39 S43.
  - M19: S44.
  - M20: S46.
  - The broad ones: M1 35, M2 17, M6 23, M7 32, M8 15, M14 45.
- **make test** exits 0. 085 passes 46/46 plain, with save and load, under the sanitizers, and in slices of 1, 7
  and 1000.
- **clang-check:** 70 files, 0 failing.
- **The longest text** over the 46 rows is S18's, now 261 characters: "(below the window)" adds 19. The next are
  S30 at 234 and S36 at 233.
