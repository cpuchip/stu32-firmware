# Unit 106 blueprint: each line's drawn text

**From:** abacus #17953, a follow-on to 105 inside Michael's T136.
- **The problem:** tally's app and primer's web calc draw T, Z, Y and X from screen_line's ASCII text in their own
  font, so 105's pair never reaches them. On the phone and the web, a 35s token and a typed (I) still look the same
  (tally's int105-35s-gap shot). Both hosts will bundle a font subset holding U+27EE and U+27EF, so they need the
  code points.
- **What abacus asked:**
  - each screen_line carries a second string beside the ASCII one: the cells as drawn;
  - R2 holds: the text, %XLINE, drawn=, the listing and the console stay ASCII;
  - answer three things here:
    - where the string lives and what it costs, the device included;
    - how it stays in step with cells_of;
    - the directive that tests it.
  - Then h106, before src.
- **VER 0.107.**

Every measurement here was taken on a throwaway prototype at 5742f06 (work/drawn-text-evidence/prototype.diff).

## The contract

1. **`char drawn[SCREEN_TEXT_MAX]` in screen_line, beside text** (firmware/screen.h). It holds the line's cells as the
   screen draws them, UTF-8, the cursor's `_` where text has it. It's the whole line, not the fitted window, as text
   is: fitting is the host's (screen.h's rule since abacus #3971).
2. **It's written from the cells, by the function that already writes cells as text.** page_line, which fills each
   screen_line from compose's line, runs cell_u8 over the line's own cells (`l->cp[0..n)`) into drawn.
   - cell_u8 is what %XDRAW has written since 104. The cells are cells_of's output (or line_add_cp's).
   - **So there's no second mapping:** the drawn string is the cells, byte for byte. A change to cells_of or the face
     changes it at once, and %XDRAW with it.
   - A line longer than SCREEN_TEXT_MAX stops at a whole cell, as text stops at a whole piece.
3. **text is unchanged** (R2), and so is everything that reads it: %XLINE, drawn= (%XDRAWN), the listing, the console.
4. **Where drawn differs from text** (the census below; nowhere else in the suite):
   - **a token's pair:** `⟮I⟯` where text has `(I)`;
   - **ŷ and ȳ:** typed precomposed (U+0177, U+0233), drawn as y with the face's combining circumflex or macron (103's
     next_cell). drawn writes them as the cell is, y then U+0302 or U+0304. text keeps the typed U+0177 and U+0233.

     The same character, decomposed. Ruling R2 below asks whether that stands.
   - x̂ is typed as x and U+0302 already, so its text and drawn are the same bytes.

## What it costs

- **screen_line grows by 1,024 B,** so screen_page grows by 4,096 B: 8,216 → 12,312 B on the host (sz.c; sz.out,
  5742f06 then the prototype).
- **The device pays no static RAM:**
  - Nothing on the device holds a screen_page in static RAM, and none on a stack.
  - The one device user is dev_bench (device/dev_bench.h:296), which mallocs it from PSRAM. That malloc grows by
    4,096 B in PSRAM.
  - compose's static lines (comp, internal RAM) don't change: drawn is built from cells they already hold.
  - The screen_render path never calls page_line.
- **Code:** page_line gains a loop. That's tens of bytes of flash, on both boards.
- **D68, predicted:** flash +40 to +120 B, DIRAM +0, IRAM +0, calc_task's frame +0.
- **The hosts** (tally's lib, primer's calc.c) allocate screen_page themselves, so each grows by 4 KB. abacus #17953
  says both read lines from screen_lines; neither needs a change to keep working, since text is where it was.

## The directive: `%XCELLS`

- **What it reads:** X's drawn string, as %XLINE reads X's text. Blanks are written `_` (spaces_out), and an empty
  line is `_`, so a cursor and a blank both read `_`, as in %XLINE.
- **It sits in %XLINE's branch** of tests/vectors.c (the screen_lines group, `#ifndef AB_DEVICE`).
- **It can hold "⟮I⟯+(I)_" against %XLINE's "(I)+(I)_",** as h106 wants (C01, measured).
- **A second reader of the same cells:** on a line that fits unwindowed, %XCELLS is %XDRAW's text after its `s:`.
  Lines in 106's file pair the two, so drawn can't drift from what the screen fits and draws.

**Measured on the prototype** (measure.txt, measure.out):

| | Keys | %XLINE | %XCELLS | %XDRAW |
|---|---|---|---|---|
| C01 | 35, `RCL:(I) + () RCL:I >` | `(I)+(I)_` | `⟮I⟯+(I)_` | `4:⟮I⟯+(I)_` |
| C02 | 33s, the same with i | `(i)+(i)_` | `⟮i⟯+(i)_` | `4:⟮i⟯+(i)_` |
| C03 | STU | `(i)+(i)_` | `⟮i⟯+(i)_` | `4:⟮i⟯+(i)_` |
| C04 | 35, `(J)` with the cursor inside the line | `(J)_×2` | `⟮J⟯_×2` | `4:⟮J⟯_×2` |
| C05 | a program line | `A002_(I)+(I)` | `A002_⟮I⟯+(I)` | `4:A002_⟮I⟯+(I)` |
| C06 | `STO (I)` (R3, plain) | `A002_STO_(I)` | `A002_STO_(I)` | |
| C07 | the list | `(I)+1` | `⟮I⟯+1` | |
| C08 | STU's area (drawn= stays `(i)+1`) | `(i)+1` | `⟮i⟯+1` | |
| C09 | `ŷ(2)+ȳ` | U+0177 … U+0233 | y U+0302 … y U+0304 | |
| C10 | `x̂+1` | x U+0302 +1 | the same bytes | |
| C11 | nine tokens | `(I)+…+(I)_` whole | `⟮I⟯+…+⟮I⟯_` whole | `2:…+⟮I⟯+…` (the window) |
| C12, C13 | a number, an empty list | `12.5000`, `EQN_LIST_TOP` | the same | |

## The census (census.py, census.txt)

- **What it read:** every vector file make test names (114, the three fuzz files from the main tree's last make test),
  9,829 vectors. After each vector's keys, screen_lines ran, and every T, Z, Y and X line whose drawn string isn't its
  text was printed.
- **The result: 106 line reads.** A vector that also reads %XLINE has its lines read twice (h103's H04 shows twice), so
  that's reads, not distinct lines.
  - 103 are a token's pair, among them 015b's B17, a token in a vector's text.
  - 3 are ŷ and ȳ: 103's B05, h103's H06 and H07.
  - Nothing else.
- **Its limit:** it reads each vector's last screen, not every key's.
- **No existing expectation moves:** text is untouched, and nothing reads drawn yet.
  - make -i test on the prototype (make-i-test.txt): 22,601/22,601 vectors and 62,034/62,034 expectations, 5742f06's
    totals, with no ignored error.
  - screen_test 9,943/9,943 checks and 73/73 frames, app_test 919/919.

## Tests (after the rulings, written before src)

- **tests/106-soroban.txt:**
  - the table's rows with `%XLINE` and `%XCELLS`, and `%XDRAW` where the line fits, so drawn and the fitted cells
    agree;
  - the cursor's place;
  - the program line, the list, STU's area;
  - ŷ, as R2 rules;
  - a control: a line with no token, the same both ways.
- **screen_test** (the line's cells are private to screen.c, so the check uses what's public):
  - on pages with a token, ŷ, x̂ and plain values in T, Z, Y and X, each line's drawn has as many cells as its text
    (screen_cells on both);
  - drawn and text agree byte for byte away from the pair and the marks;
  - and screen_page's size, so a later field that doubles it shows.
- **On 5742f06 the file can't parse %XCELLS:** it's a bad expectation there (exit 2). The prototype without its
  page_line loop is the run that must fail: drawn empty, every %XCELLS line red.
- **Mutants,** predicted before the run:
  - M0, the control;
  - M1, drawn copied from text (the second mapping this contract rules out): every pair line red;
  - M2, drawn cut at the cursor's cell;
  - M3, page_line's loop over the fitted cells (the window) in place of the whole line: C11's kind red.
- **h106:** abacus's, frozen against this blueprint before src.

## Rulings asked of abacus

- **R1, the field:** `drawn[SCREEN_TEXT_MAX]` in screen_line, the whole line, from page_line through cell_u8.
- **R2, ŷ and ȳ:** drawn writes them as the cell is (y and the combining mark), not the typed precomposed character.
  - **My recommendation: as the cell is.** The rule "drawn is the cells" then has no exception, and the hosts already
    meet U+0302 in x̂'s text.
  - **The other way** needs a reverse map in page_line, cell back to the precomposed code point. That's the second
    mapping #17953 asked me to avoid.
- **R3, %XCELLS** as the directive, X only. Y, Z and T go through the same page_line, and screen_test's check covers
  all four.
- **R4, the page rows:** the worksheet's page[].left and the area's drawn= stay as they are. That's outside #17953's
  T/Z/Y/X.
  - In ALG, `RCL (I)` is refused (alg.txt: 2 errors), so I found no key path that puts a token on a page row. That's
    a probe, not a proof.
  - If abacus knows a path, the page rows would want the same field.
