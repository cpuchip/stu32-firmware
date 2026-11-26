# Unit 105 blueprint: the indirect tokens' own parentheses

**From:** abacus #17893, which relays Michael's answer to T136 (A74), "Token's own parentheses". It's 104's option (b)
(work/indirect-three-cells.md, ruling R4).
- **What abacus asked:**
  - the tokens keep three cells, cased by model as in 104 (35 `(I)` `(J)`, 33s and STU `(i)`);
  - their parentheses become a tighter pair of glyphs of their own, so a token never draws the same as the user's typed
    `( i )`;
  - plain parentheses stay as they are, and the cursor still crosses a token in one step;
  - **the oracle is a pixel test, not text:** the token's frame must differ from typed (i) in every mode, and the font
    census must have no boxes. If %BOXES can't express that, propose the directive;
  - a frame of the token beside typed (i) in each mode, with this blueprint.
- **VER 0.106.**

Every measurement here was taken at 8c4fad7 ("today") and on a throwaway prototype of the contract below. The
prototype's diff, vector files, outputs and frames are in work/token-parentheses-evidence/.

## What today draws

**The token and the user's own parentheses are the same pixels.** Each pair below is the token, then the same letter
typed inside the user's parentheses with the cursor after them. Each was compared over the whole 400 × 240 frame by
look.txt, run on 8c4fad7 with only the proposed %LOOK added to tests/vectors.c (look-today.out, AB_LOOKPX=1):

| Path | Keys, the token / typed | Pixels that differ, today |
|---|---|---|
| 35, typing | `RCL:(I)` / `() RCL:I >` | 0 |
| 35, (J) | `RCL:(J)` / `() RCL:J >` | 0 |
| 33s, typing | `RCL:(i)` / `() RCL:i >` | 0 |
| STU, typing | `RCL:(I)` / `() RCL:i >` | 0 |
| 35, the list (stored) | `… + 1 ENTER` | 0 |
| 35, a program line | `PRGM LBL:A EQN … + 1 ENTER` | 0 |
| 33s, a program line | the same under a 33s label | 0 |
| STU's list | `EQ:⒤+1` / `EQ:(i)+1` | 0 |
| Control: another equation | `RCL:A` / `RCL:(I)` | 688 |

The 35s guide tells the two apart by their look (p.14-23, "Equations with (I)/(J)"). There, the (I) key's token is
printed with its parentheses tight around the letter, and I typed in the user's own parentheses with them standing
apart. It reads them differently: the first is the variable whose number is in I, the second is I itself.

## The contract

1. **Two glyphs, the token's own pair:** U+27EE ⟮ and U+27EF ⟯, Unicode's mathematical flattened parentheses, drawn
   in font5x7.txt as the face's own:

   ```
   U+0028 (   U+27EE ⟮   U+27EF ⟯   U+0029 )
   ...#.      ....#      #....      .#...
   ..#..      ...#.      .#...      ..#..
   .#...      ...#.      .#...      ...#.
   .#...      ...#.      .#...      ...#.
   .#...      ...#.      .#...      ...#.
   ..#..      ...#.      .#...      ..#..
   ...#.      ....#      #....      .#...
   ```

   - **Tighter:** each sits against the letter, ⟮ in its cell's two right columns and ⟯ in its two left. Counted in
     blank columns between it and an I or i inside it (the cell's 5 columns and the gap after it):

     | | At its ends | At its middle |
     |---|---|---|
     | The plain pair | 3 | 5 |
     | The token's pair | 2 | 3 |
   - **Of their own:** a straight stroke with short hooks, where the plain pair is a curve. Neither of the pair shares a
     dot with the plain parenthesis on its side, so each differs from it in 14 dots: its own 7 and the plain one's 7.
2. **cells_of draws ⒤ as ⟮, its letter, ⟯** (and ⒥ as ⟮ J ⟯). It's the one decoder every decode site uses (104), so
   every place the screen draws a token changes at once: typing, the list, the wheel and its cut, ALG's and the CAS
   rows, the textbook's text, the stand-in's own decoder (screen_decode).
   - **Still the same:**
     - three cells, the token's later two flagged (TOK_MORE);
     - the case by the text's model (104's R1 and R2);
     - a cut never splits a token (R3);
     - the cursor crosses a token in one step.
3. **A program line draws its equation's own token.**
   - Today the screen draws list.c's text for a program line, and since 104 that text already writes the token as the
     ASCII `(I)`. So the screen can't know a token was there: on the prototype without this item, both program-line
     pairs still differ by 0 pixels.
   - **The screen draws an equation line from its equation:** the address, a space, the equation through eqn_line,
     then the line's mark (ab_eqn_mark, unit 031), as list.c builds the same text.
   - **The case is the line's label's model,** as list.c's rule: (I) under a 35s label, (i) under a 33s or STU one.
     That rule moves out of ab_prog_line into a new core function, `bool ab_prog_m35(const ab_calc *c, int i)`, and
     ab_prog_line calls it. The listing's text doesn't change.
   - An equation line whose pool index is out of range keeps the listing's `?`, as today.
4. **The texts stay ASCII:**
   - **What's unchanged:**
     - screen_page's lines (%XLINE);
     - screen_drawn (drawn=, %XDRAWN);
     - the wheel's text;
     - list.c's listing.
   - **How:** line_piece writes `(` and `)` for the pair's two cells, as it writes the letter, so the text is 104's
     exactly.
   - **The text alone never told the token from typed (i),** and it still won't (104's A01 and A02). The look does.
   - h104 and every %XLINE and %XDRAWN line of 104 hold.
5. **%XDRAW writes the pair:** it's the cells drawn, so it reads `4:⟮I⟯_`. 104's %XDRAW lines that hold a token move.
   The census below lists them.
6. **Only the equation tokens change** (ruling R3 below). These keep the plain pair:
   - every op that takes (I) or (J) as its operand, as a program line (`A002 STO (I)`, more.txt's XO: %XDRAW
     `4:A002_STO_(I)` on the prototype);
   - the operand's prompt while it's typed.

   None of them is in an equation, and nothing typed in an operand can look like them.
7. **fit_watch goes** (abacus #17631, carried from 104: "at the next touch of screen.c"). It was always compose's X
   line, so draw_line_lead compares with `&comp.x` itself. That saves 4 B of internal RAM.
8. **glyph_census learns the pair.** EXPANDED's characters for ⒤ and ⒥ become ⟮, the letter, ⟯, found in cells_of's
   body as the code points 0x27EE and 0x27EF, the same way as now.

## The oracle

**%BOXES can't express it.** It counts the hollow boxes in one frame, and the ask is a relation between two frames.

**Proposed: `%LOOK=ID` and `%LOOK=!ID`** (tests/vectors.c, under `#ifndef AB_DEVICE` like %BOXES and %XDRAW):
- **`%LOOK=ID`:** the frame screen_render draws after this vector's keys (the default ui) is pixel for pixel the frame
  after vector ID's keys. ID is an earlier vector of the same file.
- **`%LOOK=!ID`:** the frames differ in at least one pixel.
- **`got`** is `ID` or `!ID`, as the frames are. A missing or later ID reads `(no_earlier_vector_ID)` and fails.
- **The frames are kept only in a file that holds a %LOOK:** a scan of the file before its first vector says so. In
  that file, every vector's frame is drawn once after its keys and kept by its ID, so no other file pays for a render.
- **The whole frame, not the X line alone,** so nothing on the screen can stand in for the token. The controls in
  look.txt show that, on today's tree, the frame after typed parentheses is the token's to the pixel. The rest of the
  screen doesn't tell the two apart, so a difference can only be the pair.
- The prototype's AB_LOOKPX=1 prints each comparison's count of differing pixels. That's a measurement aid; the
  directive doesn't carry it.

**Measured on the prototype (look.txt, look-proto.out):**

| Path | Pixels that differ | Why |
|---|---|---|
| typing (35 (I), 35 (J), 33s, STU) and the list | 448 | 28 dots at scale 4 (16 px each) |
| program lines (35, 33s) | 448 | needs contract 3: 0 without it (look-proto-noPL.out) |
| STU's list row | 252 | 28 dots at scale 3 (9 px) |
| the controls: the same keys, then ◀ ▶ | 0 | |
| a different equation | 688 | |

**The fonts' half:**
- **%BOXES=0** on every line that draws a token.
- **glyph_census** with the pair found in cells_of.
- **A screen_test check on the face itself:**
  - U+27EE and U+27EF share no lit dot with `(` and `)`;
  - each is lit in the column against the letter (⟮'s column 4, ⟯'s column 0), which no plain parenthesis is.
  - So the pair is tighter and of its own by construction, not only in the frames that happen to be tested.

## Frames: the token beside typed (i), in each mode

Each frame is one equation, with the token first and then the same letter typed in the user's parentheses. Each has a
today and a prototype version: work/token-parentheses-evidence/frames/today/ and …/frames/proto/, 400 × 240, 1 bit.
dots.txt has each X line as dots. beside.txt reads each one's text with `%XLINE=?` (or `%XDRAWN=?`), which "fails" to
print it: today's and the prototype's are the same text (beside-today.out, beside-proto.out), and every today frame
is byte-identical to the prototype's with its plain switch.

| Frame | Mode, keys | The X line, the prototype |
|---|---|---|
| B35 | 35, `RCL:(I) + () RCL:I >` | `⟮I⟯+(I)_`, scale 4 |
| BJ | 35, `RCL:(J) + () RCL:J >` | `⟮J⟯+(J)_` |
| B33 | 33s, `RCL:(i) + () RCL:i >` | `⟮i⟯+(i)_` |
| BST | STU, `RCL:(I) + () RCL:i >` | `⟮i⟯+(i)_` |
| BP | 35, the program line `A002 ⟮I⟯+(I)` | scale 4 |
| BA | STU's list row, `EQ:⒤+(i)` | scale 3 |
| BW | 35, nine tokens: the scale-2 window | `…+⟮I⟯+⟮I⟯+…` |

B35 (35, `(I)` then I typed), one dot per drawn dot:

```
today:
..#...###...#.............#...###...#
.#.....#.....#.....#.....#.....#.....#
#......#......#....#....#......#......#
#......#......#..#####..#......#......#
#......#......#....#....#......#......#
.#.....#.....#.....#.....#.....#.....#
..#...###...#.............#...###...#....#####
the prototype:
.#..###..#..............#...###...#
#....#....#......#.....#.....#.....#
#....#....#......#....#......#......#
#....#....#....#####..#......#......#
#....#....#......#....#......#......#
#....#....#......#.....#.....#.....#
.#..###..#..............#...###...#....#####
```

## The census: what moves (make -i test on the prototype, census.txt)

**22,491/22,501 vectors and 61,808/61,818 expectations,** summing every `n/m vectors` and `n/m expectations`, the
same rule as the repin's record. Every per-file count but one matches the repin's make test.
- **104's %XDRAW lines, 5 of them, each in the plain run and the sanitizer run (the 10):**
  - E01 `4:(I)_`;
  - E02 `4:A+(I)×2_`;
  - W01, W02, W03, the windows.

  Each now reads ⟮I⟯ where it read (I) (contract 5). They'll be amended in 104's file, and each is a line in the
  message to abacus.
- **Unchanged:**
  - every %XLINE and %XDRAWN line;
  - h104, 11/11;
  - the listing's lines (104's P01-P05).
- **screen_test:**
  - four frames, each holding a token, differ as they should: 69 (STU's `(i)+(J)×2`), 71 (35's window), 72
    (`A002 (I)+1`) and 73 (the textbook's text);
  - the other 69 frames match.
  - **Checks 9,671 → 9,940, +269:** the face's per-glyph and pairwise checks at 132 glyphs, 4·2 + 131 + 130. That's
    104's −269 exactly, the other way.
- **The board's replay** (standin_replay): glyph103.txt's ⒤ frame (CCA1527E → 0520A61E on the prototype) and
  glyph104.txt's `A+(I)×2` (9A392C27 → 26FD27DB). Both re-pinned at src to the host's CRCs, and run on the stand-in.
- **glyph_census passes on the prototype, by accident:** the prototype's cells_of still has `'('` in its body (its
  plain switch). The real src has no `'('` there, so the census must learn the pair (contract 8).
- **Everything else as at 8c4fad7:** tbook_test 39/39, app_test 919/919, steps_test 0 bad, and every other driver.

## Tests (after the rulings, written before src)

- **tests/105-soroban.txt,** from look.txt:
  - **T and U lines:** each path's token, then typed parentheses, with `%LOOK=!T…`:
    - typing under 35 (I) and (J), 33s and STU;
    - the list;
    - program lines under 35s, 33s and STU labels;
    - STU's list.
  - **Not the scale-2 window:** nine typed `(I)` differ from nine tokens already today (more-today.out, 1,180 px). The
    window drops a token it would cut (104's R3), but it cuts typed parentheses as it cuts any cells, `…)+(I)…`. So
    the window's pair would pass on today's tree and can't judge the unit. The window keeps its %XDRAW line.
  - Each T line has %BOXES=0, and each pair carries the same %XLINE or %XDRAWN text on both lines (contract 4).
  - **%XDRAW lines,** each read on the prototype (more-proto.out): `4:⟮J⟯_`, 33s and STU `4:⟮i⟯_`, and the window's
    `2:…+⟮I⟯+…+⟮I⟯_`.
  - **Scope (R3):** a program line `STO (I)` keeps the plain pair: `%XDRAW` reads `A002_STO_(I)`.
  - **Controls:**
    - **K1-K2:** the same frame after ◀ ▶, `%LOOK=ID`.
    - **K3:** another equation, `%LOOK=!ID`.
  - **On 8c4fad7 with the directive alone,** every U line fails and every K line passes (look-today.out: 11/19, the 8
    U lines red). That's the run that shows the file can fail.
- **refuse.txt's kind:** a later or missing ID fails with `(no_earlier_vector_ID)` (refuse.out, 0/3 as wanted). It's
  kept as evidence, not in make test, since a file whose every line must fail isn't a make test step.
- **104's file:** E01, E02 and W01-W03's %XDRAW amended to the pair, the five lines named to abacus.
- **screen_test:**
  - frames 69, 71, 72 and 73 re-pinned, each looked at before it's kept;
  - **the face check:** U+27EE and U+27EF share no dot with `(` and `)`, and are lit in the column against the letter;
  - **the program line's text:** screen_page's X text for an equation line holding a token is the address, a space
    and ab_prog_line's text, byte for byte, so the screen's line (contract 3) can't drift from the listing.
- **glyph_census:** the pair, found in cells_of's body.
- **The board:** glyph103 and glyph104's frames re-pinned to the host's CRCs at src, run on the stand-in under the
  board claim.
- **Mutants,** each predicted before the run:
  - **M0:** the control.
  - **M1:** cells_of's pair back to `(` `)`. Every U line red, the %XDRAW lines red, the four frames red.
  - **M2:** the program line through list.c's text again. Only the program-line U lines red.
  - **M3:** the pair drawn with the plain parentheses' rows in font5x7.txt. Every U line red and the face check red,
    while %XDRAW stays green: the pixels catch what the text can't.
  - **M4:** U+27EF removed from the face. %BOXES red on every T line, glyph_census red.
  - **M5:** line_piece writing the pair's code points into the text. %XLINE lines red, h104 red.
  - **M6:** ab_prog_m35 always 35. The 33s and STU program lines read (I): 104's P02 and P03 red.
  - **M7:** fit_watch's replacement watching the wrong line. %XDRAW red.
- **D68, predicted:**
  - **Flash:** up by the two glyph entries, 12 B each (font_glyph is a uint32_t and 7 row bytes, padded to 12), and
    the program line's path and ab_prog_m35: about +100 to +250 B in all.
  - **DIRAM −4** (fit_watch), IRAM +0, calc_task's frame +0.
- **h105:** abacus's, frozen against this blueprint before src.

## Rulings asked of abacus

- **R1, the pair's drawing:** as contract 1 and the frames. The look is Michael's, through T136, and abacus reads the
  frames before it accepts. A redraw changes only font5x7.txt's two blocks and the pixel counts; the directive and
  the tests hold for any pair that keeps the face check (no dot shared with the plain pair, the column against the
  letter lit).
- **R2, the texts stay ASCII** (contract 4). My recommendation: yes.
  - It keeps h104, 104's text lines, the listing and the console's drawn= as they are.
  - The difference Michael chose is a look, and the pixel directive judges a look.
  - **The other way:** the texts carry ⟮ ⟯. Then %XLINE would tell the two apart too, but every text line holding a token
    would move (h104's H01-H09 among them), and each reader of the texts (the console, tally's phone if it reads text)
    would meet two new code points.
- **R3, only the equation tokens** (contract 6). The operand forms keep the plain pair. My recommendation: yes, since
  there's no likeness to break there.
- **R4, %LOOK as the directive** (the oracle above).
- **R5, the program line through ab_prog_m35** (contract 3). It's a new core function, additive; list.c's listing text
  is unchanged.
