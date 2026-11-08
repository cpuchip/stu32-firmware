#!/usr/bin/env bash
# The mask applied to make test's output before the base/new diff (unit 087). Usage: mask.sh base.txt > base.masked.txt
#   - compile lines dropped (the new run recompiles after version.h changed);
#   - every number before s / us / ms becomes T (timings);
#   - gen_games_test's GAMES_PINS_ID becomes D (its fixture makes throwaway git repos, so the pins change each run);
#   - check-version's line: "0.86", unit 086 / "0.87", unit 087 become "VER", unit N.
grep -v -E '^(cc|c\+\+|gcc|clang) ' "$1" | sed -E 's/[0-9]+(\.[0-9]+)? (s|us|ms)\b/T \2/g; s/GAMES_PINS_ID [0-9a-f]{7}/GAMES_PINS_ID D/; s/"0\.8[67]", unit 08[67]/"VER", unit N/'
