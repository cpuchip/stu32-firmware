# Work unit 069: SYS VER names every pin

**From:** Michael's card, routed by abacus (#8126): his phone showed "VER ? / CORE ? / BUILT ?", and he couldn't tell
that the app predated the CAS. The shape was proposed in soroban #8129 and accepted in abacus #8130. **Gate:** soroban.
**Status:** built and checked on the host and on the stand-in (below).

## What it is

SYS's VER view shows every pin the build carries, not only the core:

    VER 0.69
    CORE 385ca62
    BUILT Oct 14 2026
    CASIM 4b91739 GAMES 60e775c

- **CASIM:** the first 7 hex of CASIM_PIN, the commit of casim's Casimir that the build compiled.
- **GAMES:** one digest of the games list, with no page per game. A second page would break unit 027's rule that any
  key leaves a view, and five games wouldn't fit four lines anyway. The digest is the first 7 hex of sha256 over
  "ID REPO PIN\n" for each listed game, in the list's order. It covers every listed game, a wasm one too, even in a
  host build that leaves wasm games out of APPS. Anyone can recompute it:

      grep -v '^#' games.list | awk 'NF{print $1, $2, $3}' | sha256sum | cut -c1-7

  This holds for a list whose comments start their lines, as games.list's do. The exact form, which also takes a
  comment after an entry or an indented one, is `sed 's/#.*//'` in place of the grep. The Makefile's check uses that
  form. An empty list (stu32-calc's
  `GAMES_LIST=`) shows `GAMES none`, not the empty digest e3b0c44.
- **Unknown fields:** a field the build doesn't know shows `?`, as VER, CORE and BUILT already do.
- **Fit:** the line is 27 characters at scale 2, 324 of the 378 pixels a view line has. The renderer cuts a line at
  about 30 characters with an ellipsis, so three characters are spare.

The console's X adds two lines:
- `pins: casim=<40 hex> games=<digest> babal=<40 hex> ...`: every pin in full, so a bring-up log carries them.
- `view: <line> | <line> | <line> | <line>`: an open SYS view's lines.

CORE already decides both pins, since CASIM_PIN and games.list are files in the repo at CORE. The line is for a human
who can't look a commit up. It also catches a build from another list (GAMES_LIST) or a +dirty tree, which CORE can't
(abacus #8130: keep it).

## One source for the device and the app

- **The games:** scripts/gen_games.py writes `GAMES_PINS_ID` (the digest, or "none") and `GAMES_PINS` (each
  `id=<pin>`) into games.h.
- **Casimir:** scripts/fetch-casim.sh writes `build/casim/casim_pin.h` (`AB_CASIM_PIN`, all 40 hex) beside casim.h, and
  rewrites the export when the header is missing. firmware/version.h includes it when AB_CASIM is set and the header
  is found. A build without Casimir, or with a casim taken some other way, shows `CASIM ?`. The device builds refuse
  to configure without it (the three CMakeLists check for it next to `.pin`).
- **The fill:** app_sysinfo gains `casim` and `games`. `app_sysinfo_build()` (firmware/apps.c) fills version, commit,
  built, casim and games, and leaves the rest of the struct alone. The device's dev_sysinfo_common calls it.
- **For tally:** a host app's sysinfo callback calls it too. That is the whole app half.

## The checks

- **app_test:**
  - VER from app_sysinfo_build, line for line, against `VER_CASIM` and `VER_GAMES`. The Makefile computes those apart
    from the generator: CASIM_PIN cut to 7, and the shell's sha256sum over the build's list ("none" for no game).
  - With a sysinfo that knows neither, the fourth line reads `CASIM ? GAMES ?`.
  - Run with the default list (GAMES 60e775c, the digest abacus recomputed) and with `GAMES_LIST=` (GAMES none): 686/686
    both times.
- **gen_games_test:**
  - On the two-game list, `GAMES_PINS_ID` equals sha256sum's digest of the list, and `GAMES_PINS` carries both pins.
  - An empty list gives "none" and "".
- **screen_test:**
  - Frame 21-sys-ver is the four lines; I viewed it.
  - A check reads the fourth line whole, glyph by glyph, at scale 2 in X's row.
- **Falsified:**
  - The generator planted to take hex 1-8 instead of 0-7: app_test reads 685/686 ("GAMES 0e775c4", want 60e775c), and
    gen_games_test fails.
  - The generator planted to give e3b0c44 for an empty list: gen_games_test fails.
  - casim_pin.h planted with a wrong pin: app_test reads 685/686 ("CASIM deadbee"). Deleting the header, make
    regenerates it.
  - The fourth line planted 8 characters longer: the screen check fails, and the frame shows the line cut with an
    ellipsis.
  - Each was restored and is green again.
- **The device:** the stand-in runs 9c8cbc4 (boot `ver=0.69 core=9c8cbc4`). device/standin/ver069.txt opens SYS VER
  from the keys (BLUE, C, then VER's soft key) and X's dump reads:

      pins: casim=4b9173969f66e8f1168ddb465be38ab0980bb740 games=60e775c babal=599c510... aim=5fe8cea... land=e20cd98... snake=a01609b... mines=7344045...
      view: VER 0.69 | CORE 9c8cbc4 | BUILT Oct 14 2026 | CASIM 4b91739 GAMES 60e775c

  - Before the keys and after the key that leaves, X shows no `view:`.
  - The control, which expects `VER 0.68`, fails.
  - The first run failed two expects on the script: talk.py moves past a matched line, and the script split the view
    line and wanted `pins:` after it. The board printed the right lines in that run too.
  - The console has no frame dump, so the scale-2 render on the panel is checked on the host (screen_test and the frame).
    The device shows that the renderer gets these four lines.

## Decision 68 (385ca62 -> 9c8cbc4, scripts/resources.py)

| | stand-in | Rev A head-less |
|---|---|---|
| flash | +560 | +576 |
| DIRAM, IRAM code, internal-RAM symbols | +0 | +0 |
| calc_task's frame | +0 (144) | +0 (128) |
| heap free after boot / largest block | +0 (62,819 / 31,744) | +0 (185,487 / 90,112) |
| stack, worst unmasked reading | +0 (8,188) | +0 (8,196); the figure +0 (7,812) |
| SOLVE, µs | 29,862 -> 30,530 (+668) | 17,134 -> 16,755 (-379) |
| integral, µs | 49,142 -> 51,653 (+2,511) | 30,023 -> 28,602 (-1,421) |
| PROGBENCH SIN / LN, ms | -1 / +2 | -4 / -7 |

- About 200 B of the flash is the changed code (libmain +202 / +207).
- libesp_app_format moved +364. That library's size changes on every build: +216 / +212 at 385ca62, +128 at 71962cb.
- Speed moves both ways between the targets, as it did at 385ca62. No calc path changed, so I read it as layout.

## For the consumers

- **tally's About and app** call app_sysinfo_build and show the same four lines (abacus #8130).
- **A games.h from before this unit** has no GAMES_PINS_ID, so the build shows `GAMES ?`. A host build that compiles
  firmware/app.c and apps.c gets the fill for nothing once its games.h is regenerated.
- **primer, tutor and stu32-calc** build with their own lists. Their VER line shows their list's digest, or none.
