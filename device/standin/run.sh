#!/usr/bin/env bash
# Builds and flashes the stand-in in Espressif's pinned image, then talks to it over its console.
#
#   device/standin/run.sh /dev/ttyACM0                 build, flash, then a session (stdin keys)
#   device/standin/run.sh /dev/ttyACM0 script.txt      build, flash, run the script's lines
#   FLASH=0 device/standin/run.sh /dev/ttyACM0 s.txt   no build or flash: talk to what is there
#
# The session's transcript is kept in build/standin.log. Nothing is installed on the host.
set -euo pipefail
PORT="${1:?serial port}"
SCRIPT="${2:-}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/build"
[ "${FLASH:-1}" = 1 ] && python3 "$ROOT/scripts/gen_games.py" "$ROOT/games.list" "$ROOT/build/games"
[ "${FLASH:-1}" = 1 ] && "$ROOT/scripts/fetch-casim.sh"
docker run --rm -i -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$PORT")" --device "$PORT" \
    -e HOME=/tmp -e PORT="$PORT" -e FLASH="${FLASH:-1}" -e SCRIPT="$SCRIPT" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
if [ "$FLASH" = 1 ]; then
    TARGET=esp32s3 scripts/fetch-intel-dfp.sh
    cd device/standin
    idf.py -B build -DSDKCONFIG=build/sdkconfig build
    idf.py -B build -DSDKCONFIG=build/sdkconfig -p "$PORT" flash
    cd /project
fi
python device/standin/talk.py "$PORT" /project/build/standin.log ${SCRIPT:+"/project/$SCRIPT"}
'
