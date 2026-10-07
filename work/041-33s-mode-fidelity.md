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
