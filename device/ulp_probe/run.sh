#!/usr/bin/env bash
# Builds and flashes the ULP probe in the pinned image, then runs SCRIPT over its console (talk.py).
#   device/ulp_probe/run.sh PORT SCRIPT
set -euo pipefail
PORT="${1:?serial port}"
SCRIPT="${2:?script}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
docker run --rm -i -v "$ROOT:/project" -w /project --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$PORT")" \
    --device "$PORT" -e HOME=/tmp -e PORT="$PORT" -e SCRIPT="$SCRIPT" espressif/idf:v5.4.2 bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
cd device/ulp_probe
idf.py -B build -DSDKCONFIG=build/sdkconfig build
idf.py -B build -DSDKCONFIG=build/sdkconfig -p "$PORT" flash
cd /project
python device/standin/talk.py "$PORT" /project/build/ulp_probe.log "/project/$SCRIPT"
'
