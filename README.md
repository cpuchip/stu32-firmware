# abacus-firmware

Our own RPN calculator core and firmware, written clean-room from HP's published manuals (35s primary; 33s and 32SII as cross-reference) and our spec. Nothing in this repository derives from Free42, Plus42, DB48X, WP34S, newRPL, OpenRPNCalc or any other calculator implementation; the author seat (soroban, on lepton) has never had those sources on its disk. Intended MIT; PRIVATE until Michael opens it. The hardware and build files live elsewhere and stay closed.

HP, 35s, 33s, 32SII and the other product names here are trademarks of their respective owners, used only to identify compatible behaviour. No HP code, ROM or firmware is in this repository or was used to write it.

Provenance rule: every function cites the manual page and example it was written from, and that example is its test.

Work units arrive from the abacus seat (fermion) in `work/`; the core lives in `core/`; tests in `tests/`.

## Building

- `make test` builds and runs the host tests; `make all-checks PYTHON=<a python with mpmath>` adds the accuracy
  fuzzers. The decimal library is fetched from netlib and checked against a pinned sha256 by
  `scripts/fetch-intel-dfp.sh` (make runs it).
- The APPS registry includes the games in `games.list` (Babal from cpuchip/stu32-arcade, PRIVATE), each exported
  at its pin by `scripts/gen_games.py` (make and the device scripts run it) from a local clone beside this
  repository, or in `GAMES_REPOS`; the format and each game's manifest are in that script's header. Without git
  or a clone, stage `build/games/<id>/` by hand (its files at the pin, its manifest, and `.pin`).
- Devices: `device/standin/run.sh <port>` and `device/reva/build.sh`, in Espressif's pinned image
  (`espressif/idf:v5.4.2`).
