# Unit 084: arithmetic on an expression, on the stack (STU)

**From:** abacus #12145, relaying Michael's note on poly-05-lesson (04:10-04:12Z), verbatim: "Could we do symbolic
manipulation on the stack like the hp 50g? 2 then * would take the eqn on y and multiply the whole thing by 2." Today
2 × on an expression is NOT A NUMBER, 082's deliberate placeholder (work/082-expressions-as-values.md §8). This is
082's step 3, item 3 ("arithmetic between expressions and numbers"), which 082 numbered 084 and 083 moved to 085. Its
order against 084, the CAS on the stack, is abacus's and Michael's to set. **Unit 084, VER 0.84 (abacus #12372):** units take
numbers as they land, so the CAS on the stack is 085. Michael's answer (12:59:39Z, via abacus #12365): "Build it as
recommended: as typed, all the keys, both sides of an equation, π and fractions written as such." The blueprint below
is kept as reviewed; what was built follows it under **Built**.

**How it was measured:** a throwaway prototype, a3802f9, local only and never pushed: one function in core/calc.c ahead
of 082's NOT A NUMBER guard, on 640d7be. Every number below comes from it, from casim's CLI at cd7db86, or from the
head. No code goes to main until abacus reviews this and Michael picks the options in §9.

The HP 50g user's guide (books/hp-calculators, p.5-1) says "Algebraic objects can be added, subtracted, multiplied,
divided (except by zero), raised to a power, used as arguments for a variety of standard functions (exponential,
logarithmic, trigonometry, hyperbolic, etc.), as you would any real or complex number." Its results are in screenshots
only, so this blueprint makes no claim about how the 50g writes them.

## 1. What each op does

**Two numbers in, as now.** When an expression (AB_EXPR) or a name (AB_NAME, its letter) is in an operand an op reads,
STU builds a new expression from the operands' texts:

| class | ops | given an expression on X, Y or both |
|---|---|---|
| two-argument | + − × ÷ yˣ | Y op X as one expression; the stack drops as for numbers; LASTx gets X |
| one-argument | SIN COS TAN ASIN ACOS ATAN, the six hyperbolics, LN LOG eˣ 10ˣ √x x² 1/x ABS +/− | f(X) as one expression; X replaced; LASTx gets X |
| everything else in 082's table | IDIV RMDR % %CHG nCr nPr x√y, statistics, tests, STO/RCL arithmetic, bases, IP FP RND, conversions, complex and vector mixing | NOT A NUMBER, as today |

- A number operand is written as text (§3). A name is its letter, so it stays symbolic, as Michael ruled for names
  (s3-names, "otherwise as you're trying to build up an equation it'll collapse on you to a value").
- **An equation (text with `=`) gets the op on both sides:** 2X+5=11, then 2 ×, gives `(2×X+5)×2=11×2`. Two equations
  combine side by side, and a function applies to each side. This is our own choice; no source was found for it.
- Measured on the prototype, through vectors (✓ each its expected text):

| keys (STU) | X after |
|---|---|
| `3×X^2-4` →X, 2 × (Michael's) | `(3×X^2-4)×2` |
| 2, `3×X^2-4` →X, × | `2×(3×X^2-4)` |
| `X^2` →X, 1 + | `X^2+1` |
| 5, `X+1` →X, − | `5-(X+1)` |
| `X+1` →X, 2 ÷ | `(X+1)÷2` |
| `X+1` →X, 2 yˣ | `(X+1)^2` |
| `X^2` →X, `SIN(X)` →X, × | `X^2×SIN(X)` |
| `X^2` →X, SIN | `SIN(X^2)` |
| `X+1` →X: x², 1/x, +/− | `(X+1)^2`, `1÷(X+1)`, `-(X+1)` |
| `2×X+5=11` →X, 2 × | `(2×X+5)×2=11×2` |
| `X` →X, 2 +/−, + | `X+(-2)` |
| `X` →X, √x, x² / LN, eˣ | `SQRT(X)^2` / `EXP(LN(X))` |
| 7 ENTER, `3×X^2-4` →X, 2 × | `(3×X^2-4)×2`, Y=7 |
| … 2 ×, LASTx | X = 2 |
| `X` →X, then MODE 35, 2 × | NOT A NUMBER (35s mode) |

- **On the keys, not only in vectors:** expr082.txt's keys to →X, then the 2 key and the × key, through app.c: after →X,
  X='3×X^2-4'; after 2, Y='3×X^2-4'; after ×, X='(3×X^2-4)×2' (scratch keyprobe).
- **Brackets** are added only where the operand's top operator binds looser than the op, or as tightly on the right of
  − ÷ ^, or where the operand starts with a minus.

## 2. How the result is written: kept as typed, or put through casim's canon

The prototype keeps it **as typed**. casim's `simplify` and `expand` at cd7db86 (STU, 16,384 B), on the same texts:

| as typed | simplify | expand | arena |
|---|---|---|---|
| `(3×X^2-4)×2` | `6×X^2-8` | `6×X^2-8` | 528 B |
| `5-(X+1)` | `-X+4` | `-X+4` | 336 |
| `(X+1)÷2` | `X÷2+1÷2` | `X÷2+1÷2` | 312 |
| `(X+1)^2` | `(X+1)^2` | `2×X+X^2+1` | 168 |
| `-(X+1)` | `-X-1` | `-X-1` | 248 |
| `(2×X+5)×2=11×2` | `4×X+10=22` | `4×X+10=22` | 544 |
| `X×0.1` | `0.1×X` | `0.1×X` | 88 |
| `X^2-X^2` | `0` | `0` | 232 |
| **`SQRT(X)^2`** | **`X`** | `X` | 144 |
| **`EXP(LN(X))`** | **`X`** | `X` | 112 |

- **The tension:** canon changes what the learner built.
  - It reorders (`X×0.1` becomes `0.1×X`) and splits (`(X+1)÷2`).
  - In the last two rows it drops a domain without a row: `SQRT(X)^2` has no value at X = -1, and the simplified X does.
    Simplifying after every key would quietly change an expression's meaning at points.
- **Option A, as typed (recommended):** each key adds exactly what was pressed. Simplifying is a deliberate key: 085's
  SIMP and EXPAND on X, or EVAL. This is the same reasoning as Michael's s3-names.
- **Option B, canon after every op:** tidier text, at the cost above. Each op calls casim (≤ 544 B of arena measured
  here; time not measured on the device).
- **Option C:** as typed, plus a setting that turns canon on. More to explain; not recommended.

## 3. A number written into the text

The prototype writes a number in ALL, unrounded, as `ab_format` prints it (measured):

| number | text | length |
|---|---|---|
| 0.1 | `0.1` | 3 |
| 1 ÷ 3 | `0.3333333333333333333333333333333333` | 36 |
| π | `3.141592653589793238462643383279503` | 35 |
| 1E-20 | `0.00000000000000000001` | 22 |
| 3 ÷ 4 with fraction display | `0.75` | 4 |

- Unrounded keeps the value exact, but a long number eats the 191 bytes.
- **Options for Michael:**
  - (a) unrounded, as measured;
  - (b) π written as `π` when X is π to every digit;
  - (c) with fraction display on, a fraction written as `3÷4`;
  - (d) a number written as the display rounds it (shorter, but it changes the value: not recommended).
- (b) and (c) can go with (a).

## 4. Limits

- **A result past 191 bytes is RESULT TOO LONG, nothing changed** (measured: a 189-byte expression × 2, Y kept).
- **A full expression store is MEMORY FULL, nothing changed:** the new slot is asked for before the stack moves, as →X
  does (082 §7). Mark-and-sweep frees the operands' slots once nothing names them.

## 5. UNDO, save and load, the image

- **No new kind:** a result is an AB_EXPR, so image 21 holds it, and no version bump is needed.
- Measured: seven vectors with the results as expectations pass with a save and load at every step (AB_SAVELOAD=1
  AB_STORE=1, 7/7), a stored and recalled result included.
- UNDO restores images, so it covers the new ops. The build adds an app_test check that UNDO after 2 × gives back
  `3×X^2-4` in Y and 2 in X.

## 6. Arena, flash, stack

- **Option A calls no casim:** arena +0. Option B: §2's column, at most 544 B of the 16,384.
- **D68 of the prototype** (`resources.py static 640d7be a3802f9`):
  - flash +2,640 on the stand-in and +2,672 on Rev A (libmain +2,522 / +2,546);
  - DIRAM, IRAM and calc_task's frame +0; no internal-RAM symbol changed.
- **The prototype's buffers are about 3.6 KB on the calc task's stack**, off the measured deep path. The build sizes them
  to AB_EQN_LEN (192), puts the whole build in one 192-byte scratch, and measures stack-free on the board.

## 7. STU only

- The rule is `ab_rec_model(c) == AB_MODEL_STU`. 33s and 35s modes keep NOT A NUMBER (measured), as they have no →X.
- An expression left on the stack after switching to 35s mode stays NOT A NUMBER there.
- A program stamped STU builds expressions, as its keys would. One stamped 33 or 35 doesn't. This follows from
  `ab_rec_model`, but no program was run on the prototype; the build adds a vector for each.

## 8. What would move

- **The census:** the prototype against 640d7be, every make test line on its own, every vector's image CRC compared.
  - **No vector moves:** 9,188 of 9,188 image CRCs are identical across the 90 vector lines. No vector does arithmetic
    on an expression today.
  - **expr_test, 13 of 314 checks:**
    - 082's NOT A NUMBER checks that use SIN on an expression (8, with "NOT A NUMBER is the message showing");
    - two image checks built the same way, and golden-v21-082, whose setup does one of those ops (906 B written, the
      file is 909 B);
    - the op sweep (48 of 9,440 runs now "read a number") and the op fuzz (8 of 10,000 steps).
    The sweep and the fuzz need a third class, "builds an expression", whose result must parse, beside "moves" and "NOT
    A NUMBER". The others move to an op that stays refused, such as Σ+, so 082's guard keeps its pins.
  - **screen_test:** frames 46-xe-not-number and 47-xe-memory-full, whose setups use SIN on an expression, so 3 checks.
    The same move to Σ+ keeps their pictures.
  - **app_test:** none.
- **+/− today:** with an expression in X, +/− returns OK and changes nothing (image byte-identical, measured at 640d7be).
  082's §8 table puts it under NOT A NUMBER, but it isn't in reads_x. This unit makes it `-(…)`. If the unit waits, a
  one-line fix to 082's guard closes the gap.
- **poly-05** (tutor's docs/proposals/expressions-lesson.md at 1b7040a, a proposal, not yet a lesson):
  - "It isn't a number. Type 2 and press ×. The screen says NOT A NUMBER" turns around.
  - So does exercise 2, "With 7 in X and an expression in Y, predict what × shows".
  - Exercise 1's 2X + 5 = 11, then 2 ×, would show `(2×X+5)×2=11×2`.
  - The paragraph about XEQ giving an expression a value still stands.

## 9. For Michael (through abacus)

1. **Kept as typed, or simplified after every key?** §2, A (recommended) or B.
2. **Which ops?** §1's two lists, or a smaller first set (+ − × ÷ yˣ only).
3. **An equation gets the op on both sides?** §1 (recommended), or INVALID DATA on an equation.
4. **A number in the text:** unrounded, plus π and fractions? §3.

## 10. The oracle for the build

- **A vector per row of §1's table, with its exact text,** and every one also run with AB_SAVELOAD=1 AB_STORE=1.
- **A differential fuzz:**
  - random numbers and expressions, random ops from §1;
  - the built text parsed by eqn.c and evaluated at a random X, against the same ops done on numbers;
  - every built text must also parse in casim (simplify returns OK).
  This catches a missing bracket, which a text expectation only catches where someone thought to write one.
- The expr_test sweep and fuzz with the third class (§8).
- An app_test check on the keys: Michael's 2 × from →X, then UNDO.
- Mutants: no brackets; brackets on the wrong side of − ÷; the op not applied to both sides; STU not checked; LASTx
  not set; the stack not dropped; no length check.

## Notes

- **A harness edge, not a product bug:** a vector that types a number, then `EQ:` and `EQDROP`, leaves the number's
  entry open across →X, because `EQ:` adds the equation without the EQN key. The next digit then extends that entry
  over X. On the keys, opening EQN closes the entry (measured), so the device can't reach it. One line in vectors.c (EQ:
  ends an open entry) would remove the edge. Not changed here.

## Built (soroban, 2026-11-01)

Unit 084 at 97357b9, as the blueprint reads with Michael's four answers: as typed, all the keys, both sides of an
equation, π and fractions written as such. +/- is -(…) in STU (abacus #12365); 082's guard on it stays for 35s mode.

### PREDICT: the mutants, written before any of them ran

Each mutant is one change to core/calc.c (or a test). The prediction is which check goes red. They were committed
before the run, so a surprise is visible as one.

| # | the mutant | predicted red |
|---|---|---|
| M1 | no brackets at all | 084 vectors (Q01's (3×X^2-4)×2 becomes 3×X^2-4×2), the differential fuzz, app_test (Michael's text) |
| M2 | equal-precedence brackets on the left operand, not the right | 084 vectors (Q07 5-(X+1) becomes 5-X+1), the fuzz |
| M3 | an equation: a two-argument op on the left side only | 084 vectors (Q22, Q23, Q26), the fuzz |
| M3b | an equation: a function on the left side only | 084 vectors (Q24, Q25, Q27), the fuzz |
| M4 | STU not checked (33s and 35s modes build too) | 084 vectors (Q40, Q42), 082 vectors (X12, 35s mode's +/-) |
| M4b | ALG not checked | **survives**, I expect: in ALG the keys type into the line and don't reach the dispatch with an expression |
| M5 | LASTx not set | 084 vectors (Q04) |
| M5b | LASTx set to the result | 084 vectors (Q04) |
| M6 | the stack not dropped | 084 vectors (Q03's Y=7), the fuzz (Y compared) |
| M7 | no length check (the text cut at 191) | 084 vectors (Q39) |
| M8 | a name written as its letter's value (the s3-names collapse, abacus #12192) | expr_test ("B×2") |
| M9 | π not recognized | 084 vectors (Q29, Q30) |
| M10 | fraction display ignored | 084 vectors (Q31-Q34) |
| M11 | a fraction written when it doesn't round back to the number | 084 vectors (Q35, 0.6667) |
| M12 | +/- not in the list (082's NOT A NUMBER) | 084 vectors (Q16, Q17) |
| M13 | 10^x not in the list | 084 vectors (Q18) |
| M14 | a sign after a number's E read as an operator | **survives**, I expect: no vector has a negative exponent in E form |

### The mutants' results (17 run at 21:10-21:14Z, each planted line checked in the source and each build checked)

Every prediction held. The 15 predicted reds were red in the places predicted, and M11 was also caught by the fuzz, which
the prediction didn't name. The two predicted survivors survived and are closed:

| # | result |
|---|---|
| M1 | red: 084 vectors (Q01 3×X^2-4×2), app_test, the fuzz (X+1^2=2×X^2 against 16 and 36) |
| M2 | red: 084 vectors (Q07 5-X+1), the fuzz |
| M3, M3b | red: 084 vectors (Q22, Q23, Q26; Q24, Q25, Q27), the fuzz |
| M4 | red: 084 vectors (Q40), 082 vectors (X12) |
| M4b | **survived, as predicted.** `!c->alg` can't be reached: in ALG, SIN types SIN() into the line (a probe through ab_do), so the dispatch never sees an expression. Removed at d2a3cad rather than left unpinned. |
| M5, M5b | red: 084 vectors (Q04) |
| M6 | red: 084 vectors (Q03), the fuzz (Y compared) |
| M7 | red: 084 vectors (Q39) |
| M8 | red: expr_test, "B×2" and "SIN(B)" |
| M9 | red: 084 vectors (Q29, Q30) |
| M10 | red: 084 vectors (Q31-Q33) |
| M11 | red: 084 vectors (Q35 2÷3×X), and the fuzz (2626÷2685 for an inexact number) |
| M12, M13 | red: 084 vectors (Q16, Q17; Q18) |
| M14 | **survived, as predicted.** Q44 (X × 1E-300) now pins it: under M14 it's X×(1E-300), red (d2a3cad) |

The run peaked at 330 MiB RSS.

### What moved, measured

- **Every existing vector's final state** (AB_IMGCRC over the 90 vector lines, 9,196 states, 896aabd against 97357b9):
  identical apart from 082-soroban X12, whose keys this unit moved to 35s mode on purpose.
- **expr_test:**
  - the 8 SIN setups and the two image checks now use Σ+, which is still refused;
  - golden-v21-082 stays byte for byte: build_v21 sets the message's op back to SIN, since the image records it (byte 44);
  - the sweep and the op fuzz have the third class: 456 sweep runs and 75 fuzz steps built an expression, 0 read a number.
- **screen_test:** frames 46 and 47 are set up with Σ+, and their PNGs are unchanged (48/48).
- **app_test:** +5 checks for Michael's keys with UNDO and REDO (846).
- **help:** →X's page says "Then 2 × makes (...)×2." (three lines, as help_test requires of a menu key).

### Found on the way

- **The first build had two bracket bugs the prototype didn't:** 5-X+1 for 5-(X+1), and X+-2 for X+(-2). The case
  table caught them before the fuzz existed, because someone had thought to write those two cases. The fuzz is what
  catches the cases nobody wrote: M1, M2 and M11 show it.
- **A plant that doesn't compile runs the old binary and looks green.** My first three bracket plants failed -Werror
  (an unused variable), and the stale expr_test passed. The runner now stops on a failed build and prints whether the
  plant is in the source.
- **An operand that doesn't parse builds a result that doesn't:** the op fuzz dropped a malformed equation with →X.
  As typed keeps it malformed, and the sweep's third class accepts that only when an operand was already malformed.

### The gate at d2a3cad

- **make test rc 0:**
  - check-version 0.84;
  - 084-soroban 44/44 (50/50), with save and load too;
  - expr_test 368/368, with the differential fuzz at 1,500 cases, 3,830 built, 12,564 places compared, 0 differ, 0 casim SYNTAX;
  - app_test 846/846; screen_test 9,020 and 48/48 frames; help 770,850;
  - standin_replay over 36 scripts (arith084 current); DEVICE PASS 42/42.
- **Decision 68** (`resources.py static 896aabd d2a3cad`, 21:18-21:21Z; the container peaked at 729 MiB):

| resource | stand-in | Rev A |
|---|---|---|
| flash (the app's .bin) | 3,660,816 → 3,664,288, **+3,472** | 3,600,176 → 3,603,632, **+3,456** |
| internal RAM static (DIRAM) | 211,847, +0 | 219,798, +0 |
| IRAM code | 59,022, +0 | 78,086, +0 |
| calc_task's frame, B | 144, +0 | 128, +0 |

  libmain is +3,359 and +3,373, against the prototype's +2,522 and +2,546: the number text (π, fractions), equations
  on both sides, and the length checks. No internal-RAM symbol changed. sym_arith's buffers are 192 + 2×64 B, not
  the prototype's 3.6 KB.
- **The stand-in** (talk.py on the host under `sg dialout`, by-id path):
  - d2a3cad built and flashed 21:21-21:27Z: App version d2a3cad, 3,664,288 B, sha256
    eb87b81a503051fddd0758d572ba4b761d924e0b88d0c5c959dead016c10eaa1.
  - arith084.txt: 13 PASS, and 1 NOT MET, the control (line 77).
  - From the keys: ver=0.84, X='(3×X^2-4)×2', drawn=(3X^2-4)·2, framecrc 95014040, again after SAVE and RESTART. That
    CRC is standin_replay's.
  - Stack-free 28,260 B at its lowest (floor 8,188).
  - Log: work/standin-logs/2026-11-01-arith084-d2a3cad.log.
