# Unit 101 built: what the labels say while a row's view is open, and an ALG line's words

**From:** the blueprint work/view-alg-words.md at c50b13f (abacus #16971, from tally #16969), with abacus's rulings in
#16992 and h101 frozen in #16997. h101 comes in two files:
- **h101-A**, sha256 140c760e…0190, 6 vectors, 3/6 at c50b13f;
- **h101-B**, sha256 5cb8c7b4…eab1, 5 vectors, refused whole at c50b13f, where %ALGSAY= didn't exist.

**The build:**
- f4d8b2d: the source and its tests.
- 163ee20: the mutants' predictions and their runner, before any run.
- This commit: eqn_say_test's fix (below), mutant runs 1 and 2, D68, make test, this record.

**VER 0.102.**

## For tally and primer (ruling 3)

`int ab_alg_words(ab_calc *c, const char *text, char *out, int size)` is new in core/eqn.h. It gives an ALG line's
words: the line read as STU reads it, with ANS said as "the last answer". The returns are those of ab_eqn_words: the
length, or −1 with "" when the line doesn't parse.
- **tally:** bind it in native/stu32_ffi.c beside stu_eqn_words (which calls ab_eqn_words at line 359).
- **primer:** bind it in web/calc/calc.c beside its ab_eqn_words.
- The firmware exports nothing new. On the devices it is a stub, as ab_eqn_words is, so no words go into flash.
- **Also:** while a row's view (INFO, SHOW, FRAC) is open, screen_lines' textbook_on is false and drawn is empty, and
  screen_drawn returns false. Their labels should speak X's line, which holds the view.

## What changed

- **A, firmware/screen.c:** compose's view branch also clears `k->xexpr`.
  - The frame already drew the view's line and never X's 2D form.
  - Now screen_lines and screen_drawn agree with it.
- **B, core/eqn.c:**
  - **The words entry:** ab_eqn_words and the new ab_alg_words share one body, say_text. A parser field, `ans`, set
    by ab_alg_words alone, lets ANS parse as a name.
  - **The parser's test:** it is now `alg_eval || ans`.
  - **Unchanged:** ab_eqn_words, as before byte for byte in what it says (ruling 2), checked by K05 and the 086 corpus.

## Tests

- **tests/101-soroban.txt, 14 vectors, 25 expectations.** %ALGSAY= is new in tests/vectors.c: the words of the ALG line
  X's line shows.
  - **A01-A03:** INFO, SHOW and FRAC open over an expression in X give %XDRAWN none, X's line the view's.
  - **B01-B06:** the ALG lines with ANS.
  - **K01-K05:** controls. K04 is the view ended by the next key, the drawn form back. K05 is an equation's ANS+1,
    still "A times N times S plus 1".
  - **On c50b13f** (B's lines through ab_eqn_words in STU, what the core gave): A01-A03 and B01-B06 fail, and K01-K05
    pass (work/view-alg-words-evidence/101-on-c50b13f.txt). The words there were "A times N times S plus 4".
  - **The runs:** plain and in the sanitizer run. No save/load pass: the selection and ALG's line are RAM.
- **eqn_say_test's round trip gains ALG rows** (tests/101-alg-corpus.txt): the 44 ALG lines the vector files state,
  five of them with ANS.
  - Each is said by ab_alg_words, read back, and evaluated with alg_eval set and ANS holding 7.125. The values of
    the original and the read-back text must agree.
  - **097's words in the reader.** The ALG corpus held NCDF(1.96), whose words, "the normal probability below", the
    test's reader didn't know.
    - Unit 097 added five phrases to the core's say table without a spec or a round trip. No 086 corpus line used
      them.
    - The reader now has the five. They are taken from the core, since no spec holds them, so the round trip checks
      that they read back to their function, not that they are the right words.

## Results

- **Mutants, run 1** (work/101-evidence/mutants-run1.txt; host, 1 min 29 s, largest process 116.1 MiB by `time -v`):
  - **M0:** SURVIVED. **A1:** RED, 101 A01-A03 only, as predicted. **B3:** RED, 086-corpus's 35s ANS+1, as
    predicted (the prediction corrected before the run).
  - **B2:** RED, but inexact in where. It went red in 089 (A03), 090 (A01), 096 (A06), work/029 (6 lines), 029b,
    031 and 032b, and on 101's B02, whose ENTER evaluates the line. I had named 029-soroban, which stayed green, and
    predicted 101's B lines green.
  - **B1: a miss.** RED on 101 B01-B06 as predicted, but the ALG corpus stayed green, where I predicted it would
    fail.
    - **The cause was my test.** eqn_say_test set alg_eval for the whole ALG row, the words' call included. The
      parser takes ANS when alg_eval or the field is set, so the words found ANS without ab_alg_words's field, and
      B1's fault was invisible to the round trip.
    - **The fix:** alg_eval is set only around the evaluations.
    - **Shown under B1 by hand, in a scratch worktree:** all five ANS lines fail ("read back as A×N×S+4, which
      evaluates otherwise").
- **Mutants, run 2,** with eqn_say_test fixed (work/101-evidence/mutants-run2.txt; 1 min 21 s, largest process 115.9
  MiB):
  - **B1:** RED on 101 B01-B06 and on the ALG corpus's five ANS lines (2×ANS, ANS^2, ANS+1, ANS+4, ANS×ANS), as
    predicted.
  - **M0, A1, B2 and B3:** as in run 1.
  - The runner built the restored tree again at its end.
- **D68** (`scripts/resources.py static c50b13f f4d8b2d`, work/101-evidence/d68.txt):
  - flash +48 (stand-in) and +16 (Rev A);
  - DIRAM, IRAM and calc_task's frame +0;
  - libmain.a +40 and +24.
  - The containers peaked at 765.4 MiB (35 samples, soroban-* through the shim, 4 GiB cap, --rm).
- **make test** (this record's tree, eqn_say_test fixed; work/101-evidence/make-test.txt, 722 lines): rc 0.
  - 22,274/22,274 vectors and 61,411/61,411 expectations, summing every `n/m vectors` and `n/m expectations` in the
    output. That's aa68826's 22,246 and 61,361 plus 101's 14 and 25, twice (plain and san).
  - **The words:** eqn_say_test, the ALG corpus 44 (36 said and read back, 8 refused), and the 086 corpus 959
    unchanged.
  - **The rest:** screen_test 67/67 frames, the replay's 51 scripts, app_test 917/917.
- **The board:** not run. No frame changed (67/67, the replay's 51 scripts), and the devices have no words.

## For abacus

- **h101-A and h101-B:** to run at this record's head.
- **The vector file:** tests/101-soroban.txt.
