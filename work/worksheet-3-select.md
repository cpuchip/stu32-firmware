# The worksheet, part 3: selecting a row (blueprint, before any code)

**From:** decision 95 step 4; part 2 (unit 090) as built; abacus #14041 1 (part 3: selecting a row, the row menus, moving
and scrolling in RPN, what-made-this); the material in abacus #13972: the note's step 4 (moving, the three open items,
the menus), stu-nav ("Digits pick; 8/2/4/6 move only in GRAPH and worksheet row selection, with arrows shown"),
stu-soft-shift ("gold the arrows and tools, blue the CAS verbs", with Michael's note "we'll try your recommendation after
chatting about it").

**Status:** a draft for abacus. No code. Read on the host at 88458d4. The prototype is throwaway, its frames and source
in work/worksheet-evidence/part3/ (it draws over today's screen_render).

## What part 3 is, and what it leaves

- **Part 3:** selecting a row on STU's home, moving the selection and scrolling the page with it, a row menu on the soft
  keys by the row's kind, COPY and the verbs acting on the selected row, and what-made-this (the rows it links to
  marked).
- **Left for later:** walking the links (4 and 6, "walk and replay are later units", the note); GRAPH, TABLE and the
  solvers sending results back as rows (the note's last item); the full fixed gold bank on every screen
  (stu-soft-shift); Michael's 50g note on numbering the levels (#14050, abacus's council).

## What exists today (measured)

- **No arrow keys:** the face has six soft keys, GOLD (30), BLUE (36), the digits and the ops (keymap.c). GRAPH's
  digits already move its cursor: 8 up, 2 down, 4 left, 6 right, 5 home, GOLD first for ten pixels (app.c:1122).
- **STU's home has no bar** unless X holds an expression (088's X EXPR: SIMP, EXPND, SOLVE, →EQN, ▸ to CAS). A soft key,
  shifted or not, with no bar does nothing there, so GOLD and a soft key are free on the home.
- **ALG** has its bar (ALG LINE: ◀ ▶ ( ) _ ▲ ▼). Since 090, ▲ ▼ walk the page's ALG rows, and the row shown is Y's
  line and X's result.
- **The page's plan** (090) draws the newest rows below a limit (ALG's shown row). A selection deep in the page needs the
  same walk with a different limit, so scrolling costs no new structure.

## The prototype (work/worksheet-evidence/part3/)

The chain `3 ENTER 4 × 5 + 2 ÷ √x`, with a row selected by inverting its line and a dot at the left of each row it
links to. q2 selects ÷ 8.5 (its links + 17 and 2 marked); q3 selects + 17 (× 12 and 5 marked). The soft keys show a
number's row menu. It reads at a glance on the 400 × 240 frame, and inverting a line costs nothing the font doesn't
already do.

## The leans

1. **Entering:** GOLD, then the second soft key (stu-soft-shift's gold ▲), on STU's home in RPN.
   - It selects the newest row on the page, or X's row when the page is empty. A number being typed is completed first,
     as any op key completes it (R3, its row made), so selecting never loses what was typed.
   - Part 3 places only gold ▲ and ▼ (soft keys 2 and 3), on STU's home. The rest of the fixed gold bank is
     stu-soft-shift's own unit.
   - The status band shows SEL in RPN's place while a row is selected.
2. **Moving (stu-nav):** 8 up, 2 down, 5 back to the newest, as GRAPH's digits.
   - The selection runs over every row on the screen in order: the page's rows, then the levels' (T to X).
   - Moving up past the page's top scrolls it a line, to the oldest row the page holds; then the dropped line, if there
     is one, and no further.
   - 4 and 6 do nothing in part 3 (the note's walk is later). Other digits do nothing while a row is selected.
3. **Leaving:** C or ← leaves, nothing changed. Any other key leaves the selection, then acts as it would have (a menu's
   rule, app.h: "any other key closes it and acts").
4. **The row menu,** by the row's kind (the soft keys while selected):
   - a number: COPY, STO, SHOW, →FRAC, (empty), INFO
   - an expression: COPY, STO, SIMP, EXPND, SOLVE, ▸ (088's X EXPR verbs, then CAS's page, as the bar has them)
   - an ALG row: its kind's menu, with EDIT in COPY's place (the line takes its text, as ENTER on a shown row does today)
5. **What the actions do:**
   - **COPY** puts the row's exact value in X (stack lift) and makes a row, COPY, linked to the selected row. That's R4's
     rule for LASTx and RCL: a value brought back links to the row it came from. Its left reads COPY.
   - **ENTER** on a selected row is COPY in RPN. The note's worry (a scrolled-up student losing the line being typed) is
     met by 1: the entry was completed on the way in.
   - **A verb** (SIMP, SOLVE, →FRAC...) is COPY, then the verb on X: two rows, each with its links, and the answer at the
     bottom (the note: "Answers land at the bottom").
   - **STO** stores the row's value in a letter. That's a move: the letter holds the selected row, and no row is made.
   - **SHOW** is today's SHOW of that value.
   - **INFO** shows the row's op and what it read in X's place until the next key, e.g. "÷ OF 17 (ROW 5), 2 (ROW 6)",
     reaching links off the screen too.
   - After an action the selection ends, and the screen returns to the newest rows with the result at X.
6. **What-made-this:** a dot at the left of each line the selected row links to (rows on the page and the levels alike).
   A link off the screen gets no dot; INFO names it.
7. **ALG:** its ▲ ▼ stay as 090 built them (the page's ALG rows; 029's vectors unmoved). The row shown there is a
   selected row: it gets the row menu (4) on GOLD's bank of the ALG LINE bar, and the dots (6). The question for abacus
   is whether ALG should instead walk every row, as RPN does. That would move 029's ▲ vectors.

## The costs (to measure in the build)

- One walk a frame, as 090's, with the limit the selection's window. A key's time isn't touched.
- compose's plan gains the selected serial and the window's anchor (8 B), and a flag per page line for the dot (8 B):
  DIRAM about +16 B, measured by D68.
- The menus: three new menus in keymap.c's table (flash).

## What the build would test (for abacus's oracle)

- **Dumps:** `%SEL=serial` or `-`; `%PAGEMARK-k=S|L|-` (the page line k selected, linked, neither); `%LEVELMARK-L=` the
  same for a level; the soft keys as `%SOFT-n=`. %PAGE-k as 090's, over the scrolled window.
- **Vectors:**
  - selecting the newest row, moving with 8 and 2, scrolling past the page's top and stopping at the oldest;
  - the dropped line as the end;
  - entering while typing (the number's row made first);
  - C and ← leaving with nothing changed; another key leaving and acting;
  - COPY's row and link; ENTER as COPY;
  - a verb as two rows;
  - STO linking the letter to the selected row;
  - INFO's text;
  - the dots;
  - ALG's shown row with its menu;
  - the 33s and 35s modes untouched (no selection, GOLD-soft as before).
- **Frames:** the prototype's q2 and q3 and the menus, by eye; no existing frame moves (the home without a selection
  draws as 090's).
- **Mutants,** predicted before they're run:
  - the window not following the selection;
  - COPY without its link;
  - a verb acting on X instead of the selected row;
  - C acting as a key;
  - the selection reaching past the oldest row.

## Questions for abacus

1. The scope as 1-7, leaving the link walk, the solvers' results and the full gold bank to later units.
2. GOLD ▲ ▼ on STU's home now, the first piece of stu-soft-shift, or a different way in.
3. The selection running through the levels as well as the page.
4. COPY as R4's rule (a row linked to the selected one), ENTER as COPY, a verb as COPY then the verb.
5. ALG keeping 090's ▲ ▼ over its own rows, with the row menu, or walking every row.
