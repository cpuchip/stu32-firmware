#!/usr/bin/env bash
# The profile (unit 050): the stand-in built with its core instrumented (build-prof, apart from run.sh's build),
# flashed, then a script run; the report's addresses named from that build's ELF.
#   device/standin/prof.sh /dev/ttyACM1 device/standin/eqnbench.txt
# Afterwards flash the ordinary build again (device/standin/run.sh).
set -euo pipefail
PORT="${1:?serial port}"
SCRIPT="${2:?script}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# a changed sdkconfig.defaults takes effect: ESP-IDF never re-reads it into an existing sdkconfig, so that one is made
# again (unit 051: QIO set in the defaults stayed DIO in an old build)
if [ "$ROOT/device/standin/sdkconfig.defaults" -nt "$ROOT/device/standin/build-prof/sdkconfig" ]; then
    rm -f "$ROOT/device/standin/build-prof/sdkconfig"
fi
python3 "$ROOT/scripts/gen_games.py" "$ROOT/games.list" "$ROOT/build/games"
"$ROOT/scripts/fetch-casim.sh"
docker run --rm -i -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$PORT")" --device "$PORT" \
    -e HOME=/tmp -e PORT="$PORT" -e SCRIPT="$SCRIPT" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
TARGET=esp32s3 scripts/fetch-intel-dfp.sh
cd device/standin
idf.py -B build-prof -DSDKCONFIG=build-prof/sdkconfig -DAB_PROF=1 build
idf.py -B build-prof -DSDKCONFIG=build-prof/sdkconfig -p "$PORT" flash
cd /project
python device/standin/talk.py "$PORT" /project/build/prof.log "/project/$SCRIPT"
xtensa-esp32s3-elf-nm device/standin/build-prof/abacus_standin.elf > /project/build/prof.nm
'
python3 - "$ROOT/build/prof.log" "$ROOT/build/prof.nm" <<'PY'
import re, sys
names = {}
for l in open(sys.argv[2]):
    p = l.split()
    if len(p) == 3 and p[1] in "tTwW": names[int(p[0], 16)] = p[2]
for l in open(sys.argv[1], errors="replace"):
    m = re.match(r"prof: 0x([0-9a-f]+) (.*)", l)
    if m:
        a = int(m.group(1), 16)
        print(f"prof: {names.get(a, hex(a)):32} {m.group(2)}")
    elif l.startswith("prof:") or l.startswith("eqnbench:"):
        print(l.rstrip())
PY
