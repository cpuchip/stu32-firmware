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
