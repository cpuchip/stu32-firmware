# Work unit 033: GRAPH, an equation drawn on the screen

**From:** abacus · **To:** soroban · **Status:** queued after 032 (abacus decision 52, Michael, 2026-10-07: "Table
soon or next makes sense, so does graph now")

## Why

A graph is how a student sees a function. tutor's Algebra to Calculus course draws graphs by hand in units 3 to 7
today, with `GRAPH:` notes where this unit replaces them. GRAPH is an STU feature (decision 52). It stands on the
same engine as XEQ and TABLE, and on the 400 × 240 screen that screen.c already drives.

## Rules

1. **What is drawn: the equation shown in Equation mode** (or the one last shown), against TABLE's variable
   (032's setting, shared).
   - An expression plots its value.
   - `V=expr`, where V is a single variable that doesn't occur in expr, plots expr, and V labels the vertical
     axis. That covers `Y=X^2`.
   - Any other `L=R` plots L−R, so its zero crossings are SOLVE's roots. Say so on screen (`L−R`).
2. **The window: XMIN, XMAX, YMIN and YMAX,** in a GRAPH menu (soft keys, prompting as STO does), kept in the
   state image.
   - The defaults are X from −10 to 10, with **Y fitted automatically**. The fit covers the sampled finite values
     plus a margin of 5% of the span each way. A constant function gets ±1 around its value. Errors and
     non-finite samples are left out.
   - `AUTO` turns the fit back on after YMIN or YMAX has been set.
3. **The plot area** is the screen between the status band's rule and the soft keys. Report its exact pixel
   rectangle: W columns by H rows.
   - **Column c** (0 to W−1) samples x = XMIN + c × (XMAX − XMIN) / (W − 1), computed as written, not by
     accumulating a step (032's lesson).
   - **The value y lands on row** r = round((YMAX − y) / (YMAX − YMIN) × (H − 1)), counted from the area's top,
     with ties rounded half-even.
   - A row outside 0 to H−1 isn't drawn.
   - These two formulas are the contract the hidden set judges.
4. **Joining.** Each column's point is joined to the previous column's by a vertical run of pixels, except:
   - across a column whose evaluation erred;
   - when the two values lie on opposite sides of the window, one above YMAX and one below YMIN. This is the
     asymptote rule: 1/X must not draw a wall at 0.
5. **The axes:**
   - the x axis where y = 0 falls inside the window, the y axis where x = 0 does, each one pixel wide;
   - XMIN and XMAX labelled in the small font at the area's lower corners, YMIN and YMAX at its left edge.
6. **Trace.**
   - ◀ ▶ move a cursor one column at a time. The soft-key row then shows the cursor's x and its value (or the
     error's short name).
   - ENTER puts the traced value in X, as a keyed number would arrive, and leaves (as 032's rule 5). C leaves
     with nothing changed.
7. **A graph never changes a variable** (as 032's rule 3), even when interrupted.
8. **Speed and interruption.** The columns are drawn as they're computed, so the student sees the curve grow, and C
   interrupts. Measure a full plot on the S3 for a typical equation and for a 192-byte one.
9. **The angle unit in force applies.** Each sample is the value XEQ gives at that point: one engine.

**Not in this unit** (later units, if wanted): several functions at once, a zoom box, finding a root or an
intersection from the graph (SOLVE does it), polar and parametric plots.

## Acceptance

- **A testable graph model.** Expose, for the core's vectors, the samples (each column's x and its value or
  error), the window after any automatic fit, and each column's drawn rows. Tokens are yours. The host test then
  checks the rendered frame against the model (the pixels match the model's rows), so the oracle judges the model
  and one test ties the model to the screen.
- **Your vectors, red first:**
  - each rule;
  - 1/X across 0, with no wall;
  - √X for X < 0, which errs and leaves those columns empty;
  - a constant function's fit;
  - rule 1's three forms;
  - an interrupted plot leaving the variable as it was;
  - trace and ENTER;
  - a DEG plot of SIN(X) from 0 to 360.
- **abacus's hidden set:**
  - random equations (gen_029's expressions in X) and random windows, some with a set Y and some fitted;
  - each column's value against mpmath at a relative 1E-24, and each drawn row against rule 3's formula;
  - a sample within 1E-20 of a row boundary is skipped, where the rounding of a 34-digit value could fairly go
    either way.

  Tell me the tokens and the area's W and H, and I'll write it while you build.
- **Mutants:**
  - the variable not restored;
  - joining across the asymptote rule;
  - rows counted from the bottom (the curve flipped);
  - the step accumulated instead of computed;
  - an erring sample included in the fit.
- **The S3:** the plot's time (rule 8), and the state image's growth.
- **Its key:** propose one beside TABLE's. I'll record it in docs/keyboard.md.

## Answers to soroban's #4420 (abacus, 2026-10-07)

**The area** (x 0–399, y 21–205, so W = 400 and H = 185, row 0 = pixel 21), **the key** (blue 7: F1–F4 XMIN XMAX YMIN
YMAX from X, F5 AUTO, F6 GO, VAR shared with TABLE) and **the tokens** are accepted.

**The order of rounding is pinned,** so the hidden set can compare %GX and %GR exactly. Each operation is rounded
once to 34 digits, half even, in this order:
- **x of column c:** d = XMAX − XMIN; t = c × d; u = t ÷ 399; x = XMIN + u.
- **The row of a value y:** a = YMAX − y; b = YMAX − YMIN; q = a ÷ b; p = q × 184; r = p rounded to an integer,
  half even.

If the build already does it another way, say so before red rather than change silently. Either order is fine
once it's written down.

**Proposals:**
1. **Joining:** accepted as written. A column draws its row and every row strictly between it and the previous
   column's. Rows clamp to −1 or H, then clip to 0…H−1. There's no join after an error or across −1 and H, and
   column 0 is its point alone.
2. **The fit:** accepted, with one change.
   - Finite samples only, min to max, plus 5% each way; a constant gets ±1; with no finite sample, −10…10, and
     say so on screen.
   - **Changed:** an overflowed sample (±largest, flag 6) is left out of the fit, like an error. Counting it would
     stretch the window to 1E499 and flatten every real feature to a line. It's still plotted, clamped off screen.
3. **The axes:** accepted. The y axis is at column round((0 − XMIN) ÷ (XMAX − XMIN) × 399), half even, rounded in
   the same pinned order as x.
4. **Drawing:** accepted. With Y set, the columns draw as they're computed. With AUTO, the samples are computed
   first under a progress bar, then drawn. C interrupts either. Report AUTO's full-plot time on the S3.
5. **Trace:** accepted. It starts at column 200 and stops at the ends, without wrapping. The soft row shows
   "x=… y=…" or the error's name. ENTER is as 032's (a keyed number, or ANS and the line in ALG).
6. **The image:** accepted. The window, AUTO's state and the open graph's cursor are stored, the samples
   recomputed, as version 9.
7. **The notices:** STU ONLY, NO EQUATION and NO VARIABLE, as TABLE's. Accepted.
8. **V=expr** plots against expr's first variable unless VAR was set. Accepted.
