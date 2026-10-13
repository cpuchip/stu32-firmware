#!/usr/bin/env bash
# Builds Rev A's firmware in Espressif's pinned image (there is no board to flash yet).
set -euo pipefail
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WASM="${STU_WASM:-0}"   # STU_WASM=1: with WAMR (decisions 67, 68), in build-wasm
BENCH="${AB_BENCH:-0}"  # AB_BENCH=1: with the benches (dev_bench.h), for bring-up timing (unit 061), in build-bench
OFFCLK="${AB_OFFCLK:-0}"   # AB_OFFCLK=1: 068 (3) P0's bench image (reva_main.c), in build-offclk; never packaged as the product
TRIM="${OFFCLK_TRIM:-0}"   # with OFFCLK_TRIM=1, offclk-trim.defaults on top, in build-offclk-trim
DEFAULTS=sdkconfig.defaults
B=build
[ "$WASM" = 1 ] && B=build-wasm
[ "$BENCH" = 1 ] && B=build-bench
[ "$OFFCLK" = 1 ] && B=build-offclk
[ "$OFFCLK" = 1 ] && [ "$TRIM" = 1 ] && B=build-offclk-trim DEFAULTS="sdkconfig.defaults;offclk-trim.defaults"
# a changed sdkconfig.defaults takes effect: ESP-IDF never re-reads it into an existing sdkconfig (unit 051)
if [ "$ROOT/device/reva/sdkconfig.defaults" -nt "$ROOT/device/reva/$B/sdkconfig" ] ||
    { [ "$TRIM" = 1 ] && [ "$ROOT/device/reva/offclk-trim.defaults" -nt "$ROOT/device/reva/$B/sdkconfig" ]; } ||
    { [ "$WASM" = 1 ] && [ "$ROOT/device/sdkconfig.wasm" -nt "$ROOT/device/reva/$B/sdkconfig" ]; }; then
    rm -f "$ROOT/device/reva/$B/sdkconfig"
fi
python3 "$ROOT/scripts/gen_games.py" "$ROOT/games.list" "$ROOT/build/games"
"$ROOT/scripts/fetch-casim.sh"
[ "$WASM" = 1 ] && "$ROOT/scripts/fetch-wamr.sh"
docker run --rm -v "$ROOT:/project" -w /project --user "$(id -u):$(id -g)" -e HOME=/tmp -e AB_CORE_COMMIT_HOST="$(sh "$ROOT/scripts/host-commit.sh" "$ROOT")" -e STU_WASM="$WASM" -e B="$B" -e BENCH="$BENCH" -e OFFCLK="$OFFCLK" -e DEFAULTS="$DEFAULTS" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
TARGET=esp32s3 scripts/fetch-intel-dfp.sh
cd device/reva
X=; [ "$BENCH" = 1 ] && X=-DAB_BENCH=1
[ "$OFFCLK" = 1 ] && X="$X -DAB_OFFCLK=1"
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" "-DSDKCONFIG_DEFAULTS=$DEFAULTS" $X build
'
