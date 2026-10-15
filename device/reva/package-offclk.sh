#!/usr/bin/env bash
# 068 (3) P0's bench package for Rev A (abacus #7586, #7587, #7601): two AB_OFFCLK images from one commit, as built and
# trimmed (offclk-trim.defaults), each one file for a single write at 0x0, with p0.txt's session. Never the product:
# package.sh builds that and refuses this banner.
#
#   device/reva/package-offclk.sh [COMMIT]          (default HEAD) -> build/reva-offclk-<sha7>/
#   SESSION=p074.txt device/reva/package-offclk.sh   another session (unit 074's wake probe; p0.txt by default)
set -euo pipefail
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SHA="$(git -C "$ROOT" rev-parse --verify "${1:-HEAD}^{commit}")"
S7="${SHA:0:7}"
WT="$ROOT/build/res/pkg-offclk-$SHA"
OUT="$ROOT/build/reva-offclk-$S7"
[ -d "$WT" ] || git -C "$ROOT" worktree add --detach "$WT" "$SHA"
[ "$(git -C "$WT" rev-parse HEAD)" = "$SHA" ] || { echo "package-offclk.sh: $WT is not at $SHA" >&2; exit 1; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no)" ] || { echo "package-offclk.sh: $WT has edits" >&2; exit 1; }
rm -f "$WT/device/reva/build-offclk/sdkconfig" "$WT/device/reva/build-offclk-trim/sdkconfig"
GAMES_REPOS="$ROOT/.." CASIM_REPO="$ROOT/../casim" AB_OFFCLK=1 "$WT/device/reva/build.sh"
GAMES_REPOS="$ROOT/.." CASIM_REPO="$ROOT/../casim" AB_OFFCLK=1 OFFCLK_TRIM=1 "$WT/device/reva/build.sh"
for c in build-offclk build-offclk-trim; do
    d="$WT/device/reva/$c"
    grep -aq 'AB_OFFCLK image' "$d/abacus_reva.bin" || { echo "package-offclk.sh: $c: no AB_OFFCLK banner" >&2; exit 1; }
    grep -q '^CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y' "$d/sdkconfig" || { echo "package-offclk.sh: $c: the console is not USB's" >&2; exit 1; }
done
grep -q '^CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP=y' "$WT/device/reva/build-offclk-trim/sdkconfig" ||
    { echo "package-offclk.sh: the trimmed image does not skip validation" >&2; exit 1; }
! grep -q '^CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP=y' "$WT/device/reva/build-offclk/sdkconfig" ||
    { echo "package-offclk.sh: the as-built image skips validation" >&2; exit 1; }
rm -rf "$OUT"
mkdir -p "$OUT"
for c in build-offclk build-offclk-trim; do
    n=abacus_reva-${c#build-}-$S7.bin
    docker run --rm -v "$WT/device/reva/$c:/b" -v "$OUT:/out" -w /b --user "$(id -u):$(id -g)" -e HOME=/tmp "$IMAGE" bash -c "
set -euo pipefail
. \"\$IDF_PATH/export.sh\" >/dev/null
esptool.py --chip esp32s3 merge_bin -o /out/$n \$(head -1 flash_args) \$(tail -n +2 flash_args)
"
done
SESSION="${SESSION:-p0.txt}"
cp "$WT/device/standin/talk.py" "$WT/device/reva/$SESSION" "$OUT/"
(cd "$OUT" && sha256sum abacus_reva-offclk-"$S7".bin abacus_reva-offclk-trim-"$S7".bin talk.py "$SESSION" > SHA256SUMS)
echo "package-offclk.sh: $OUT"
cat "$OUT/SHA256SUMS"
