# Unit 094 built: SOLVE's report, ENTER copies and EVAL evaluates

**From:** the blueprint work/094-report-enter.md (302b7c4); abacus #14168's rulings (1-5 as proposed; the tension ruled
(a); tokens CASEVAL, and `-` for a SOLVE row with no link); Michael's confirmation of (a) (#14171, decision 98). h094 was
frozen at abacus 0f5d527 before any src (10 vectors, sha256 a2a521d3…4196, private until acceptance). The build:
edc8c58 (src), 7a097a3 (the mutants' predictions, before any run), 3ca88f3 (the mutant runs and a test made
independent), 30dac26 (the stand-in's scripts). VER 0.94. Also adopted in this stretch: h093 (3ee180d, abacus #14169).

## What was built

- **ENTER on an answer row copies** its right side into X (casrep_key): 088's rule, `cas_plain_number` a number and
  anything else an expression (`ab_expr_new`); ` (MULT k)` dropped; the rows not read; lift as RCL's; the report closes.
  MEMORY FULL leaves everything as it was, the view open.
- **EVAL** (AB_CASEVAL, nonprogrammable) is what ENTER was at 99a62eb: the rows checked at the stored letters,
  CONDITION FAILS with the cursor on the failing row, else the value. It's soft key 3 of CAS REPORT (▲ ▼ EVAL). Outside
  the report it does nothing on the stack, in Equation mode and in ALG; the loader refuses it as a program line.
- **The SOLVE row's link** is the row the solve read: `cas_rep_row`, set when the report opens (X's row on the stack, 0
  from the list), beside the report and, like it, not in the image. That fixes 089's link to "Y after the lift", which on
  the list's path was whatever X had held (094's K01 and K02 now cover it).
- **HELP:** EVAL's entry; SOLVE's says "ENTER copies one, EVAL its value". help_test gained a root with the report open
  (R_REP, and `rep` in the state signature), so EVAL's entry is reached: 315 of 315 entries shown.
- **Tests:** tests/094-soroban.txt (22 vectors, 53 expectations, worked out before the first run), in make test and under
  the sanitizers; the vectors' CASEVAL token; app_test's bar (▲ ▼ EVAL), EVAL through soft key 3, ENTER's copy through
  the app.

## What moved

- **The 18 vectors** the blueprint listed, ENTER to CASEVAL, every expectation kept: 048 C01-C13, L06, R02; 088 S04, S05;
  076 C03. Each description now says it was ENTER before 094. The nine that keep ENTER stayed green unchanged.
- **Two board scripts the blueprint's census missed:** repin-eedbf16.txt (line 74) and repin-v160.txt (line 73) pressed
  ENTER on R=V÷I at I=0 and expected CONDITION FAILS on the console. standin_replay checks only FRAMECRC lines, so the
  trap census couldn't see those expectations. Both now press EVAL (K2, soft key 3), and their frames are unchanged.
  My miss: the census covered the replay's checks, not the console's.
- No screen_test frame and no other vector moved. app_test's five ENTER-on-a-root checks stay as they were (plain numbers).

## Results

- **make test** at 30dac26: rc 0; 214 lines, 21,475 vectors, 58,189 expectations (every `n/n vectors` line, repeats
  included). That's 093's 211 / 21,402 / 57,992 plus h093 (29, 91) and 094 twice (22 and 53 each, plain and sanitized).
  094 22/22 and 53/53; h093 29/29 and 91/91; app_test 893/893; help_test 849,580 checks, 315 of 315 entries;
  screen_test 63/63 frames; standin_replay 46 scripts, every current frame matching. Peak RSS 414 MiB, 3:56
  (work/094-evidence/make-test.txt).
- **make accuracy:** rc 0, 3:55, peak RSS 1.23 GiB (work/094-evidence/make-accuracy.txt).
- **clang-check:** 73 files, 0 failing, the control refused.
- **Mutants** (predictions at 7a097a3 before any run; work/094-evidence/mutants-run1.txt and -run2.txt): run 1, 7 of 9
  exactly as predicted, P0 (the control) green.
  - **P4** (the copy always an expression) was my miss on one app_test check: I predicted 088's "blank keys do nothing"
    red, and it stayed green. The test's next key is SIMP, and SIMP on the expression `3` gives the plain number 3
    again (088's rule), so the check passes either way. Everything else P4 predicted went red: 15 vectors, 048's and
    088's ENTER checks, cas088's replayed frame.
  - **P7** (EVAL off the bar) also turned app_test's ENTER check red. That check had reused the state the EVAL check
    left, so it wasn't independent. It now sets up its own state (3ca88f3), and run 2 (P0, P7) went exactly as
    predicted: the bar check, the EVAL check, help_test's 314 of 315.
  - P6 (EVAL's rows check removed) reds 038's M1 set exactly (048 C01, C03, C05-C10, C12, C13), plus 088 S05 and 094
    K06, as the blueprint said it would.
- **D68** (`scripts/resources.py static 3ee180d 30dac26`, the commit before the src; work/094-evidence/d68.txt): the
  stand-in and Rev A each flash +400, DIRAM +0, IRAM +0, calc_task's frame +0; no internal-RAM symbol changed.
- **The stand-in** (image 30dac26, its binary built 02:48:18Z, then flashed; work/094-evidence/board/):
  - report094 (new): 10 PASS. R=V÷I's frame 80608263; after ENTER, soft1=SIMP (X's bar: an expression in X) and the
    frame E0837082, the host's; stack free 25,708; only the control red.
  - repin-v160 (EVAL now): 22 PASS, msg=CONDITION FAILS on the board; red only `ver=0.88` and its two controls.
  - cas088 (unedited, ENTER on X=3 now the copy): 18 PASS; red only `ver=0.88` and the control.
- **Claims:** lepton docker soroban-* 4 GiB and the stand-in board 02:45:17-02:52:14Z (clang-check, D68, the flash and
  three scripts); peak 731.6 MiB (docker stats every 2 s, soroban-* containers, 93 samples). The host's make test and
  make accuracy ran without a claim (under the small-run line). The board is left on the stand-in's image at 30dac26.

## For the next unit

- eq-04: K02, K03, F03 and E02B to EVAL (soft key 3), TOPICS:99 in tutor's words (#14150). Tutor's, after acceptance.
- Unit 092's build (the store's ring), as queued.
- (c) from the tension, INFO naming a copied answer's rows: a later addition, ruled not in 094.
