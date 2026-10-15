# Work unit 071: the games, more keys and a sixth game

**From:** arcade (stu32-arcade docs/salvo.md, docs/snake.md, docs/stage2.md) · **Asked and ruled by:** Michael
(abacus #8558: two control modes for SNAKE, more keys for BABAL, a shooter, folders; #8569: the shooter Galaga-style,
in space, and a GAMES group; #8610: one best for SNAKE's two modes) · **Blueprint read:** abacus #8580 (SALVO), and
the code review of the public head, #8623 · **Gate:** soroban · **Status:** built and checked on the host and on the
S3's bench; the board runs are the gate's.

## What changes

Three commits, each green on its own, so one that fails at the gate can be dropped without the others.

1. **BABAL** at stu32-arcade 5947c16. In play, SOFT1 also steers left and SOFT6 right (the thumbs at the screen's top
   corners), and 9, × and + also jump beside 8 and ENTER. A side held by two of its keys steers as one: babalcore
   would add the two, so the app tells it the side went down with the first key and up with the last. On the title
   the soft keys still pick a skill, and the help names the play keys. The rules are babalcore's, unchanged. Babal now
   lives in stu32-arcade's games/babal, so games.list names the manifest games/babal/stu32-game.txt.
2. **SNAKE** at 5947c16: two control modes. TURN, the default, turns a quarter turn left with 4 or SOFT1 and right
   with 6 or SOFT6, from the last queued direction (two quick lefts make a U-turn over two steps); 8 2 4 6 is the
   other. 4 or 6 on the title changes the mode, and the title says which mode and how. The mode is kept in the save's
   first spare byte, so a save from before loads as TURN with its best, and one best covers both modes.
   device/standin/snake.txt's second game now turns up with 4 (TURN's left) where it pressed 8.
3. **SALVO** at 5947c16, the sixth game: a fixed-screen space shooter. The ship moves with 4 or SOFT1 and 6 or SOFT6
   and fires with 8, ENTER, 9, × or +, held; 32 aliens come in from the top, sway, and dive at the ship in twos and
   threes, firing as they come; three ships, one more every 10,000 points (at most 5). Its manifest says GAMES;
   until the folder menu (unit 072) it sits in APPS like the others, on the second page after MINES. New:
   device/standin/salvo.txt.

The keys are Michael's, and Rev A's matrix takes two keys at once: the rollover log on board 0005 (abacus #8604, 182
events) shows every down with its up and nothing stuck. Ten of the twenty steer and fire pairs and all three steer
pairs were held together, and 6, × and + held as three corners of a matrix rectangle never ghosted the fourth (R/S).

## Decision 68

**Static, `scripts/resources.py static 3f6d0ea 3c6a8c9` (the unit):**

| | stand-in | Rev A |
|---|---|---|
| flash | +6,128 | +6,160 |
| static internal RAM (DIRAM) | +0 | +0 |
| IRAM code | +0 | +0 |
| calc_task's frame | +0 | +0 |

libmain is +5,806 on the stand-in and +5,850 on Rev A, the games' code (SALVO's 5.3 KB and the other two's keys);
libesp_app_format +256 on both. No internal-RAM symbol moved.

**SALVO alone, `scripts/resources.py static a79dcb4 3c6a8c9`:** flash +5,488 on the stand-in and +5,584 on Rev A
(libmain +5,324 and +5,408, libesp_app_format +132 on both, as with the other games' units); static internal RAM,
IRAM code and calc_task's frame +0 on both; no internal-RAM symbol moved.

**At run time:** SALVO's ctx is 488 B, in PSRAM, and nothing is allocated. SNAKE's ctx is 1,952 B and BABAL's 164,688 B.
Every game file builds under `-Wframe-larger-than=1024`.

## The checks (stu32-arcade, on the host, at 5947c16)

**SALVO, `make salvocheck`:**
1. The logic against a Python model written by a fresh reader from docs/salvo.md alone (its 41 open points are now
   rules in the doc): 300 fuzzed games and the model's own six scripts, 242,403 state lines identical.
2. Long games: an autopilot that looks ahead on copies of the model, three seeds of 20,000 steps and then to the end,
   61,457 steps identical, waves 50 to 53, five ships held, each game over.
3. The rules at their edges: 34 cases with their values worked out by hand, among them a shot meeting a diver
   head-on, two things on the ship in one step, the enemy shots' cap and a refused shot not tried again.
4. The app on the SDK runner: scripts with frame hashes as committed (key frames included), each tick's ask, the
   save, every tone and tune; ticks of 50, 60, 99, 100 and 250 ms; ENTER refused at 0.499 s into GAME OVER and taken
   at 0.51 s; five damaged saves refused; the same frames, asks and saves through the .wasm and the bridge.
5. Planted, each red (36).

A second fresh reviewer read the code and ran its own mutants. It found six faults, all fixed before this unit: a
shot could pass through a diving alien between two steps (shots are now tested over the path they moved), the
manifest was missing, a tune was cut short by the next click, the ship's burst stopped after 20 of its 40 steps, the
wave counter wrapped at 256, and two things on the ship at once had no check. Its nine surviving mutants are plants.

**SNAKE, `make snakecheck`:** 260 fuzzed and grown games (913,897 lines, turns included) identical to the model; the
cycle autopilot wins at length 880 for three seeds; 17 edge cases by hand (three of TURN's); the app's scripts in
TURN (the soft keys and 4/6 in turn) and in 8 2 4 6, a save with a mode of 2 refused; 20 plants red.

**BABAL, `make appcheck`:** every script identical to babalcore tick for tick, every frame to the reference renderer;
the scripts press both keys of a side and every jump key; plants for SOFT1 not steering, either key's release ending
a side held by two, and 9 not jumping, each red. `make test`, `oracle`, `sdkcheck`, `driftcheck`, `wasmcheck` green.

**On the S3** (soroban's stand-in, about 23:05Z, stu32-arcade at 5947c16): DEVICE PASS for all three.

| bench | script | frames | device = host |
|---|---|---|---|
| salvobench | play | 2,060 | ca3f39832b7d905c |
| salvobench | refuse | 201 | 1fa3b09dee8d3159 |
| snakebench | play | 14,506 | 1e688bba5c0462c4 |
| snakebench | refuse | 77 | 16b34054d18bf748 |
| appbench | 9 scripts | 32,032 | each equal |

Each bench's control is red. SALVO: tick 12 µs on average (75 worst), draw 1.38 ms (2.39 worst). SNAKE: draw 0.54 ms
(2.11 worst). BABAL: tick 175 µs (552 worst), draw 1.54 ms (4.67 worst).

Here, with the games listed (3c6a8c9 on 3f6d0ea): `make test` green after each of the three commits, and
`make clang-check` 63 files, none failing.

## The board (device/standin/*.txt, for the gate)

- **salvo.txt:** APPS, its second page (K5, more), SALVO on K1; ENTER for a game; 6 and 8 held together for 5 s with
  FRAMEBENCH in it; the keys let go and the ship left to be lost three times (an idle ship lasts about 18 s on the
  host, so 25 s pass); ENTER for a second game; stack-free at least 2,048, C, and 2 ENTER 3 + gives 5 from X = 4.
- **snake.txt:** as before, with the second game's 8 now 4.
- **babal.txt:** unchanged; soft key 2 still picks EASY and starts it.

## Not in this unit

- SALVO's frame time on Rev A: its blueprint computes about 30 ms a frame on average for the rows the game changes,
  at Rev A's 1 MHz LCD clock, and 55 ms at worst. The stand-in cannot measure it (it draws to an RGB panel), and
  Rev A's console has no FRAMEBENCH. Worth measuring when the gate's Rev A run comes.
- SALVO's difficulty stops rising at wave 8. A pilot without look-ahead loses every game by wave 5, so it is kept as
  designed; a late ramp is Michael's to ask for once he has played it.
- The ten untried steer and fire pairs (ENTER with a steer key, and SOFT1 or SOFT6 with 8, × or +): two-key holds
  cannot ghost on any matrix, so a second log is a formality for when the board is in its case.
