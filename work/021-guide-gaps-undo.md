# Work unit 021: the guide's remaining gaps, CLEAR ALL, UNDO, and four accurate functions

**From:** abacus · **To:** soroban · **Status:** ready (independent of 019; its items fill 019's slots)

## Why

The core still leaves three of the 35s guide's behaviours undone (the "later unit" notes in fn.c and
calc.c), and the keyboard has an UNDO key that does nothing. Michael's roadmap lists them as unit 021.
The menu draft (unit 019) also asks for four functions the WP 34S added for accuracy near zero.

## Rules

1. **x! of a non-integer is Γ(x+1)** (UG p.4-15: "The x! function calculates Γ(x + 1). The value for x
   cannot be a negative integer.").
   - Integers keep today's exact path (0 to FACT_MAX).
   - A non-integer, negative ones included, gives Γ(x+1).
   - A negative integer is the error appendix F names for factorial or gamma, so it uses that message.
   - Accuracy: the core's usual bound (unit 003's), checked against mpmath's gamma in the hidden
     model.
   - Out of range: overflow is the core's overflow, and a result too small to represent behaves as
     unit 003's underflow does.
2. **Clearing the unnamed indirect variables** (UG p.1-5).
   - The 35s's CLEAR menu offers CLVARx: a three-digit address, then every indirect variable whose
     address is *greater* than it is erased. Its example: "CLVAR056 erases all indirect variables
     whose address is greater than 56".
   - Give the core the op with its address argument.
   - Unit 019's CLEAR menu takes it as its sixth item, with a three-digit prompt (a KM_ARG kind for
     it).
   - Whether CLVARS also clears them stays as the guide has it now (direct variables only, p.3-5).
3. **CLEAR ALL with its confirmation** (UG p.1-29: clearing all of memory "erases all numbers,
   equations, and programs you've stored. It does not affect mode and format settings", after a
   confirmation prompt that "safeguards against the unintentional clearing of memory").
   - The confirmation is the device's: app.c shows the prompt, the next key is yes or no, and any other
     key cancels.
   - The core gets the op that clears data and keeps modes. Read appendix B for what "settings" means,
     and say if it differs from ab_memory_clear.
   - This is CLEAR's F5 in unit 019.
4. **UNDO (ours)** is one level, and a second UNDO puts it back.
   - Corrected 2026-10-15 (abacus #8495, #8497): this rule first said "the 35s has none". It has one. The 35s's
     Undo key (UG p.1-11) "serves largely to recover from the deletion of an entry": an entry, an equation
     deleted in Equation mode, a program line, or a register cleared from the CLEAR menu, only straight after
     the deletion; and while editing, a character or an expression just deleted. Ours is wider: it undoes any
     key that changes the state.
   - Before each key that changes the calculator's state, the device keeps the state image (unit
     016's), so UNDO restores the state from before the last such key. Two calculators' images are
     about 212 KB, so one more image is affordable in PSRAM.
   - Keys that change nothing (a shift, a menu opening, VIEW) don't move the undo point.
   - A program run is one key: UNDO after XEQ restores the state before the run.
   - CLEAR ALL is undoable too: that is the case UNDO is for. Say if the image size makes that a problem
     on the stand-in.
   - The core needs nothing new beyond the image; this is app.c's.
5. **Four accurate functions (ours, after the WP 34S), for 019's EXP menu:** 2ˣ, LOG₂ x, eˣ−1 and LN(1+x).
   - The last two must stay accurate where x is tiny; that is their point.
   - Domains: LOG₂ like LOG; LN(1+x) needs x > −1.
   - The hidden model checks them against mpmath (expm1, log1p) to the core's usual bound, with cases
     at 1e-20 and below.

## Acceptance

- Vectors (`work/021-vectors.txt`, mine, from the guide and mpmath):
  - x! at 0.5 (√π/2), at −0.5 (√π) and at a large non-integer;
  - −3 x! is the error;
  - CLVARx at 0, 56 and 800;
  - CLEAR ALL keeps FIX 4 and RAD and clears a program;
  - the four functions near zero and across their domains.
- app_test:
  - UNDO after +, after CLSTK, after CLEAR ALL and after a short program;
  - UNDO twice is a redo;
  - a shift or a menu opening is not an undo point;
  - CLEAR ALL's confirmation: yes clears, no and any other key cancel.
- My hidden models run every vector set, as usual. Red first, then green, and three mutants of your
  choosing.
