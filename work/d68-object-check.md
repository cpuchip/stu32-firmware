# D68 reports whether a moved section's object moved too (abacus #17284)

**Why:** three repins (1.7.0, 1.8.0, 1.9.0) moved functions of unchanged sources in the linked map: Rev A's calc.c
do_key +16, solve_core +308, then +612. Each time, the object's own section was byte for byte the same in all four
builds, so the move was the Xtensa linker's relaxation and layout, not code. That was checked by hand with a script
copied from one evidence folder to the next. abacus ruled (#17284): put it in scripts/ and have D68 print the verdict.
It's a report, not a pass or fail, so decision 68's rules and budget are unchanged, and a work note is enough.

## What changed

- **scripts/obj_section.py:** the ELF section reader (the old CLI kept: `OBJ SECTION...`), the map's loaded sections,
  and `moved BUILD_A BUILD_B [--target NAME...]`.
- **scripts/resources.py static:**
  - after the internal-RAM symbols, a list headed "Loaded sections that moved outside the sources these commits
    change";
  - each line is a (section, object) whose size moved in the map, with "object identical" (the section and its
    `.rela` section the same bytes in the two builds' objects) or "object changed";
  - an object it can't find in the build tree (a toolchain library's) reads "object not in the build tree".
- **The targets** are the basenames of `git diff --name-only BASE NEW`, with casim.c when CASIM_PIN changes. Their moved
  lines are counted, not listed.
- **read_build** returns its build directory.

## Its limits, named

- **Only moved sizes.** A section whose size didn't move isn't listed, even if its code changed. D68 is about size.
- **It errs toward "changed".** Relocations carry symbol indices, so an object whose code is the same but whose symbol
  table was renumbered reads "object changed". It never errs the other way: "identical" means the bytes are.

## Shown failing (abacus's condition)

- **The plant:** a one-instruction change in an object whose source the commits don't change.
  - A plant in calc.c itself would make calc.c a target, and its lines would only be counted.
  - So the plant is a `nop` in bigint_impl.h's inline, compiled into bigint.c and sbig.c, as a header change reaches
    an object.
  - It was a scratch commit 91d2dcc on e5cf3f1, never pushed.
- **`scripts/resources.py static e5cf3f1 91d2dcc`** (evidence: d68-object-check-evidence/d68-planted.txt):
  - **Both targets:** every moved bigint.c and sbig.c section reads "object changed", 10 lines each. The nop changed
    gcc's inlining too: bi_mul −75, sb_mul −74.
  - **The rest:** the sections that moved only by layout (app.c's literals, calc.c's gk15 on Rev A, state.c's and one
    casim.c literal) read "object identical".
  - **The targets' count:** "0 moved lines of the changed sources, bigint_impl.h".
- **The control, the accepted 1.9.0 repin** (`static d638ef6 e5cf3f1`, d68-repin-v190.txt):
  - every listed line reads "object identical": calc.c's solve_core +612, gk15, integ_core, list.c, state.c, and the
    IDF's and games' literals;
  - those are the 11 sections checked by hand at 1.9.0 and more;
  - casim.c's 159 (Rev A) and 173 (stand-in) moved lines are counted as a target's.
- The containers peaked at 819.3 MiB (soroban-repin-* through my shim, 4 GiB cap, --rm; the planted builds). The
  control's builds were cached.

It lands with this note, for the next unit or repin that runs D68.
