# Work unit 064: MINES, the fourth and last game of decision 71

**From:** arcade (stu32-arcade docs/mines.md) · **Ruled by:** Michael's decision 71 (abacus aa30714: Mines after Aim,
Lander and Snake, "Make them all!"); the rules, every board clearable without a guess with the classic rule as the
fallback, on Michael's board as mines-rules (abacus #6246) · **Gate:** soroban · **Status:** built and measured on the
host and the S3's bench; the board run is the gate's.

## What it is

Clear a field of 24 × 13 cells hiding 48 mines: 8, 2, 4 and 6 move the cursor, 5 opens a cell, 0 flags one, and
opening a number whose flags match it opens its neighbours. The first open is never a mine, nor its neighbours, and
the game only deals a board its own solver can clear from there by reasoning alone, so no game is lost to a coin
flip. The time counts from the first open, and the best winning time is kept. The game is stu32-arcade games/mines
at 1def492 (mines_logic: the field, the chord, dealing and the solver; mines_app: the app_def), MIT, written for the
STU-32. MINES has five letters, so it is its own APPS name; nothing calls it "Minesweeper" (Microsoft's title).

The series is one commit: games.list lists `mines` at 1def492, and device/standin/mines.txt is its board script.

## Decision 68

**Static, `scripts/resources.py static 4845dc6 <064>`:**

| | stand-in | Rev A |
|---|---|---|
| flash | +4,864 | +4,880 |
| static internal RAM (DIRAM) | +0 | +0 |
| IRAM code | +0 | +0 |
| calc_task's frame | +0 | +0 |

libmain is +4,738 on the stand-in and +4,754 on Rev A, the game's code; libesp_app_format +104 on both; esp_driver_i2c
+4 on the stand-in, libbid +4 on Rev A. No internal-RAM symbol moved. As with the other games, the small moves come
with a listing; I have not traced them further.

**At run time:** the ctx is 2,272 B, in PSRAM: the field, the solver's own copy and the flood's stack. Nothing is
allocated; both files build under `-Wframe-larger-than=1024` (dealing keeps a 624 B copy of the allowed cells on its
frame). Dealing runs inside the first open's key.

## The checks (stu32-arcade `make minescheck`, on the host, at 1def492)

1. The logic against an independent Python model (oracle/mines/model.py, from the blueprint alone): fuzzed opens and
   flags over 200 games, and 30 games won by opening only cells a complete solver proves safe, every state line
   identical (the mine shown as hit included).
2. No board needs a guess. The C deals the model's boards for 400 seeds, and a complete solver (oracle/mines/proof.py:
   every placement of each frontier group that fits the numbers, combined with the cells beyond and the mine total,
   so a cell is safe exactly when no consistent placement puts a mine there) clears every one opening only proven
   cells. The control, a board with a true 50/50 on its bottom edge, is reported as needing a guess; no random board
   the game's solver clears needs one. Over 20,000 seeds dealing took 14 attempts at most, and the fallback (100)
   never came.
3. The rules at their edges, 15 hand-made cases whose lines were written by hand (the chord with too few, too many,
   the right and a wrong flag; a chord on an opened 0; two mines in one chord; a flag on an opened cell; a flood
   stopping at a flag; the first open's neighbours; a flag before the first open), every state line the model's.
4. The app on the SDK runner: scripted games played by moving the cursor key by key, keys 7 ms after a tick, with
   frame hashes as committed (the frames drawn on keys included, by the runner's new `-keyframes`), and the save and
   every tone and tune the blueprint's rules give: a win and the first best, a wrong flag and a loss, a best tied and
   one beaten slower, ENTER at exactly 0.5 s, the cursor's repeat, its edges and a second key, four damaged saves each
   refused by its own rule. The same through the .wasm and the bridge.
5. Planted, each red (21), and a plant that does not build, matches nothing or runs forever (each run has a time
   limit) is red too.

A reviewer that had not written it read the code against the blueprint and ran mutants before this unit. It found the
no-guess claim sound (the game's solver order-free, the complete solver complete, the control a true 50/50), and two
display faults (a first open's frame showing a time below zero, the end box hiding mines) and checks that could not
see them (keys always on a tick, only tick frames hashed); all fixed at 1def492.

**On the S3** (soroban's board, 08:55-08:56Z, stu32-arcade device/minesbench at 731e70b): play 2,835 frames
(f3ba06436b3a3749), refuse 4,280 (38ed9a47917260e3) and cursor 666 (87be99de1d3d5085), each the same as the runner's
with `-keyframes`; the control red at its host hash. Dealing alone: the worst seed of 20,000 (8031, 14 attempts) in
47.9 ms; seeds 0-99 at 6.3 ms median and 23.1 ms at most; every attempt count the model's. A draw takes 3.6 ms on
average and 9.1 ms at worst.

Here, with the game listed (this commit on 4845dc6): make test green (app_test 668/668), clang-check 61 files and
none failing. clang-check reads the firmware's own files, not the games, so MINES' two files were also compiled with
its clang-18 image and flags (and the frame guard): clean.

## The board (device/standin/mines.txt, for the gate)

APPS lists KEYS, BABAL, AIM, LAND and SNAKE on its first page, so MINES is K0 on the second (K5, more). ENTER, a first
open at the cursor (12, 6), which deals the board on the chip from the hardware RNG, FRAMEBENCH, then 80 presses round
the field's edge, a move and an open at a time, each followed by 0.2 s. The board cannot see the field; on the host
the same keys end the game (a loss, almost always) for 199 of 200 seeds, and the script passes either way. Then the
calc task's stack-free (at least 2,048, read before `app:`), C, and 2 ENTER 3 + must give 5. 182 key lines; the
longest runs without a sleep are the closing eight and the launch's four. Not yet run: it waits on the gate's board.
