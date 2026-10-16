# Work unit 075: HELP, then a key: what it does, the key not run

**From:** Michael's idea on stu-keys, adopted with stu-discover, through abacus #8736 · **Proposed:** soroban #8737 ·
**Ruled:** abacus #8738 (plain text; "HELP" in the shift's place; the MODE list from the audit, #8740) and #8743
(about 26 KB accepted) · **Status:** abacus's wording review (#8749, #8750, #8751) answered; the page's refusal note
ruled (#8753); tutor reads the entries for a student.

## What it does

- **Blue 9 is HELP in every mode** (KM_DEV_HELP). It was TUTOR, a menu never defined. The legend changes in
  layout/stu32-v0.json; the printed cap goes to draftsman.
- **HELP arms:** HELP_ on the X line, and HELP light on dark in the band's shift place. A shift pressed next arms,
  switches or cancels as ever, and takes that place (HELP, GOLD, 7 explains GOLD 7).
- **The next key is resolved as the app would dispatch it, and never run:** an open menu's soft key (HELP keeps the
  menu open), Equation mode's bar or its GOLD bank, else the key in its shift, its face where the shift has none.
- **Its page is a view in place of the stack,** as SYS's are:
  - line 1 is the key as pressed and its legend ("GOLD 7: …", "F5 in CAS2: Σ");
  - then up to three lines of its entry in the mode the key acts in. That's the calculator's mode, or in a program
    the label's stamp (ab_rec_model, the audit's H52).
  - A menu's key shows its entry's first line, then the menu's items from the keymap.
  - The paged menus' soft keys show [app] or [const], with the app's or the constant's name.
- **Any key or C closes the page and does nothing else.** The core is never called: no stack lift, no UNDO mark, no
  image change.
- **The page says when the key is refused now** (abacus #8753): RPN ONLY on STU's ALG line, NOT IN EQN in Equation
  mode.
  - It's on the title row when it fits in 31 cells ("STO: STO  NOT IN EQN"). Only otherwise is it the bottom row,
    "Now: NOT IN EQN.", where the entry's last line gives way only if the page is full. help_test lists every such
    entry; today there are none.
  - It comes from the core's own test. ab_alg_refuses and ab_eqn_refuses are pure predicates, and alg_key and eqn_key
    ask them first, so the dispatch and the page share one source.
  - A prompted key is judged as app_key takes it: refused before its argument, else with a letter. XEQ in Equation
    mode evaluates the equation. MEM's catalog runs or opens the label shown.
- **A shift after HELP only arms,** so a shift has no page of its own: HELP, GOLD, 7 is GOLD 7's page.
- **The keys app_key takes before their own action are explained as they act** (fresh eyes):
  - a digit while a graph is open ([graphpad]: the cursor, GOLD ten pixels);
  - HYP's trig keys (the hyperbolic function's entry, titled "HYP SIN");
  - an operator under 33s mode's CMPLX prefix ([cmplxpair]);
  - a paged menu's sixth key ([more]); APPS's GAMES key ([menu:GAMES]).
  - In MEM's catalog, HELP keeps the catalog and its bar. The keys the catalog takes are explained as it takes
    them (the review's sweep): C leaves it, CLEAR clears the item, ← backs out to MEM, ENTER recalls the variable,
    XEQ or R/S runs the label.
  - A soft key with nothing over it is titled by its place ("F3: -").

## The text is data

- **help/help.txt,** generated into firmware/help_text.c by scripts/gen_help.py (`--check` in make test). Its format
  is in the script's docstring:
  - an entry per action or family, [AB_SIN], [AB_TEST] (with {} for the legend), [AB_TEST:3], [menu:CAS],
    [dev:UNDO], [shift:GOLD], [app], [const], [soft], [none], [undefined];
  - lines prefixed 33: 35: STU: (or a list) replace the plain ones in that mode.
- **The names are emitted as C names,** so an entry for an op or a device action that no longer exists fails the
  build.
- **The MODE list:** abacus's twelve mode cases from the hidden-key audit (#8740):
  - CMPLX (H19); XEQ and GTO (H21, H54); the point (H26); 3 and 0 in a letter prompt (H28);
  - GRAPH, TABLE, CAS and ALG (H41); C after an error (H51); E (H53);
  - equations: the comma, %, and 2^3^2 (H55); CONST (H56).
  - H18's HYP is one text since unit 073. H52 is help_page's own.
- **304 entries,** drafted by four subagents reading the code, then merged, checked and folded (TEST's twelve, SUF's
  four, KREG's four as one {} entry each).
  - Text 22,755 B plus a 3,588 B table (12 B an entry), about 26 KB, accepted (#8743). After the review: 304 entries,
    the text about 22.6 KB.
  - The review dropped [none], [shift:GOLD] and [shift:BLUE], which no page can show. It added [catkey:C],
    [catkey:CLEAR], [catkey:BACK] and [catkey:ENTER].
  - Menu entries carry their name before HELP_MENU_END, so the record has no separate pointer.

## Checks

- **tests/help_test.c, 10143 checks:**
  - every key on all three layers and every menu item has an entry (314 actions);
  - every entry is reached by some action (no stale text);
  - every line fits a page's 31 cells in each mode with the legend it is shown for;
  - every glyph is in font5x7 (no semicolon or bar: 15 lines caught);
  - no line reaches the byte cap (a cut line can't be measured whole; help_lines never cuts inside a character);
  - [more], [menu:GAMES], [graphpad] and [cmplxpair] exist;
  - an op hidden in any mode (a 35s feature in 33s, the 33s's own elsewhere) varies by mode;
  - the MODE entries vary.
- **Falsified,** each red on its own check:
  - a missing entry, a stale one, an overlong line, a semicolon, a MODE entry without variants;
  - a hidden op without one, in both directions (CUBE hidden in 35s, CLSTK in 33s; the first check looked only at
    33s-hidden ops);
  - an unknown name, which fails the build.
- **Fresh eyes on the code (a subagent) found five real defects, each verified by a run, each now with a check red
  without its fix:** HELP broke MEM's catalog; APPS's GAMES key and the paged menus' more had no page; with a graph
  open GOLD 7 was explained as SOLVE; the HYP and CMPLX prefixes were explained as the plain key. Smaller: the bare
  soft key's title; truncation that could split a character; the --check that a Makefile rule made vacuous.
- **app_test, 768:** HELP_ and the band's HELP; HELP SIN's page with the image and UNDO unchanged; the closing key
  doing nothing; GOLD 7's page; C closing; HELP over CAS2 keeping it open, and F5 explaining Σ, not starting it.

## Not in this unit

- CATALOG (a stu-keys remap, later); help inside apps (an app's keys are the app's).
- The printed HELP cap and (I)/(J) on 3 and 0: draftsman.

## Decision 68 and the stand-in

**Static, `scripts/resources.py static a44047d df9ae2a`:**

| | stand-in | Rev A |
|---|---|---|
| flash | +30,384 | +30,416 |
| DIRAM | +32 | +32 |
| IRAM, calc_task's frame | +0 | +0 |
| internal-RAM symbols | app 688 → 720 (view_text widened, `help`) | the same |

- **Where the flash went (the stand-in's map):**
  - help_text.c, 26,991 B: the strings 23,355 (0x5b3b) and the table 3,636 (0xe34);
  - help.c, 1,201 B: help_find 246, help_for_action 342, help_lines 613;
  - the rest is app.c's help_page and the wiring.
- **idf_size misbooks the strings.** It books 23,452 B to libesp_app_format.a, whose objects did not change. The
  strings sit in an unnamed merged section, `.rodata.str1.4`, and the map puts that section in help_text.c.obj.
- help_mode_list is dropped from the device image: only help_test reads it.

**The stand-in at df9ae2a, device/standin/help075.txt:** ALL MET, and the control failed as it must.
- HELP SIN shows SIN's page (X still 2).
- C closes it, X still 2, no view; then SIN runs (0.0348994967…).
- HELP GOLD 7 shows GOLD 7's page, and the closing 7 types nothing.
- Internal heap 62,747 B free, stack-free 29,044 B.
- The first run lost the port at the flash (ACM2 moved to ACM3, I/O errors before any key). The rerun on the new
  port, with no flash, passed.

## abacus's wording review (#8749, #8750, #8751; the note ruled in #8753)

Each item was verified in the code at df9ae2a before its text changed. These are the items, and what was done:

- **1, 3: the menus and the sweep.**
  - help_page shows a menu entry's first line, then its items, so every later line was dead. That was 18 entries:
    abacus's 15, and LOGIC, SUFX and CAS, whose cut left "algebra on the" mid-sentence. Each is now one line per mode,
    with MODE's LBL fact and 33s CMPLX's prefix kept in it.
  - help_test fails a menu entry of more than one line. The sweep (item 3) finds the same independently.
- **2:** the shift entries are gone. [dev:HELP] says "A shift first picks its legend."
- **4 to 19, 22 to 30:** rewritten as abacus proposed, fitted to 31 cells. Siblings were fixed too:
  - every digit's graph line is gone, since [graphpad] replaces those pages and no digit's page ever showed it with a
    graph open;
  - ÷, +, − and × gained STU lines for ALG;
  - every statistic says "Equation, ALG: types its name."
- **10: the 33s never opens TABLE's or GRAPH's menus.** Their keys' 33: lines are gone. The 35s opens them, sets
  silently and refuses GO, so those keys have 35: lines.
- **Dead mode text (the sweep):** LOGIC's six ops and SUF ("33: Not in 33s mode.") are reached in no 33s state, not
  even in program entry under a 33s label. Neither are the CAS ops' and ∞'s "33,35: STU ONLY". Those lines are gone.
- **19: the suspect holds, and the code isn't changed.** GTO's dot form counts its digits by the device's mode
  (`need = a->c->m35 ? 3 : 4`, app.c). The letter form and HELP follow the label's stamp (`ab_rec35`). So on a label
  stamped for the other mode they disagree. This is put to abacus.
- **20, 21: the note above** replaces the hand edits. The real sets, measured: 80 entries refused RPN ONLY in ALG, and
  65 refused NOT IN EQN (62 in 33s).
- **31:** [const] is as the code does. The question goes to Michael through abacus.

### The checks

- **tests/help_test.c, 766,787 checks.**
  - The static checks are kept. Coverage now skips the actions with no page (a soft key's shifted layers, a shift).
  - **The sweep** drives the real app. Its roots, in each mode, are: plain, the Equation list, an equation typed, ALG,
    an ALG result, a graph, a table, Σ's upper bound, and program entry under each label stamp. It reaches every state
    their keys reach, each base included: 1,378 states, with every menu and page. In each, HELP then every key on three layers. The entry the page
    used is read by wrapping help_lines at link time. It checks that:
    - every entry is reached (304 of 304);
    - every mode an entry is reached in shows all its lines;
    - a mode no state reaches has no text of its own;
    - a page for an op hidden in that mode carries the mode's line;
    - the title fits;
    - the note matches the key pressed for real, and goes on the bottom row only when the title can't hold it.
  - **refusals():** the predicates beside ab_do for every op and argument (-1, 0, 1), in ALG (an empty line and after a
    result) and in Equation mode (the list and typing), in each mode.
- **Mutants,** each red on its own check:

| mutant | what it breaks | failed |
|---|---|---|
| D1 | eqn_handles takes R↓; the dispatch still refuses it | 459 |
| D2 | alg_handles takes →POL; the dispatch still refuses it | 12 |
| D3 | the predicates lose ab_do's early gates | 126 |
| N1 | the page never notes | 10,894 |
| N2 | the note never on the title row (first survived: no check yet) | 10,894 |
| C1 | no catalog mirror | 24 |
| H1 | no HEX digit mirror | 13 |
| T1 | a menu entry of two lines | 6 |
| T2 | a dead 33: line on a CAS op | 2 |
| T3 | CLSTK without its 33s line | 36 |

- **What the sweep found on the way, beyond the review:**
  - CONST's paged keys and MEM's catalog keys had no note;
  - ALG's blank soft key outside STU and a hidden VARx were noted though they do nothing;
  - the catalog's five keys were explained as their plain selves.
  - Each was fixed and checked.
- **Fresh eyes (a subagent) on this round found two defects and a blind spot, each verified by a run:**
  - In HEX, keys 6 to 11 type A to F before their own action, and the page judged the face op instead. So on ALG's
    line Σ+ read "RPN ONLY" while it types F. help_page now mirrors the rule (H1).
  - The sweep's state signature had no base, so HEX states were folded into decimal ones. That's why the sweep
    missed it. The signature now has the base and ALG's shown result, and an ALG result is a root.
  - [catkey:ENTER] said "to X". In EQN or ALG it types the letter, and now says so.
  - Smaller:
    - "Ignored in BIN" is now "BIN: ignored in a number", since a digit still types in an equation;
    - RCL says what EQN and ALG do;
    - the face fallback applies in every shift, as app_key's;
    - help_test's Makefile rule lists its headers;
    - the dispatch's old refusals are marked as backstops.
  - It found no core behavior change.
- make test rc 0; app_test 768; keymap_test 2867.
