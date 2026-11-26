# Unit 104 blueprint: ⒤ and ⒥ drawn as three cells

**From:** abacus #17595, which relays Michael's T120 decision, "Three cells". His note: "Ooh 1 cell is too tight and it
looks goofy. Lets do 3 xells."
- **What abacus asked:**
  - measure the widths and cursor cells: the equation line, the cursor over each token, editing across them, 33s and
    35s;
  - say whether 103's one-cell glyphs are retired or kept as fallbacks;
  - keep %BOXES at 0;
  - h104 the usual way.
- **tally redraws nothing:** the phone takes these from the frame or the text.
- **VER 0.105.**

Every measurement here was taken at dc0cdeb ("today") and on a throwaway prototype of the contract below. The
prototype's diff, vector files, outputs and frames are in work/indirect-three-cells-evidence/.

## What doesn't change: the core's token

The core keeps ⒤ (U+24A4) and ⒥ (U+24A5) as one code point each, one token, in every equation, the image and the
parser. Unit 104 changes only how the screen draws them, and the program listing's text.
- **The editor already steps by code point** (calc.c's cp_prev and cp_next), so one ◀ or ▶ crosses a whole token and
  backspace removes it whole. The cursor can never stand inside one.
- **The ALG words** (101) still say "indirect I".

## The contract

1. **Three cells.** Wherever the screen turns text into cells, ⒤ becomes `(`, its letter, `)`, and ⒥ becomes `(` `J`
   `)`.
   - **There are six decode sites, all in screen.c:**
     - screen_cells;
     - screen_decode (the stand-in's own text, STU's pages);
     - screen_text;
     - line_add (the X/Y/Z/T lines, the program line);
     - eqn_line (typing, the list, ALG's rows, the CAS rows);
     - the wheel's cut (screen_wheel_text).
   - All six share one decoder, so a site can't draw the token differently from another.
2. **One counter.** The textbook (firmware/tbook.c) measures its atoms and wraps its text-fallback lines by its own
   code-point count. It will count with the screen's decoder instead, through a new `screen_cells_n(s, len)`.
   Otherwise a token measures as one cell and draws as three.
3. **The case follows the model the text belongs to, as the listing's (I) already does** (core/list.c since 025 and
   099):
   - **35:** `(I)`;
   - **33 and STU:** `(i)`;
   - **⒥:** `(J)` always. ⒥ only works under 35: the 33s refuses it as INVALID (i) (DOMAIN, calc.c's resolve), STU
     refuses it with NO REGISTER (099), and STU's (J) key types (i).
   - **The guides:** the 35s guide writes (I)/(J) (p.14-21 to 14-23), and the 33s guide writes (i) (p.13-21 to 13-24).
   - **By path:**

     | Path | The model |
     |---|---|
     | Typing | ab_rec_model: the label's stamp in a program, else the mode |
     | A list entry, and the wheel's text | the entry's stamp (eqn_model; 0 counts as 35) |
     | A program line | its label's stamp (list.c's line_m35) |
     | ALG's rows, STU's pages, the textbook | STU, so `(i)` |
4. **The listing's equation line** (core/list.c, ab_prog_line for AB_EQLINE) writes the token as its other lines write
   (I).
   - Measured on the prototype: `A002 (I)+1`, beside `A002 STO (I)` under a 35s label. Under 33s and STU it's
     `A002 (i)+1`.
   - The rewrite keeps the length: the token is 3 bytes in UTF-8 and 3 in ASCII.
5. **The text the screen exports matches its cells.**
   - **screen_page:** the text carries the three ASCII characters, and its cursor is counted in cells, as screen.h
     already says.
   - **screen_drawn** (the console's drawn=, the %XDRAWN directive): today it writes the core's `⒤`. It will write the
     same three characters, by the same case rule, so no exported text carries a glyph the face no longer has. The
     prototype didn't do this yet; its D01-D03 still read `⒤`.
6. **The window never shows part of a token (ruling R3).** At scale 2, a cut falling inside a token leaves that token
   out whole on that side, and the ellipsis stands next to the remaining cells.
   - Today's window would show `…)+(I)+…`, a stray `)` after the ellipsis (frame P2).
   - The ellipsis takes the cut's place as before. The cells of a token it would split are dropped, so the line draws
     one or two cells short.
   - The cursor is never inside a token, so it stays in view.
   - Function names (`SIN(`) are cut as they are today; this unit leaves them alone.
7. **The one-cell glyphs are retired** (U+24A4 and U+24A5 leave font5x7.txt):
   - Nothing draws them once the decoder expands the tokens.
   - A kept glyph would be dead flash, and the drawn census couldn't tell it apart from the three cells.
8. **glyph_census learns the expansion.** It finds ⒤ and ⒥ as mapped by reading the decoder's own body, as it reads
   next_cell's today. %BOXES stays 0.

## Measured (the evidence folder; X lines as %XLINE prints them, `_` the cursor)

| | Keys | Today | Prototype | Under the contract |
|---|---|---|---|---|
| T02 35s | `A + (I) × 2` | `A+⒤×2_` 6 cells, scale 4 | `A+(i)×2_` 8, scale 4 | `A+(I)×2_` |
| T03 | ◀ | `A+⒤×_2`: cursor cell 4 | cell 6 | `A+(I)×_2` |
| T04 | ◀◀ | `A+⒤_×2`: cell 3 | cell 5 | `A+(I)_×2` |
| T05 | ◀◀◀: one key crosses the token | `A+_⒤×2`: cell 2 | cell 2 | `A+_(I)×2` |
| T06 | ◀◀◀ ▶ | cell 3 | cell 5 | `A+(I)_×2` |
| T07 | ◀◀ ⌫: removes the token, 3 cells | `A+_×2` | `A+_×2` | `A+_×2` |
| T08 | ◀◀◀ ⌫ | `A_⒤×2` | `A_(i)×2` | `A_(I)×2` |
| T09 35s | `(J) + 1` | `⒥+1_` | `(J)+1_` | `(J)+1_` |
| T11/T12 33s | `(i) ^ 2`, typed and stored | `⒤^2_`, `⒤^2` | `(i)^2_`, `(i)^2` | the same |
| T13 STU | `(I) + (J) × 2` | `⒤+⒥×2_` | `(i)+(J)×2_` | the same |
| T15 35s | 5 tokens joined by + | 10 cells, scale 4 | 20 cells, **scale 3** | the same |
| T16 35s | 9 tokens | 18 cells, scale 3 | 36 cells, **scale 2, window** `…)+(i)+…` | `…+(I)+(I)+(I)+(I)+(I)+(I)+(I)_` (30 cells) |
| L01 / L02 / L03 | an equation line in a program, under a 35s, 33s or STU label | `A002 ⒤+1` | `(I)`, `(i)`, `(i)` | the same |
| L05 35s | typed in a program, before ENTER | `⒤+1_` | `(i)+1_` | `(I)+1_` |

- **Widths:** each token is 2 cells wider. The X line is scale 4 up to 15 cells, 3 up to 21 and 2 past that, with a
  31-cell window (NUM_X 20 to NUM_R 398).
- **Five tokens in an equation** move it from scale 4 to 3, and nine from 3 to 2.

## Two things that hold, and one observation

- **Coverage of what moves:** on the prototype, make test fails only where it should:
  - frame 69, which is the point of the unit;
  - glyph103's ⒤ frame on the replay, AC98D863 → CCA1527E;
  - glyph_census, which didn't know the expansion yet.

  Nothing else in the suite draws or lists a token as text. So 104's own vectors carry the unit, and the program
  listing's change moved no vector at all.
- **The drawn census, with the glyphs retired:** 132 runs, 0 boxes.
  - At 103, removing ⒤'s glyph alone drew 17 boxes (mutant M2), so the census reaches those frames, and every one now
    goes through the three cells.
- **screen_test runs 269 fewer checks** (9,935 → 9,666). That's the face's per-glyph and pairwise checks at 130 glyphs
  instead of 132: 4·2 + 131 + 130 = 269, exactly.
- **An observation, outside this unit:** the textbook's parser doesn't take ⒤ or ⒥. So an STU equation holding a token
  is always shown as its linear text, never in 2D (frame P4, today and the prototype alike).
  - 104 keeps that.
  - A token in 2D would be a unit of its own if wanted.
- **Ruled out:** x̂'s two code points counted as two cells by the textbook. Its frame is byte-identical on both trees
  (P1), so there's no gap to fix.

## A tension for Michael (ruling R4)

The 35s guide's p.14-23 tells two things apart by their look: the (I) key's token, the variable I indirectly, and I
typed inside the user's own parentheses, which is the variable I itself. The guide draws them in different forms.
- **Three plain cells make them identical.** Frame P3 shows the token beside a typed `(I`, and L06's `(I_)` against
  T01's `(I)_`.
- **What still tells them apart:**
  - the cursor (one ◀ crosses the token, three cross the typed form);
  - ALG's words ("indirect I").
  - The listing doesn't.
- **(a) As abacus relayed it:** plain `(`, `)`.
- **(b) The token's own parentheses:** two new glyphs, a tighter pair of the face's own drawing, at the token's ends,
  still three cells. The guide's distinction survives. It's a few bytes and two glyphs, and the cells and cursor are
  as in (a).

The look is Michael's. I'd build (a) unless he picks (b), and (b) would be a small follow-on.

## Rulings (abacus #17600, on fd70092)

- **R1:** the case by the text's model, as contract 3. abacus's "(i)" in #17595 was shorthand; the listing's
  convention wins.
- **R2:** `(J)` upper, under 35.
- **R3:** yes. A window cut never splits a token; it drops it whole (contract 6).
- **R4:** to Michael as card T136, with P3 attached and abacus's recommendation of (b). **Build (a)** so the unit isn't
  held; (b) is the small follow-on if he picks it.
  - **A correction to the reason abacus gave, measured after the ruling** (case.txt, C01-C03): under (a), the
    token and the typed form look identical in **every** mode, not only 35s.
    - In 35s, the token reads `(I)`, and the variable I typed in the user's parentheses reads `(I)`.
    - In STU and 33s, the index variable is itself the lower-case i (025, 099). So i typed in parentheses reads `(i)`
      (C02, C03), the same as the token's `(i)` (C01). Case doesn't keep them apart.
- **R5:** retired.
- **A known limit, kept:** the textbook's parser refuses ⒤, so an STU equation holding a token shows linear (P4, as
  today).
- **%XDRAW and the pure fit function** are agreed as the test seam.

## Rulings asked of abacus (as sent, #17599)

- **R1:** the case by model (contract 3), or `(i)`/`(j)` in every mode as #17595 words it. My recommendation is R1 as
  written: it is the listing's rule since 025 and 099, and the guides'.
- **R2:** `(J)` upper case. ⒥ exists only under 35, where the guide and the listing write (J).
- **R3:** the window drops a token it would split (contract 6).
- **R4:** the tension above, to Michael.
- **R5:** the glyphs retired (contract 7).

## Tests (after the rulings, written before src)

- **tests/104-soroban.txt:** the table's rows as vectors under the contract, with %XLINE, %BOXES=0, and a new
  `%XDRAW=s:cells`.
  - %XDRAW is the X line as screen_render fits it: its scale, and the cells drawn after the window and the ellipsis.
  - It comes from a pure fit function split out of draw_line_lead, which draws from that function's result.
  - It's under `#ifndef AB_DEVICE`, as %XDRAWN is. It carries T15's scale 3 and T16's window.
  - The rows also cover:
    - each path's case: typing under 35, 33 and STU; a 35-stamped entry in STU's wheel; a program line under each
      label;
    - %XDRAWN's three characters.
  - Every row's expectation is read against the prototype before src, and the "today" column shows each row's
    before.
- **screen_test:**
  - frame 69 re-pinned, looked at before it's kept;
  - new frames for 35's window (T16) and a 35 program line;
  - a check that screen_page's cursor cell is the '_''s cell in its text, at T03-T06.
- **glyph_census:** U+24A4 and U+24A5 found mapped, in the decoder's body.
- **The board:** device/standin/glyph103.txt's ⒤ frame re-pinned to the host's new CRC. A 35s frame (`A+(I)×2`) goes
  in a new script, glyph104.txt, keys checked on the host first.
- **Mutants,** each predicted before the run:
  - M0, the control;
  - M1, the decoder's expansion removed, so the tokens draw boxes;
  - M2, the case always lower;
  - M3, always upper;
  - M4, the textbook counting code points again;
  - M5, the window splitting tokens;
  - M6, list.c's rewrite removed;
  - M7, screen_drawn's rewrite removed.
- **D68, predicted:** flash down by the two glyphs' entries and up by the decoder and the fit split, a net of tens of
  bytes. RAM +0, and calc_task's frame +0.
- **h104:** abacus's, frozen before src.
