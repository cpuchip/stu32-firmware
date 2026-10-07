# Work unit 041: 33s mode as close to a 33s as possible (the audit's rulings)

**From:** abacus · **To:** soroban · **Status:** next after 040b (abacus decisions 56 and 58, Michael, 2026-10-07)

## Why

Michael: "I feel like the 33s mode should act as close to a 33s as possible". abacus's audit
(abacus docs/audits/2026-10-07-33s-mode-fidelity.md, every item cited to the 33s guide) found 14 extensions
reachable in 33s mode and 13 gaps. He accepted its recommendations as written (decision 58). The general finding:
apart from a few rulings, the firmware doesn't check the mode, so the 35s's features leak into 33s mode.

Each part below can land as its own commit pair. Do them in this order.

## Part A: hidden in 33s mode

These stay in 35s and STU.

| Audit | Feature | The 33s's evidence (the audit's citations) |
|---|---|---|
| A1, A2 | One-value complex numbers: CMPLX's i, θ and ARG entry, complex stack values, and DISP's xiy/rθa. In 33s mode CMPLX is only the prefix (039). | p.9-1 to 9-3; p.1-19 |
| A3 | Vectors ([ ], the "," between elements) | not in the operation index |
| A4 | The built-in 2×2/3×3 solvers in the equation list. The list starts empty, and EQN shows EQN LIST TOP. | p.6-7 |
| A5 | LOGIC (AND, OR, XOR, NOT, NAND, NOR) and the d/h/o/b suffixes, in BASE and in equations | p.10-1 |
| A6 | CLEAR's STK and VARx; CLSTK in STACK | p.1-6 |
| A7 | →km and →mile, on keys and in equations | p.4-13 |
| A11 | REGX…REGT and ARG in equations (SYNTAX, as `i` already is) | p.6-15 |
| A14 | The TABLE and GRAPH menus. In 33s mode they don't open at all, rather than GO refusing. | — |

- **What "hidden" means:**
  - the key or menu item does nothing in 33s mode (or the menu doesn't show the item);
  - an equation typed in 33s mode refuses the feature (SYNTAX);
  - a program line typed in 33s mode can't record it.
- **Existing data stays readable.** A value already on the stack or in a variable, or a line already written,
  keeps working. Say what an existing complex value in 33s mode shows, and propose it.
- **The keymap stays one physical layout.** The mode decides what a legend does. Where a key does nothing in 33s
  mode, propose whether its legend is dimmed in the app and screen.

## Part B: the constants

- **In 33s mode,** CONST shows the 33s's 40 (p.4-8's table), with today's CODATA 2022 values (Michael: "33s's 40,
  current values"). Check the 33s's list against the 35s's: the audit says the 35s adds the math constant e as
  #41. Say exactly which differ.
- **35s and STU** keep the 41.

## Part C: the missing 33s features, added in 33s mode

Also in 35s and STU wherever the 35s has them.

| Audit | Feature | The 33s guide |
|---|---|---|
| B3 | x³ and ∛x. **Placement:** propose. The freed slot (blue above √x, a duplicate ABS since v0) is a candidate. | p.4-2 |
| B4 | ENG→ and ←ENG (shift the shown exponent by 3) | p.1-20 |
| B5 | MEM: bytes free; the VAR catalog; the PGM catalog (run or delete a program from it) | p.1-24, 3-3, 12-20 |
| B7 | FN= from the keyboard (SOLVE or ∫ of a program) | p.14-1 |
| B10 | HYP as a prefix in 33s mode (HYP then SIN), as the 33s has it. The menu stays in 35s and STU. | p.4-6 |

**Checksums** (B6, p.6-18 and 12-21) are not in this unit: they need a byte format, a design of their own.

## Part D: the 33s's equation syntax, in 33s-mode equations (B9)

Per the 33s guide's p.6-5 and p.6-15:
- the colon separates arguments (the 35s's comma in 35s/STU);
- RND takes two arguments;
- CB and CBRT;
- →HR;
- Cn,r and Pn,r;
- RANDOM.

Read the rendered pages for the exact spellings. This rides on 031's stamps: an equation's stamp decides its syntax,
as it decides its grouping.

## Acceptance

- **Each item:** a vector or app_test check that it's hidden or present in 33s mode, and unchanged in 35s and STU.
- **Parts C and D:** values against abacus's oracles.
  - x³ and ∛x are correctly rounded (038's exact cube; ∛x through the hp root).
  - ENG shifts show the guide's examples.
- **No change** to any accepted vector in 35s or STU.
  - Accepted 33s-mode vectors that used a hidden feature are moved to 35s mode or amended in place, saying which,
    with the decision 58 reference.
  - tutor's lessons are checked by their own harness. poly-04 already plans a 33s variant through 039's pairs.
- **Mutants:** a hidden feature leaking back into 33s mode, one per part; HYP's prefix leaking into 35s.

## Part A accepted (abacus, 2026-10-07, at 58f108d)

- **abacus's build** (an export, Babal staged): app_test 520/520; 041a 53/53; 015, 039, 010 and 040 unchanged;
  042's sweep 401/401.
- **Every hidden set** (44 files, about 547,000 vectors) was run on the old core (3b0e722) and the new one, each in
  its own mode. Each vector that newly failed in 33s mode was retried with MODE35 (decision 58).
  - Every newly failing vector in h004, h008, h009, h010 and their round-trip copies passes under MODE35: 87,485
    vectors in all (abacus first wrote 87,522, a sum stated before it was computed). The retried vectors use logic, one-value complex, vectors and →km/→mile.
  - **653 fail both ways,** all in the h001 family. Every one combines CLSTK (hidden in 33s mode) with one of
    decision 22's two 33s/35s differences: E with no mantissa, or the first key after an error. No faithful mode
    can run such a vector. They aren't regressions; abacus's generator will choose one mode per vector.
  - **No other set changed.**
- **abacus's own debt, found on the way:** h005, h011, h012, h014 and one h033 case already failed on the old core.
  Those sets predate later rulings (025's 33s indirect model; 026's overflow warning), plus one ill-conditioned
  graph case (SIN of COSH(209) in DEG). abacus will refresh those generators. None of this is 041's.
- **Rulings (#4799):** (a) migrate the pre-041 33s-stamped equations; (b) as is; (c) soft keys only. Rule 3's 2 and 3
  wait on soroban's read of the guide.

## Unit 043 accepted (abacus, 2026-10-07, at 58f108d)

- app_test 520/520 on abacus's build includes the p.7-6 example keyed as printed (2×2), the 3×3 case, and an
  ordinary equation still prompting, in 35s and STU. Both of soroban's mutants are caught.
- **Rule 3's list:** new key-path app_tests for ∫, TABLE VAR and ISG. Two findings carried: FN='s missing key
  (041 C) and line addresses in 33s mode (a guide read).

## Part A's rulings and Part B accepted; the games list (abacus, 2026-10-07, at 2596588)

- **The build:** a git-archive export beside a stu32-arcade clone, so gen_games.py exports Babal at e695309.
  - app_test 524/524;
  - 041a 62/62 and 041b 6/6;
  - 011, 012, 013, 004 and 040 green;
  - 042's sweep 401/401.
- **3.2 (line addresses):** the 33s guide reads "It is not possible to branch to a specific line number during a
  program" (p.13-4; abacus found the sentence in its own text dump).
  - In abacus's per-vector check, h011 and h012 (programs) now fail in 33s mode almost entirely and pass under
    MODE35. Every one of their 40,000 vectors uses an XEQ or GTO line address.
  - The 653 h001-family vectors are unchanged, and there are no other regressions.
- **3.3 (XEQ while typing),** (a) the v11 migration with the golden v10 image, and (b)/(c) as ruled: accepted.
- **Part B:** 40 constants in 33s mode, 41 in 35s and STU, CODATA 2022 in both: accepted.
- **The games list (46cbe7b):** accepted. The gate's fixes are the right kind: the path whitelist after a
  demonstrated $(shell) injection; one -I per game; the full origin/HEAD ref; symlinks refused; names restricted.
  gen_games_test's refusals are each red on the earlier generator.

## Parts C and D accepted (abacus, 2026-10-07, at 8f49876)

- **abacus's build** (an export beside clones of stu32-arcade and casim): make test green, app_test 600/600.
  - 041a 62/62, 041b 6/6, 041c 47/47, 041d 46/46;
  - 040 54/54, 042's sweep 401/401, cas004 28/28.
- **The per-vector hidden check** against the pre-041 core 3b0e722:
  - the 653 h001-class vectors are unchanged (CLSTK with a decision-22 difference);
  - h015 and h017 newly fail in 33s mode on 688 and 2683 vectors, every one with a 35s spelling in an EQ: token
    (comma, nCr, nPr, RAND, HMS, %), and all pass under MODE35;
  - h014: 5852 such vectors, plus 392 without a 35s spelling, every one of which already failed on the pre-041
    core (abacus's stale syntax-error rows);
  - **no regressions.**
- **Ruled** (abacus #4880, #4883):
  - RND(x:n) is provisional, pending field check 12;
  - strict both ways, with the restamp migration;
  - **editing keeps the equation's stamp** (reversing 031's review F4: a restamp on edit could silently change a
    value, as 2^3^2 would go from 64 to 512);
  - % is refused in 33s equations (p.6-14 has %CHG, no plain %).
- **The S3:** DEVICE PASS 44/44 with cas004-s3 344/344; the stand-in boots with Babal.
