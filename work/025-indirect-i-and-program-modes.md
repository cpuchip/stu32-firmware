# Work unit 025: the 33s's indirect (i), and each program keeps the mode it was written in

**From:** abacus · **To:** soroban · **Status:** ready. Michael ruled on 2026-10-06 (abacus decision 45): "33s mode
uses the 33s model". With that, programs behave differently by mode, so his idea comes with it: "should we save
what mode it was stored in? ... let them switch which mode the program itself runs under".

## Why

The manual review (abacus docs/evidence/2026-10-06-manual-review-33s-32sii; your work/024-review) found the one
divergence that silently changes results. On the 33s, `1 STO i, 5 STO (i)` stores in A. Here it writes unnamed
register 1, with no error, so a 33s program runs and uses different storage.

## Rules

1. **33s mode's indirect model** (33s p.3-7, p.13-20 to 13-21, F-2):
   - The index is **i**, a 27th variable separate from the letter I. It holds a real number and is stored, recalled
     and cleared like the letters (CLVARS clears it; it is in the state image).
   - Only the absolute value of i's integer part counts:
     - |i| 1-26 address A-Z;
     - 27 addresses i itself;
     - 28-33 address n, Σx, Σy, Σx², Σy², Σxy, in that order;
     - 0, a magnitude under 1, or 34 and up is **INVALID (i)** (the message reads `INVALID (i)`).
   - There are no unnamed registers in 33s mode.
   - (i) works wherever the 35s takes (I): STO, RCL, storage arithmetic, x↔, VIEW, INPUT, ISG/DSE, and SOLVE and ∫
     of (i). XEQ (i), GTO (i) and FN= (i) take labels ±1-26 (33s p.13-21).
   - The keys: in a variable prompt in 33s mode, the keys that give I-indirect and J-indirect in 35s mode give
     **i** and **(i)** instead. Show it on the prompt (`STO (i)`). Propose the exact keys; the 10" panel can try
     them first.
2. **35s mode is unchanged:** I and J, negatives for the letters, 0-800 unnamed (35s p.14-21/22).
   - **The unnamed registers survive a switch to 33s mode** (they are memory, not mode), but 33s mode can't reach
     them.
   - **Switching modes never moves data.** i and the unnamed registers both stay where they are.
3. **Each program label carries a mode.**
   - When a LBL line is entered, the label is stamped with the mode in force (33s or 35s). The listing shows it on
     the LBL line (`LBL A ·33`).
   - Lines before the first label run under the calculator's mode.
4. **A program runs under its label's mode.**
   - **When it starts:** whenever execution enters a label (XEQ, GTO, R/S from a label's line, and SOLVE or ∫ of an
     FN=), the label's mode governs every mode-dependent behaviour while it runs. Today that is only the indirect
     model (rule 1); later rulings add more. That includes reading `c->m35`.
   - **Calls:** the mode travels with the return stack. A 35s label calling a 33s label runs the callee under 33s
     rules, and its RTN restores the caller's mode. An error stop or an interrupt leaves the running label's mode
     in effect until the program is resumed or abandoned.
   - **Keyboard behaviour always follows the calculator's mode:** E with no mantissa, and the first key after a
     message (unit 017 and 024). That includes number lines typed into a program.
   - Keep one function that says which mode is in force, so the next ruling has one place to look.
5. **Changing a label's mode.**
   - In program entry, with the pointer on a LBL line, the MODE menu's 33s/35s keys restamp that label instead of
     the calculator.
   - If the label's code (up to the next label) uses an indirect operation, show the notice `(i) MEANS ...` or
     similar, briefly, so the user knows its storage changes. Propose the wording.
   - It isn't programmable.
6. **The state image** (version 5):
   - It carries i and each label's mode.
   - An image from before this unit loads with every label stamped with that image's calculator mode (the best
     guess at how it was written) and i = 0.

## Vectors and amendments

- **Your vectors.** Add `STO:i`, `RCL:i`, `STO:(i)`, `RCL:(i)` (and the arithmetic forms) to tests/vectors.c.
- **Must turn green, in 33s mode.** The review vectors on this:
  - stack I01-I09 and M27;
  - programs Q45, Q46, Q48-Q51, Q53-Q59 and S07;
  - cxbasestat S31, S32, G01, G02.

  Amend them to the new tokens where they used I as a stand-in, noting the amendment in place.
- **The default changes.** It is 33s mode (decision 22), so the existing indirect vectors written from the 35s guide
  (units 005, 012, 013 and the rest) now need `MODE35` at their start. Amend them in place with a note pointing
  here.
- **New vectors:**
  - a 35s-mode label called from a 33s-mode label and back, each indirecting through its own model;
  - a restamped label;
  - an image saved with stamped labels and loaded back;
  - a version-4 image loading with its labels stamped from its mode;
  - SOLVE of a 33s-mode FN= whose function uses (i).
- **Mutants, each caught:**
  - (i) using the 35s map in 33s mode;
  - a callee's mode not restored at RTN;
  - a label stamped with the wrong mode at entry;
  - a pre-025 image loading with labels unstamped.
- **Regression:** report all five work/024-review folders before and after. Only the vectors named above should
  change.
