# Work unit 020: equations from the keys (the soft keys as the 35s's cursor pad)

**From:** abacus · **To:** soroban · **Status:** ready after 019 (it shares app.c's menu code)

## Why

The core types, edits, browses and evaluates equations (units 014 and 015). Its eqn_key takes SST and
BST through the list, KLEFT and KRIGHT, KBKSP, ENTER, XEQ, SOLVE, ∫ and C, plus KEQ, KPAREN, KSPACE and
KREG. But the STU-32 has no key for =, ( ), SPACE or the cursor. The 35s puts them on its four-way pad
and on shifted keys we didn't keep. So EQN is a bare toggle, and the app shows it as "not yet"
(#3878). Michael is asleep and asked for as much as can be done without him: this unit is
provisional, and he decides it on the panel and the stand-in.

## Rules

1. **The soft keys follow Equation mode** (ours, provisional). While Equation mode is on, the soft
   keys are the editor's, not a menu's.
   - Showing the list (not typing):
     - F1 **▲** = BST;
     - F2 **▼** = SST;
     - F3 **EDIT** = KRIGHT (editing starts at the start of the equation, p.6-7);
     - F4 **NEW** (ours: shows EQN LIST TOP's position, so typing starts a new equation after it);
     - F5 —;
     - F6 **EXIT** = EQN (off).
   - While typing:
     - F1 **◀** = KLEFT;
     - F2 **▶** = KRIGHT;
     - F3 **( )** = KPAREN;
     - F4 **=** = KEQ;
     - F5 **SPC** = KSPACE;
     - F6 **REG ▸**, which opens REGX · REGY · REGZ · REGT (KREG 0-3) for one choice, then returns
       to the typing bar.

   C stores the equation and leaves Equation mode (p.6-4, as the core does now). ENTER ends the
   typing and shows the equation (p.6-4). The soft labels come from app_ui, as menus' do.
2. **Variables by RCL then a letter** (p.6-4: "To enter a variable in an equation, press RCL
   variable"). RCL is already a variable prompt (KM_ARG_VAR). In Equation mode, pass the completed
   AB_RCL to the core, which types the name, as eqn_key already expects. Check that (I) and (J) type
   too.
3. **Function keys type their names.** In Equation mode, keys whose functions take an argument (SIN,
   LN, √x, and the rest) go to the core as now; the core types `SIN(` and so on. A key the core
   ignores in an equation does nothing. No change is needed here; it is listed so the tests cover it.
4. **EQN (gold 0) turns Equation mode on and off**, and shows the bar. The 019 menus still open from
   their keys in Equation mode, and their items type into the equation where the core takes them.
   This is the 35s's way with its menus (p.6-4: "Equations can contain variables, numbers, vectors,
   functions").
5. **The keymap's EQN stays a KM_OP.** The bar is app.c's state, driven by c->eqn_mode and
   c->eqn_typing, so it needs no new keymap kind. Tell tally when it lands: the app's EQN guard
   (#3878) comes off then.

## Acceptance

- app_test drives the guide's ch.6 entry examples from the keys, with the soft keys for ( ) and =.
  For example, the guide's first equation is typed key by key and evaluated with ENTER, and its
  result matches unit 014's vector for the same equation.
- ◀ ▶ inside an equation, then ← deletes at the cursor; a typing mistake is fixed and the equation
  evaluates.
- ▲ ▼ wrap through EQN LIST TOP (p.6-7).
- EDIT on a built-in does nothing (p.6-8).
- REG ▸ REGY types REGY.
- RCL A types A, and RCL (I) types (I).
- C in the middle of typing stores and leaves.
- EXIT leaves with the list unchanged.
- The stand-in shows the bar's labels (tests/device: a screen check at the soft band).
- Red first, then green. Mutants: ▲ and ▼ swapped, = typing (, and the bar not leaving after C. Each
  must fail.
