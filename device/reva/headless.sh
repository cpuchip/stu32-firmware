#!/usr/bin/env bash
# Rev A's main, head-less, on the stand-in's S3 (unit 053): built with REVA_HEADLESS (no Rev A pin driven, the benches
# in) and an 8 MB flash header, flashed, then a script of the stand-in's run against it (device/standin/talk.py).
#
#   device/reva/headless.sh /dev/ttyACM1 device/standin/accept.txt
#   FLASH=0 device/reva/headless.sh /dev/ttyACM1 script.txt      no build or flash
#   MASK=1 device/reva/headless.sh /dev/ttyACM1 script.txt
#       the bench image for the stack's masked readings (AB_MASK, abacus #6636): MASK ON / MASK OFF on the console mask
#       interrupts under keys and runs; the interrupt watchdog off (headless-mask.defaults); in build-headless-mask.
#       Never a product image: package.sh refuses its banner.
#   OFFCLK=1 device/reva/headless.sh /dev/ttyACM1 script.txt
#       068 (3) P0's bench image (AB_OFFCLK): OFF sleeps (deep sleep, no Rev A pin held), a timer wake every OFFCLK s;
#       OFFCLK reports each wake's timing, OFFCLK PRINT logs the wall clock at each. In build-headless-offclk.
#   USB=1 device/reva/headless.sh /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_<MAC>-if00 script.txt
#       Rev A's own console, the USB-Serial-JTAG port (unit 061), on the stand-in's native USB port: flashed and talked
#       to there, in build-headless-usb. The port can come back under another name after a reset, so /dev is shared
#       with the container and talk.py opens the by-id path again.
#
# The transcript is kept in build/reva-headless.log.
set -euo pipefail
PORT="${1:?serial port}"
SCRIPT="${2:-}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
USB="${USB:-0}"
B=device/reva/build-headless
DEFAULTS="sdkconfig.defaults;headless.defaults"
DEV=(--device "$PORT")
MASKDEF=
if [ "${MASK:-0}" = 1 ]; then
    B=device/reva/build-headless-mask
    DEFAULTS="$DEFAULTS;headless-mask.defaults"
    MASKDEF=-DAB_MASK=1
fi
if [ "${OFFCLK:-0}" = 1 ]; then                # 068 (3) P0: the off clock's timing (AB_OFFCLK); deep sleep, head-less
    B=device/reva/build-headless-offclk
    MASKDEF="$MASKDEF -DAB_OFFCLK=1"
    [ -n "${OFFCLK_DEFAULTS:-}" ] && DEFAULTS="$DEFAULTS;$OFFCLK_DEFAULTS"   # (a trim under test, relative to device/reva)
fi
if [ "$USB" = 1 ]; then
    B=device/reva/build-headless-usb
    DEFAULTS="$DEFAULTS;headless-usb.defaults"
    DEV=(-v /dev:/dev --device-cgroup-rule='c 166:* rmw')    # (166: ttyACM)
fi
mkdir -p "$ROOT/build"
for d in "$ROOT/device/reva/sdkconfig.defaults" "$ROOT/device/reva/headless.defaults" "$ROOT/device/reva/headless-usb.defaults" \
         "$ROOT/device/reva/headless-mask.defaults"; do   # (unit 051's guard)
    if [ "${FLASH:-1}" = 1 ] && [ "$d" -nt "$ROOT/$B/sdkconfig" ]; then rm -f "$ROOT/$B/sdkconfig"; fi
done
[ "${FLASH:-1}" = 1 ] && python3 "$ROOT/scripts/gen_games.py" "$ROOT/games.list" "$ROOT/build/games"
[ "${FLASH:-1}" = 1 ] && "$ROOT/scripts/fetch-casim.sh"
docker run --rm -i -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" --group-add "$(stat -L -c %g "$PORT")" "${DEV[@]}" \
    -e HOME=/tmp -e AB_CORE_COMMIT_HOST="$(sh "$ROOT/scripts/host-commit.sh" "$ROOT")" -e PORT="$PORT" -e FLASH="${FLASH:-1}" -e SCRIPT="$SCRIPT" -e B="$B" -e DEFAULTS="$DEFAULTS" -e MASKDEF="$MASKDEF" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
if [ "$FLASH" = 1 ]; then
    TARGET=esp32s3 scripts/fetch-intel-dfp.sh
    cd device/reva
    b="${B#device/reva/}"
    idf.py -B "$b" -DSDKCONFIG="$b/sdkconfig" "-DSDKCONFIG_DEFAULTS=$DEFAULTS" -DREVA_HEADLESS=1 $MASKDEF build
    idf.py -B "$b" -DSDKCONFIG="$b/sdkconfig" -p "$(readlink -f "$PORT")" flash
    cd /project
fi
python device/standin/talk.py "$PORT" /project/build/reva-headless.log ${SCRIPT:+"/project/$SCRIPT"}
'
