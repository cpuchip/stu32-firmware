# Work unit 039: the 33s's CMPLX operations on stack pairs (33s mode)

**From:** abacus · **To:** soroban · **Status:** queued after 036b (abacus decision 45, "Add them"; decision 56,
Michael, 2026-10-07: "the 33s mode should act as close to a 33s as possible")

## Why

The real 33s has no complex-number values. It works on complex numbers as pairs on the stack. The 33s guide,
chapter 9 (p.9-1 to 9-3), read:
- "To enter a complex number: 1. Type the imaginary part. 2. Press ENTER. 3. Type the real part."
- "To do a complex operation, press [CMPLX] before the operator."
- The operations are +, −, ×, ÷, z₁^z₂, −z, 1/z, ln z, eᶻ, sin, cos and tan.
- "(The first line is the imaginary and the second is the real part.)"
- "For two-number operations, the first complex number, z1, is replicated in the stack's Z and T registers."

Decision 45 ruled these in. No unit built them. tutor's poly-04 then showed that 33s mode offers the 35s's x i y
instead.

## Rules

1. **In 33s mode, CMPLX is a prefix.** CMPLX then an operator applies that operator to complex numbers held as
   pairs:
   - **one number:** z = X + iY (X real, Y imaginary);
   - **two numbers:** z1 in Z and T, z2 in Y and X, as the guide's figure on p.9-2 shows. Read the rendered page
     for exactly which register holds which part.
   - The operators are +, −, ×, ÷, yˣ (z₁^z₂), +/− (−z), 1/x, LN, eˣ, SIN, COS and TAN.
   - The result is left as a pair: real in X, imaginary in Y. The rest of the stack is as the guide's figure shows,
     including z1 replicated in Z and T.
   - LASTx as the guide says, if it says.
2. **Accuracy:** each result part is within the core's existing complex accuracy (unit 009's rules). The pair form
   is only a different packaging of the same arithmetic, so route it through the existing complex engine.
3. **The polar form** (33s p.9-5 "Using Complex Numbers in Polar Notation"): read it and do what it says. →POL and
   →REC on pairs are likely what it uses.
4. **Programs:** the CMPLX operations are programmable on the 33s if its program index says so. Read it.
5. **Undecided, waiting on Michael's audit (decision 56):** whether 33s mode keeps or loses the 35s's x i y entry
   and complex values. This unit adds the 33s's way and removes nothing.
6. **The display:** a pair is just two real numbers on Y and X, as the guide shows ("the first line is the imaginary
   and the second is the real part").

## Acceptance

- **The guide's own examples** as vectors: sin(2 + i3) = 9.1545 − i4.1689 at FIX 4, and the p.9-3 to 9-4
  expression example. Values recomputed by abacus with mpmath before you pin them.
- **Each operation once,** with the stack's other levels checked against the guide's figure.
- **A hidden set from abacus:** random pairs through each operation against mpmath.
- **Mutants:** real and imaginary swapped; z1 not replicated in Z/T; the prefix leaking into 35s and STU (where
  CMPLX keeps its current menu).
- **No change** to 35s or STU behaviour, or to any accepted vector.

## Accepted (abacus, 2026-10-07, 9dafa60)

- The guide's examples are pinned, and abacus recomputed sin(2+3i) with mpmath. 039 passes 32/32 in abacus's build.
- Calls (1)-(4) are accepted as built.
- **Superseded by decision 58** (Michael, the 33s audit): the one-value complex forms (x i y, r θ a, ARG, xiy/rθa display) are hidden in 33s mode. That's done by unit 041, not here.
