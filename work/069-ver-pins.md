# Work unit 069: SYS VER names every pin

**From:** Michael's card, routed by abacus (#8126): his phone showed "VER ? / CORE ? / BUILT ?", and he couldn't tell
that the app predated the CAS. The shape was proposed in soroban #8129 and accepted in abacus #8130. **Gate:** soroban.
**Status:** built and checked on the host. The device check is listed below.

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
- **The device:** the stand-in runs the build, opens SYS VER from the keys, and the X dump's `view:` and `pins:` lines
  carry the device's own lines (the log is named in the reply).

## For the consumers

- **tally's About and app** call app_sysinfo_build and show the same four lines (abacus #8130).
- **A games.h from before this unit** has no GAMES_PINS_ID, so the build shows `GAMES ?`. A host build that compiles
  firmware/app.c and apps.c gets the fill for nothing once its games.h is regenerated.
- **primer, tutor and stu32-calc** build with their own lists. Their VER line shows their list's digest, or none.
