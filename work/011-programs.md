# Work unit 011: keystroke programs, part 1 (entry, labels, running, branches, tests, flags, loops)

**From:** abacus · **To:** soroban · **Status:** ready after 010 (light brief; you extract examples)

## Scope (HP 35s User's Guide ch.13 pp.13-1 to 13-11, 13-19 to 13-23; ch.14 pp.14-1 to 14-19)

Program memory as labelled routines (LBL A..Z, lines numbered A001, A002, ...); program-entry mode
(keys are recorded as lines instead of executed; a typed number is one line); RTN; running with
XEQ label, XEQ label+line; R/S, STOP and PSE; GTO label and GTO label+line (programmed and from the
keyboard); subroutines and their nesting limit (find the guide's depth); the comparisons x?y and
x?0 (all six relations each: "do if true", skipping the next line when false, p.14-7); flags (SF,
CF, FS?, FC? for the guide's user flags, keeping unit 007's 7, 8 and 9 as they are); ISG and DSE
with the ccccccc.fffii counter (p.14-18/14-19); error stops (p.13-19); deleting lines and clearing
programs (pp.13-20 to 13-23). Not in this unit: INPUT, VIEW and messages (pp.13-12 to 13-18), the
checksum (p.13-23; HP's algorithm is not ours to copy, and we will define our own later), BASE in
programs (p.13-24, follows naturally if lines are ops), equations in programs, indirect labels
(p.14-23), and ALG mode anywhere (Michael ruled RPN first; ALG is out of scope for the product until
he says otherwise).

## Rules (manual first; ours where noted)

1. **A program line is one core operation with its argument**, the same operations ab_do_arg
   already performs (so every function of units 001-010 is programmable without special cases).
   Which keys the guide calls nonprogrammable (p.13-24) are refused in program-entry mode.
2. **Ours, execution in the core:** running is synchronous inside the core, with a step budget
   (default 10,000,000 lines per run, a named constant) so an endless loop returns a reported
   condition instead of hanging; R/S from the keyboard and the device's background task come with a
   device unit. A run ends at a top-level RTN, STOP, an error (the program stops at the failing line,
   p.13-19) or the budget.
3. Stack lift, LAST X and errors inside a program behave exactly as from the keyboard; the stack
   and variables a program leaves are the test (vector the guide's examples as end states).
4. **Ours, capacity:** program memory far larger than the 35s's (say what you set; a named
   constant). The 35s's line-number format (A001..A999) is kept as the address form; say what
   happens past 999 lines in one label (the guide's limit is the starting point).
5. Program memory is core state; persistence across power cycles is a device unit's (as with
   variables).
6. **Ours, test vectors:** extend the vector format so a vector can enter a program
   (`PRGM` ... `PRGM` around the recorded tokens, or your better form) and then run it.

## Acceptance and report

As before: the guide's examples as M-vectors (p.13-2/13-6 the first program, p.14-2/14-3 nested
subroutines, p.14-5 GTO, p.14-8 the comparison example, the flags example from p.14-9 on, p.14-17
to 14-19 the loops), red then green, with the instruction budget and the nesting limit each vectored.
At review: hidden random programs (straight-line arithmetic, counted loops with ISG/DSE,
comparisons, subroutine chains) against a reviewer interpreter written from the guide.
