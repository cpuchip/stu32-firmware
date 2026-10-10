#!/usr/bin/env bash
# Rev A's image to flash (unit 061), built from a commit (the committed tree, never the working tree's edits):
#
#   device/reva/package.sh [COMMIT]          (default HEAD)
#
# Builds Rev A in a worktree under build/res/pkg-<sha> (the games and casim from the main tree's neighbouring clones), then
# writes build/reva-image-<sha7>/:
#   abacus_reva-<sha7>.bin       one image for a single write at 0x0 (the three below, merged by esptool)
#   bootloader.bin, partition-table.bin, abacus_reva.bin, flash_args   the build's own, for a three-part write
#   talk.py, bringup.txt         the console and the bring-up session (device/standin/talk.py, device/reva/bringup.txt)
#   sdkconfig                    the configuration the image was built with
#   FLASH.md                     how to flash and run the session, on Windows (fermion) and on lepton
#   SHA256SUMS                   every file above
# The image is not reproducible to the bit: ESP-IDF stamps the compile time into the app. SHA256SUMS names this build.
set -euo pipefail
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SHA="$(git -C "$ROOT" rev-parse --verify "${1:-HEAD}^{commit}")"
S7="${SHA:0:7}"
WT="$ROOT/build/res/pkg-$SHA"
OUT="$ROOT/build/reva-image-$S7"
[ -d "$WT" ] || git -C "$ROOT" worktree add --detach "$WT" "$SHA"
[ "$(git -C "$WT" rev-parse HEAD)" = "$SHA" ] || { echo "package.sh: $WT is not at $SHA" >&2; exit 1; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no)" ] || { echo "package.sh: $WT has edits" >&2; exit 1; }
rm -f "$WT/device/reva/build/sdkconfig"            # the committed defaults, whatever an earlier build left
# the game and casim clones are the main tree's neighbours, so their pins are checked as a build here checks them
GAMES_REPOS="$ROOT/.." CASIM_REPO="$ROOT/../casim" "$WT/device/reva/build.sh"
B="$WT/device/reva/build"
grep -q '^CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y' "$B/sdkconfig" || { echo "package.sh: the console is not USB's" >&2; exit 1; }
grep -q '^CONFIG_ESPTOOLPY_FLASHMODE_QIO=y' "$B/sdkconfig" || { echo "package.sh: not QIO" >&2; exit 1; }
rm -rf "$OUT"
mkdir -p "$OUT"
cp "$B/bootloader/bootloader.bin" "$B/partition_table/partition-table.bin" "$B/abacus_reva.bin" "$B/sdkconfig" "$OUT/"
sed -e 's#bootloader/##; s#partition_table/##' "$B/flash_args" > "$OUT/flash_args"
cp "$WT/device/standin/talk.py" "$WT/device/reva/bringup.txt" "$OUT/"
read -r FLASH_OPTS < "$OUT/flash_args"             # --flash_mode dio --flash_freq 80m --flash_size 16MB
docker run --rm -v "$OUT:/out" -w /out --user "$(id -u):$(id -g)" -e HOME=/tmp "$IMAGE" bash -c "
set -euo pipefail
. \"\$IDF_PATH/export.sh\" >/dev/null
esptool.py --chip esp32s3 merge_bin -o abacus_reva-$S7.bin $FLASH_OPTS \$(tail -n +2 flash_args)
"
sed -e "s/@SHA@/$SHA/g; s/@S7@/$S7/g" "$ROOT/device/reva/FLASH.md.in" > "$OUT/FLASH.md"
(cd "$OUT" && sha256sum abacus_reva-"$S7".bin bootloader.bin partition-table.bin abacus_reva.bin flash_args talk.py \
    bringup.txt sdkconfig FLASH.md > SHA256SUMS)
echo "package.sh: $OUT"
cat "$OUT/SHA256SUMS"
