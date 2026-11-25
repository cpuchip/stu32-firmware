# Blueprint: the characters the face lacks, and the steps view's flag (unit 103)

**From:** abacus #17479, from tally #17478. **Status:** a blueprint for abacus's acceptance. No src is written. A
throwaway prototype in a scratch worktree answered what moves (work/glyphs-steps-evidence/).

## 1. The characters the face lacks

tally saw ';' drawn as screen.c's hollow box on X^3=2's STEPS page. The census abacus asked for says ';' is not the
only one.

**The census, two ways** (evidence):
- **Static** (glyph-census-static.py, .txt): every code point in the C string literals of core/, firmware/ and
  casim.c, and in casim's golden corpus, against firmware/font5x7.txt's 129 glyphs. Eleven code points have none.
  Read in context, six of them never reach the screen:
  - `"` and `\`: the corpus file's own syntax (its comments, `\N` for null);
  - `±`: limit's side, an input;
  - `{` `}`: help text's placeholders, filled before it is drawn;
  - `|`: tbook's drawn-text export for ABS's bars, not drawn as a glyph.
- **Drawn** (glyph-census-frames.txt): a throwaway build drew every key's frame through screen_render over every plain
  vector run make test makes (130 runs), and logged each hollow box. 27 boxes:

  | code point | boxes | where |
  |---|---|---|
  | `;` U+003B | 6 | 3 vectors' STEPS pages (095 S04, the 1.8.0 and 1.10.0 repins' P lines) |
  | `⒤` U+24A4 | 15 | 6 vectors' equations (086 M07, 099 E01, and 015, 015b and 025's) |
  | `⒥` U+24A5 | 6 | 3 vectors' equations |

- **ŷ (U+0177) and ȳ (U+0233)** are equation tokens (calc.c, eqn.c: the statistics' ŷ( and ȳ). No vector shows one, so the drawn
  census missed them. Drawn directly (`STU EQ:ŷ(2)+ȳ`), each is a box.
- **So five characters are drawn as boxes:** `;`, `⒤`, `⒥`, `ŷ`, `ȳ`.
  - `;` is in every CAS report and steps page joined by "; ", on the boards too.
  - The other four appear wherever an equation holds them: STU's and the 33s's indirect, the statistics' ŷ and ȳ.
  - x̂ and x̄ draw correctly, as x and a combining mark.

**The change:**
- **`;`:** a glyph in font5x7.txt (ours, as the face is), `:`'s upper dot over `,`'s tail:
  ```
  .....
  .##..
  .##..
  .....
  .##..
  ..#..
  .#...
  ```
- **`ŷ` and `ȳ`:** drawn as `y` with the face's combining circumflex and macron, as x̂ and x̄ are (screen.c's next_cell
  maps them). No glyph, and one cell each, so nothing's width changes.
- **`⒤` and `⒥`:** a glyph each, one cell, the letter between the parentheses' curves (frames/F03-after.png):
  ```
  ⒤ .#.#.    ⒥ .#.#.
    #.#.#      #.#.#
    #...#      #...#
    #.#.#      #.#.#
    #.#.#      #.#.#
    #.#.#      ##..#
    .#.#.      .#.#.
  ```
  - At the equation line's scale 3 these read as (i) and (j), but cramped. The other way is to draw them as three
    cells, `(i)`, as the listing names them. That changes the equation line's width and the editor's cursor cells,
    which count code points.
  - The lean is one cell: no width, fit or cursor change. (Ask 1.)
- The font grows by three glyphs. D68 measures what that costs in flash.

## 2. The steps view's flag

screen_page's `view` is set for SYS's view, SOLVE's report and the steps view alike. The phone draws the first two as
text rows, so it can't tell when to draw rows 21-205 from the frame.
- **The change:** `bool steps_on` in screen_page, set with `view` when compose shows the steps view (its own
  `k->steps`, already there).
- **The prototype:** %STEPSON reads 1 on a STEPS page and 0 on SOLVE's report and on an equation list. With `view`
  alone, today's code reads the report as 1 too (probe.txt's F02 on e83e8d2).

## What moves (the prototype's whole-suite run)

- Nothing in vectors, screen_test's 67 frames, the replay's 51 scripts or app_test. No current frame draws these five
  characters except the vectors' own, and those aren't frames.
- screen_test's checks go from 9,529 to 9,931: it checks every glyph of the face.
- **One prototype miss:** the on-chip test app (device_host, AB_DEVICE) has no screen. The %STEPSON directive goes
  under `#ifndef AB_DEVICE`, as %XDRAWN is.

## Rulings (abacus #17497)

1. **⒤ and ⒥:** built as one-cell glyphs now, since that moves no width and no cursor cell. Their look is Michael's, on
   card T120 with F03-before and F03-after. If he picks three cells, that is a unit of its own.
2. **The drawn census** is a script, scripts/glyph_frames.py, run once at this unit and at any later unit that touches
   the face or the screen. abacus's acceptance runs it. The static census stays in make test.
3. **A grammar addition before h103,** so the hidden set can hold glyph rows: `%BOXES=n`.
   - **What it reads:** the hollow boxes in the frame after the vector's keys. screen_missed is set to 0, screen_render
     draws the frame with the default ui, and n is screen_missed after it.
   - **Where it compiles:** under `#ifndef AB_DEVICE`, as %XDRAWN and %STEPSON are, since the on-chip runner has no
     screen.
   - **On today's code:** a frame with a ';', ⒤, ⒥, ŷ or ȳ in it reads n > 0. After the unit it reads 0.

## The oracle (written before src; h103 by abacus before src)

- **The face's census, kept** (tests/glyph_census.py, in make test): the static census above.
  - Every printable code point in core/, firmware/ and casim.c's string literals and the golden corpus's answer
    column must have a glyph or a mapping.
  - A short allowlist names each one that is never drawn, with its reason.
  - It fails today on the five.
- **screen_test:**
  - three new frames, `;` on a STEPS page, ⒤ and ⒥ in an equation, and ŷ and ȳ in one, each CRC recorded and drawn
    with no hollow box (`screen_missed == 0`, which screen_test already checks over its frames);
  - a control: a code point the face still lacks draws a box and counts one.
- **tests/103-soroban.txt:** %STEPSON 1 on a STEPS page, 0 on SOLVE's report, SYS's view, the home and an equation list;
  %BOXES=0 on frames holding each of the five, and a control that still reads a box (a code point the face lacks).
- **The drawn census, scripts/glyph_frames.py** (ruling 2): every key's frame over the suite's plain vector runs, 0
  boxes. It runs at this unit and at later units that touch the face or the screen.
- **Mutants, predicted before the run:**
  - M0, the control;
  - M1, `;`'s glyph removed;
  - M2, ⒤'s;
  - M3, the ŷ mapping;
  - M4, steps_on not set;
  - M5, steps_on set for every view.
- **D68,** and **the board:** a stand-in script drawing a STEPS page with `;` and an equation with ⒤, its frame the
  host's CRC (claimed).
- **VER 0.104.**

## Asks for abacus

1. **⒤ and ⒥:** one-cell glyphs (the lean, frames/F03-after.png), or `(i)` as three cells, which changes the equation
   line's widths and cursor cells. The glyph's look may be Michael's call; tally can show him the frame.
2. **The drawn census:** a make test step (every vector's frames, about double the vectors' time), or once at this
   unit with the static census kept.
3. **h103's hash before src.**
