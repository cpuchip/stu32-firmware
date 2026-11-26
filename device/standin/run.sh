#!/usr/bin/env bash
# Builds and flashes the stand-in in Espressif's pinned image, then talks to it over its console.
#
#   device/standin/run.sh /dev/ttyACM0                 build, flash, then a session (stdin keys)
#   device/standin/run.sh /dev/ttyACM0 script.txt      build, flash, run the script's lines
#   FLASH=0 device/standin/run.sh /dev/ttyACM0 s.txt   no build or flash: talk to what is there
#   STU_WASM=1 device/standin/run.sh ...                with WAMR (decisions 67, 68), in build-wasm
#   GAMES_LIST=games-wasm-gate.list ...                  another games list (the wasm gate's), default games.list
#
# The session's transcript is kept in build/standin.log. Nothing is installed on the host.
set -euo pipefail
PORT="${1:?serial port}"
SCRIPT="${2:-}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WASM="${STU_WASM:-0}"
B=build
[ "$WASM" = 1 ] && B=build-wasm
LIST="$ROOT/${GAMES_LIST:-games.list}"
mkdir -p "$ROOT/build"
# a changed sdkconfig.defaults takes effect: ESP-IDF never re-reads it into an existing sdkconfig, so that one is made
# again (unit 051: QIO set in the defaults stayed DIO in an old build)
if [ "${FLASH:-1}" = 1 ] && { [ "$ROOT/device/standin/sdkconfig.defaults" -nt "$ROOT/device/standin/$B/sdkconfig" ] ||
    { [ "$WASM" = 1 ] && [ "$ROOT/device/sdkconfig.wasm" -nt "$ROOT/device/standin/$B/sdkconfig" ]; }; }; then
    rm -f "$ROOT/device/standin/$B/sdkconfig"
fi
[ "${FLASH:-1}" = 1 ] && python3 "$ROOT/scripts/gen_games.py" "$LIST" "$ROOT/build/games"
[ "${FLASH:-1}" = 1 ] && "$ROOT/scripts/fetch-casim.sh"
[ "${FLASH:-1}" = 1 ] && [ "$WASM" = 1 ] && "$ROOT/scripts/fetch-wamr.sh"
# the container's environment, the same for the flash and the talk
run_in() {
    local dev; dev=$(readlink -f "$PORT")
    docker run --rm -i -v "$ROOT:/project" -w /project \
        --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$dev")" --device "$dev" \
        -e HOME=/tmp -e AB_CORE_COMMIT_HOST="$(sh "$ROOT/scripts/host-commit.sh" "$ROOT")" -e PORT="$dev" -e SCRIPT="$SCRIPT" \
        -e STU_WASM="$WASM" -e B="$B" "$IMAGE" bash -c "set -euo pipefail; . \"\$IDF_PATH/export.sh\" >/dev/null; $1"
}
if [ "${FLASH:-1}" = 1 ]; then
    run_in 'TARGET=esp32s3 scripts/fetch-intel-dfp.sh
        cd device/standin
        idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" build
        idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -p "$PORT" flash'
    # The port after a flash (abacus #17527): esptool's reset re-enumerates the board's USB serial during its first boot,
    # and a container holds only the device node it started with, so talking from the flash's container lost the console
    # at every flash (units 087, 096, 100, 103). Talk from a new container once the by-id link is back: 3 s (as
    # scripts/resources.py's board path waits), then up to 10 s for the link to name a character device. Not opened here:
    # an open of a cdc-acm port raises DTR and RTS.
    sleep 3
    for _ in $(seq 1 20); do
        [ -c "$(readlink -f "$PORT")" ] && break
        sleep 0.5
    done
    [ -c "$(readlink -f "$PORT")" ] || { echo "run.sh: $PORT did not come back after the flash" >&2; exit 1; }
fi
run_in 'python device/standin/talk.py "$PORT" /project/build/standin.log ${SCRIPT:+"/project/$SCRIPT"}'
