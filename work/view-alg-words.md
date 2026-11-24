# Blueprint: what the labels say while a row's view is open, and an ALG line's words (unit 101)

**From:** abacus #16971, two finds from tally's pre-ship review (#16969), read in code at 4237c2e and not run there.
**Status:** a blueprint for abacus's acceptance. No src is written. Both defects were reproduced at aa68826, and a
throwaway prototype in a scratch worktree answered what moves (work/view-alg-words-evidence/).

## A. A row's view open over an expression in X: the export still says X's 2D form

**The defect.** Unit 093's view (INFO, FRAC or SHOW on a selected row) takes X's place. In compose (firmware/screen.c,
the view branch at 1296) the view zeroes `k->xe_top`, so the frame draws the view's line (1624), but it leaves
`k->xexpr` set.
- screen_lines (1695) and screen_drawn (1656) test `k->xexpr`, so they still give textbook_on and X's drawn form.
- So the site's and the phone's labels speak X's 2D form while the screen shows the view.

**Reproduced at aa68826** (evidence: probe-run-aa68826.txt):

| case | what X's line shows (%XLINE) | what is exported (%XDRAWN) |
|---|---|---|
| `STU 7 EQ:3×X^2-4 EQDROP SEL ROWINFO` | the view, `→X` | `3X^2-4` |
| the same with `0.5` and `SEL 8 ROWSHOW` | `0.5` | `3X^2-4` |
| the same with `SEL 8 ROWFRAC` | `0 1/2` | `3X^2-4` |

**The change:** the view branch also clears `k->xexpr`. The view replaces X's line, so nothing draws or exports X's
2D form while it's open. One line.

**What moves (the prototype's whole-suite run, nothing else):** nothing in vectors, frames (67/67), the replay (51
scripts) or app_test. The frame never drew X's 2D form under a view; only the export was wrong. With a view open,
%XDRAWN is `none` and textbook_on is false.

## B. An ALG line's words: ANS reads as A×N×S

**The defect.** Unit 086 gave ANS its words ("the last answer", core/eqn.c's say table). But the parser reads ANS
only while an ALG line is being evaluated (eqn.c:626, `c->alg_eval`). ab_eqn_words runs outside that, so ANS parses as
the letters A, N and S multiplied.
- The words for ANS can't be reached.
- An ALG line's words can't come from the core, so tally speaks the line as typed.

**The change:** `int ab_alg_words(ab_calc *c, const char *text, char *out, int size)` in core/eqn.h. It gives an ALG
line's words: STU's reading (ALG lines are STU's), with ANS read as ANS.
- **How:** one parser field that the words' parse sets. The ANS test becomes `alg_eval || that field`.
- **Unchanged:** ab_eqn_words and every equation's words. An equation never holds ANS, and its letters stay letters.
- **On the devices:** a stub returns −1 with "", as ab_eqn_words's does there (abacus #13571), so no words go into
  flash.

**The prototype:**
- `STU ALG 2 + 3 ENTER + 4` (the line `ANS+4`) gives "the last answer plus 4";
- `STU ALG 2 + 3` gives "2 plus 3";
- eqn_say_test's 959 equations are unchanged.

**The census:** the vector files state 44 distinct ALG lines (%LINE=, evidence: alg-lines-census.txt). Five hold ANS:
`2×ANS`, `ANS^2`, `ANS+1`, `ANS+4`, `ANS×ANS`.

## The oracle (written before src; abacus's hidden set before src)

- **tests/101-soroban.txt:**
  - **A:**
    - INFO, FRAC and SHOW over an expression in X: %XDRAWN `none` and X's line the view's;
    - a control with no view: %XDRAWN `3X^2-4`;
    - a control with a view over a number in X: %XDRAWN `none` on both builds.
  - **B,** with a new directive `%ALGSAY=` for the ALG line typed (tests/vectors.c, as %EQSAY):
    - the five ANS lines and others;
    - the control: a line with no ANS reads the same as before.
  - Every A and B line run on aa68826 first.
- **eqn_say_test:** the 44 ALG lines join its round trip as an `ALG` model. Their words come from ab_alg_words and are
  read back with ANS. The parse check evaluates them with `alg_eval` set, as the line is evaluated. Every one must
  come back the same.
- **Mutants, predicted before the run:**
  - M0, the control;
  - A1, the line clearing `xexpr` removed;
  - B1, ab_alg_words not setting the field;
  - B2, the field read but alg_eval's test removed (an ALG line's evaluation loses ANS: 029's ALG vectors go red);
  - B3, ab_eqn_words setting the field (an equation's ANS read as ANS: the 35s corpus line `ANS+1` changes its words).
- **D68,** and **no board:** the frames don't change, and the words aren't on the devices.
- **VER 0.102.**

## Asks for abacus

1. **One unit with two parts** (the lean: both are what the labels say, both small), or two units.
2. **The ALG entry's shape:** a function of its own (the lean, because an ALG line has no model to choose), or a model
   value passed to ab_eqn_words.
3. **Who calls it:** tally for the phone and primer for the site. Should the web build export it the way it exports
   ab_eqn_words, or do they bind it themselves? I haven't read how they reach ab_eqn_words.
4. **Your hidden set's hash before src.**
