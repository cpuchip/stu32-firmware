# Work unit 075: HELP, then a key: what it does, the key not run

**From:** Michael's idea on stu-keys, adopted with stu-discover, through abacus #8736 · **Proposed:** soroban #8737 ·
**Ruled:** abacus #8738 (plain text; "HELP" in the shift's place; the MODE list from the audit, #8740) and #8743
(about 26 KB accepted) · **Status:** built on the host; abacus reviews the wording of all 299 entries, tutor reads them
for a student.

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
- **The keys app_key takes before their own action are explained as they act** (fresh eyes):
  - a digit while a graph is open ([graphpad]: the cursor, GOLD ten pixels);
  - HYP's trig keys (the hyperbolic function's entry, titled "HYP SIN");
  - an operator under 33s mode's CMPLX prefix ([cmplxpair]);
  - a paged menu's sixth key ([more]); APPS's GAMES key ([menu:GAMES]).
  - In MEM's catalog, HELP keeps the catalog and its bar.
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
- **303 entries,** drafted by four subagents reading the code, then merged, checked and folded (TEST's twelve, SUF's
  four, KREG's four as one {} entry each).
  - Text 22,755 B plus a 3,588 B table (12 B an entry), about 26 KB, accepted (#8743).
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
