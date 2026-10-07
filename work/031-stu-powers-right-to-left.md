# Work unit 031: in STU, a chain of powers groups right to left

**From:** abacus · **To:** soroban · **Status:** queued after 030 (abacus decision 52, Michael, 2026-10-07)

## Why

Michael ruled: "I think 2^3^2 On stu mode should be 512 … right to left: 512. Where as the hp 35s I think would just
do it left to right or 64". 512 is the textbook's reading, 2^(3^2), and STU is the student's mode. It is also the 50g's:
its Advanced User's Reference, p.E-5, lists "Power (^) from top to bottom (i.e. 2^2^3^4 = 2^(2^(3^4)))"
(books/hp-calculators/hp50g-aur-en.pdf, fetched 2026-10-07).

The 33s and 35s modes keep left to right (014 O03: 64). None of the three HP guides says how a chain of powers
groups, so the 35s's behaviour waits on a field check on Michael's own 35s.

The decision names the tension with your #4251: grouping now depends on the mode. It's one switch in one parser,
not two parsers.

## Rules

1. **In STU, `^` groups right to left:** a^b^c is a^(b^c). This holds on the ALG line, in Equation mode, and in the
   equation lines of a label stamped STU.
2. **The 33s and 35s modes are unchanged:** left to right, and 014 O03 stays.
3. **An equation means what it meant where it was typed.** An equation in the list, or a program's equation line,
   gives the same value whichever personality is in force when it is evaluated. 2^3^2 typed in STU gives 512 when
   evaluated in 35s mode, and 2^3^2 typed in 35s mode gives 64 in STU.
   - Two routes; choose and say why:
     - **(a)** stamp each equation with its personality, as decision 45 stamps labels;
     - **(b)** in STU, store a chain with its grouping made explicit (2^3^2 is kept as 2^(3^2)).
   - Route (b) changes no 33s/35s text. But a chain typed in 33s/35s and evaluated in STU still needs (a)'s
     stamp, or the same rewrite the other way, which would put parentheses the real 35s never shows. So (a) looks
     like the honest route. Measure the state image's cost.
   - Whichever you choose, the screen must not show the same text meaning two different numbers. If an equation
     typed in one mode is shown in the other and its chain would read differently, mark it (as `·STU` marks a
     label), or show the grouping.
4. **Precedence is otherwise unchanged:**
   - power before unary minus (−2^2 = −4);
   - 2^−2 = 0.25;
   - everything else from 029's Answers.

## Acceptance

- **Your vectors, red first:**
  - 2^3^2 = 512 on STU's ALG line and in STU Equation mode;
  - 64 in 33s and 35s modes;
  - rule 3 both ways;
  - an STU-stamped program line;
  - a four-high chain, 2^1^2^3: 2^(1^(2^3)) = 2 in STU, ((2^1)^2)^3 = 64 in 33s and 35s;
  - −2^2 still −4 in every mode.
- **abacus's hidden ALG set** is rerun with its oracle grouping right to left. gen_029 gains the switch, and its
  self-check runs through STU Equation mode first.
- **Mutants:** right to left leaking into 35s mode; rule 3 broken (an equation re-read by the mode in force).
- **No change** to any accepted vector. 029's L12 (2^3^2 = 64 in STU ALG) is amended in place, as this ruling
  supersedes it, and says so.
