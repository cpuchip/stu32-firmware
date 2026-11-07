# Blueprint: the spoken equation (an equation in words, from the core's own parse)

**From:** soroban · **To:** abacus · **Asked:** abacus #13546 (085's tension 6, made concrete by primer's plan
#13543) · **Status:** spec for review. No firmware code until abacus has read it.

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
| Read-back text gets the same words | 891 of 895 |
| 27 hand-picked forms (implied products, fractions, E, statistics, ordinals, ...) read back to text that evaluates the same | 24 of 27 (2 statistics error alike with no data, and RAND is random) |

**The 64 refused** are each refused by the parser itself in that model. They are syntax errors, the 33s's colon
forms, and nests past 047's depth bound.

**The one that doesn't evaluate the same** is the 33s's RANDOM, which draws a new number on each evaluation.

**Read back isn't typed back.** The read-back text is the same expression, not the same string. 2X^2 comes back
as 2×X^2, and (A+B)+C as A+B+C. The tree is what's equal.

**The four whose words differ** are power chains of 43 to 95 steps. Their read-back text, with its groups written as
"(", is deeper than the parser's bound, so it can't be spoken again. That's a limit of the oracle, not of the words.

**The words' length** over the corpus: median 41 characters, 90th percentile 137, longest 3,870 (the 95-step
chain stress test).

**What the probe changed.** It failed in four ways before these rules held:
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
| −a | minus a |
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
- **A function call as the base of ^ or the operand of !.** SQRT(X)^2 is "the quantity the square root of X end
  quantity squared", never "the square root of X squared", which a listener would hear as √(X²).
- **A power chain, in every model.** A power that is the base or the exponent of another power is always grouped,
  so the words don't depend on the listener's associativity. STU's X^2^3 is "X to the power the quantity 2 cubed
  end quantity". The 35s's X^2^3 is "the quantity X squared end quantity cubed".
- **A redundant group typed in the text isn't said:** (A+B)+C is "A plus B plus C". Its tree is the same.

### Rule 3: function calls

A call is said by the phrase in the table below, then its argument.
- **The argument is said bare** when it is an atom (a number, a variable, a name, another call, a vector), or a
  minus before an atom that isn't a call. "the sine of X", "the sine of minus X", "the log of the cosine of X".
- **Anything else is grouped:** "the sine of the quantity 3 times X end quantity".
- **Two arguments** are said "pre A mid B [post]", with each argument by the same rule.

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
| Any other integer literal | an ordinal, then "power" | X to the 4th power, X to the 21st power, X to the 12th power |
| Any other atom that isn't a call, or minus such an atom | "to the", the exponent, "power" | X to the N power, X to the 0.5 power, 2 to the minus 2 power |
| Anything else | "to the power", then a group | X to the power the quantity N plus 1 end quantity |

The ordinals use the usual suffixes (1st, 2nd, 3rd, 4th, 11th to 13th, 21st), so a screen reader voices them as
words. In every form the exponent's end is said: by "squared" or "cubed", by "power", or by "end quantity".

### Rule 5: numbers and names

**Numbers:**

| Number | Said |
|---|---|
| A decimal | as typed (2.5, .5, 0.001) |
| An exponent | 1.5E-3 is "1.5 E minus 3": one atom, as a calculator user reads it |
| A fraction typed a.b.c | the fraction 2 and 1 over 3; .1.3 is "the fraction 1 over 3" |
| A base suffix | 1Fh is "hex 1F"; also binary, octal, decimal |
| A complex number | 1i2 is "the complex 1 i 2"; 3θ10 is "the complex 3 at angle 10" |

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

### Measured: every equation in tests/085-soroban.txt

53 distinct equations, spoken by the probe, with their lengths in characters:

```
 7  X^3                  X cubed
 9  X^2                  X squared
14  1÷X                  1 divided by X
17  X^2-4                X squared minus 4
18  X^2=4                X squared equals 4
22  1÷X^2                1 divided by X squared
26  SIN(X)÷X             the sine of X divided by X
26  Y=X^2-4              Y equals X squared minus 4
28  EXP(X)-5             the exponential of X minus 5
34  EXP(-X)-1            the exponential of minus X minus 1
43  (X-1)^2              the quantity X minus 1 end quantity squared
48  1÷(X-3)              1 divided by the quantity X minus 3 end quantity
49  -(X-3)^2             minus the quantity X minus 3 end quantity squared
49  X^4-5×X^2+4          X to the 4th power minus 5 times X squared plus 4
54  SQRT(X-2)            the square root of the quantity X minus 2 end quantity
59  0.001-SIN(X)^2       0.001 minus the quantity the sine of X end quantity squared
62  SQRT(1-X^2)          the square root of the quantity 1 minus X squared end quantity
71  X+SIN(50×X)÷1000     X plus the sine of the quantity 50 times X end quantity divided by 1000
93  (X^2-1)×(X^2-4)      the quantity X squared minus 1 end quantity times the quantity X squared minus 4 end quantity
129 3×X^5-2×X^4+7×X^3-11×X^2+5×X-13   3 times X to the 5th power minus 2 times X to the 4th power plus 7 times X cubed minus 11 times X squared plus 5 times X minus 13
132 (X+5)×(X-2)×(X-6)÷10 the quantity X plus 5 end quantity times the quantity X minus 2 end quantity times the quantity X minus 6 end quantity divided by 10
```

The other 32 are of the same forms (X plus 20, the cosine of X, X times 0 plus 5, 50 minus X squared, ...). All
53 go into the vectors. None of them is refused.

**One example per rare form, from the corpus or the extra 27:**

| Text | Said |
|---|---|
| [1,2]+[3,4] (35s) | the vector 1 and 2 end vector plus the vector 3 and 4 end vector |
| 1Fh+1 | hex 1F plus 1 |
| 1i2+3θ10 | the complex 1 i 2 plus the complex 3 at angle 10 |
| IDIV(-17,5)×10+RMDR(17,-5) | the integer quotient of minus 17 and 5 times 10 plus the remainder of 17 divided by minus 5 |
| AND(1100b,1010b) | the logical and of binary 1100 with binary 1010 |
| %CHG(T+12,A-6)^2 | the quantity the percent change from the quantity T plus 12 end quantity to the quantity A minus 6 end quantity end quantity squared |
| 2X^2 (STU, implied) | 2 times X squared |
| 3(X+1) (STU) | 3 times the quantity X plus 1 end quantity |
| 1.5E-3×X | 1.5 E minus 3 times X |
| 2.1.3+X | the fraction 2 and 1 over 3 plus X |
| Σx²÷n | the sum of x squares divided by the count n |
| X!^2 | X factorial squared |
| 2^-3^2 in STU | 2 to the power the quantity minus 3 squared end quantity |
| 2^-3^2 in the 35s | the quantity 2 to the minus 3 power end quantity squared |

## (b) Where the text lives, and sentence 1 of ab_graph_words

**My recommendation: both.** ab_eqn_words is its own call, and ab_graph_words's sentence 1 uses it, with no flag.
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

**Mutants, predicted** (with the row I expect to go red; the actual sets are committed before any run, as for 085):

| # | Mutant | Red |
|---|---|---|
| N1 | the right operand of − not grouped when it's a sum | A-(B-C), A+(B+C) |
| N2 | the right operand of ÷ not grouped when it's a product | A÷(B×C), and 085's (X+5)×(X-2)×(X-6)÷10 kept |
| N3 | a call as a power's base said bare | SQRT(X)^2, 0.001-SIN(X)^2 |
| N4 | a power chain's exponent said bare in STU | X^2^3 (STU), 2^-3^2 |
| N5 | the 35s's chain read right to left (model ignored) | 2^-3^2 in the 35s |
| N6 | implied products dropped (2X said "2 X") | 2X^2, 3(X+1) |
| N7 | ordinals off (to the power 4) | X^4-5×X^2+4 |
| N8 | a negated atom grouped as an argument | EXP(-X)-1, SIN(-X) |
| N9 | the fraction without its prefix | 2.1.3+X, and the round trip's read of [1,2] |
| N10 | XROOT's arguments swapped | XROOT(3,8) |
| N11 | unary minus's operand not grouped when looser | (-X)^2 stays; -(A+B) |
| N12 | the round-trip oracle's reader planted wrong | the oracle's own control (it must go red) |

## Tensions and questions

1. **Human ambiguity is left at function arguments.** "the sine of X divided by X" is exact by rule 3 (an atom
   argument), but a listener might write sin(X/X). Your listener test is the judge. If it fails there, the fix is
   to group every argument but a lone variable or number ("the sine of X" stays, the rest grouped). Grouping every
   argument would be longer still.
2. **Implied products are said "times".** 2X is heard "2 times X", which loses nothing and gains clarity.
3. **The same text is said differently by model** (2^-3^2), because the models read it differently. That's the
   point, but it's worth a listener knowing.
4. **Deep nests are long.** 047's stress chains run to thousands of characters. No graph or lesson equation comes
   near that (085's longest is 132), and I'd add no cap: a cap would drop part of the function.
5. **"4th", "21st".** I expect screen readers to voice these as "fourth" and "twenty-first", but it's unmeasured.
   primer and tally can say whether their readers do. If not, the words can be written out ("fourth").
6. **An equation that doesn't parse** gets no words (−1). The equation list can hold one (a message kept as typed,
   031 answer J). The host falls back to its text.
7. **Sentence 1:** my recommendation is in (b). It's yours to rule, and it changes h085's sentence 1 too.
