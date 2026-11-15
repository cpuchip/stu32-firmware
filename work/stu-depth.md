# STU's stack at 16 levels (blueprint)

**PARKED: for a future 50g or RPL mode (abacus decision 101); not built.** Michael ruled that STU stays at 4 levels (his
words, through abacus #15019: "Yes 4 level stack now record it" and "Please keep them for a 50g or RPL mode"). Unit
099 is withdrawn; no src was written. The evidence and prototype.diff stay. In such a mode the 50g's model comes whole,
as its own mode: no T copy-down and an interactive stack, not this blueprint's hybrid of the 35s at four and the 50g
above. What led to the ruling: tutor's walk diff on prototype.diff (#15005, via abacus #15008) moved 49 of 161 STU
walks, 4 of them visibly to learners (rpn-01's T-copy section), because a walk keeps one device through a lesson.

Asked by abacus (#14960), after the page-levels release (primer 4580f0e, live 11-12 20:56Z). Decision 100: STU's stack
has 16 levels; the 33s and the 35s stay at 4. This is the blueprint, with no src. The one piece of code is a throwaway
prototype in a scratch worktree, kept as `work/stu-depth-evidence/prototype.diff` so its measurements can be repeated.
Everything is measured on the host at ffc9fa4 unless it says otherwise.

## The answer in short

- **The rule:** STU's stack moves over its *live depth*: the highest level that isn't bare (090's B′ mark), and never
  less than 4.
  - With nothing live above T, every key does exactly what it does on the 35s: T copies down on a drop, and R↓ and R↑
    roll four levels.
  - Above T, the stack behaves as the 50g's does. A lift pushes T up into level 5 instead of losing it. A drop brings
    level 5 down into T and leaves the vacated top bare, so the stack shrinks back toward 4 as values are used.
  - A lift with all 16 live loses level 16, as the 35s loses T.
- **The measurement behind it:** the prototype passes the whole suite except one vector: 21,960 of 21,962 vectors and
  60,982 of 60,984 expectations. The one is my own 098 O03, counted twice (make test and the sanitizer run). Its keys push
  a value past T, and on 16 levels that value comes back down where the 35s loses it. screen_test's case on the same keys
  is the only other change.
- **What doesn't move:** the 33s and the 35s, a program stamped 33 or 35 even when it runs in STU, SOLVE's and ∫'s
  evaluations, and an equation's evaluation all keep four levels. Levels 5 to 16 wait untouched while they run.
- **The frame:** it draws as many live levels as fit, up to 8 (X, Y, Z, T, 5, 6, 7, 8). When more are live, the top
  line counts them ("9 LEVELS ABOVE"). The page above gets the lines that are left.
- **The image:** a new section, T_DEEP (state version 27), written only when a level above T is live. An image of a
  stack four deep or less is the same as today's; an older image loads with levels 5 to 16 bare.
- **The cost:**
  - ab_calc: +776 B, measured in the prototype, in PSRAM on both boards.
  - Internal RAM: comp's level arrays and one line for the deep levels, about 2.1 KB. That's arithmetic, and D68
    measures it at the build.
  - screen_page: about +12.7 KB on hosts. On the device, only dev_bench holds one, in PSRAM.
- **For tutor:** a lesson's T check changes only where the stack went past T at some point before it. Horner and the
  guide's constant example are unchanged from a fresh or cleared stack. poly-01's STU version adds one key, CLSTK.

## 1. The levels and what each key does

**Storage.** X, Y, Z and T stay the named fields every op reads (454 lines in core and firmware match
`(->|\.)(x|y|z|t)\b`, some of them other structs' fields; the 33s and 35s code is among them). Levels 5 to 16 are a new array, `ab_val deep[12]` after T. Only these touch it: the lift, the drop, R↓, R↑, the
clears, save and load, the screen, the held rows and expr_mark. Lift and drop each have one site today (`stack_lift`
and `stack_drop`, calc.c:108-121, named on 10 lines), and every other write to T is a restore or a result in place
(the census is in §6).

**The live depth** n: the highest level, from 16 down, that isn't bare, and at least 4. Bare is 090's mark (a cleared
level's 0, never made by an op, carried by every copy). The 35s's rules are the n = 4 case.

| key | n = 4 (nothing live above T) | n > 4 |
|---|---|---|
| lift (ENTER, a number arriving with lift on) | as the 35s; T goes to 5 instead of being lost | T to 5, … , 15 to 16; 16 is lost when it's live |
| drop (a two-number op) | as the 35s: T copies down (UG p.2-5) | Z to Y, T to Z, 5 to T, … , n to n−1, then n left bare |
| R↓ | as the 35s: X to T (UG p.2-3) | over n: Y to X, … , X to level n |
| R↑ | as the 35s: T to X (UG p.2-4) | over n: level n to X, X to Y, … |
| x↔y, LASTx, CLx | unchanged | unchanged |
| CLSTK, CLEAR ALL, MEMORY CLEAR | all 16 bare 0, in every mode | the same |

- **"Does the top level copy down as T does?"** T does, whenever nothing live is above it, exactly as on the 35s. A top
  level above T doesn't: it leaves bare, so a deep stack drains back to four instead of filling with copies of its top.
  I considered the plain rule, "level 16 copies down", and set it aside, because with 16 levels it breaks the 35s's own
  uses of T's copy. After `1.5 ENTER ENTER ENTER`, the copy that would come down is level 5's bare 0, not 1.5. And R↓ on
  `2 ENTER 3` would send the 3 to level 16, out of sight.
- **The lift when T goes to 5:** that's the one change to the n = 4 column. On the 35s the old T is lost; here it's kept,
  and it comes back down at the next drop. 098 O03 is that case, and it's why the suite moved there and nowhere else.
- **The bare mark moves with its value,** as it already does: a bare T lifted into 5 stays bare, so a fresh stack's
  ENTERs don't raise n.
- **The 16 limit:** a lift with level 16 live loses it with no message, as the 35s loses T (UG p.2-5). A STACK FULL
  message is possible. I don't propose one, since a learner reaches it only with 16 values live, and the 35s's own
  loss at T has none.

**Where the deep levels hold still** (n is taken as 4, and 5 to 16 aren't moved):
- **The 33s and 35s modes.** Their stack is four levels, as decided. Levels 5 to 16 are kept through a mode switch and
  reappear when STU returns, since a mode setting is neutral to the stack today (calc.c's AB_MODEL). A clear clears them
  in any mode, so a 35s CLSTK followed by STU doesn't bring old values back (prototype P17).
- **A program stamped 33 or 35, run in STU.** It runs on four levels, as it would on that machine (unit 025's stamp,
  `ab_rec_model`). A program stamped STU runs on 16 (P14, P15).
- **SOLVE's and ∫'s evaluations** (`c->active`). Their results are defined on the 35s's four levels: SOLVE's root,
  previous estimate and D-value in X, Y and Z (UG p.7-2); ∫'s result in X and its uncertainty in Y (UG p.8-6), with
  the limits in Z and T as integ_finish writes them (calc.c:2918). The levels above T keep the
  user's values across the whole solve (P16).
- **An equation's op** (`ab_eqn_op`, calc.c:5934). It runs do_key on a borrowed X and Y and restores X to T afterwards,
  so a drop inside it mustn't move levels 5 to 16. A hold counter covers it.

## 2. The frame (STU's home)

The layout's own constants (screen.c:391-395) fix what fits. X's bottom is at 200, Y's at 166, and each level above is
20 px higher: Z 146, T 126, 5 106, 6 86, 7 66, 8 46. Level 8's top is at 32, under the band's rule at 22. A ninth
would start at 12, so **8 levels is the most the area holds**, X included.

- **Which levels are drawn:** the live ones from X up (B′ as now: a bare top level is hidden), as many as fit. If more
  are live than fit, the highest line that fits becomes a count, "k LEVELS ABOVE", drawn at the left like the page's
  "230 OLDER ROWS DROPPED". With all 16 live, that's X to 7 and "9 LEVELS ABOVE". X in 2D (082) raises X's top, so
  fewer fit, by the same rule.
- **The page gets what's left:** 4 lines with 4 levels drawn, then 3, 2, 1 and 0 as levels 5 to 8 appear. This is the
  one visible choice, the same kind as 090's A and B:
  - **(A), my lean:** as many levels as fit. The stack is the work in hand, and the page comes back as the values are
    used.
  - **(B):** the levels and the count take at most 6 lines, so the page keeps 2: 6 levels when 6 are live, else 5
    levels and the count above them. (Corrected 11-15 at the frames: 5e0c7b2 said "at most 6 levels … with the count
    above level 6", which leaves the page 1 line, not 2.)
  - **The frames** (abacus #14992), rendered from the prototype: `work/stu-depth-evidence/frames/`, made by
    depthframes.c, with their keys and hashes in frames.txt.
    - d1, 7 levels live and 3 rows for the page: (A) draws X to 7 and 1 page line; (B) draws X to 5, "2 LEVELS
      ABOVE" and 2 page lines.
    - d2, 16 live: (A) X to 7 and "9 LEVELS ABOVE"; (B) X to 5 and "11 LEVELS ABOVE".
    - d0, 4 live: (A) and (B) are the same frame, as today's.
  - I can render both as frames from the build's code for Michael's card, as p1-A and p1-B were.
- **Names past T:** 5, 6, 7 and 8 in the letter's place at x 2. Each is one cell at scale 2, as X Y Z T are.
  - The 50g labels its levels with numbers from the bottom (50g UG p.1-15), and Michael's note names the 50g. Letters
    past T have no convention.
  - 10 to 16 are never drawn on the device, since 8 is the most that fits. Two cells would reach x 24, over the op
    name's place at NUM_X + 2 = 22.
- **The op beside a level (098) and the marks (093)** go with each drawn level, 5 to 8 included. Their fit is Y's
  (scale 2).
- **Unchanged:** ALG (the line and its result; no levels), the 33s and 35s screens, and every STU screen that isn't
  the home: program entry, Equation mode, TABLE, SHOW, SYS's view, the steps view and the textbook.

## 3. What screen_page and the vectors export

**screen_lines (screen_page):**
- **Unchanged:** `t`, `z`, `y` and `x` keep their meaning and their text, so a host reading four lines (the web page's
  stage2, the app, the 10" panel) works as it does today, with T still level 4.
- **New:** `screen_line deep[12]` for levels 5 to 16, each with its label ("5" to "16") and its text, whether drawn or
  not. That's the rule t, z and y follow now (090: "every one's text kept"). A host larger than the device can draw
  more than 8, and the web page's screen-reader label can read the whole stack.
- **Widened:** `levels[5]` to `levels[9]` ("8765TZYX" at most, top to bottom, one character each); `levelmark[4]` to
  `[8]`; `levelop[4][32]` to `[8][32]`.
  - Their order stays X, Y, Z, T, then 5 to 8, so index i means the same level as before. 098's blueprint promised
    these three would widen together, here.
- **New:** `int depth` (the highest level not bare, 1 to 16; unlike §1's n it isn't floored at 4) and `int above` (the count line's
  k, 0 when there's none).
- **The cost:** 12 × 1,048 B for the lines (screen_line is 1,048 B on the host, measured), plus 136 B for the widened
  arrays and 8 B for the two ints. That's about +12.7 KB, on a screen_page of 8,208 B today. The device's only
  screen_page is dev_bench's, in PSRAM; internal RAM +0 from this.
- **A question for primer, through you:** does the web page's calc_screen read screen_page through named fields that
  its own build compiles, or by byte offsets? Widening levelmark and levelop in place moves the offsets of what follows
  (xmark, levelop). If it reads offsets, the new fields go at the struct's end instead and the old ones keep 4 entries.
  calc_screen isn't in this repo, so I can't check it.

**The vector grammar (tests/vectors.c), all additions; every existing expectation reads as before:**
- `^n=v` for level n, 5 to 16, as `X=v` reads X; `%LBL:^n` for the row that level holds.
- `%DEPTH=n`: screen_page's depth. `%ABOVE=k`: the count line's k, 0 for none.
- `%LEVELS=` takes the wider string: "8765TZYX", and so on down.
- `%LEVELOP-k` and `%LEVELMARK-k` take k from 5 to 8 as levels. A level not drawn stays `?`, and 9 to 16 are always `?`.

## 4. The saved state

- **A new section, T_DEEP (state version 27).** It holds levels 5 to n:
  - a count byte (n − 4);
  - each level's value as T_STACK writes one (wval);
  - its row (4 B);
  - one byte of marks (bare, rounded).
  It goes after T_STACK, ahead of T_ROWREF, T_BARE and T_ROUNDED, which keep their fixed fields for X to T.
- **Written only when a level above T is live,** in any mode, since the levels outlive a mode switch. That keeps the
  082 rule that an image carries the version its content needs.
  - An image of a stack four deep or less is the same as today's, byte for byte, version 25 or 26 as now. A test
    checks that against ffc9fa4's images for the same keys.
- **Loading:**
  - An older image (no T_DEEP) loads with levels 5 to 16 bare.
  - A version-27 image on an older build loads X to T and drops the deep levels. That's the existing rule for a newer
    image (state.c:1567, "what it adds dropped").
- **Size, from state.c's encoding** (wval: 3 B, plus 19 B for each part that isn't zero; wnum is 1 + 2 + 16; an
  expression is 3 B plus its text, up to 192 B):
  - a real level is 27 B with its row and marks, so 12 real levels are 7 + 324 = 331 B;
  - the worst case, 12 distinct expressions of 192 B, is 7 + 12 × 200 = 2,407 B;
  - IMAGE_CAP and the store's slot are each 256 KB (reva and standin: abstate 0x80000, two slots), so neither is near.
- **UNDO** takes its images from ab_state_save, so it brings the deep levels back with no change of its own.
- **Held rows:** `ab_rows_held` and its two callers in state.c grow from 33 to 45 values, so the page never drops a row
  a deep level holds. T_DEEP carries those rows, and T_ROWREF's 33 stay as they are.
- **Expressions:** the store's 48 slots hold everything distinct at once: 16 levels + LASTx + ANS + 27 letters = 45.
  Copies share a slot, so a store of 48 needs no growth. `expr_mark` (calc.c:5529) must walk levels 5 to 16, or a
  garbage pass frees their slots. That gets a test of its own.

## 5. The lessons that check T, and poly-01's Horner

I can't read tutor's lessons, so here is the rule with the measurements behind it, and an offer.

- **A T check is unchanged unless some key before it lifted a live T into level 5** (a value the 35s would have lost),
  **and a drop or a roll then brought it back down.** Any sequence that never lifts a live T gives exactly the 35s's
  stack. The whole suite is the evidence: its only change is 098 O03, which is that case.
- **Measured on the prototype** (`work/stu-depth-evidence/probes.txt`, predictions written before the run; 15 of 17 held,
  and the two that didn't are below):
  - **The guide's constant example** (UG p.2-7, growth 1.5 for three days from 100) on a fresh STU gives X 337.5 and Y,
    Z, T 1.5, as the 35s (P01).
  - **Horner,** 2x³ − 3x² + 4x − 5 at 2 by `2 ENTER ENTER ENTER 2 × 3 − × 4 + × 5 −`, gives 7 from a fresh stack (P03),
    after CLSTK (P05), and by RCL× from any stack (P06).
  - **With a leftover value** under the copies (`9 √x` first), the copy-down stops once that value reaches T.
    - The constant example still gives 337.5, but Z and T hold the leftover 3 (P02).
    - Degree 3 still gives 7, since Y, Z and T's three copies cover its three multiplications.
    - Degree 5 doesn't: x⁵ + x⁴ + x³ + x² + x + 1 at 2 gives 139, not 63 (probes-stacks.txt, Q04b).
  - **My two wrong predictions:**
    - P02: I'd traced a lift when 100 was typed. A number typed after ENTER writes over X (UG p.B-5, as calc.c's ENTER
      cites it), so the leftover reaches T after the first ×, not the fourth.
    - P04: I'd predicted degree 3 would break with the leftover, and wrote it as "not 7" while its line expected 7. It
      gives 7, for the reason above, so the line passed and my prediction didn't.
- **For the eight lessons** (eq-02, exp-01, exp-04, num-01, num-04, poly-01, rpn-01, seq-01):
  - Each is unchanged if its keys never lift a live T into level 5.
  - Each changes where they do and the value comes back down, or where it starts from the learner's stack with values
    above T.
  - **An offer:** send me the eight lessons' keys and T checks as vectors in your own wording (no tutor files), with
    how tutor's runner starts a lesson (a fresh calculator or the learner's stack). I'll run them on the prototype and
    send which change, before you judge this blueprint or after, as you like.
- **poly-01's Horner in STU:** start with CLSTK.
  - It keeps the lesson's idea, T's copy feeding every step, and adds one key.
  - It can say why in one line: STU's stack is deeper, so a value left above would come back down.
  - The RCL× form (`2 STO X 2 RCL× X 3 − RCL× X 4 + RCL× X 5 −`) works on any stack and in all three modes. It fits a
    lesson about registers better than one about the stack.

## 6. The census of the four-level code

- **Moves:** `stack_lift` and `stack_drop` (calc.c:108-121, 17 lines naming them), R↓, R↑ and CLSTK (calc.c:6024-6060).
- **Clears:** `init_fields` (calc.c:55), `ab_clear_all` (5867), CLSTK (6026).
- **Restores or results in place, no move:**
  - SOLVE's and ∫'s suspended stack (susp, ev_*: 2026, 2675-2737, 3037, 3663-3688);
  - integ_finish's limits into Z and T (2931);
  - SOLVE's 3×3 solution into Z (3732);
  - `ab_eqn_op`'s restore (5936-5947).
  These are held as §1 says, so none of them needs a deep copy, and susp stays 424 B.
- **Readers that stay four:**
  - the 33s's CMPLX pairs (Z, T: 924-928);
  - rows.c's per-key checks, `pre[4]`, `lv0[4]` and `stale()`. A deep level changes only by a move, which carries its
    row with its value, so rowctx stays 2,160 B.
- **Readers that widen:**
  - the screen: compose and page_plan, `lvl_bot`, `lvl_at` and `lvl_mark` to 8;
  - screen_lines;
  - state.c: T_DEEP, plus the held rows (33 to 45 at state.c:526 and 589, and rows.c:218);
  - expr_mark (calc.c:5531);
  - vectors.c (`reg`, `lbl_val`).

## 7. The cost

| | where | change | how known |
|---|---|---|---|
| ab_calc | PSRAM (standin_main.c:80, dev_bench.h:370) | +776 B (185,088 to 185,864 on the host) | measured, the prototype |
| comp (screen.c:803, about 8 KB) | internal DIRAM | level arrays 4 to 8 (+36 B), one `line` for levels 5 to 8 composed in turn (+2,064 B) | arithmetic; D68 at the build |
| screen_page | hosts; dev_bench's in PSRAM | about +12.7 KB | arithmetic from measured sizes |
| the image | flash store, UNDO's buffers | +0 at four deep; 331 B at 16 reals; 2,407 B worst | state.c's encoding |
| calc_task's frame | internal | the held-row arrays +48 B each, if they're on its path | D68's frame line |
| flash | | unknown until built | D68 |

- **Why only one deep line:** drawing levels 5 to 8 through one shared `line`, filled level by level, keeps comp's
  growth to about 2 KB. Four lines in comp would cost about 8.3 KB.
- **The headroom:** reva's DIRAM is 215,374 of 341,760 B at 0919f8b (work/098-evidence/d68.txt), so either fits. The
  smaller is the lean.

## 8. "As memory allows", a later option

Michael's 10-08 note, as abacus quotes it in #14960: "16 or infinite as memory allows like the HP 50g". 16 is what
gets built. The rule in §1 is already written in terms of the live depth, so going past 16 later changes the storage
and the bounds, not the rules. What it would cost:

- **Storage:** levels past T become a growable array in PSRAM, 64 B a level, so 1,000 levels is 64,000 B. That's the
  size of the worksheet's whole page (AB_ROWS_RAM 65,536 B), and a cap is still needed: at it, MEMORY FULL, or the top
  is lost as at 16.
- **Expressions:** the 48 slots stop being enough once more than about 19 levels hold distinct expressions (45 at 16).
  The store would grow with the stack, at 192 B a slot.
- **The image:** each level is 27 B (a real) to 200 B (an expression). The slot's 256 KB would hold about 9,000 real
  levels, but a save has to fit beside the page's 8 KB and everything else.
  - So it needs a rule of its own, as the page has: keep the bottom N levels and count the rest as dropped. Michael
    would have to rule on that, since a reload would lose values.
- **UNDO:** two image buffers of 256 KB each, the same bound.
- **Held rows and the screen:** the held-row arrays become dynamic. The frame's count line already handles any depth,
  and screen_page's `deep[12]` would become a window or a count.
- **The 50g's own stack is more:** it holds objects of any type, and its levels scroll interactively (an inspector). A
  stack view to scroll past 8 on the device would be its own unit, not part of either option.

## 9. The test (the oracle), for the build

1. **The suite.** Every vector and frame as at ffc9fa4, with one exception: 098 O03 and its screen_test twin change by
   design.
   - O03 is mine. I'd rewrite its keys to stay four deep, so it still tests that T's copy carries its row. The deep
     case becomes a new vector.
   - h098 and every other set of abacus's pass unchanged, as they did on the prototype.
2. **Vectors** (099-soroban), each written from the rule before the build:
   - the probes, corrected;
   - each key in §1's table at n = 4 and n > 4;
   - 16 live and a lift;
   - the holds (the 33s and 35s, a 35s-stamped run, SOLVE, ∫, an equation's op);
   - CLSTK in each mode;
   - UNDO after a drop at n > 4;
   - the new grammar (`^n`, `%DEPTH`, `%ABOVE`, `%LEVELS` past T, `%LEVELOP-5`, `%LBL:^n`).
3. **Frames (screen_test):**
   - 5, 6, 7 and 8 levels drawn;
   - 16 live (X to 7 and the count);
   - X in 2D under a deep stack;
   - a prompt over a deep stack;
   - a selection on level 6 (its inverted band);
   - each new frame's hash recorded, and the existing 67 unchanged;
   - level_ops' pixel check extended to levels 5 to 8.
4. **The image:**
   - 16 mixed levels round-trip: a real, a complex number, a vector, an expression, a name, bare and rounded marks, rows;
   - an image at four deep is the same bytes as ffc9fa4's for the same keys;
   - a version-26 image loads with levels 5 to 16 bare;
   - the save and load vectors (AB_SAVELOAD=1) over the whole suite.
5. **expr_mark:** an expression at level 12 survives a garbage pass.
6. **Mutants, each predicted before the run:**
   - the drop at n > 4 copies the top instead of leaving it bare;
   - R↓ rolls 16 instead of n;
   - SOLVE's hold removed;
   - the 35s-stamped run at 16;
   - T_DEEP not written;
   - expr_mark without the deep levels;
   - the count line off by one level.
7. **D68** at the build: flash, DIRAM, IRAM and calc_task's frame on both boards.
8. **The board:** the stand-in, since the frames change. That's a claim on the board, and the frames are read back from
   it.

The build would be unit 099, VER 0.100 (basecamp's call, #14733). stu-indirect follows it, as #14960 orders.

## For abacus

- **To judge:**
  - the rule in §1, which answers "does the top level copy down";
  - the holds;
  - (A) or (B) for the frame, which looks to me like Michael's to see with frames;
  - the labels 5 to 8;
  - CLSTK clearing all 16 in every mode;
  - level 16 lost silently on a lift.
- **To answer:**
  - calc_screen's reading of screen_page, named fields or offsets (§3);
  - whether to run the eight lessons on the prototype (§5).
