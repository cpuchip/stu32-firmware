#!/bin/sh
# check-version: AB_FW_VERSION is "0.<the last work unit>" until a 1.0 release (abacus #6810), so VER names the unit
# a build carries; each unit's own commit bumps it, and this check refuses a tree where they differ. A unit built
# out of order (after a higher-numbered one) adds one from then on: work/version-extra.txt (abacus #14190).
# The last unit is the highest NNN of a work/NNN-*.md record or a tests/NNN-*.txt file: unit 079's record was first
# work/cplx-wording.md, so the records alone said 078 and a tree at 0.78 passed (abacus #10800); its tests said 079.
#   sh scripts/check-version.sh [VERSION_H [WORK_DIR [TESTS_DIR]]]
h=${1:-firmware/version.h}
w=${2:-work}
t=${3:-tests}
unit=$( { ls "$w" | sed -n 's/^\([0-9][0-9][0-9]\)[a-z]*-.*\.md$/\1/p'
          ls "$t" | sed -n 's/^\([0-9][0-9][0-9]\)[a-z]*-.*\.txt$/\1/p'; } | sort -n | tail -1)
[ -n "$unit" ] || { echo "check-version: no work unit in $w or $t" >&2; exit 1; }
# work/version-extra.txt (abacus #14190): each unit line there is a unit built out of order, which moved VER on by one
extra=0
[ -f "$w/version-extra.txt" ] && extra=$(grep -c '^[0-9][0-9][0-9] ' "$w/version-extra.txt")
want="0.$(expr "$unit" + "$extra")"
have=$(sed -n 's/^#define AB_FW_VERSION "\([^"]*\)".*/\1/p' "$h")
[ "$have" = "$want" ] || { echo "check-version: AB_FW_VERSION is \"$have\", the last unit is $unit: want \"$want\"" >&2; exit 1; }
echo "check-version: AB_FW_VERSION \"$have\", unit $unit"
