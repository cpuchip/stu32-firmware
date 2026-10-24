# Work unit 078: 3720, MOLES and CATCH, three games for a young player who doesn't read yet

**From:** arcade (stu32-arcade ff2725c..7686766) · **Asked and ruled by:** Michael (abacus #10254: 3720; #10256: the
mole game and CATCH; #10265: 3720's fire keys; #10280: MOLES, appended to GAMES, 3720's soft row, the full-mode stun;
basecamp #10367: CATCH's stars within reach, and difficulty levels for CATCH and MOLES) · **Read:** abacus
(blueprints #10278, #10280; the levels #10387; MOLES middle's pacing #10416; replays #10373, #10449) · **Gate:**
soroban · **Status:** ready for the gate, the figures below.

## What changes

1. **games.list:** three rows at the end, pinned at stu32-arcade 7686766: rocks (the menu's 3720; an id starts with a
   letter), moles, catch. GAMES pages five games and more (072), so its second page is now SALVO, 3720, MOLES, CATCH.
2. **tests/app_test.c:** 072's GAMES block collected at most 8 games, so a ninth would drop off the list it checks;
   it now collects up to 16, and its second-page row names SALVO and CATCH, last.
3. **device/standin:** rocks.txt, moles.txt and catch.txt reach each game through GAMES and tell a live key from a
   dead one by the console's tones (077's way), with + as the dead key. Each plays gentle, the level the title opens
   on.
4. **version.h:** 0.78.

The games themselves (stu32-arcade docs/3720.md, docs/moles.md, docs/catch.md): 3720 has gentle and full; MOLES and
CATCH have gentle, middle and full, picked on the title from three pictures, and every CATCH star falls within reach
of the one before it.

## Decision 68

`scripts/resources.py static b83431f <this unit>` (2026-10-24; the measured commit was this unit's tree as one
scratch commit, fb3bf6b):

| | flash (the app's .bin) | internal RAM static (DIRAM) | IRAM code | calc_task's frame |
|---|---|---|---|---|
| stand-in | 3,593,552 -> 3,615,312 (+21,760) | 211,847 (+0) | 59,022 (+0) | 144 B (+0) |
| Rev A | 3,533,904 -> 3,555,296 (+21,392) | 219,798 (+0) | 78,086 (+0) | 128 B (+0) |

The flash is libmain.a's (+21,115 on the stand-in, +21,071 on Rev A: the three games' code, sprites and tables); the
other libraries move by 164 B or less (by inference the version and commit strings and alignment, as at earlier
units; not traced symbol by symbol). No internal-RAM
symbol moved (nm, 0x3FC88000-0x3FD00000): each game's state lives in the app ctx the framework already holds (3720 744
B, MOLES 192 B, CATCH 136 B on the host, under APP_CTX_MAX), so internal RAM is +0.

## The checks

- **stu32-arcade at 7686766:** rockscheck, molescheck and catchcheck GREEN with the .wasm legs, the whole suite 14 of
  14; replayed by abacus from a bundle (#10449), and the history scrub that followed checked by abacus as one doc
  line and the check scripts' re-pointed commit (#10456). S3 bench (the three apps on the chip, frame for frame
  against the host): DEVICE PASS, 11 of 11 scripts, 3 controls red.
- **make test:** rc 0 at the unit's head: app_test 779/779 (the second page's row included), standin_test 349/349,
  the games-list generator GREEN. The row's falsifier, CATCH dropped from games.list: app_test 778/779 (run on the
  unit's first draft, the same row).
- **The stand-in runs at the gate:** soroban's, below.

## The gate (soroban, 2026-10-24)

- **The patches:** sha256 b9995efc… and 7506dfda…, as sent (arcade #10464). git am onto b83431f gives tree 0b7e1fd30c9d,
  as arcade's. The games built from a fresh stu32-arcade clone at 7686766 (on origin/main).
- **make test rc 0** (peak RSS 170,284 KB): app_test 779/779, standin_test 349/349, help_test 305 of 305 entries, every
  vector file full; gen_games 9 games; version 0.78. make clang-check 66 files, 0 failing.
- **Mutants on the second page's row,** app_test each time:
  - CATCH dropped from games.list: 778/779.
  - GAMES's block back to eight slots: 778/779.
  - **3720 and MOLES swapped: 779/779, a gap.** The row pinned SALVO first and CATCH last, not the two between,
    though its message names all four. 70d6a11 checks each name in order: swapped, 778/779; MOLES moved last,
    778/779; the real list 779/779.
- **Decision 68, `scripts/resources.py static b83431f 70d6a11`:** arcade's figures to the byte. Flash +21,760 on the
  stand-in and +21,392 on Rev A (libmain +21,115 and +21,071). DIRAM, IRAM and calc_task's frame +0; no internal-RAM
  symbol changed.
- **The stand-in at 70d6a11** (ver=0.78, pins rocks, moles and catch at 7686766):
  - **moles.txt's first run failed, on my instrument.** Lines 164 and 183, the two + sweeps' expect-nones, failed at
    once, before the board had echoed the + keys, with no 220 Hz tone on the board. talk.py put its own verdicts in
    the lines expects search, and expect-none's verdict repeats its TEXT, so the next expect-none of the same TEXT
    matched it. 94c56cb keeps verdicts out (the port's notes and a failed send stay in, since scripts wait for
    them). No earlier result changes. help075.txt and ver069.txt repeat a same-text expect-none too, but with plain
    expects between, which move the search past the verdict (abacus #10482). A verdict could also have made a later
    plain expect pass if its text sat inside one; the tree's one such shape (reva/usb-headless.txt line 73's `state:`
    after line 59's expect-none) has three plain expects between, so it could not.
  - With the fix: rocks.txt, moles.txt and catch.txt ALL MET. Five bonks (220 then 1,320 Hz) in five sweeps and none
    in the + sweeps; four wall bumps in CATCH; 3720 at 479.1 frames a second; stack-free 29,044 B in each.
  - **Each dead key's check fails when a live key replaces +** (throwaway scripts in build/): 3720 with GOLD for +,
    FAIL at line 82 (the shot's click); MOLES with a real sweep for the first + sweep, FAIL at 164 on a sixth bonk;
    CATCH with SOFT1 for +, FAIL at 58 (a wall bump).
  - salvo, babal, land, aim, snake and mines ALL MET: the first page and SALVO's place on the second are unchanged.
  - The port moved at the flash (ACM3 to ACM1); the reruns had no flash.
