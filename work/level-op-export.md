# The levels' op names in screen_page (blueprint)

Asked by abacus (#14703), for the web page's STU frame (page-levels B, primer #14699). On STU's home the frame draws each
level's op name beside its letter (unit 090's p2: "Y +", "X √x"; abacus's cases: "XEQ T" by X at fn-02 T02, "×" by Y at
poly-05 E02B), but screen_page doesn't carry it, so the page's screen-reader label can't say it. This export adds it.
It is a view only: nothing new is computed and nothing is saved.

## What draws it now (8234192)

`level_op` (firmware/screen.c:1548) draws a level's op at (NUM_X + 2, the level's bottom − 14), scale 2, when all of these
hold:
- the level's own record is on the page (`lvl_at[i]` isn't UINT32_MAX) and parses;
- its op isn't a typed number (AB_ROW_OP_NUM) or an ALG line (AB_ROW_OP_ALG), and `ab_op_text` spells it (> 0 bytes, into
  32);
- the value, at the largest scale that fits (from 2 for Y, Z and T, from 4 for X), leaves the op's span plus 12 pixels
  between NUM_X + 2 and the value.

`draw_page` (screen.c:1582) calls it for Y, Z and T when they're drawn and Y isn't a prompt. It calls it for X only when
X's line is drawn as text (no 2D expression, `xe_top` 0) with its letter and as a value (`x_letter`, SCREEN_VALUE); there
the value's cells count the ≈ (096). In ALG no op is drawn.

## The change

1. **One source for the decision and the text.** `level_op` splits in two:
   - `static bool level_op_text(const ab_calc *c, const composed *k, int i, char *t, int tsz)` holds every condition
     above, including draw_page's (which levels, the prompt, X's three, ALG), and the cells each line was drawn with
     (X: `x.n` plus the mark's cell, as `draw_xline` returns);
   - the drawing, where `draw_page` draws what it returns, unchanged in place.
   - The frame's pixels don't change. screen_test's frame hashes (frame 67 and the rest) are the check that they don't.
2. **The field.** screen_page gains, after `levelmark`:
   `char levelop[4][32];   /* X, Y, Z, T: the op name drawn beside the level, UTF-8, the frame's own bytes; "" none */`
   - `screen_lines` fills it from `level_op_text` when `page_on`, and leaves "" off the home and in ALG.
   - Its order and shape are `levelmark`'s. 32 is `level_op`'s own buffer, so no op is cut.
   - It's +128 bytes per screen_page. The device's only screen_page is dev_bench's (PSRAM). The app and comp don't hold
     one, so internal RAM is +0, which D68 measures at the build.
3. **No state version.** screen_page is not in the image, and no image tag or version changes. The row record already holds
   the op; this only reads it.

## The test

1. **Pixels, judged apart from `level_op_text` (tests/screen_test.c).**
   - For each state in a corpus, render the frame with `screen_render` (what the page's calc_render draws) and read
     `screen_lines`.
   - For each level whose `levelop` is non-empty, the rectangle at (NUM_X + 2, its bottom − 14) of the text's span × 14
     must equal, pixel for pixel, the text alone drawn by `screen_text` on a blank frame. The column after it must be
     blank.
   - For each drawn level whose `levelop` is "", the first two cells' rectangle there must be blank. That catches an op
     drawn but not exported. The corpus holds only states where the value leaves that room.
   - The test needs each level's bottom. It takes these from a test-only reader beside `screen_page_window`:
     `void screen_level_bottoms(const ab_calc *c, int bot[4])`, -1 for a level not drawn. That reader is the one thing
     added beside the field.
   - The corpus:
     - each of X, Y, Z and T with an op (√x, +, ×, XEQ T);
     - X's ≈ with an op;
     - a typed number (none);
     - Y as a prompt (none);
     - X drawn as an expression in 2D (none);
     - a value too long to leave room (none, judged by the first cell only where the value can reach it);
     - ALG (none, `levelop` all "");
     - the 33s and 35s modes (page off, all "").
2. **Vectors (tests/vectors.c), a grammar for abacus's check.** `%LEVELOP-k=TEXT`, with k one of X Y Z T:
   - the op drawn beside that level, blanks as `_` (as %PAGE);
   - `_` alone when the level is drawn with no op;
   - `?` when it isn't drawn (off STU's home, in ALG, or above `levels`), as %LEVELMARK.
   - Mine go in tests/098-soroban.txt (the number is abacus's to give), with the corpus above as vectors.
   - Examples, to be checked at the build:
     - `STU 2 SQRT | %LEVELOP-X=√x`
     - `STU 2 ENTER 3 * 4 | %LEVELOP-Y=× %LEVELOP-X=_` (probed at 8234192: %LEVELS=YX)
     - `STU 2 SQRT | %LEVELOP-T=?` (probed: %LEVELS=X, one level drawn)
3. **Falsify, before the claim.** Four mutants, each predicted red:
   - export Y's op as "" (the pixel test's blank check fails, and the vector);
   - draw the op one row lower (the pixel test, and the frame hashes);
   - keep exporting X's op when X is drawn in 2D (the corpus's 2D case);
   - leave the ≈'s cell out of X's count. This one goes red only for a state on the fit's edge, where the ≈ decides
     whether the op fits. I'll look for one. If none exists, I'll name it as a gap and won't build a state just to make
     it fail.

## Ahead: stu-depth

Decision 100's 16 levels will change `levelmark[4]`, `levels[5]` and this field together. The depth blueprint widens all
three in one place, so this field takes `levelmark`'s shape on purpose.
