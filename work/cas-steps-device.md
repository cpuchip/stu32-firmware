# cas-steps-device: Casimir's show-steps on the calculator (blueprint, before any code)

**From:** Michael's "Later: after the worksheet, as its own unit, blueprint and measured cost first" (relayed by abacus
#14050), placed after 092 (#14221). The repin to 1.6.0 (work/repin-v160.md) found that no key reaches `casim_step`, so
the linker drops CAS 039's letter steps and everything else of show-steps.

**Status:** draft. No code. The costs below are measured, from a throwaway: a host census and a bench on the stand-in,
neither in the repo. Their source and output are in work/cas-steps-evidence/ (census.c, corpus.txt, census.out,
stepbench.patch, standin-stepbench.out, d68-stepbench.txt).

## What the device would show

`casim_step` pages the working of four ops (casim.h): derive, integrate, solve and partfrac. The device has keys for the
first three (D/DX, ∫DX, SOLVE). Partfrac's working still shows up inside an integral done by partial fractions.

- Each page is one step: its text ("d/dX(before) = after", "∫(before)dX = after", a solve's "...=0" or "F=0: X=..."),
  its depth (0 the root, then its parts), and its rule (`casim_rule_name`: SUM, PRODUCT, CHAIN, COVER-UP...).
- `CASIM_STEPS_ONE_LINE` leaves out the template lines; abacus #6252 ruled one rule for the firmware.
- The last page is the op's own answer.
- **Each call is complete in itself.** Page k runs the whole op again with steps on, so a page costs a whole op, and
  only the page being shown needs computing.

## The measured cost

**Flash** (`scripts/resources.py static aedaa0f 60a3eea`: the throwaway, a STEPBENCH console command on the stand-in
that calls `casim_step`, against its base):
- The stand-in's image grows +47,856 B.
- By object (the two maps): casim.c.obj +46,224 B (the steps code the linker dropped until now) and libc's strpbrk
  +1,617 B. The rest is the throwaway bench itself (standin_main.c +6,122).
- So steps cost about **47.8 KB of flash** before the view's own code.
- DIRAM +0, IRAM +0, calc_task's frame +0.
- The app partitions have the room: the stand-in's is 5 MiB with 2.34 MB used, Rev A's 7 MiB with 2.27 MB.
- Rev A's build wasn't measured. It compiles the same casim.c, so the figure should carry; the build's D68 will say.

**The arena:** the host census (census.c over corpus.txt) found the smallest arena that pages every step, in 1 KiB steps.
- At most **10 KiB** of the firmware's 16 KiB `ab_cas_arena`.
- No case went NO_ROOM at 16 KiB.
- **The corpus:** the 64 distinct D/DX, ∫DX and SOLVE cases in tests/*.txt, plus 11 stress cases made for this: deep
  chains, long products, by-parts cycles, a four-factor partial fraction, a sextic.
- **Refusals:** 22 of its 150 runs are refusals, and each is the op's own (COEF NOT NUMERIC, NOT INTEGRABLE), as
  `casim_run` gives.

**The page's length:**
- The longest page in the corpus is **151 cells**: X³×SIN(X)×EXP(X)×LN(X) differentiated.
- A run's longest page has a median of 28 cells and a 90th percentile of 92.
- At the report's 31 cells a row, 151 cells is 5 rows.

**The time on the S3** (the stand-in, its 16 KB instruction cache; STEPBENCH from the calc task, with the CAS arena),
one-line mode, in ms:

| case | steps | counting them | the slowest page | every page |
|---|---|---|---|---|
| solve X^2-5×X+6=0 | 4 | 1.7 | 1.8 | 7.3 |
| solve V=I×R for R (CAS 039) | 3 | 2.5 | 2.8 | 7.9 |
| derive X^3×SIN(X)×EXP(X)×LN(X) | 5 | 1.0 | 1.5 | 6.0 |
| derive (X^2+1)^5×(X-3)^4÷(X+2)^3 | 13 | 1.2 | 1.9 | 19.2 |
| integrate X^2 | 2 | 1.2 | 1.7 | 3.4 |
| integrate EXP(X)×SIN(X) | 8 | 15.8 | 21.5 | 141 |
| solve (X-1)×...×(X-6) | 13 | 61.4 | 61.5 | 798 |
| integrate ATAN(X) | 9 | 59.4 | 121 | 809 |
| integrate X×ATAN(X) | 15 | 71.4 | 148 | 1,816 |
| integrate X×ATAN(X)+1÷(X^2-1) | 30 | 137 | 286 | 6,331 |
| integrate 1÷((X-1)×(X-2)×(X-3)×(X-4)) | 22 | 259 | 384 | 6,401 |

- With template lines (not one-line) the counts are up to 44 steps, and the pages take the same time.
- **Typical cases:** a page takes about 2 ms.
- **Heavy integrals:** a page can take 0.1-0.4 s, so paging them all would take 6 s. So the view computes a page only
  when it's shown, never all of them.
- Rev A's 32 KB instruction cache made its CAS about 40% faster than the stand-in in unit 059 (SOLVE 29.6 → 17.4 ms).
  That's an estimate here, not a measurement.

**The stack:** the calc task's low-water mark over the whole bench was 24,332 B free of 32,768, against the deep paths'
floor of 8,188 B.

**A miss in my own bench:** its first run gave SYNTAX on three cases. C's `"\xc3\x97EXP"` reads `\x97E` as one escape;
the firmware's casbench splits such literals for exactly that reason. Rerun with them split: standin-stepbench.out (the
first run's output is kept, named so).

## The leans

1. **What steps are shown for:** the CAS answer just made by D/DX, ∫DX or SOLVE, in Equation mode or on the stack (088's
   SOLVE in X).
   - The core keeps that answer's request in RAM: op, letter, the equation's text, its model, the angle mode.
   - It stays until the next CAS op, MEMORY CLEAR, CLEAR ALL or an image load. It isn't in the image, as SOLVE's report
     isn't.
   - SIMP, EXPND, COLCT, SUBST, FACTOR, ∫AB, Σ, QUOT, GCD and CANCL have no steps in Casimir, so STEPS after them does
     nothing.
2. **The key:** STEPS on CAS3's empty fifth key (QUOT GCD CANCL CPLX _ ▸ today), acting on that answer. On SOLVE's report
   it's also the fourth key: ▲ ▼ EVAL STEPS. That's one place to find it from the list, and the report's own bar.
3. **The view** (SYS's view, as the report):
   - The status line: `STEP k OF n` and the rule's name.
   - The step's text, wrapped over the rows. 151 cells measured is 5 rows of 31; a longer one scrolls with the page,
     as the report's rows do.
   - Its depth: a leading `·` for each level.
   - ▲ ▼ page, wrapping as the report does. C and ← leave. Any other key leaves and acts.
   - It opens on the last page (the answer), since a student usually asks how that answer came.
4. **The time:** opening counts the steps (about one page's time) and computes the shown page. Each ▲ ▼ computes one
   page. A page past a frame's time shows the busy mark the app shows for any long CAS op (048's `computing`). C during
   it discards the page and leaves, as C during a SOLVE does (048's interrupt).
5. **One-line**, as abacus #6252 ruled for the firmware.

## What the build would test (for abacus's oracle)

- **Vectors' tokens and dumps:** CASSTEPS (the key); `%STEPN=n`, `%STEPK=k`, `%STEP=text` (blanks as `_`),
  `%STEPRULE=name`, `%STEPDEPTH=d`.
- **Cases:**
  - each op's pages against the census's texts;
  - CAS 039's letter solve (V=I×R for R: BOTH SIDES, LINEAR with its row, ANSWER);
  - STEPS after an op with no steps doing nothing;
  - STEPS after MEMORY CLEAR doing nothing;
  - the view's keys;
  - the refusal (an op Casimir refused has no request kept).
- **Frames:** a step page at 1, 3 and 5 rows.
- **The stand-in:** STEPBENCH's cases through the keys, each page's frame against the host. On Rev A head-less, the
  same, timed.
- **D68,** against the 47.8 KB measured here.
- **Mutants,** predicted first:
  - the request not cleared by MEMORY CLEAR;
  - a page computed for every step on opening;
  - the depth's marks dropped;
  - template lines shown;
  - STEPS acting after SIMP.

## Questions for abacus

1. The scope: the three ops the device has keys for, one-line, the last CAS answer only. Steps for an older answer in
   the list would mean keeping a request per entry.
2. The key: CAS3's fifth key and the report's fourth (▲ ▼ EVAL STEPS).
3. Opening on the last page (the answer) or the first (the working from the top).
4. The 47.8 KB: acceptable, given the room measured above.
