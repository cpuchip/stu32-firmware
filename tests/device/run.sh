#!/usr/bin/env bash
# Builds, flashes and runs the on-chip test in Espressif's pinned ESP-IDF image.
#
#   tests/device/run.sh esp32s3 /dev/ttyACM0
#   tests/device/run.sh esp32c6 /dev/ttyACM1
#
# The report is printed and kept in build/device-<target>.log; exit 0 on DEVICE PASS. Nothing is
# installed on the host: the toolchain is the image's, and the port is passed into the container
# with its group. The C6 (4 MB of flash) gets the bundle's --small set; ABACUS_BUNDLE_ARGS overrides.
set -euo pipefail
TARGET="${1:?target: esp32s3 or esp32c6}"
PORT="${2:?serial port}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

mkdir -p "$ROOT/tests/device/main/generated" "$ROOT/build"
[ "$TARGET" = esp32c6 ] && DEFAULT_ARGS=--small || DEFAULT_ARGS=
python3 "$ROOT/tests/device/bundle.py" "$ROOT/tests/device/main/generated/bundle.c" ${ABACUS_BUNDLE_ARGS:-$DEFAULT_ARGS}
docker run --rm -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$PORT")" --device "$PORT" \
    -e HOME=/tmp -e TARGET="$TARGET" -e PORT="$PORT" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
scripts/fetch-intel-dfp.sh
cd tests/device
B="build-$TARGET"
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET="$TARGET" build
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET="$TARGET" size
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET="$TARGET" -p "$PORT" flash
python capture.py "$PORT" "/project/build/device-$TARGET.log" 3600
'
