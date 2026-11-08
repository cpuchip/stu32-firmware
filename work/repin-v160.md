# Repin: Casimir 1.6.0 (casimir 175c160, the v1.6.0 tag)

**From:** abacus #13870 (the order: casim tags 1.6.0, I repin to the tag, then tally, tutor and primer), #13953 (1.6.0
published 14:36:55Z; the tag object decf4ec, on commit 175c160) · **Status:** built at 5b2f2aa (CASIM_PIN only); results
below.

## What changes

- **CASIM_PIN 175c160a1b3c778ab67f049fe14baac81d1e23e2,** the v1.6.0 tag's commit (`git tag --points-at` in casim's
  clone gives v1.6.0, and the commit is on its origin/HEAD; the clone was read, never fetched into).
- **1.6.0's source over eedbf16** (`git diff --stat eedbf16 175c160 -- src`): casim.c +73 −6 lines, casim.h +4 −4.
  - casim.c: CAS 039, show steps for a letter solve. `do_ssteps` returned BAD_ARG for a solve answered with letters
    (CAS 038); it now calls the new `ls_steps`, which pages BOTH SIDES (or POLYNOMIAL), LINEAR and ANSWER. Every changed
    line is in the steps path.
  - casim.h: CASIM_VERSION "1.6.0" (minor 6), and the steps comment.
- **The firmware calls casim_run only** (`grep casim_ core firmware`: casim_op, casim_req, casim_run, casim_status;
  no casim_step). So no key reaches 1.6.0's change, and the repin moves no answer, row or frame. No firmware code
  changes; VER stays 0.88, as the eedbf16 repin kept 0.87.
- **What the device shows:** VER's CASIM and the console's `pins: casim=` name 175c160.

## Results

**1. make test** at 5b2f2aa (evidence: make-test.txt): rc 0, and nothing moved.
- 199 vector files: 15,205/15,205 vectors, 35,108/35,108 expectations.
- expr_test 427/427, app_test 865/865, help_test 771,024/771,024 (308 entries), keymap_test 2,917/2,917,
  screen_test 9,020/9,020 (48/48 frames), standin_test 349/349, tbook_test 39/39, draw_test 0 failures.
- standin_replay: 38 scripts before repin-v160.txt (39 with it), every current frame matching, among them the
  eedbf16 repin's repin-eedbf16.txt (5 of 5) and 088's cas088.txt (4 of 4).
- The binaries are the repin's: build/casim/.pin is 175c160, casim.h says 1.6.0, and app_test was built with
  VER_CASIM "175c160" at 14:39Z, after the pin's change.
- **New:** device/standin/repin-v160.txt is repin-eedbf16.txt's keys with VER 0.88, the pin 175c160, and the old pin
  (eedbf16) as its last control. On the host it replays 5 of 5 frames, the same five CRCs.

**2. D68,** `scripts/resources.py static 1d19d01 5b2f2aa` (evidence: d68.txt):

| | stand-in | Rev A head-less |
|---|---|---|
| flash (the app's .bin) | 2,310,288 -> 2,310,304 (+16) | 2,245,952 -> 2,245,952 (+0) |
| DIRAM, IRAM code, calc_task's frame | +0 | +0 |
| libmain.a | +4 | +4 |

- **By the maps** (evidence: map-diff.py, map-diff.txt): the one loaded section that moved is `.text.casim_run`,
  1,876 -> 1,880 (+4), on both targets. Everything else that moved is debug information.
- **casim_step, do_ssteps and CAS 039's ls_steps are all in the maps' discarded sections** on both targets: nothing
  calls casim_step, so the linker's gc drops the steps path. ls_steps is listed only in the new map, which shows the
  check sees a function that exists.
- The stand-in's +16 on flash, for +4 of code: the image is padded to 16 bytes.
- casim.c.obj's loaded sections in Rev A's map (address non-zero, debug left out): 188,908 -> 188,912 (+4), under
  decision 94's 301,056 B line. 188,908 is the eedbf16 repin's figure too, so 088 didn't move casim.

**3. make clang-check:** 71 files, 0 failing (the control refused) (evidence: clang-check.txt).

**4. The boards** (evidence: board/; talk.py's verdicts in board/talk-verdicts.txt). The stand-in's app was written at
14:47:18Z (2,310,304 B, D68's size) and Rev A head-less's at 14:51:44Z.

| script, image | PASS | fails | stack-free, least |
|---|---|---|---|
| repin-v160.txt, stand-in | 23 | its two controls: a frame CRC, and the old pin eedbf16 | 25,716 |
| cas088.txt, stand-in (088's bar) | 19 | its control frame | 25,300 |
| repin-eedbf16.txt, stand-in, unedited | 21 | ver=0.87 (088's VER), pins eedbf16 (the repin), its two controls | 25,716 |
| repin-v160-reva.txt, Rev A head-less | 18 | the old pin's control | 25,732 |

- **The stand-in's five frames are eedbf16's five:** 80608263, 9922B897, 11D1A4C6, 1F7002A7, 42D43CA9. ENTER on R=V÷I at
  I=0 is still msg=CONDITION FAILS on row I≠0.
- **cas088.txt's counts are 088's** (19 PASS, the control failing), so 088's bar runs on 1.6.0 as on eedbf16.
- **Running the old script unedited is the transcript check.** Of its 25 expects, the ones that move on 1.6.0's image are
  ver= (0.87 -> 0.88: unit 088's bump, not this repin) and the pins line (eedbf16 -> 175c160: the repin). Every report,
  row, frame and message expect still passes.
- **Rev A head-less:** repin-v160.txt without its FRAMECRC lines (Rev A has none), at
  work/repin-v160-evidence/repin-v160-reva.txt. Its counts are the eedbf16 repin's (18 PASS, the old pin failing).
- **A miss of mine:** the first Rev A run was given the script by its scratch path. headless.sh runs talk.py in the
  container, which sees only the repo, so it stopped at the open after flashing (board/try1-reva-headless.out). The rerun
  used the flashed image (FLASH=0) with the script copied into the evidence folder.

**Claims** (lepton, old shim, `soroban-*` 4 GiB):
- docker: 14:43-14:46Z for D68 and clang-check, peak 732 MiB (docker stats every 5 s).
- docker and the stand-in board: 14:46:56-14:54:02Z for the four runs.
  - Peak 256 MiB over the first script (the stand-in's three runs and the first Rev A flash).
  - The FLASH=0 Rev A rerun had no sampler. It built nothing.
- Both released.

## Downstream (abacus #13870)

tally, tutor and primer follow from this commit, each in its own seat. Nothing in this repo changes for them.
