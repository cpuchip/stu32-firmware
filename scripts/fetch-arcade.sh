#!/bin/sh
# Babal (cpuchip/stu32-arcade, PRIVATE): its rules (babal/) and its app (app/) at ARCADE_PIN, exported
# read-only with git archive from a local clone (ARCADE_REPO, default ../stu32-arcade) into
# build/arcade, the way casim consumes the core: a build never writes into that clone.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PIN=$(cat "$ROOT/ARCADE_PIN")
REPO=${ARCADE_REPO:-$ROOT/../stu32-arcade}
OUT=$ROOT/build/arcade
if [ -f "$OUT/.pin" ] && [ "$(cat "$OUT/.pin")" = "$PIN" ]; then
    exit 0
fi
rm -rf "$OUT"
mkdir -p "$OUT"
git -C "$REPO" archive "$PIN" babal app/babal_app.c app/babal_app.h app/render.c app/render.h | tar -x -C "$OUT"
echo "$PIN" > "$OUT/.pin"
echo "arcade $PIN"
