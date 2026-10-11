#!/bin/sh
# check-version: AB_FW_VERSION is "0.<the last work unit>" until a 1.0 release (abacus #6810), so VER names the unit
# a build carries; each unit's own commit bumps it, and this check refuses a tree where they differ.
#   sh scripts/check-version.sh [VERSION_H [WORK_DIR]]
h=${1:-firmware/version.h}
w=${2:-work}
unit=$(ls "$w" | sed -n 's/^\([0-9][0-9][0-9]\)[a-z]*-.*\.md$/\1/p' | sort -n | tail -1)
[ -n "$unit" ] || { echo "check-version: no work unit in $w" >&2; exit 1; }
want="0.$(expr "$unit" + 0)"
have=$(sed -n 's/^#define AB_FW_VERSION "\([^"]*\)".*/\1/p' "$h")
[ "$have" = "$want" ] || { echo "check-version: AB_FW_VERSION is \"$have\", the last unit is $unit: want \"$want\"" >&2; exit 1; }
echo "check-version: AB_FW_VERSION \"$have\", unit $unit"
