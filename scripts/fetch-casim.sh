#!/bin/sh
# Casimir (cpuchip/casimir, public since decision 72; the casim seat's computer algebra, CAS 004): its src/ at CASIM_PIN,
# exported read-only with git archive from a local clone (CASIM_REPO, default ../casim) into build/casim;
# a build never writes into that clone. The pin must be on the clone's origin (a pushed commit, as of
# its last fetch), as the games list requires. Beside them, casim_pin.h: AB_CASIM_PIN, the pin, for SYS's VER and the console's
# X (unit 069; firmware/version.h). Unit 080: its golden corpus too (tests/golden/corpus.txt, as golden/corpus.txt), the
# answers tests/tbook_test.c reads back.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PIN=$(cat "$ROOT/CASIM_PIN")
REPO=${CASIM_REPO:-$ROOT/../casim}
OUT=$ROOT/build/casim
if [ -f "$OUT/.pin" ] && [ "$(cat "$OUT/.pin")" = "$PIN" ] && [ -f "$OUT/casim.c" ] && [ -f "$OUT/casim.h" ] &&
    [ -f "$OUT/casim_pin.h" ] && [ -f "$OUT/golden/corpus.txt" ]; then
    exit 0
fi
case "$PIN" in *[!0-9a-f]*|"") echo "fetch-casim: CASIM_PIN is not a commit" >&2; exit 1;; esac
HEAD=$(git -C "$REPO" symbolic-ref -q refs/remotes/origin/HEAD) || { echo "fetch-casim: $REPO has no origin/HEAD" >&2; exit 1; }
git -C "$REPO" merge-base --is-ancestor "$PIN" "$HEAD" || { echo "fetch-casim: $PIN is not on $HEAD (unpushed?)" >&2; exit 1; }
rm -rf "$OUT"
mkdir -p "$OUT"
git -C "$REPO" archive "$PIN" src/casim.c src/casim.h tests/golden/corpus.txt | tar -x -C "$OUT" --strip-components=1
printf '#define AB_CASIM_PIN "%s"\n' "$PIN" > "$OUT/casim_pin.h"
echo "$PIN" > "$OUT/.pin"
echo "casim $PIN"
