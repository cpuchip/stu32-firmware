# Work unit 040: GRAPH's plot menus after the 50g (zoom, cursor, function tools)

**From:** abacus · **To:** soroban · **Status:** queued after 034 (abacus decision 57, Michael, 2026-10-07: "we could
borrow the HP 50g menu for the graph plot menu, its like 3 pages long and has a cursor and zoom feature")

## Why

tally's screenshot showed AUTO flattening a cubic's interesting part against its far ends, and tutor found the trace
starting at x = 0.0250626…. Both want what the 50g's plot screen has: zoom, a free cursor, and function tools. The
source is the 50g user's guide (books/hp-calculators/hp50g-ug-en.pdf), read for **behaviour, not code**. Its pages
are cited below. Read them yourself before building.

## Rules

1. **The plot screen's soft keys are three pages** (◀ ▶ or a "more" key moves between them):
   - **ZOOM**;
   - **TRACE and (X,Y)**;
   - **FCN**.

   Propose the exact layout of the 6-key pages.
2. **ZOOM** (50g guide p.12-47 to 12-49), each redrawing with the window it describes:
   - ZFACT sets the H and V factors (default 4, say; propose);
   - ZIN and ZOUT, centred on the cursor;
   - ZLAST;
   - BOXZ: two corners chosen with the cursor, and the box fills the screen;
   - CNTR;
   - ZDFLT (our default window) and ZAUTO (y fitted to the current x range, as 033's AUTO);
   - HZIN, HZOUT, VZIN and VZOUT;
   - ZDECI: x limits rounded so the columns fall on decimal steps. Report the window it makes on our 400 columns:
     with W − 1 = 399 intervals, a step of 0.05 gives an x span of 19.95;
   - ZINTG (a column a unit);
   - ZSQR (1:1 pixels);
   - ZTRIG.
3. **The cursor:** a free cursor moved by the arrow keys over the whole plot (not only along the curve), with (X,Y)
   showing its coordinates. TRACE locks it to the curve, as 033's trace does. Both use 033b's readout text.
4. **FCN** (50g guide p.12-5), each acting near the cursor and showing its result as the readout:
   - **ROOT and ISECT:** the curve's root near the cursor, by our SOLVE engine with guesses from the cursor's column
     and its neighbour. NO ROOT FND as SOLVE's.
   - **SLOPE:** the derivative at the cursor's x. Numeric now (state the method and its accuracy). Casimir's exact
     d/dx can replace it later, behind the same key.
   - **EXTR:** an extremum near the cursor (a root of the slope).
   - **AREA:** between two cursor marks, by our ∫.
   - **F(X):** the value at the cursor's x.
   - **TANL:** the tangent line drawn at the cursor.

   Each FCN result can be placed in X with ENTER, as 033's trace.
5. **The window state** (the factors, ZLAST's previous window) is in the image, a version bump. The menus are STU
   only, as GRAPH is.

## Acceptance

- **Vectors** for each zoom's resulting window (exact, by a pinned formula you write down, as 033's rounding order).
- **FCN results against an oracle:** abacus's hidden set (roots, extrema, areas and slopes by mpmath on random
  functions; SOLVE's tolerance applies, so state the bar).
- **app_test:** the menu pages, BOXZ drawing, the cursor's readout. The frame tie, as 033's.
- **Mutants:** ZOUT as ZIN; BOXZ corners swapped; ROOT ignoring the cursor; ZDECI not landing on decimals.
- **The S3:** a redraw's time after each zoom.

## Zoom, cursor and layout accepted (abacus, 2026-10-07, 5ccd3a4); FCN pending

- **Accepted as built:** the bar kept, the readout in the status band, ZFACT cycling, the pinned formulas.
  040 passes 50/50 in abacus's build.
- **040b:** ROOT, EXTR and AREA interruptible by C, as SOLVE and ∫ are.
- **FCN's acceptance** waits for abacus's hidden FCN set.
- **Huge windows near 1E6144** are a documented limit. BLUE+digit is as in 033.
