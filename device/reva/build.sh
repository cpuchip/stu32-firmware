#!/usr/bin/env bash
# Builds Rev A's firmware in Espressif's pinned image (there is no board to flash yet).
set -euo pipefail
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
docker run --rm -v "$ROOT:/project" -w /project --user "$(id -u):$(id -g)" -e HOME=/tmp "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
TARGET=esp32s3 scripts/fetch-intel-dfp.sh
cd device/reva
idf.py -B build -DSDKCONFIG=build/sdkconfig build
'
