#!/usr/bin/env bash
# Rev A's main, head-less, on the stand-in's S3 (unit 053): built with REVA_HEADLESS (no Rev A pin driven, the benches
# in) and an 8 MB flash header, flashed, then a script of the stand-in's run against it (device/standin/talk.py).
#
#   device/reva/headless.sh /dev/ttyACM1 device/standin/accept.txt
#   FLASH=0 device/reva/headless.sh /dev/ttyACM1 script.txt      no build or flash
#
# The transcript is kept in build/reva-headless.log.
set -euo pipefail
PORT="${1:?serial port}"
SCRIPT="${2:-}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
B=device/reva/build-headless
mkdir -p "$ROOT/build"
for d in "$ROOT/device/reva/sdkconfig.defaults" "$ROOT/device/reva/headless.defaults"; do     # (unit 051's guard)
    if [ "${FLASH:-1}" = 1 ] && [ "$d" -nt "$ROOT/$B/sdkconfig" ]; then rm -f "$ROOT/$B/sdkconfig"; fi
done
[ "${FLASH:-1}" = 1 ] && python3 "$ROOT/scripts/gen_games.py" "$ROOT/games.list" "$ROOT/build/games"
[ "${FLASH:-1}" = 1 ] && "$ROOT/scripts/fetch-casim.sh"
docker run --rm -i -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$PORT")" --device "$PORT" \
    -e HOME=/tmp -e PORT="$PORT" -e FLASH="${FLASH:-1}" -e SCRIPT="$SCRIPT" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
if [ "$FLASH" = 1 ]; then
    TARGET=esp32s3 scripts/fetch-intel-dfp.sh
    cd device/reva
    idf.py -B build-headless -DSDKCONFIG=build-headless/sdkconfig "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;headless.defaults" \
        -DREVA_HEADLESS=1 build
    idf.py -B build-headless -DSDKCONFIG=build-headless/sdkconfig -p "$PORT" flash
    cd /project
fi
python device/standin/talk.py "$PORT" /project/build/reva-headless.log ${SCRIPT:+"/project/$SCRIPT"}
'
