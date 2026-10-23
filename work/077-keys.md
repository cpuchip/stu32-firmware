# Work unit 077: one key layout for BABAL, LAND and SALVO

**From:** arcade (stu32-arcade e3988b1, a45ce94, bca20f0) · **Asked and ruled by:** Michael (basecamp #9900: 8 is hard to
hit quickly, more so on the phone, so add GOLD and × and maybe 7 and 9; #9906: the layout for all three games; #9907: why)
· **Read:** abacus #9979 · **Gate:** soroban · **Status:** built and checked on the host and on the S3's bench; the board
runs are the gate's.

## What changes

Michael's words (#9906): "4 / 6 and soft 1 / 6 move. Gold 7 8 9 * and soft 2 / 5 fire / jump", and "the + one is useless
and we can drop it". His reason (#9907): "This allows you to shoot/jump and move at the same time from either side. Since
we dont have a dedicated dpad or a/b like buttons we need the split i think." So in all three games:

| job | keys |
|---|---|
| move (BABAL steers, LAND turns, SALVO moves) | 4 or SOFT1 left, 6 or SOFT6 right |
| BABAL jumps, LAND thrusts, SALVO fires | GOLD, 7, 8, 9, ×, SOFT2, SOFT5 |

ENTER keeps the role it had: BABAL jumps on it and SALVO fires on it; LAND starts a game on it and nothing else. It is the
one key outside Michael's list, put to him as a question. + does nothing in play now (it jumped in BABAL and fired in
SALVO). Each title lists the keys.

1. **Tests, app_test:** GOLD (key 30) reaches a running app, its down and its up, and the shift stays 0. That was already
   so in app.c (app_key hands every key but C to the app first). The two new checks are red with app.c changed to keep
   GOLD from the app (777 of 779).
2. **The games at stu32-arcade bca20f0:**
   - **BABAL:** 7, GOLD, SOFT2 and SOFT5 jump in play. On the title SOFT2 and SOFT5 still pick a skill.
   - **LAND:** a bit for each of its eleven keys. Every key's down reaches the ship, so a press counts. A turn or the
     engine is let go only with the last of its keys. ENTER after GAME OVER is judged by the key's own time, as in
     SNAKE and SALVO; it was the last tick's time, up to a tick late.
   - **SALVO:** the fire keys in its rules are now 8, ENTER, 9, ×, 7, GOLD, SOFT2 and SOFT5.

   The board scripts press the new keys: land.txt holds the engine on GOLD, and salvo.txt holds 6 with SOFT5 (moving on
   the number side while a thumb fires).

The phone: tally read stu32-app (#9904). Each drawn key sends its down and its up, GOLD as key 30, with no shift of the
app's own. A touch the system cancels sends no up, so a held thrust would stay on; tally fixes that with this unit's APK.

## Decision 68

**Static, `scripts/resources.py static 917deea 708a1e2`:**

| | stand-in | Rev A |
|---|---|---|
| flash | +304 | +320 |
| static internal RAM (DIRAM) | +0 | +0 |
| IRAM code | +0 | +0 |
| calc_task's frame | +0 | +0 |

libmain is +243 on both and libesp_app_format +68 on both. No internal-RAM symbol moved. The measured head (708a1e2)
had the unit's code but not this file or the version's 0.76 to 0.77. The file builds nothing, and the new version
string is the same length.

**At run time:** LAND's ctx gains a 2-byte key mask and SALVO's three held-key flags. On the host (x86-64) neither ctx
changes size, because padding takes them (LAND 320 B, SALVO 496 B, before and after). The S3's sizes were not measured;
by the structs' alignment each grows by at most 8 bytes, in PSRAM. Nothing is allocated. Every game file builds under
`-Wframe-larger-than=1024`.

## The checks (stu32-arcade at bca20f0, on the host)

- **BABAL, `make appcheck`:** every script identical to babalcore tick for tick and to the reference renderer; the
  scripts press every jump key and +. 34 plants red, among them 7, GOLD, SOFT2 and SOFT5 each not jumping, and +
  jumping.
- **LAND, `make landcheck`:**
  - The play script spreads the autopilot's turns and burns over all eleven keys. Its frames equal the 8-only run's
    until the script's added turns, apart from the title.
  - It holds a move key and an engine key together in both pairings (26 of the 28 pairs), overlaps two engine keys
    and both turn pairs, and taps one engine key while another holds.
  - GOLD, SOFT2 and SOFT5 each hold the engine through another key's tap. + is held with no engine key down and fires
    nothing.
  - The refuse script refuses ENTER at 0.499 s into GAME OVER and takes it at 0.5 s, between two ticks.
  - 26 plants red, among them each of the eleven keys left out, an 8-bit key mask, a release letting go a key still
    held, + firing, and ENTER judged by the tick. The .wasm gives the same frames.
- **SALVO, `make salvocheck`:** 300 fuzzed games with the new keys, 245,114 state lines identical to the model; three
  long games, 61,457 steps; the 34 edges. The play script holds each new fire key with a move key in both pairings, and
  holds + for nothing. New plants (each new key left out, + firing, the logic's fire keys stopping at ×) red. The .wasm
  gives the same frames.
- `make test`, `oracle`, `sdkcheck`, `driftcheck`, `aimcheck`, `snakecheck`, `minescheck` and `wasmcheck` green.
- A fresh reviewer read the change and found no defect in the games. It found four holes in the checks, the 8-bit mask
  among them; each now has a check and a plant.

**On the S3** (soroban's stand-in, 2026-10-23, stu32-arcade at bca20f0): DEVICE PASS for all three, each control red.

| bench | script | frames | device = host |
|---|---|---|---|
| appbench | 9 scripts | 26,240 | each equal |
| landbench | play | 3,086 | f08c03f18a00990e |
| landbench | refuse | 4,702 | becccee43573f135 |
| salvobench | play | 2,128 | 5c8f31b5b420d69d |
| salvobench | refuse | 201 | a30090a03b552c79 |

Here, on 917deea: `make test` green, app_test 779 of 779; VER reads 0.77, and its GAMES becomes e3869b9.

## Not in this unit

- ENTER in BABAL and SALVO: Michael's to keep or drop.
- The stand-in cannot show a game's picture, so the board scripts show only that the games run and end cleanly with
  the new keys held. The pictures are the benches' and the host's.

## Soroban's gate (2026-10-23, on 917deea)

- **The patches:** stu32-arcade 0271fee's patches/firmware/077, sha256 bf9f9679… and 8472c0a2… as sent. `git am` onto
  917deea gives tree 6581db23ddf4. Every pin in games.list is on stu32-arcade's main.
- **Read:** BABAL, LAND and SALVO from their old pins to bca20f0. No defect found. No include, allocation or file is
  new. LAND's 16-bit mask holds its eleven keys, a key's down on the title still falls to the ENTER check, and SALVO's
  fire loop now runs to SALVO_KEYS.
- **Host:** make test rc 0 (peak 172,132 KB), app_test 779/779, VER_GAMES e3869b9. Two app.c mutants, GOLD kept from
  the app and GOLD setting the shift, each fail exactly the two new checks (777/779). make clang-check 66/0.
- **Decision 68, `scripts/resources.py static 917deea 0e82e68`** (the head with this file and 0.77): flash +304 on the
  stand-in and +320 on Rev A; DIRAM, IRAM and calc_task's frame +0; no internal-RAM symbol changed. That's arcade's
  table to the byte.
- **The stand-in at 0e82e68:** ver=0.77, games=e3869b9, the three pins bca20f0. land.txt, salvo.txt and babal.txt
  are ALL MET. LAND: 657.8 frames a second, stack-free 28,868 B.
- **The board can tell the new keys apart, beside a control.** The scripts' expects pass with a dead key too, so each
  was rerun with the new key swapped for + (dead in play now), and both runs were compared:
  - **LAND, GOLD held 3 s:** the ten crashes come 10.8, 15.4, … 53.1 s after the frame-rate line. With + held they
    come 2.8, 7.2, … 44.1 s after it. A round takes 4.2 to 5.2 s in both runs, so the first crash's 8 s is the engine.
  - **SALVO, 6 with SOFT5:** 12 shot tones (1,200 Hz), 3 hit tones (880 Hz) and 3 ships lost. With 6 and + there is
    one 1,200 Hz tone, no hit tones and no lost ship.
  - The timing comes from a line-timestamping wrapper around run.sh, so it isn't a script expect. The control scripts
    were throwaway copies (build/land-ctl077.txt, build/salvo-ctl077.txt).
- The port moved at the flash (ACM1 to ACM3); the runs on ACM3 were made without a flash.
