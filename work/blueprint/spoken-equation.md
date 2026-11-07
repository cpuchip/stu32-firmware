# Blueprint: the spoken equation (an equation in words, from the core's own parse)

**From:** soroban · **To:** abacus · **Asked:** abacus #13546 (085's tension 6, made concrete by primer's plan
#13543) · **Status:** revision 2, after abacus's fresh listener (#13551). No firmware code until abacus has read it.

**Revision 2** (abacus #13551, from a fresh listener who heard 15 names once and wrote each down):
- **What the listener got:** 11 right, 2 wrong, and 2 right but flagged ambiguous.
- **The pattern:** where a call's argument ends, and what follows it, isn't heard unless something closes it.
  "the exponential of minus X minus 1" was written exp(−x−1). The listener's words: "the argument seems to run
  to the end of the line".
- **The changes:**
  - a call is closed unless its argument is lone and nothing follows it (rule 3);
  - a unary minus is "negative" (rule 1);
  - ordinals are written as words (rule 4).
- **Ruled:**
  - "times" for implied products;
  - no length cap;
  - sentence 1 of ab_graph_words uses this, and h085 changes with it.
- **Kept, to be heard again by the next listener:** precedence inside a group (one listener heard "X times X
  minus 9" as X(X−9)).
- **The measurements below are revision 2's.**

## The question

How should a screen reader say `X^2-4`, `(X-1)^2` or `SQRT(1-X^2)`, so that a listener writes down the same
function?

A substitution table can't do it. Screen readers often leave "(" unvoiced, so `(X-1)^2` comes out as "X minus 1
squared", which is a different function. The words have to come from the parse, and from one implementation in the
core that learn and the APK both read.

## What was measured first

The probe is throwaway and lives in my scratch folder, never committed: a copy of core/eqn.c with a recorder added,
a driver, and a Python reader that turns the words back into calculator text.

**The corpus** is every equation typed in the vector files (`EQ:` in tests/*.txt and work/*-vectors.txt), 959
distinct, in their models: 659 STU, 247 35s, 53 33s.

| Result | Count |
|---|---|
| Spoken | 895 |
| Refused | 64 |
| Read back to text by the Python reader | 895 of 895 |
| Read-back text evaluates bit for bit the same as the original (the core, the original's model, 4 sets of random values for A to Z) | 894 of 895 |
| Read-back text gets the same words | 890 of 895 |
| 27 hand-picked forms (implied products, fractions, E, statistics, ordinals, ...) read back to text that evaluates the same | 24 of 27 (2 statistics error alike with no data, and RAND is random) |

**The 64 refused** are each refused by the parser itself in that model. They are syntax errors, the 33s's colon
forms, and nests past 047's depth bound.

**The one that doesn't evaluate the same** is the 33s's RANDOM, which draws a new number on each evaluation.

**Read back isn't typed back.** The read-back text is the same expression, not the same string. 2X^2 comes back
as 2×X^2, and (A+B)+C as A+B+C. The tree is what's equal.

**The five whose words differ** are past the parser's depth bound. Four are power chains of 43 to 95 steps, and
one is a 15-deep nest of SIN. Their read-back text, with its groups written as "(", can't be parsed again. That's
a limit of the oracle, not of the words.

**The words' length** over the corpus: median 48 characters, 90th percentile 151, longest 3,870 (the 95-step
chain stress test). Revision 1 was 41 and 137: closing the calls costs about seven characters at the median.

**Also measured:** 20 ordinal edges (0, 1, 11 to 13, 40, 100, 101, 1,021, 12,345, a million, nine digits, and ten
digits) and 23 rare forms all read back to the same value.

**What the probe changed in revision 1.** It failed in four ways before revision 1's rules held:
- **"and" did two jobs.** It closed a vector element and joined a mixed fraction ("2 and 1 over 3"), so "the vector
  1 and 2" read wrong. A fraction now says "the fraction" first.
- **The reader had the 33s's spellings wrong.** That was my reader, not the words.
- **Exponents didn't end.** "X to the power 4 minus 5 times X squared" and "e to the power minus X minus 1" are
  exact for a machine, but a person can't hear where the exponent ends. Exponents now end themselves (rule 4), and
  EXP is "the exponential of".
- **%(200,15)** read "the percentage 200 of 15", which isn't English.

## How it works

**The parse records itself.** The core's recursive-descent parser (eqn.c) runs in check mode, as it does for a
syntax check. Each operation it would apply is recorded in postfix: a number with its text, a variable, a name, a
function, a unary minus, a factorial, a binary operator (with whether its product was implied), a vector, and the
=. The record is rebuilt as a tree, and the words come from the tree.

Because the parse is the evaluator's own, the words follow the equation as the core reads it:
- STU's right-to-left powers (2^-3^2 is 2^(-(3^2)));
- the 33s's and the 35s's left-to-right powers ((2^-3)^2);
- STU's implied products (2X is 2×X);
- unary minus below powers (-X^2 is -(X^2));
- the model's own names.

**The call:** `int ab_eqn_words(ab_calc *c, const char *text, int model, char *out, int size)`, in core/eqn.c and
declared in eqn.h. model is 33, 35 or AB_MODEL_STU.
- It returns the length.
- When the text doesn't parse in that model, it returns −1 with "". The caller then falls back to the text.
- It's pure: check mode computes nothing and changes nothing.
- The recorder holds at most one node per byte (an equation is at most 192 bytes), on the stack, so statics stay
  +0. I'll report the frame at the build.

## (a) The words

The words are the expression written out in order, one word or phrase per symbol. Groups are added only where the
parse needs them. A listener who writes the words down in order, with "(" and ")" for "the quantity" and "end
quantity", gets back the same expression.

### Rule 1: the operators

| Form | Said |
|---|---|
| a + b | a plus b |
| a − b | a minus b |
| a × b, and an implied product | a times b |
| a ÷ b | a divided by b |
| −a | negative a (#13551: it keeps "negative X minus 1" apart from "minus X minus 1") |
| a! | a factorial |
| L = R | L equals R |

### Rule 2: groups

A group is said `the quantity … end quantity`. It is used where the expression said in order would parse
differently, given the grammar's levels (eqn.h). From loosest to tightest, the levels are: =, then + −, then × ÷,
then unary minus, then ^, then !, then atoms.

**Where a group is needed:**
- **The right operand of + or −** when it is itself a sum: A − (B − C) is "A minus the quantity B minus C end
  quantity". So is A + (B + C).
- **The left operand of × or ÷** when it is looser than a product.
- **The right operand of × or ÷** when it is a product or looser: A ÷ (B × C).
- **A unary minus's operand** when it is looser than a unary minus.
- **A factorial's operand** when it is anything but an atom or a factorial.

**Where a group is always written, though the grammar wouldn't need it:**
- **A closed call** (rule 3).
- **A power chain, in every model.** A power that is the base or the exponent of another power is always grouped,
  so the words don't depend on the listener's associativity. STU's X^2^3 is "X to the power the quantity 2 cubed
  end quantity". The 35s's X^2^3 is "the quantity X squared end quantity cubed".
- **A redundant group typed in the text isn't said:** (A+B)+C is "A plus B plus C". Its tree is the same.

### Rule 3: function calls (revision 2)

A call is said by its phrase in the table below, and then its argument.
- **A bare call.** A call is said bare only when its argument is lone (a letter, a number that isn't a fraction or
  a complex, or a name without arguments such as pi) and nothing follows it in its own level: before the end of the
  group, the argument, the side of an =, or the line. That gives "the sine of X", "the square root of X", "the
  sine of X equals 0.5".
- **A closed call.** Every other call is closed: "the quantity" + the call + "end quantity". Its one argument then
  runs to the end quantity, as the listener expects, so it isn't grouped again. That gives:
  - "the quantity the exponential of negative X end quantity minus 1";
  - "the quantity the sine of X end quantity divided by X";
  - "the quantity the square root of 1 minus X squared end quantity";
  - "the quantity the log of the cosine of X end quantity". The cosine there is bare: its argument is lone, and
    the end quantity follows it.
- **A reader's rule.** "the quantity" followed at once by a call's phrase is a closed call. A group whose inside
  is a lone call isn't needed, so it never occurs.
- **Two arguments** are said "pre A mid B [post]". Each argument is bare when it is lone or a negative lone, since
  the next word ends it, and grouped otherwise. The call is bare or closed by the same rule as a one-argument call:
  "the root of index 3 of 8", but "the quantity the integer quotient of negative 17 and 5 end quantity times 10".

| Function | Said | Function | Said |
|---|---|---|---|
| LN | the natural log of | LOG | the log of |
| EXP | the exponential of | ALOG | the antilog of |
| SQ | the square of | SQRT | the square root of |
| INV | the reciprocal of | IP | the integer part of |
| FP | the fractional part of | RND | the rounding of (33s RND(x:n): the rounding of x to n digits) |
| ABS | the absolute value of | SGN | the sign of |
| INTG | the floor of | SIN COS TAN | the sine of, the cosine of, the tangent of |
| ASIN ACOS ATAN | the arcsine of, the arccosine of, the arctangent of | SINH COSH TANH | the hyperbolic sine of, and so on |
| ASINH ACOSH ATANH | the inverse hyperbolic sine of, and so on | →DEG →RAD | the conversion to degrees of, the conversion to radians of |
| HMS→ (33s →HR) | the conversion from hours minutes seconds of | →HMS | the conversion to hours minutes seconds of |
| →L →GAL →MILE →KM →KG →LB →°C →°F →CM →IN | the conversion to liters of, gallons, miles, kilometers, kilograms, pounds, Celsius, Fahrenheit, centimeters, inches | SEED | the seeding with |
| ARG | the argument of | x̂ ŷ | x hat of, y hat of |
| NOT | the logical not of | CB, CBRT (33s) | the cube of, the cube root of |
| IDIV(a,b) | the integer quotient of a and b | RMDR(a,b) | the remainder of a divided by b |
| %CHG(a,b) | the percent change from a to b | XROOT(a,b) | the root of index a of b (XROOT(3,8) is 2: measured) |
| nCr(a,b) (33s Cn,r) | the combinations of a choose b | nPr(a,b) (33s Pn,r) | the permutations of a taken b at a time |
| AND OR XOR NAND NOR (a,b) | the logical and of a with b (or, exclusive or, nand, nor) | %(a,b) | the percentage of a at b percent |

**The angle mode isn't said.** SIN(X) is "the sine of X" in DEG and RAD alike.

### Rule 4: powers

| Exponent | Said | Example |
|---|---|---|
| The literal 2 | squared | X squared |
| The literal 3 | cubed | X cubed |
| Any other integer literal of up to nine digits | its ordinal in words, then "power" | X to the fourth power, X to the twenty-first power, X to the one hundred first power |
| Any other atom that isn't a call, or negative such an atom | "to the", the exponent, "power" | X to the N power, X to the 0.5 power, 2 to the negative 2 power, X to the 1234567890 power |
| Anything else | "to the power", then a group | X to the power the quantity N plus 1 end quantity |

**The ordinals are words in the core** (#13551), so nothing depends on the screen reader. That gives zeroth, first,
second, third, fifth, eighth, ninth, twelfth; twentieth, fortieth; one hundredth, one thousand twenty-first, one
millionth. Hyphens join the tens and ones, as in "twenty-first". In every form the exponent's end is said: by
"squared" or "cubed", by "power", or by "end quantity".

### Rule 5: numbers and names

**Numbers:**

| Number | Said |
|---|---|
| A decimal | as typed (2.5, .5, 0.001) |
| An exponent | 1.5E-3 is "1.5 E negative 3": one atom, as a calculator user reads it |
| A fraction typed a.b.c | the fraction 2 and 1 over 3; .1.3 is "the fraction 1 over 3" |
| A base suffix | 1Fh is "hex 1F"; also binary, octal, decimal |
| A complex number | 1i2 is "the complex 1 i 2"; 1i-2 is "the complex 1 i negative 2"; 3θ10 is "the complex 3 at angle 10" |

**Names:**
- A variable is its letter. The 33s's i is "i".
- π is "pi". RAND is "a random number".
- **The statistics are said as words,** because spoken "b" and "B" sound the same. That gives "s x", "sigma x",
  "x bar", "x bar w", "y bar", "the correlation r", "the slope m", "the intercept b", "the count n", "the sum of
  x", "the sum of x squares" (Σx²), and "the sum of x y products".
- REGX is "register X", and likewise for Y, Z and T.
- (I) is "indirect I", and (J) "indirect J".
- ANS is "the last answer".
- A vector is "the vector a and b end vector".

### Measured: every equation in tests/085-soroban.txt (revision 2, for the next listener)

53 distinct equations, spoken by the probe, each with its length in characters. All 53 read back to the same
value.

```
  7  X^3                                X cubed
  9  X+20                               X plus 20
  9  X+30                               X plus 30
  9  X^2                                X squared
  9  X×0                                X times 0
 11  X-100                              X minus 100
 12  1000×X                             1000 times X
 13  SIN(X)                             the sine of X
 14  1÷X                                1 divided by X
 15  COS(X)                             the cosine of X
 16  TAN(X)                             the tangent of X
 16  X^2+1                              X squared plus 1
 16  X×0+5                              X times 0 plus 5
 17  5-2×X                              5 minus 2 times X
 17  X^2-4                              X squared minus 4
 17  X^2-9                              X squared minus 9
 18  50-X^2                             50 minus X squared
 18  X^2-50                             X squared minus 50
 18  X^2=4                              X squared equals 4
 19  ATAN(X)                            the arctangent of X
 20  LN(X)                              the natural log of X
 20  SQRT(X)                            the square root of X
 21  -1000×X                            negative 1000 times X
 21  X^2-4.285                          X squared minus 4.285
 22  1÷X^2                              1 divided by X squared
 23  X^3-3×X                            X cubed minus 3 times X
 26  Y=X^2-4                            Y equals X squared minus 4
 33  X^2-2×X-3                          X squared minus 2 times X minus 3
 33  X^2=2×X+3                          X squared equals 2 times X plus 3
 43  (X-1)^2                            the quantity X minus 1 end quantity squared
 43  (X-2)^2                            the quantity X minus 2 end quantity squared
 45  X×(X-0.2)                          X times the quantity X minus 0.2 end quantity
 46  X^3-6×X^2+9×X                      X cubed minus 6 times X squared plus 9 times X
 46  X×(X-0.09)                         X times the quantity X minus 0.09 end quantity
 48  1÷(X-3)                            1 divided by the quantity X minus 3 end quantity
 52  -(X-3)^2                           negative the quantity X minus 3 end quantity squared
 52  SIN(X)÷X                           the quantity the sine of X end quantity divided by X
 52  X^4-5×X^2+4                        X to the fourth power minus 5 times X squared plus 4
 53  (X-1)^2+0.01                       the quantity X minus 1 end quantity squared plus 0.01
 54  EXP(X)-5                           the quantity the exponential of X end quantity minus 5
 54  SQRT(4-X)                          the quantity the square root of 4 minus X end quantity
 54  SQRT(X-2)                          the quantity the square root of X minus 2 end quantity
 56  1÷(X^2-4)                          1 divided by the quantity X squared minus 4 end quantity
 59  0.001-SIN(X)^2                     0.001 minus the quantity the sine of X end quantity squared
 60  SIN(X)^2-0.0004                    the quantity the sine of X end quantity squared minus 0.0004
 62  -(X+4)^2+0.001                     negative the quantity X plus 4 end quantity squared plus 0.001
 62  SQRT(1-X^2)                        the quantity the square root of 1 minus X squared end quantity
 63  EXP(-X)-1                          the quantity the exponential of negative X end quantity minus 1
 71  X+SIN(50×X)÷1000                   X plus the quantity the sine of 50 times X end quantity divided by 1000
 93  (X^2-1)×(X^2-4)                    the quantity X squared minus 1 end quantity times the quantity X squared minus 4 end quantity
115  ABS(X×X-9)-(X×X-9)                 the quantity the absolute value of X times X minus 9 end quantity minus the quantity X times X minus 9 end quantity
132  (X+5)×(X-2)×(X-6)÷10               the quantity X plus 5 end quantity times the quantity X minus 2 end quantity times the quantity X minus 6 end quantity divided by 10
134  3×X^5-2×X^4+7×X^3-11×X^2+5×X-13    3 times X to the fifth power minus 2 times X to the fourth power plus 7 times X cubed minus 11 times X squared plus 5 times X minus 13
```

**One example per rare form** (revision 2; all 23 read back to the same value):

| Text | Said |
|---|---|
| [1,2]+[3,4] (35s) | the vector 1 and 2 end vector plus the vector 3 and 4 end vector |
| 1Fh+1 | hex 1F plus 1 |
| 1i2+3θ10 | the complex 1 i 2 plus the complex 3 at angle 10 |
| IDIV(-17,5)×10+RMDR(17,-5) | the quantity the integer quotient of negative 17 and 5 end quantity times 10 plus the quantity the remainder of 17 divided by negative 5 end quantity |
| AND(1100b,1010b) | the logical and of binary 1100 with binary 1010 |
| %CHG(T+12,A-6)^2 | the quantity the percent change from the quantity T plus 12 end quantity to the quantity A minus 6 end quantity end quantity squared |
| nPr(5,2)+1 | the quantity the permutations of 5 taken 2 at a time end quantity plus 1 |
| XROOT(3,8) | the root of index 3 of 8 |
| LOG(COS(X)) | the quantity the log of the cosine of X end quantity |
| SIN(X)=0.5 | the sine of X equals 0.5 |
| 2X^2 (STU, implied) | 2 times X squared |
| 3(X+1) (STU) | 3 times the quantity X plus 1 end quantity |
| 1.5E-3×X | 1.5 E negative 3 times X |
| 2.1.3+X | the fraction 2 and 1 over 3 plus X |
| Σx²÷n | the sum of x squares divided by the count n |
| X!^2 | X factorial squared |
| -X^2 | negative X squared |
| (-X)^2 | the quantity negative X end quantity squared |
| -(A+B) | negative the quantity A plus B end quantity |
| A-(B-C) | A minus the quantity B minus C end quantity |
| A÷(B×C) | A divided by the quantity B times C end quantity |
| 2^-3^2 in STU | 2 to the power the quantity negative 3 squared end quantity |
| 2^-3^2 in the 35s | the quantity 2 to the negative 3 power end quantity squared |

## (b) Where the text lives, and sentence 1 of ab_graph_words

**Ruled (#13551): both.** ab_eqn_words is its own call, and ab_graph_words's sentence 1 uses it, with no flag.
- **A separate call** because equations are heard outside the graph too: the equation list, the equation line,
  and an expression in X (unit 082). The hosts already label those from screen_lines' text, and could label them
  from this instead.
- **Sentence 1 uses it** because ab_graph_words exists only for a listener. A name in symbols there is exactly the
  defect this unit fixes, and a flag would double the contract for a form no listener wants.
- **The new sentence 1:** "Graph of X squared minus 4." For the L−R form: "Graph of X squared equals 4, left side
  minus right side." If ab_eqn_words returns −1 (it can't here, since GRAPH draws only an equation that
  evaluates), the name falls back to the text.
- **The cost:** sentence 1 changes in every row with a %GSAY, both in tests/085-soroban.txt and in h085. The rest of
  each text is untouched. I'd regenerate tests/085-soroban.txt's sentence 1 from the build, and check each change
  by hand against this spec's table for those 53 equations.

## (c) Vectors, the oracle and mutants

**The token.** `%EQSAY=<text>` speaks the equation shown (eqn_cur) in its stamped model. Like %GSAY, it takes the
rest of the line.

**The vectors.** A file tests/086-soroban.txt, red first against an empty stub:
- the 53 equations of 085;
- the table's forms, one row each: every function phrase, every name, each number form, vectors, both chain
  directions, implied products;
- the 27 extra forms above.

tests/085-soroban.txt's %GSAY rows change in sentence 1 only.

**My oracle,** kept beside the vectors and not in make test: the round trip. Words are read back to text by a
reader that knows only this spec's grammar, and the core evaluates both. The probe's results: 895/895 read, 894
bit-identical (RANDOM aside), 891 the same words (4 too deep to re-parse). At the build it becomes a host test
over the corpus, in make test. The reader would be a small C reader in tests/, so no Python enters the check.

**Your hidden set** (your reader and your listener) is independent of mine.

**Mutants, predicted** (the exact sets are committed before any run, as for 085):

| # | Mutant | Red |
|---|---|---|
| N1 | the right operand of − not grouped when it's a sum | A-(B-C), A+(B+C) |
| N2 | the right operand of ÷ not grouped when it's a product | A÷(B×C) |
| N3 | a call left bare when something follows it (revision 2's rule off) | SIN(X)÷X, EXP(X)-5, 0.001-SIN(X)^2 |
| N4 | a call left bare when its argument isn't lone | SQRT(1-X^2), LOG(COS(X)) |
| N5 | a closed call's argument grouped again | the same rows, said "the sine of the quantity ..." inside |
| N6 | a power chain's exponent said bare in STU | X^2^3 (STU), 2^-3^2 |
| N7 | the 35s's chain read right to left (the model ignored) | 2^-3^2 in the 35s |
| N8 | "minus" for a unary minus | -1000×X, -(X-3)^2, EXP(-X)-1 |
| N9 | ordinals as digits ("4th") | X^4-5×X^2+4, X^21 |
| N10 | the fraction without its prefix | 2.1.3+X |
| N11 | XROOT's arguments swapped | XROOT(3,8) |
| N12 | "equals" not closing a side (the call before it closed) | SIN(X)=0.5 |
| N13 | the round-trip oracle's reader planted wrong | the oracle's own control (it must go red) |

## Tensions and questions (revision 2)

1. **Does "equals" close a call?** SIN(X)=0.5 is "the sine of X equals 0.5", with the call bare. I treat "equals"
   as closing a side, as end quantity closes a group. If the listener hears sin(X=0.5), N12's form ("the quantity
   the sine of X end quantity equals 0.5") is one rule away.
2. **Precedence inside a group** stands as you ruled: "X times X minus 9" is X²−9, for the next listener to judge.
3. **Closed calls cost words.** The median grows from 41 to 48 characters, and 085's longest name from 132 to 134.
   SIN(X)^2 is "the quantity the sine of X end quantity squared", where a person would say "sine squared of X". I
   kept one form for every call, rather than a special one for powers of trig functions.
4. **The angle mode isn't said;** the graph's own window sentence carries the numbers.
5. **An equation that doesn't parse** gets no words (−1). The host falls back to its text.
