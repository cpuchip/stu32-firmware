# Work unit 043: SOLVE on a built-in solver, from the keys, prompts A at once

**From:** abacus · **To:** soroban · **Status:** small. Do it at 041's next part boundary. (It was found by tutor's
student run, #4778, at d75fc75.)

## The defect

The 35s guide, p.7-6, "Solving built-in Equation": with "2*2 lin. solve" or "3*3 lin. solve" selected, pressing
SOLVE requests the variables A to F (or A to L) in turn. No letter is keyed. Vector M21 (015) pins that, and it
passes, but only on the vector runner's token path.

**On the device's key path** (app_key, as tutor's keyrun drives it, in 35s and STU):
- GOLD EQN ▲ ▲ (2*2 lin. solve), then GOLD SOLVE, shows "SOLVE _" and waits for a letter.
- A learner keying the guide's example as printed (1 R/S 2 R/S 5 R/S 3 R/S 4 R/S 11 R/S) has the 1 taken as the
  letter Y. A? then appears, and each later input shifts one place.
- The result is X = 0.3333, Y = 2.5000, with no warning. The answer should be x = 1, y = 2.
- With SOLVE then ENTER, or any letter, the prompts start at A and the answer is right.

## Rules

1. **On a built-in solver, SOLVE from the keys goes straight to A's prompt,** in every mode that has the solvers
   (35s and STU; 041 hides them in 33s mode). It is the same as the token path, and the same as the guide.
2. **SOLVE on an ordinary equation still asks for the unknown,** as now.
3. **Find the class, not only this case.** List every key whose token path in tests/vectors.c bypasses something
   the app's key path does: a prompt, a menu, a shift. For each, add an app_test that drives the real keys to the
   vector's result, or say why none is needed. Report the list.

## Acceptance

- **app_test:**
  - the guide's p.7-6 example keyed as printed (GOLD SOLVE 1 R/S 2 R/S 5 R/S 3 R/S 4 R/S 11 R/S) gives x = 1 and
    y = 2, in 35s and STU;
  - the 3*3 case, likewise;
  - SOLVE on an ordinary equation still prompts for the unknown.
- **Red first** at the current head.
- **Mutants:**
  - the skip applied to ordinary equations;
  - the skip missing in STU.
- **tutor:** unit 8's solver lesson is written against it, in 35s and STU.
