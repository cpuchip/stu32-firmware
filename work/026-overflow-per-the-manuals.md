# Work unit 026: overflow and underflow as the manuals have them

**From:** abacus · **To:** soroban · **Status:** ready (Michael, 2026-10-06, abacus decision 45: "Follow the manuals
(warning + flag 6, underflow to 0)")

## Why

All three manuals agree:
- an overflow returns the largest number with an OVERFLOW warning and sets flag 6;
- with flag 5 set, a running program stops after the overflowing line;
- an underflow returns 0, silently.

The sources: 33s p.1-16, F-3, p.13-9; 32SII p.1-12, E-3, p.13-9 and 13-10; 35s p.14-9 and the same appendix.

We made both errors that leave the stack unchanged (unit 001 amendment 2 ruling 5, unit 003 rule 4). Those were the
seat's rulings, never Michael's, and he has now ruled for the manuals. This withdraws them. The manual review's
N24-N27, Q20-Q22 and B18 are the vectors.

## Rules

1. **Split the status.** Today `abn_intel.c` returns one ABN_RANGE for overflow, underflow and non-finite results
   (lines 60-71, 252-253), and ABN_RANGE is also borrowed for things that aren't range at all (`base_parse`,
   calc.c:277; `ar_xn`, calc.c:531). Give overflow and underflow their own statuses, with overflow's sign known. A
   NaN or an invalid operation stays a domain error, as now.
2. **Overflow** (|result| beyond decimal128's largest finite value):
   - the result is ±the largest finite value (9.99…9E6144, 34 nines), with the sign the true result would have had;
   - the operation completes as any other does: the stack moves and LASTx is saved;
   - flag 6 is set;
   - the notice `OVERFLOW` shows briefly, by unit 024's mechanism for a clamped base result;
   - it is not an error: the error message, and the stop it causes in a program, don't happen.

   Our largest number stays decimal128's, not the HP's ±9.99999999999E499 (the range is ours, unit 001).
3. **Flag 5:** while a program runs, an overflow with flag 5 set stops the program at the line after the one that
   overflowed (33s p.13-9), with the notice shown. Flag 5 clear: it runs on. Both flags are user flags otherwise:
   SF, CF and FS? work on them, and nothing else clears 6.
4. **Underflow** (a nonzero result below the smallest normal magnitude): the result is 0. No message, no flag.
5. **Internal intermediate values are out of scope.** The functions that already handle their own internal
   underflow (fn.c around 60, 270 and 431; unit 021's review) keep doing so. Only a result an operation returns is
   governed here.
6. **SOLVE and ∫:**
   - A function value that overflows is the largest number, the way the manuals' SOLVE treats it (it was an error
     inside the evaluation before).
   - Flag 6 is set as for any overflow. Flag 5 doesn't stop a SOLVE or ∫ in progress; it governs program execution,
     and SOLVE's own iterations aren't program steps (say if you read the guide otherwise).
7. **Typed numbers** outside the range (`1E7000`) keep today's behaviour. That's entry, not arithmetic, and isn't
   in this ruling.
8. **Both modes.** Michael ruled for the manuals, and all three agree, so no mode.

## Acceptance

- **Turn green:** the review's N24-N27 (stack), Q20-Q22 (programs) and B18 (cxbasestat), amended where they assumed
  the HP's E499 limit to decimal128's.
- **Amend in place, with a note pointing here:** every older vector that expects an error on overflow or underflow
  (grep the work/0*-vectors files for overflow, `1E6000` and `EXP` of big arguments). Each becomes the largest
  number with flag 6, or 0.
- **New:**
  - the largest number's exact digits;
  - the sign of an overflowing product of two negatives;
  - LASTx after an overflow;
  - flag 6 set and cleared by CF;
  - flag 5 stopping a program at the next line, and not stopping with flag 5 clear;
  - an underflow in a chain (`1E-6000 ENTER × 5 +` gives 5);
  - an overflowing f inside SOLVE.
- **Mutants, each caught:**
  - overflow still an error;
  - the wrong sign on the substituted result;
  - flag 6 not set;
  - flag 5 ignored;
  - underflow still an error.
- **Regression:** the five work/024-review folders, before and after. Only the vectors named above should change.
