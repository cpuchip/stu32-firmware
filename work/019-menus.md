# Work unit 019: the menus (provisional, from abacus's draft v1)

**From:** abacus · **To:** soroban · **Status:** ready (after 018)

## Why

32 menus in the keymap are "not yet" (-1). That is the biggest gap between the calculator and its
keys: the stand-in, Rev A and the app reach none of the functions behind them, though the core has
nearly all of them. Michael asked for as much as can be done without his judgment while he sleeps, so
I drafted every menu (abacus `docs/menus-draft-v1.md`). The draft draws on:
- the 32SII manual's 14 menus and the 35s guide's 16 (p. 1-5, 1-6);
- the HP forum's complaints (CLSTK buried on the 35s CLEAR menu's second page);
- the core's 185 ops.

Build the drafts provisionally, as PRGM (#3877) and ∡MODE (decision 33) were. They live on Michael's
roadmap page; when he marks one decided with other contents, the decided version replaces the
draft. Keep the menu data in the generator, as now, so a change is data only.

## Rules

1. **The menus whose items are all core ops today:** build them exactly as drafted. Item order is
   the soft keys F1-F6, and an empty slot is KM_NONE.

   | Menu | F1 · F2 · F3 · F4 · F5 · F6 |
   |---|---|
   | HYP | SINH · COSH · TANH · ASINH · ACOSH · ATANH |
   | STAT | x̄ · ȳ · sx · sy · σx · σy |
   | L.R. | x̂ · ŷ · r · m · b · x̄w |
   | SUMS | n · Σx · Σy · Σx² · Σy² · Σxy |
   | PROB | Cn,r · Pn,r · n! · RAND · SEED · — |
   | CLEAR | CLx · CLSTK · CLVARS · CLΣ · (ALL: unit 021) · — |
   | FLAGS | SF · CF · FS? · — · — · — |
   | x?y | x≠y? · x≤y? · x<y? · x>y? · x≥y? · x=y? |
   | x?0 | x≠0? · x≤0? · x<0? · x>0? · x≥0? · x=0? |
   | TEST | x=y? · x≠y? · x=0? · x≠0? · FS? · x<y? |
   | BASE | DEC · HEX · OCT · BIN · LOGIC ▸ · SUFFIX ▸ |
   | LOGIC (from BASE) | AND · OR · XOR · NOT · NAND · NOR |
   | SUFFIX (from BASE) | d · h · o · b · — · — |
   | CMPLX | i · θ · ARG · [ · — · — |
   | FRAC | FDISP · /c · RCL/c · precise · factors · fixed |
   | CONV | →kg · →lb · →°C · →°F · →cm · →in |
   | UNITS | →l · →gal · →km · →mile · — · — |
   | ANGLE | →POL · →REC · →HMS · HMS→ · →DEG · →RAD |
   | VARS | x↔ · VIEW · CLVARS · — · — · — |
   | STACK | CLSTK · LASTx · R↑ · x↔y · — · — |
   | MODE | 33s · 35s · — · — · — · — |
   | POW | IP · FP · INTG · RND · SGN · ABS |

   - The test items are AB_TEST with the relation's argument (the panel's numbering).
   - SF, CF and FS? ask for a flag number with the digit prompt (KM_ARG_DIGITS). Flags 10 and 11 come
     by `.0` and `.1`, as the 35s does and as DISP already does.
   - x↔, VIEW and the arithmetic-STO family ask for a variable (KM_ARG_VAR).
   - MODE sets AB_MODEL 33 or 35. Show which is in effect if the screen can (a mark on the soft key).
   - POW holds the parts of a number under its printed legend. Michael is asked to rename it PARTS (the
     32SII's name), but the legend stays POW until he rules.
2. **FRAC's three formats are two flags each** (UG p.5-6):
   - precise: 8 clear;
   - factors: 8 set, 9 clear;
   - fixed: 8 and 9 set.

   One soft key must do both. A small action list in the keymap or a core op is your choice; say
   which.
3. **CONST pages** (the 35s's 41 constants). F1-F5 show five constants and F6 shows "more", which
   moves to the next five and wraps at the end. This is the one paged menu. Add what app.c needs
   (a page index kept with the open menu), and test it at the last page and on wrapping.
4. **SOLVE (gold 7) and ∫ (gold 8) become direct, the 35s's way:** a prompt for the variable, then the
   op, not a menu. The layout file still boxes them as menus. Override that in the generator's data
   with a comment that cites this unit, and Michael's roadmap answer decides the legend.
5. **Still "not yet" after this unit:**
   - EXP (2ˣ, LOG₂, eˣ−1, LN(1+x) need new ops: unit 021);
   - MEM, SYS (catalog and info screens: a later unit);
   - RADIX, SETUP (unit 022);
   - GRAPH, TABLE, TUTOR, APPS (reserved).

   The keymap keeps -1 for these.
6. **One level.** No item opens a menu that opens another, except BASE's two. The forum's complaint
   about the 35s is keystrokes.

## Acceptance

- keymap_test: every menu above has exactly the drafted items in order, every item's op exists, and
  no legend in the layout is left KM_UNDEFINED. `--check` still matches the layout.
- app_test:
  - a key opens each menu;
  - each soft key does its op (spot-check one per menu, plus all of FRAC's three and both MODE items);
  - SF then `.1` sets flag 11;
  - CONST pages and wraps;
  - BASE ▸ LOGIC ▸ AND does AND;
  - C closes a menu and does nothing else;
  - SOLVE prompts for a variable.
- Red first, then green, as always. Mutants: one item moved, CONST failing to wrap, and FRAC fixed
  setting only flag 8. Each must fail.
- The stand-in and the 10" panel get these from the table with no code of their own. The panel's
  provisional fallbacks go quiet on their own, because the table answers first; I retire them after.
