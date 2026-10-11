# Work unit 067: a new calculator starts in FIX 4

**From:** Michael's answer to the first-boot-display card (abacus #6964: "Both FIX 4, like the HP") · **Gate:**
soroban · **Status:** built and measured on the host; the consumers' readers are listed below.

## What it is

A new calculator (ab_init: first boot, a wiped state, a host app's fresh core) now shows FIX 4, the same as after
MEMORY CLEAR. The 35s guide calls FIX 4 the original default: MEMORY CLEAR "restore[s] all format and modes to their
original, default settings", and the table's MEMORY CLEAR (Default) column gives Display format FIX 4 (UG p.B-3).
Unit 002's ALL start, which was ours, goes. ALL remains a setting the keys choose.

The change is two lines of ab_init, disp_mode AB_FIX and disp_n 4. A saved image keeps its own display setting, so a
calculator already in use does not change when it updates. Only a new or wiped one starts in FIX 4.

## The checks

- **Oracle first,** tests/067-soroban.txt, with a new harness expectation %DISP (the display setting) in
  tests/vectors.c:
  - D01: a new calculator shows FIX4;
  - D02: MEMORY CLEAR from ALL gives FIX4;
  - D03: 2.77777 RND gives 2.7778;
  - D04: 1/3 RND gives 0.3333;
  - D05, D06: ALL and FIX 2 are still chosen by the keys.
  
  Before the change, D01, D03 and D04 were red; D02, D05 and D06 were green, which shows %DISP reads ALL, FIX4 and
  FIX2. After it, 6/6, with save/load and the store too.
- **app_test:** two new checks. A new calculator's X line is "0.0000", and VIEW A shows "A=5.0000".
- **What read the old ALL start,** measured by running every make test step on a prototype:
  - **Tests about something else that assumed ALL now set it explicitly:**
    - app_test: VAR, VIEW, TABLE, and ENG on a grouped line, 12 checks;
    - screen_test: 14 frames' setups and 3 text checks;
    - work/041c: C34, C37, C43 and C44 (their own descriptions say "ALL's 2" and "ALL's 0").
    
    Each still checks what it checked; no expected value changed.
  - **What shows a new calculator changed:**
    - screen_test's 01-fresh golden: T, Z, Y and X from 0 to 0.0000. Viewed, nothing else changed.
    - standin_test's five screen CRCs: the stack lines from 0 / 0 / 1 to 0.0000 / 0.0000 / 1.0000. Diffed pixel by
      pixel against HEAD's renders: only the stack's block differs, and the keyboard is unchanged.
- No board script reads the display.
- `make test` green, `make clang-check` 62/0.

## For the consumers

Anything that reads a fresh core's display now sees FIX 4: "0.0000" where it saw "0". tally's app tests and primer's
web probes read the first-boot display (abacus #6964); tutor's may too. Each updates its expectation at its next pin.
A saved state is unaffected.

## Decision 68

From the CORE fix's head (949a8fd):

- **Static** (`scripts/resources.py static`): +0 on both boards in flash, DIRAM, IRAM code and calc_task's frame; no
  internal-RAM symbol changed.
- **Board** (`scripts/resources.py board`, both targets, with speed):
  - heap and its largest block +0;
  - every stack row +0, masked and unmasked, in all three states: Rev A's figure 8,196 (SIN in RAD 8,916), the
    stand-in 8,180 / 8,900;
  - the interrupt's largest cost on a row 0 on both commits, and the run exited 0;
  - the full state's script hashes are new, because the full image stackkeys writes now carries FIX 4. BEFORE and
    AFTER ran the same script, as resources.py requires.
- **Speed**, stand-in / Rev A: SOLVE +18 / +6 µs, integral -20 / +118 µs (+0.4% at most), PROGBENCH SIN and LN +0.
