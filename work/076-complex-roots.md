# Work unit 076: CPLX, SOLVE's complex roots

**From:** decision 88 (Michael), CAS 020's CASIM_COMPLEX · **Proposed:** soroban #9798, #9799 · **Ruled:** abacus #9803
(approved as drafted; one more vector: a non-solve op byte for byte the same with the bit on). It doesn't gate Casimir
1.0.0.

## What it does

- **CAS3's fourth key is CPLX,** empty before. It's a toggle, off at MEMORY CLEAR, and STU's, as every CAS key: in
  33s or 35s mode the core says STU ONLY.
  - When on, CAS3 shows it marked, as MODE's keys are.
  - CAS3 stays open after CPLX, as the buzzer's USE keys keep theirs, so the mark shows the toggle at once. Fresh eyes
    asked for this, since every other menu closes after its key.
- **With CPLX on, every CAS call carries CASIM_COMPLEX** in its angle (CAS 020 rule 2: any op takes it, only solve reads
  it).
  - SOLVE prints a quadratic factor's conjugate pair in casim's form: X^2+X+1 is
    `SOLVED: X=(-1-SQRT(3)×0i1)÷2; X=(-1+SQRT(3)×0i1)÷2`.
  - ENTER on a root puts its value in X through the core's evaluation, as for a real root (048 R02). In STU that's a
    complex X.
- **The CAS menu is STU's only, so "35s and STU modes" is the equation's stamp** (abacus #9803). A 35s- or STU-stamped
  equation gets the pair. A 33s-stamped one stays real, because casim ignores the bit in model 33, whose equations
  have no complex numbers.
- **NO REAL ROOTS says where to turn it on** (the stu-discover rule). With CPLX off, an equation not stamped 33s, and
  casim's row NO REAL ROOTS, the report's status row reads `NO REAL ROOTS. CAS3 CPLX ON` (27 cells). A 33s equation
  keeps plain NO REAL ROOTS, since the setting can't help it.
  - **Approved as `NO REAL ROOTS: CAS3 CPLX ON` (#9803); built with a period.** The report splits casim's line at its
    first ": " into the status row and the root rows (ab_cas_row). With the colon the hint became a root row of its
    own, where SST would stop and ENTER would try to evaluate "CAS3 CPLX ON". The first build showed it: 048 S05's
    %CASROW read NO_REAL_ROOTS, the hint gone from the status row.
- **A setting:**
  - the calculator's field cas_cplx, in the image (version 20, the section T_CASCPLX, one byte; an image before 20 has
    none and loads it off);
  - toggling takes an UNDO mark;
  - NONPROGRAMMABLE in program entry, as ENTRYM;
  - it acts in Equation mode (eqn_setting) and on ALG's line (alg_setting), so 075's refusal predicates take it
    from their own lists.
- **HELP:** [AB_CASCPLX], and [AB_CASSOLVE]'s third line, "Complex ones: CAS3 CPLX on."

## Not in this unit

- With CPLX off, (X-2)×(X^2+1) says `SOLVED: X=2`, and (X^2+1)×(X^3-2×X-5) says `PARTIAL, DEGREE 3 LEFT` (fresh
  eyes). Each leaves the pair out without a word. That's casim's "leave them
  out", right over the reals. A note that a complex pair was left out is casim's to add after 1.0 (abacus #9803, on
  casim's follow-up list).
  - **Added in Casimir 1.1.0 (CAS 028):** they now say `SOLVED, 2 CPLX: X=2` and `PARTIAL, DEGREE 3 LEFT, 2 CPLX`. C05
    and C16 moved, and C21-C23 are new (work/repin-rc7.md, "Casimir 1.1.0").
- Programs toggling CPLX: after v1 if anyone asks (#9803).

## Checks

- **tests/076-soroban.txt, 20 rows.** Each text is casim's CLI at e5f798b (v1.0.0-rc7), 16,384 B, with and without
  `--complex`.
  - Off: the hint (C01), and the pair left out over the reals (C05).
  - On: the pair (C02); ENTER on a root gives X = -0.5 i 0.866… (C03); the real root first, then the pair (C04).
  - The toggle twice is off (C06).
  - 33s-stamped, on and off: plain NO REAL ROOTS (C07, C08). 35s-stamped, on: the pair (C09).
  - CPLX acts while the list is shown (C10).
  - A derivative and a sum, byte for byte the same with the bit on (C11, C12; #9803's addition).
  - NONPROGRAMMABLE (C13).
  - From fresh eyes:
    - four pairs pass the 191-byte answer, RESULT TOO LONG where off gave the hint (C14);
    - PARTIAL with the quadratic's pair on (C15), and off without it (C16);
    - CPLX in 35s mode is STU ONLY (C17), and leaves the setting unchanged (C18).
- **app_test:** CAS3's fourth key is CPLX, unmarked at MEMORY CLEAR and marked when on. The image keeps it on, and keeps
  it off. UNDO takes the toggle back.
- **load_fuzz:** CASCPLX is never a program line (ab_line_ok). The first build left it out, and load_fuzz found it: a
  forged CASCPLX line was accepted.
- **The image's older versions:** a T_CASCPLX section in an image below 20 is skipped, as T_EQNOTE's below 17. The
  first build refused it, and 025's N04 and N13 found that: they relabel a new image as version 4.
- **048 S05 moved:** an STU equation with no real roots now shows the hint, as ruled.
- make test rc 0, peak 170,952 KB. help_test 766,849 checks, 305 of 305 entries shown (the sweep reaches CPLX). app_test
  777. keymap_test 2,871.
- **Mutants,** each red:

| mutant | what it breaks | red |
|---|---|---|
| M1 | the bit never sent | C02 C03 C04 C09 C10 |
| M2 | the hint for 33s equations too | C08 |
| M3 | no hint | C01 C06 |
| M4 | the image byte not written | app_test's image check; C19 (added after acceptance, abacus #9863) |
| M5 | not an Equation-mode setting | C07 C09 C10, 3 app_test |
| M6 | programmable | C13 |
| M7 | the toggle only turns on | C06 |
- **Fresh eyes (a subagent) found no defect.** The bit reaches every casim call (one casim_run). casim strips it before
  any angle check; six ops matched with it on and off in DEG, RAD and GRAD. The hint fires exactly on casim's lone
  NO REAL ROOTS. The image, UNDO and MEMORY CLEAR are sound.
  - What came of its notes:
    - the PARTIAL case is in "Not in this unit" and C15/C16;
    - CPLX's help says "a quadratic's complex pair";
    - SOLVE's help says C ends the report again;
    - C14 pins the RESULT TOO LONG;
    - CAS3 stays open after CPLX;
    - CPLX is STU's.
  - Each of the last two is red without its fix: C17 and C18 for the STU check, 3 app_test checks for the open menu.

## Decision 68 and the stand-in

**Static, `scripts/resources.py static 351271e dff79e5`:**
- Flash +656 on the stand-in and +592 on Rev A.
- Internal RAM +0, IRAM +0, calc_task's frame +0. No internal-RAM symbol changed.
- idf_size books 128 B of the help text's strings to libesp_app_format, as in 075.

**The stand-in at dff79e5** (on Casimir 1.0.0's pin):
- **cplx076.txt:** ALL MET, the control red.
  - From the keys in STU: SOLVE of X^2+X+1 reports NO REAL ROOTS. CAS3 CPLX ON.
  - CAS, CAS2, CAS3, CPLX turns it on, and CAS3 stays open.
  - SOLVE again: SOLVED: X=(-1-SQRT(3)×0i1)÷2; X=(-1+SQRT(3)×0i1)÷2.
  - stack-free 26,084 B; ver=0.76.
  - **The first run failed on my script, not the firmware.** CAS3 stays open after CPLX, so the script's keys for the
    Equation bar went to CAS3, and its F1 was D/DX. A replay of the script's keys through the app on the host showed
    the same thing, and the fixed keys (F6, F6, F1) gave the pair. The rerun with them, with no flash, passed.
- **casrows.txt:** ALL MET.
- make clang-check 66/0.

## Accepted (abacus #9863, at fca2079)

- **From abacus's clean clone:** make test 0, 076 18/18, and its 7 hidden rows 7/7. Those were a second pair, a cubic,
  the hint in 35s and none in 33s, SAVE/MEMCLR/LOAD keeping CPLX, MEMCLR clearing it, and ENTER giving 1i2.
- **Its falsifications,** each red as it predicted. The image writing CPLX as 0 went red only on its own H04, so
  the vectors didn't guard the image.
- **C19 and C20 are added for that:** SAVE, MEMCLR, LOAD gives CPLX back, and MEMCLR alone clears it. C19 goes red
  with the image byte written as 0.
