# Repin: Casimir 1.0.0's last release candidate (rc7), with the CAS arena at 16,384 B

**From:** abacus #9747 (Michael: "Let's get soroban and tally (android?) in on this and cut the v1 casimir"; decision 89:
1.0.0 final comes after the calculator runs the last rc) · decision 88 (the arena) · decision 90 (no known wrong answer
in 1.0: CAS 025, 026).

## What changed

- **CASIM_PIN e0ef88a:** CAS 026 built, accepted by abacus at 10bf979. rc7's tag is e0ef88a plus its version text. The
  pin moves to the tag's commit once abacus has checked the tag.
- **AB_CAS_ARENA 12,288 to 16,384 B** (core/arena.h, with the reason). The arena is in PSRAM.
  - casim's device bench at the rc6 tag (work/arena-16384.txt): DEVICE PASS on 1,347 cases. The gate is 12,288's,
    and the most a case needs is 12,424 B. Five answers move to what 65,536 B gives.
- **The interrupt watchdog stays at 300 ms** (abacus #9754).
  - The bench's 2,000 ms is its own setting: casim masks interrupts around each run to read stacks.
  - The firmware never masks around a CAS call. There's no critical section in device/ or core/, and the task WDT is
    off.
  - "Shipping" in decision 88's wording was corrected.
- **casim.h, 2ba7217..e0ef88a:** only CASIM_VERSION and CASIM_COMPLEX (32, unit 076's switch). No new status, so no
  switch over casim_status gains a case.

## The rows

- **No existing row moved.** At e0ef88a and 16,384 B make test is green as it stood: 048 65/65, 056 43/43,
  057 37/37, 070 58/58 before the new rows.
  - casim's 020 to 026 change sums with irrational ratios, negative-power letters and function factors, and their
    rows. No 048/056/057/070 row has one of those.
  - The three arena moves in casim's record are a SIN 42-deep derivative, two Taylor series and a ratio sum. No row
    reaches them either: the derivative is longer than an equation, and the firmware's keys have no Taylor.
- **So four rows are new, each red without what it guards:**
  - **070 S60:** the arena. casim's record's sum, NO_ROOM at 12,288 B and an answer at 16,384, as at 65,536. The closed
    form equals the exact partial sums for N = 1..40 (Python's fractions). At 12,288 it goes red.
  - **070 S61, S62:** CAS 025 on the keys. (X÷Y)^K÷Y from 0 to ∞ answers -1÷(X-Y), with the rows Y≠0 and then
    ABS(X÷Y)<1. At 2ba7217 there's one row, and the first is the ABS row: red.
  - **070 S63, S64:** CAS 026 on the keys. K×(SIN(Z)×Y)^K÷SIN(Z) from 0 to ∞, its first row SIN(Z)≠0. At 2ba7217
    that's red the same way.
  - Each expected text is casim's CLI at e0ef88a, in RAD and in DEG alike.

## Checks

- make test rc 0, peak RSS 170,568 KB. 070 63/63.
- casim-build: casim e0ef88a built against core 6f12c13.
- **device/standin/repin-rc7.txt** (the stand-in, with interrupts live):
  - the pin;
  - from the keys in DEG: S60's sum answering; S63's answer and its SIN(Z)≠0 row, stepped with SST;
  - the stack's low-water mark no lower than the deep paths' 8,188 B free.

## Decision 68 and the stand-in

**Static, `scripts/resources.py static 6f12c13 ae71971`:**
- Flash +17,008 on the stand-in and +16,848 on Rev A (casim's code, CAS 020 to 026).
- Internal RAM +0, IRAM +0, calc_task's frame +0. No internal-RAM symbol changed.
- The arena's +4,096 B is PSRAM (libmain's +20,940 counts it).

**The stand-in at ae71971, interrupts live:**
- **repin-rc7.txt:** ALL MET, the control red.
  - The pin is e0ef88a.
  - From the keys in DEG: S60's sum answers past 12,288 B; S63's answer, then SST to its first row, SIN(Z)≠0, labeled
    WHERE.
  - stack-free 23,732 B (floor 8,188); heap 62,747 B.
  - The port moved at the flash (ACM1 to ACM3). The rerun on the new port, with no flash, passed.
- **casrows.txt:** ALL MET.
- **sum070.txt:** ALL MET bar its control.
- make clang-check 66/0.

## The tag: v1.0.0-rc7 (abacus #9784)

- **The pin moves to e5f798b,** the commit of v1.0.0-rc7 (annotated tag 52aeadd).
- **e0ef88a..e5f798b,** read here:
  - src changes only CASIM_VERSION, "1.0.0-rc6" to "1.0.0-rc7";
  - the README's counts, examples/hello.c's arena (8,800 to 16,384) and idf_component.yml's version;
  - work/rc7-mutants.txt, 428 of 428 on that tree.
- make test rc 0 at e5f798b, peak RSS 235,576 KB; 070 63/63.
- **The stand-in at 6c32e57:** repin-rc7.txt ALL MET, the control red.
  - The pin is e5f798b.
  - S60's sum answers, and S63 reaches SIN(Z)≠0 | note=WHERE.
  - stack-free 23,732 B. The port held through the flash this time.

## Casimir 1.0.0 (abacus #9829)

- **The pin moves to 01267e8,** the commit of v1.0.0 (annotated tag edab17b), published as Latest.
- **e5f798b..01267e8,** read here: src changes only CASIM_VERSION, "1.0.0-rc7" to "1.0.0"; the rest is
  work/release-1.0.0-rc7.md and the notes.
- device/standin/repin-rc7.txt's pin line names 01267e8. Its rows are rc7's, and the src is the same.
- make test rc 0 at 01267e8 (070 63/63).
- **The stand-in at cd59f06:** repin-rc7.txt ALL MET, the control red.
  - pins: casim=01267e8b2c2f22b91e3e1f15639cf9ebd3bc44de, the field VER's CASIM shows.
  - S60 and S63/S64 as at rc7; stack-free 23,732 B.
  - The port moved at the flash (ACM3 to ACM1). The rerun on the new port, with no flash, passed.

## Casimir 1.0.1 (abacus #10132)

- **The pin moves to c40422c,** the commit of v1.0.1 (annotated tag 5f62b43), published as Latest.
- **01267e8..c40422c, read here:** casim.h changes only the version (1.0.1, PATCH 1). casim.c is CAS 027 (accepted at
  cfdb18f, #10109): the sum's shift reads a function factor of the ratio (SIN(Z), SQRT(Z), LN(Z)) as one more letter.
  cfdb18f..c40422c is the version text alone.
- **No row moved.** make test rc 0 at c40422c, peak RSS 236,800 KB: 048 65/65, 056 43/43, 057 37/37, 070 63/63,
  076 20/20, every one of the 87 vector files full. S63/S64's sum runs from 0, where CAS 027's shift (s <= lo) has no room.
- device/standin/repin-rc7.txt's pin line names c40422c.
- **070 S65, S66 put CAS 027 on the keys.** S63's sum from 1 answers
  `-Y÷(Y×SIN(Z)-1)-Y^2×SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2)` with one row, ABS(Y×SIN(Z))<1. That's casim's CLI at
  c40422c, 16,384 B, the same in RAD and DEG.
  - At 01267e8 both are red: the answer divides by SIN(Z), there are 4 lines, not 3, and the first row is Z≠0. That
    answer has no value at Z = π, where the sum is Y.
  - make test rc 0 with them, peak RSS 171,580 KB; 070 65/65 (127/127), its slices 1, 7 and 1,000 too.
- **The stand-in at 1da5600:** ver=0.77, pins casim=c40422c34b8a14f32c58f6e1c018372568ddf664.
  - repin-rc7.txt: every row met, with the control at its line 384 red. S60's sum answers, S63's answer and its
    SIN(Z)≠0 row (WHERE) are there, and stack-free is 23,732 B (floor 8,188).
  - casrows.txt: ALL MET.
  - The port moved at the flash (ACM3 to ACM1); the rerun with no flash passed.
- abacus checked S65's text against its own log at both tags (#10153). tally builds the APK from 1da5600.
