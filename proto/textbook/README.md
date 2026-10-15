# proto-textbook: casim's answers laid out 2D on the LS027 (a THROWAWAY prototype)

abacus #8391: a textbook (2D) display of results, with entry kept linear. **The unknown:** does a 2D layout of casim's answers fit and read on the 400 × 240 LS027
in font5x7, and what does it cost? Host only; this branch never merges. Clean room: the layout is ours, from the
typesetting idea (stacked fraction, raised exponent, radical, bars, sized brackets) and our own face; no calculator's
source was read.

Build and run from the repo root (writes build/proto/, x3 previews in build/proto/x3/):

    cc -std=c11 -Wall -Wextra -O2 -Ifirmware -Iproto/textbook proto/textbook/main.c proto/textbook/tb.c firmware/font5x7.c -o build/proto/tbproto && ./build/proto/tbproto

`frames/` holds the panel's own pixels (400 × 240, 1 bit); `frames/x3/` the same magnified 3 times for a monitor.
`numbers.txt` is the run's output.

## What it is

- **tb.c** (the renderer, the part that would ship): a recursive-descent parser of casim's linear text into a node pool
  (18 B a node, no allocation), a box layout (width, height, and an axis: the row of the face's minus, 3 × scale from
  a glyph's top, so a fraction bar lines up with a minus), and drawing straight into the frame.
- **Shapes:** a÷b stacked with a bar (a÷b÷c drawn a over b·c, never a fraction over a fraction); x^y with the exponent
  raised one scale smaller (scale 2 → 1, 3 → 2; never below 1); SQRT as a radical with an overbar; ABS as bars a
  little taller than a letter; brackets drawn to the height of what they hold; EXP(u) as e^u. Brackets appear only
  where 2D needs them; casim's own parentheses around a numerator or denominator are dropped.
- **Products:** `3X`, `0.5X²`, `2(X+1)`, `(X−1)(X+1)` with no sign; a thin space before a function word
  (`0.2 LN|X−2|`, `X ATAN(X)`); `·` before a digit. `frames/*-times.png` shows every × kept, for comparison.
- **Too wide:** break at the top-level + and − into lines, each right-aligned, a continuation line starting with its
  operator (`tb_wrap`). Then shrink a scale. Scroll is drawn for comparison only.
- **Conditions** (`; X≠-1; X≠1`): one row under the answer, `X≠−1, X≠1`, right-aligned.
- **The worksheet mock** (`ws-*.png`): the status band, entries stacked up from the soft row, the newest at the bottom,
  each with its input linear at the left and its answer right-aligned under it, a dotted rule between; a bar at the
  left marks the selected entry. An older entry that doesn't fit whole isn't drawn; a newest one taller than the area
  shows its top.

## Fit (391 px of width for an answer; 180 px of height for entries)

| sample | form | bytes | one line at 2 | lines at 2 | height at 2 | one line at 3 | lines at 3 | height at 3 |
|---|---|---|---|---|---|---|---|---|
| X LN X | casim | 26 | 208 px, yes | 1 | 15 | 315, yes | 1 | 26 |
| | fractions | 21 | 132, yes | 1 | 35 | 201, yes | 1 | 56 |
| ATAN | casim | 26 | 251, yes | 1 | 19 | 378, yes | 1 | 32 |
| | fractions | 24 | 217, yes | 1 | 39 | 327, yes | 1 | 62 |
| EXP SIN cycle | casim | 39 | 264, yes | 1 | 15 | 399, no | 2 | 61 |
| | fractions | 35 | 200, yes | 1 | 35 | 303, yes | 1 | 56 |
| ATAN, DEG | casim | 29 | 243, yes | 1 | 39 | 366, yes | 1 | 62 |
| 3X^(4÷3)÷4 | casim | 14 | 35, yes | 1 | 45 | 55, yes | 1 | 76 |
| LN ABS | casim | 53 | 386, yes | 1 | 18 | 579, no | 2 | 63 |
| | fractions | 51 | 298, yes | 1 | 38 | 447, no | 2 | 123 |
| partial fractions | casim | 58 | 323, yes | 1 | 35 | 489, no | 2 | 121 |
| sum, X≠±1 | casim | 47 | 143, yes | 1 | 58 | 233, yes | 1 | 89 |
| long sum | casim | 106 | 517, no | 2 | 95 | 794, no | 3 | 213 |
| | fractions | 88 | 421, no | 2 | 95 | 650, no | 2 | 143 |
| Taylor, 189 B | casim | 189 | 922, no | 3 | 264 | 1,389, no | 5 | 691 |
| | fractions | 194 | 650, no | 2 | 194 | 985, no | 4 | 603 |

- **At scale 2:** 12 of the 16 forms fit one line. The four that don't wrap at + and − into 2 or 3 lines, every term
  whole. Two wrapped forms are taller than the 180 px area: the Taylor answer, 264 px as casim prints it and 194 px in
  fractions. They need a vertical scroll within the entry.
- **At scale 3:** 8 of 16 fit one line. Every sample still wraps with whole terms, but the Taylor answer is 5 lines
  and 691 px, close to four screens.
- **No term was ever too wide to wrap,** at 2 or at 3. The widest single terms are the long sum's last fraction and the
  Taylor fractions.
- **The fraction forms** for ATAN, the cycle, LN ABS, the long sum and the Taylor answer are my hand conversions:
  0.5 → ÷2, 0.0390625 → 5÷128 and so on. The long sum's 0.5×A÷(X−0.5X²−0.5) became A÷(2X−X²−1). They are not
  casim's output.
- **The fraction forms are narrower** in every case, by 14–37 %, because a stacked fraction's width is its
  wider part. They are taller.

## Does it read?

The pitch is 0.147 mm (abacus #8393, Sharp Table 3-1). A glyph is 1.03 mm tall at scale 1, 2.06 mm at 2 and 3.09 mm at 3.

- **Scale 2 with exponents at 1** (`ws-b`, `ws-d`, `s2-*`): three entries fit a screen, and everything but the
  exponents is the size of today's T/Z/Y lines. The structure reads at once on a monitor. **Exponents at scale 1 are
  1.03 mm,** the size of the clock's A or P mark, and fraction exponents (4/3, 3/2) are two 1 mm digits stacked. I can't
  judge the glass from a PNG. That is the one question this prototype can't settle (below).
- **Scale 3 with exponents at 2** (`ws-c`, `ws-e`, `s3-*`): it reads like print, and exponents are as large as
  today's stack lines. The cost: one or two entries a screen, and half the samples wrap.
- **The input line and the conditions at scale 1** (`ws-a`) are, I'd guess, too small on glass, as abacus doubts. At 2
  they cost 14 px an entry.
- **Fixed on the way (each seen in a frame first):**
  - ABS bars at letter height read as I ("LNIX−2I"). Now they are taller than a letter, with a thin space after the
    function name.
  - "X ATAN(X)" ran together as XATAN. Now a full cell's space goes before a function word.
  - Wrapped lines touched. Now there are 3 × scale pixels between them.
  - Base and exponent touched. Now there is a 2 px gap.
  - Partly shown entries had their exponents clipped under the band.
- **Open:** a binary minus between two fractions sits on the bars' row, as a textbook sets it, and in this face it reads
  like an underscore. The pixels are right (`s2-cycle`: bars on row 94, the minus on rows 94–95). It's optical. Insetting
  the bar by half a stroke helped little. A bar 1 px thinner than the minus was tried and didn't help either, so it
  isn't kept. More space around the minus, or a minus 1 px
  higher than the bar, would be the next try.

## Overflow: what I'd do

**Use the largest scale at which the answer fits the width on one line.** That is 3, else 2: the rule the X line
already follows (4, else 3, else 2).
- **Else, at 2, wrap at the top-level + and −,** right-aligned, each continuation line starting with its operator.
- **Else, when the wrapped answer is taller than the area,** the selected entry scrolls vertically, a line at a time.
- **Never shrink to 1.** It fits the long sum in 238 px (`long-shrink`), but at 1 mm it doesn't read, and the
  exponents can't shrink further.
- **No horizontal scroll** (`long-scroll-s2-*`). A 391 px window onto a 517 px expression cuts a fraction in half and
  hides the structure the 2D view exists to show.
- **`1D` on the selected entry** shows casim's linear text, wrapped. That's for someone who wants to read it
  character by character, or to copy it.

## Cost (decision 68's shape, for the renderer alone, tb.c)

| | |
|---|---|
| code, S3, -O2 (the firmware's CONFIG_COMPILER_OPTIMIZATION_PERF) | 8,728 B text (xtensa-esp32s3-elf-gcc, IDF v5.4.2, -mlongcalls) |
| code, S3, -Os | 6,833 B |
| code, host x86-64, -Os | 10,018 B |
| the face | already in the firmware: nothing added |
| RAM a node | 18 B |
| RAM, the worst sample (Taylor, fractions, with the wrap's nodes) | 114 nodes, 2,052 B |
| RAM, the prototype's pool | 320 nodes, 5,808 B (tb_tree); a real one sizes the pool to casim's longest answer |
| stack, S3 -O2, a frame each | tb_layout 160 B, tb_draw 176 B, a level of brackets in the parser 144 B (power + term + expr, 48 each), tb_wrap 464 B once |
| stack, the worst sample (depth 11) | about 2.1 KB drawing (12 × 176 + a glyph's 48 + 32), about 2.4 KB laying out inside a wrap (464 + 12 × 160) |
| host time, a 5950X, -O2, parse + fit + draw, best of 5 × 2,000 | 0.65 µs (3X^(4÷3)÷4) to 17.3 µs (the Taylor answer as casim prints it); a three-entry worksheet 6.5 µs |
| S3 time | **a guess,** not measured: 30 to 80 times the host, so about 0.5–1.4 ms for the worst answer and 0.2–0.5 ms a worksheet |

- **For scale,** sending a full frame to the panel takes about 50 ms at its 2 MHz limit: 240 lines of 52 bytes plus 2,
  12,482 bytes (from ls027.h's format). Drawing isn't where the time goes.
- **Stack is the real constraint.** The recursion is as deep as the answer. casim's depth guard allows 44 levels, and
  44 × 176 B is about 7.7 KB, which is the calc task's whole margin (8,188 B free at the worst reading). A real renderer
  needs one of three things: a depth cap with the 1D text past it, an explicit stack in place of the recursion, or a
  task of its own. It mustn't draw on the calc task at casim's depth.
- **Speed:** the glyph lookup is a linear search of the face, as draw.c's is. It's the obvious first thing to index if
  the S3 numbers matter.

## Keys in a worksheet (proposal only)

- **Navigate on R↓ and x↔y, both RPN ONLY in ALG,** so no ALG meaning is lost, and each is one press.
  - **R↓ moves the selection up,** to an older entry. It "rolls" history down into view, as R↓ brings Y down to X. It
    is also the upper of the two keys (row 2 against row 3), so the key's place and its direction agree.
  - **x↔y moves the selection down,** toward the edit line.
  - With no entry selected, R↓ selects the newest.
- **ENTER on a selected entry copies its answer to the edit line** (COPY), and **← returns to the edit line**
  without one.
- **All six soft keys stay free for the selected entry:** COPY, EDIT (the input back to the edit line), STO (the
  answer stored as a value), 1D, DEL, INFO. The mocks show that row.
- **The alternative is ▲ ▼ on the first two soft keys.** It is the easiest to discover, but it costs two of the six
  actions. I'd keep it for the scroll inside a tall entry, where the soft row is otherwise idle.

## What it doesn't do

- **No cursor in a laid-out answer,** no editing, and no selection of a part. Display only.
- **No break inside a term.** A term wider than the screen would shrink and then fail. No sample had one, but a long
  enough product could.
- **Inputs:** the "(sample)" input lines are placeholders, and the ∫ inputs are my reconstructions.
- **The S3:** its time is a guess, and no frame has been shown on glass.
- **The next step that settles readability:** show `ws-b`, `ws-c` and `s2-taylor189` on the stand-in's panel and put
  them in front of Michael. That is a device run, outside this card.
