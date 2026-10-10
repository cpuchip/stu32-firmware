# Work unit 065: MINES revision 2, the settings

**From:** arcade (stu32-arcade docs/mines.md, revision 2) · **Ruled by:** Michael's decisions 76 and 77 (abacus
b12e275 and c20db07: a settings page with classic rules, sound effects and three board sizes; hard as 30 x 16 with
99 mines dealt in the background with the cap at 400; no time limit) · **Blueprint approved:** abacus #6455, at
stu32-arcade fd86980 · **Gate:** soroban · **Status:** built and measured on the host and on the S3's bench; the
board run is the gate's.

## What changes

MINES gets a settings page (0 on the title, or at the end once ENTER would play again): BOARD (EASY 9 x 9 with 10
mines, MEDIUM 16 x 16 with 40, the default, HARD 30 x 16 with 99), CLASSIC RULES (a box: deal the first board drawn,
which may need a guess) and SOUND EFFECTS (a box: the game's own tones and tunes; the device's VOL, BEEP and key
click are untouched). Revision 1's single 24 x 13 board with 48 mines is gone.

The hard board's no-guess deal can take up to 2 s on the S3, so the first open no longer deals inside its key: it
starts the deal, the screen says DEALING, and each tick makes up to 4 attempts until one is dealt (cap 400). The clock
starts on the tick that deals, so a slow deal is not timed. Keys during the deal do nothing.

The save grows from 16 to 80 bytes, version 2: the settings, then the best time, wins and games for each board and
rule. A revision 1 save is refused (its best is for the board that is gone), so the first launch after this unit
starts at the defaults.

The series is one commit: games.list pins `mines` at stu32-arcade 7344045 (the game's files there are those of
a47067d, which the S3 measured; 7344045 adds the measured step times to the blueprint), and device/standin/mines.txt
is its new board script.

## Decision 68

**Static, `scripts/resources.py static 8cb8e2a <065>`:**

| | stand-in | Rev A |
|---|---|---|
| flash | +1,792 | +1,808 |
| static internal RAM (DIRAM) | +0 | +0 |
| IRAM code | +0 | +0 |
| calc_task's frame | +0 | +0 |

libmain is +1,584 on the stand-in and +1,604 on Rev A, the game's code; libesp_app_format +196 on both, as with the
other games' units. No internal-RAM symbol moved.

**At run time:** the ctx is 3,536 B (2,272 before), in PSRAM: the field and the solver's scratch at the largest
board's 480 cells. Nothing is allocated. Both files build under `-Wframe-larger-than=1024`; revision 1's 624 B copy
of the allowed cells on dealing's frame is gone (the list is rebuilt for each attempt, the same list), so the largest
frame is the solver's, 224 B.

## The checks (stu32-arcade `make minescheck`, on the host, at a47067d and 7344045)

1. The logic against the independent Python model (oracle/mines/model.py, from the blueprint alone): fuzzed opens,
   flags and deals a few attempts at a time over 270 games on every board and both rules, and 30 games won by
   opening only cells a complete solver proves safe, every state line identical.
2. No board needs a guess. The C deals the model's boards for 400 seeds a board, and with classic rules all 300
   deals are the first attempt. The complete solver (oracle/mines/proof.py) clears every dealt board on every board
   (the most attempts: 5 easy, 10 medium, 161 hard). Its group step is now a sweep that never lists the placements
   (the listing overflowed on a dealt hard board), cross-checked against the listing at every step of 500 random
   boards' proofs; two plants in the sweep go red. A new 50/50 control on the medium board is reported as needing a
   guess. Real boards never need 400 attempts, so the fallback is checked with the C and the model built at a cap of
   3: they deal the same 100 hard boards, 92 of them at the cap.
3. The rules at their edges: 22 cases written by hand (the chord, flags, floods, each board's corners and right
   edge, a deal under way, a flagged first open), every state line the model's.
4. The app on the SDK runner: eight scripted games with frame hashes as committed (key frames included): the
   settings page (every row, its ends and the wrap), a 213-attempt hard deal with keys pressed during it and a key
   held across it, sound off (no tone or tune), a win on the dealing tick (kept as 1 ms), a 1,000 s win (the top line
   stops at 999), a save with HARD, classic and sound off loaded, ties and slower wins, the cursor's repeat and edges;
   the saves, every tone and tune, and what each tick asks for (1 ms dealing, 20 in a game, 100 otherwise) as the
   app model gives them; nine damaged saves each refused by its own rule; the same through the .wasm and the bridge.
5. Planted, each red (53).

A reviewer that had not written it read the code against the blueprint and ran its own mutants before this unit. It
found no rule the C does differently, one screen fault (from 1,000 s the top line's TIME and NEW BEST overprinted)
and 17 mutants the checks could not see (settings loaded from a save, a game counted at its deal, the fallback, the
ticks' asks, and others). All are fixed at a47067d, each with a check and a plant.

**On the S3** (soroban's board, 21:45-21:47Z, stu32-arcade device/minesbench at a47067d): DEVICE PASS.

| script | frames | hash (device = host) |
|---|---|---|
| play | 1,643 | 36838d2ffaa5fda3 |
| refuse | 3,230 | da6f0e96a2814a9d |
| cursor | 678 | 5e2d677b23ea558b |
| settings | 101 | 211797fe480481be |
| hard | 189 | 433472b2e885dfec |
| quiet | 1,367 | b884d76e56db3b9a |
| instant | 121 | 2b3f401d5651bd2a |
| loadset | 58 | 3fb61f7eac2e45f1 |

The control (play with its first move made a key MINES ignores) is red. Dealing alone, in steps of 4 attempts as the
ticks make them, every attempt count the model's: EASY's worst found 7 attempts in 3.9 ms (100 more: median 0.5 ms);
MEDIUM's 16 attempts in 47.9 ms (median 5.0 ms); HARD's 213 attempts in 1.77 s (median 108 ms, most of the 100
0.61 s). The longest step of 4 attempts on any board was 83 ms, so a tick while dealing is under 0.1 s. Draw: 6.6 ms
at most.

Here, with the game listed (this commit on 8cb8e2a): make test green (app_test 668/668), clang-check 61 files and
none failing; MINES' two files compiled with its clang-18 image and flags and the frame guard: clean.

## The board (device/standin/mines.txt, for the gate)

APPS lists KEYS, BABAL, AIM, LAND and SNAKE on its first page, so MINES is K0 on the second (K5, more). On the title:
0 for the settings, 6 twice (HARD from any board the save holds; the rules and sound rows stay as the save has them),
ENTER back, ENTER for a game. A first open at the cursor (15, 8) deals on the chip from the hardware RNG, across
ticks; 3 s pass, then FRAMEBENCH and 79 presses round the field's edge, a move and an open at a time, each followed
by 0.2 s. The board cannot see the field; on the host the same keys end the game for 200 of 200 seeds (all losses),
and a control without the first open ends none of 20. Then the calc task's stack-free (at least 2,048, read before
`app:`), C, and 2 ENTER 3 + must give 5, from X = 4 (soroban's 11bc108 shape). 97 presses; at most 6 in a row without
a sleep. Not yet run: it waits on the gate's board. The board's MINES save from revision 1 is refused on the first
launch, as above.
