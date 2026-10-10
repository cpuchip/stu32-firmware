# Work unit 062: LAND, the third game

**From:** arcade (stu32-arcade docs/land.md) · **Ruled by:** Michael's decision 71 (abacus aa30714: Lander after
Aim, "Make them all!"); the name LAND is on Michael's board (abacus #6193), the recommended one, since LANDER has six
letters · **Gate:** soroban · **Status:** built and measured on the host; the board run is the gate's.

## What it is

Set a ship down on a pad: 4 and 6 turn it in 15° steps, 8 fires the engine. One screen of terrain made from the
seed, with three flat pads (×5 the narrowest, ×3, ×2); a landing must be upright, on one pad, at most 20 px/s down
and 10 px/s across, and scores 50 times the pad's multiplier. One fuel tank of 2,000 lasts the game, one unit for
each 20 ms the engine fires and 200 for a crash; the game ends when a round ends with it empty. The physics is all
integers, so the host, the .wasm and the S3 draw the same frames (abacus approved, #6193). The game is stu32-arcade
games/land at e20cd98 (land_logic: the physics, the contact test, the judge and the terrain; land_app: the app_def),
MIT, written for the STU-32. Nothing calls it "Lunar Lander" (Atari's title).

The series is one commit: games.list lists `land` at e20cd98, and device/standin/land.txt is its board script.

## Decision 68

**Static, `scripts/resources.py static c6092c0 <062>`:**

| | stand-in | Rev A |
|---|---|---|
| flash | +7,600 | +7,584 |
| static internal RAM (DIRAM) | +0 | +0 |
| IRAM code | +0 | +0 |
| calc_task's frame | +0 | +0 |

libmain is +7,445 on the stand-in and +7,441 on Rev A, the game's code; libesp_app_format moves +140 on both, and on
the stand-in esp_lcd +12, esp_driver_i2c −8 and hal −4, on Rev A libbid +8, esp_driver_i2c −8, esp_driver_gpio −4 and
hal −4. No internal-RAM symbol moved. AIM's listing moved the same libraries by similar amounts and its control
commit (a board script, no game listed) moved none, so these come with a listing; I have not traced them further.

**At run time:** the ctx is 320 B, in PSRAM. Nothing is allocated; both files build under `-Wframe-larger-than=1024`.
A step tests 7 outline points against the ground and each terrain vertex (at most 48, three times for the wrap)
against 7 edges, in 64-bit products.

## The checks (stu32-arcade `make landcheck`, on the host, at e20cd98)

1. The logic against an independent Python model (oracle/land/model.py, from the blueprint alone, all integers):
   fuzzed key scripts over 200 rounds and five dives that reach the speed clamp, the terrains and every step's
   state, identical.
2. Every pad is landable: a reference autopilot, flown in the model through the keys, lands on all 3,000 pads of
   1,000 terrains within 700 fuel (median 279, the most 418); the control, 100 fuel, fails, and the check says so;
   300 of its flights replayed through the C land on the same step, every state identical.
3. The landing rule at its edges, 20 hand-made cases whose ends were written by hand: the speed limits exactly and
   one over (falling and rising), one turn step either side of upright, a foot at a pad's end and one pixel past, a
   peak narrower than the ship poking between its points, a foot 1/65,536 px above a slope (the ground's rounding).
   The C and the model give each.
4. The app on the SDK runner: two scripted games (the autopilot's landings, a key held into the next round, a burn
   to an empty tank; refused keys, a loaded save, ENTER partway through a title tick, falls until a crash at exactly
   200 fuel beeps and ends the game) with frame hashes as committed, and the save, tunes and tones the blueprint's
   rules give; the same scripts through the .wasm and the bridge (wamrc from the pinned series) give the same.
5. Planted, each red: gravity left out, the judge ignoring the angle, the vertex test left out, a pad that is not
   flat, no wrap at the edges, the speed clamp left out, the ground rounded down, the upward speed limit left out,
   pads drawn thin.

A reviewer that had not written it read the code against the blueprint before this unit; its findings (the low-fuel
beep on a crash's cost, the first tick after ENTER, a key held across rounds, the top line over the ship, a cliff
at the wrap's seam) are fixed at e20cd98, and checks 1, 3 and 4 gained what it showed they did not reach.

Here, with the game listed (this commit on c6092c0): make test green (app_test 668/668), clang-check 61 files and
none failing. clang-check reads the firmware's own files, not the games, so LAND's two files were also compiled with
its clang-18 image and flags (and the frame guard): clean.

## The board (device/standin/land.txt, for the gate)

APPS lists KEYS, BABAL, AIM and LAND; LAND's title, ENTER for a game, FRAMEBENCH, then the rounds flown blind, since
the terrain comes from the hardware RNG: the engine held 3 s, then every round falls and crashes until the tank is
empty and GAME OVER shows. On the host, with the same keys, all of 200 seeds had their tenth crash (the empty tank)
within 55 s of the release, and GAME OVER follows 1.5 s after it; the script waits 75 s, then ENTER starts a new
game. Then the calc task's stack-free (at least 2,048, read before `app:`, as 060's gate found), C, and 2 ENTER 3 +
must give 5. The script sends 22 key lines (11 presses), none in a burst. Not yet run: it waits on the gate's board.

## Open

- The board run above; the S3 bench's frame hashes against the host's, as AIM had (device/aimbench), if the gate
  wants them: a board window of its own.
