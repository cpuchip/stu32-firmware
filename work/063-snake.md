# Work unit 063: SNAKE, the fourth game

**From:** arcade (stu32-arcade docs/snake.md) · **Ruled by:** Michael's decision 71 (abacus aa30714: Snake after Aim
and Lander, "Make them all!") · **Gate:** soroban · **Status:** built and measured on the host and the S3's bench;
the board run is the gate's.

## What it is

Eat, grow, and keep off the walls and yourself: 8, 2, 4 and 6 turn the snake on a 40 × 22 grid; each food makes it
one longer and the steps quicker (150 ms down to 60); a snake that fills all 880 cells has won. The rules are
integers, and the turns go through a queue of two, so two quick keys inside one step both count. The game is
stu32-arcade games/snake at a01609b (snake_logic: the grid, the queue, the step and the food; snake_app: the
app_def), MIT, written for the STU-32. SNAKE has five letters, so it is its own APPS name.

The series is one commit: games.list lists `snake` at a01609b, and device/standin/snake.txt is its board script.

## Decision 68

**Static, `scripts/resources.py static fd00c0f <063>`:**

| | stand-in | Rev A |
|---|---|---|
| flash | +2,912 | +2,848 |
| static internal RAM (DIRAM) | +0 | +0 |
| IRAM code | +0 | +0 |
| calc_task's frame | +0 | +0 |

libmain is +2,726 on both, the game's code; libesp_app_format +88 on both; on the stand-in libbid +96 and libc,
esp_hw_support, freertos, esp_driver_i2c and esp_lcd within 12 B either way. No internal-RAM symbol moved. As with
AIM and LAND, the small moves come with a listing; I have not traced them further.

**At run time:** the ctx is 1,944 B, in PSRAM, most of it the snake's ring of 880 cells. Nothing is allocated; both
files build under `-Wframe-larger-than=1024`. Placing food walks the grid once, 880 bitmap reads.

## The checks (stu32-arcade `make snakecheck`, on the host, at a01609b)

1. The logic against an independent Python model (oracle/snake/model.py, from the blueprint alone): fuzzed key
   scripts over 200 games and 60 grown ones (the autopilot below for 5-60 foods, then random keys; 36 die into their
   own bodies), every key and step identical.
2. A full game: an autopilot that follows a Hamiltonian cycle of the grid cannot hit itself, so it eats every food.
   For three seeds the C and the model each win at length 880 after 876 foods, identical over 594,424 steps: the
   longest snake, every wrap of the ring, food placed with one free cell left.
3. The rules at their edges, 14 hand-made cases whose lines were written by hand (the head into the tail's cell,
   into the body, into each wall; a reverse key and a repeat dropped; two turns inside a step; a third dropped; the
   interval at 0, 1, 17, 18 and 19 foods; a full grid won on its last cell), and every state line the same as the
   model's.
4. The app on the SDK runner: scripted games with frame hashes as committed, and the save and every tone and tune
   (pitch, length and time) the blueprint's rules give: ENTER partway through a title tick and during play, ticks of
   100 and 250 ms (the cap), a game that fills the grid in the app (210,367 ticks, its save and sound only), and a
   game left alive with no save and with four damaged ones, all refused. The same through the .wasm and the bridge.
5. Planted, each red (13): among them the tail freed after the body test, a reverse key taken, food over all cells,
   a queue of 1, a missing wall, a body death that moves the tail, no 100 ms cap, the tunes swapped, a damaged save
   taken, ENTER restarting a game, the best not kept at each food.

A reviewer that had not written it read the code against the blueprint and ran 32 mutants before this unit. It
found no rule the C broke, and several faults the checks let through (no body death was ever checked, the cap and
the sound were never exercised, no damaged save was ever loaded); checks 1, 3 and 4 gained them at a01609b, with the
small app fixes it named.

**On the S3** (soroban's board, 07:46-07:47Z, stu32-arcade device/snakebench at e69cfad): both app scripts played
through apps.h's calls on the chip, every frame hashed as the runner's `-hashes` does: play 14,360 frames
(625326c2ab8d36c5) and refuse 77 (43a9b40c45442d1a), each the same as oracle/snake/frames. The control (play with
its first direction key made a 0) is red, and its chip hash is the one the host gives it. A draw takes 0.5 ms on
average and 1.3 ms at worst. So the chip draws the host's frames, for these two scripts.

Here, with the game listed (this commit on fd00c0f): make test green (app_test 668/668), clang-check 61 files and
none failing.
clang-check reads the firmware's own files, not the games, so SNAKE's two files were also compiled with its clang-18
image and flags (and the frame guard): clean.

## The board (device/standin/snake.txt, for the gate)

APPS lists KEYS, BABAL, AIM, LAND and SNAKE (K4, the page's fifth); SNAKE's title, ENTER for a game, FRAMEBENCH
while it runs right into the wall, GAME OVER, then ENTER and 8 for a second game that runs up into the top wall. The
food comes from the hardware RNG, so the script cannot steer to it; on the host, with the same keys and sleeps, all
of 200 seeds ended both games (two GAME OVER tunes) before the next key. Then the calc task's stack-free (at least
2,048, read before `app:`), C, and 2 ENTER 3 + must give 5. The script sends 22 key lines, the longest run without
a sleep the closing eight. Not yet run: it waits on the gate's board.
