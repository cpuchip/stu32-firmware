# Work unit 031: STU's parser: powers right to left, and implied multiplication

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

### Part two: implied multiplication (abacus decision 53, Michael, 2026-10-07)

Michael: "Oh im pretty sure the hp 50g does implicit multiplication and its nice." The precedent we can cite is
the HP Prime's User Guide, p.26: "If you enter AB, for example, the result is A*B", and "the calculator adds the
operator to the expression in history". None of the 50g's manuals describes it.

5. **In STU, two operands side by side multiply:**
   - a number before a variable, a parenthesis or a function: `2A`, `2(3)`, `2SIN(X)`;
   - a closing parenthesis before an operand: `(A+1)(A−1)`, `(A+1)B`;
   - a variable before a parenthesis or a function: `A(B+1)`, `XSIN(X)` meaning X×SIN(X);
   - two variables: `AB`.

   The 33s and 35s modes keep SYNTAX.
6. **An implied product ranks exactly as a typed ×:** 1/2A is (1/2)×A = A/2, and 2A^2 is 2×(A^2). This is the
   Prime's "the result will be the same" as with the operator.
7. **The × is written in, as the Prime does.**
   - When an STU line is entered (=) or an STU equation is stored, the text keeps an explicit × at each implied
     product (`2A` becomes `2×A`). The history and the equation list show what was computed.
   - This also settles rule 3 for implied products: the stored text means the same in every mode.
8. **Tokens.**
   - **E right after digits stays the exponent:** `2E3` is 2000, and `2E` is an unfinished exponent, as today. The
     variable E next to a number needs its × written.
   - **A known name followed by its parenthesis is that name:** `SIN(` is SIN, `IP(` is IP. Otherwise a run of
     letters is one variable per letter.
   - Propose any case these leave ambiguous, and don't guess (`ASIN(` is the arcsine, not A×SIN: say how you read
     a name that ends inside a longer one).

## Acceptance

- **Your vectors, red first:**
  - 2^3^2 = 512 on STU's ALG line and in STU Equation mode;
  - 64 in 33s and 35s modes;
  - rule 3 both ways;
  - an STU-stamped program line;
  - a four-high chain, 2^1^2^3: 2^(1^(2^3)) = 2 in STU, ((2^1)^2)^3 = 64 in 33s and 35s;
  - −2^2 still −4 in every mode;
  - each form in rule 5, rule 6's 1/2A and 2A^2, rule 7's stored text, rule 8's 2E3 and names, and SYNTAX for
    `2A` in 33s and 35s modes.
- **abacus's hidden ALG set** is rerun with its oracle grouping right to left. gen_029 gains the switch, and its
  self-check runs through STU Equation mode first.
- **Mutants:**
  - right to left leaking into 35s mode;
  - rule 3 broken (an equation re-read by the mode in force);
  - an implied product ranked above ÷ (1/2A = 1/(2A));
  - the × not written into the stored text;
  - implied multiplication leaking into 35s mode.
- **No change** to any accepted vector. 029's L12 (2^3^2 = 64 in STU ALG) is amended in place, as this ruling
  supersedes it, and says so.

## Answers to soroban's #4316 (abacus, 2026-10-07)

**A to F and H to K are accepted as proposed:**
- **A.** Route (a), stamps. Each stored equation carries 33, 35 or STU, and evaluation reads the stamp. Image
  version 7. Older images stamp every equation left to right, which is faithful, since STU grouped left to right
  before 031.
- **B.** On a restamp, the equation's own stamp wins (rule 3).
- **C.** The mark is shown only for a chain under the other grouping: `2^3^2 ·STU` or `2^3^2 ·35`.
- **D.** An implied × only before a letter-start operand (a variable, a name with its "(", a constant name) or
  "(". A number never follows implicitly (A2, (A+1)2 and 2 3 stay SYNTAX), and a vector isn't implied.
- **E.** The longest known name with its "(" wins: ASIN( is the arcsine, AASIN( is A×ASIN(, ALOG( is 10^x.
  tutor will be told that a name can hide a product.
- **F.** 2EXP(1) is SYNTAX. Write 2×EXP(1).
- **H.** No implied × across a space.
- **I.** If writing the × in would overflow AB_EQN_LEN, the entry is refused with the editor-full status, and
  the text is unchanged.
- **J.** A message is stored as typed, stamped STU.
- **K.** On the ALG line, the × is written into the line and the history entry at =.

**G, changed.**
- As proposed, `10b` would be binary 2 while `12b` was 12×b. That's the same letter read two ways depending on
  the digits before it, which is a trap for a student.
- **Ruled:** no implied × before the base-suffix letters b, o, d and h. Those keep today's reading exactly: a based
  number when the digits fit, else SYNTAX. Write 12×b for the intercept.
- The statistics names that are not suffixes (n, m, r) do take an implied ×: 2n is 2×n.
- Pin 10b, 12b (SYNTAX) and 2n in vectors.
