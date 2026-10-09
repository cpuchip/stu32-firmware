#!/usr/bin/env bash
# Builds Rev A's firmware in Espressif's pinned image (there is no board to flash yet).
set -euo pipefail
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WASM="${STU_WASM:-0}"   # STU_WASM=1: with WAMR (decisions 67, 68), in build-wasm
B=build
[ "$WASM" = 1 ] && B=build-wasm
# a changed sdkconfig.defaults takes effect: ESP-IDF never re-reads it into an existing sdkconfig (unit 051)
if [ "$ROOT/device/reva/sdkconfig.defaults" -nt "$ROOT/device/reva/$B/sdkconfig" ] ||
    { [ "$WASM" = 1 ] && [ "$ROOT/device/sdkconfig.wasm" -nt "$ROOT/device/reva/$B/sdkconfig" ]; }; then
    rm -f "$ROOT/device/reva/$B/sdkconfig"
fi
python3 "$ROOT/scripts/gen_games.py" "$ROOT/games.list" "$ROOT/build/games"
"$ROOT/scripts/fetch-casim.sh"
[ "$WASM" = 1 ] && "$ROOT/scripts/fetch-wamr.sh"
docker run --rm -v "$ROOT:/project" -w /project --user "$(id -u):$(id -g)" -e HOME=/tmp -e STU_WASM="$WASM" -e B="$B" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
TARGET=esp32s3 scripts/fetch-intel-dfp.sh
cd device/reva
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" build
'
