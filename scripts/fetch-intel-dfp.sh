#!/usr/bin/env bash
# Fetches the Intel Decimal Floating-Point Math Library from Intel's distribution at
# netlib (and nowhere else), verifies the pinned size and sha256, and builds libbid.a.
#
#   scripts/fetch-intel-dfp.sh                  -> build/intel/libbid.a, headers in third_party/
#   TARGET=esp32s3 scripts/fetch-intel-dfp.sh   -> build/intel-esp32s3/libbid.a (also esp32c6),
#                                                  cross-built in its own tree with the ESP-IDF
#                                                  toolchain on PATH (tests/device/run.sh)
#
# Ruled 2026-10-03 (work unit 001 amendment). BSD-3-Clause; the notice is kept in
# third_party/LICENSE.intel-dfp.txt. The source is not committed: this script and its hash
# are the provenance.
set -euo pipefail

URL="https://netlib.org/misc/intel/IntelRDFPMathLib20U5.tar.gz"
SIZE=5817987
SHA256="85dafd70f0fe2a8da218ade4233fca9d3228b0b04cd6d8527f7499926be01037"
# By-value arguments, rounding mode and status flags passed on every call (no globals).
OPTS=(CALL_BY_REF=0 GLOBAL_RND=0 GLOBAL_FLAGS=0 UNCHANGED_BINARY_FLAGS=0)

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/intel-dfp"
TARBALL="$DEST/IntelRDFPMathLib20U5.tar.gz"
TARGET="${TARGET:-}"
# A cross build (abacus's recipe, #3719): the library's 32-bit configuration and its Linux
# conventions, scripts/fenv-nearest.h included for the rounding-mode calls (it explains why),
# each target's code model.
case "$TARGET" in
"")      CROSS=(); TREE="$DEST"; OUT="$ROOT/build/intel"; CC="${CC:-cc}" ;;
esp32s3) CC=xtensa-esp32s3-elf-gcc; TREE="$DEST/$TARGET"; OUT="$ROOT/build/intel-$TARGET"
         CROSS=(CC_NAME=gcc "AR_CMD=xtensa-esp32s3-elf-ar rv" CFLAGS_ARCH=-Dia32 CFLAGS_OS=-DLINUX
                "CFLAGS_OPT=-O2 -mlongcalls" "CFLAGS_AUX=-include $ROOT/scripts/fenv-nearest.h") ;;
esp32c6) CC=riscv32-esp-elf-gcc; TREE="$DEST/$TARGET"; OUT="$ROOT/build/intel-$TARGET"
         CROSS=(CC_NAME=gcc "AR_CMD=riscv32-esp-elf-ar rv" CFLAGS_ARCH=-Dia32 CFLAGS_OS=-DLINUX
                "CFLAGS_OPT=-O2 -march=rv32imac_zicsr_zifencei -mabi=ilp32" "CFLAGS_AUX=-include $ROOT/scripts/fenv-nearest.h") ;;
*)       echo "fetch-intel-dfp: unknown TARGET '$TARGET' (esp32s3, esp32c6)" >&2; exit 2 ;;
esac
SRC="$TREE/IntelRDFPMathLib20U5"

mkdir -p "$DEST" "$TREE" "$OUT"
if [ ! -f "$TARBALL" ]; then
    curl -fsS -o "$TARBALL.part" "$URL"
    mv "$TARBALL.part" "$TARBALL"
fi
got_size=$(wc -c < "$TARBALL" | tr -d ' ')
got_sha=$(sha256sum "$TARBALL" | cut -d' ' -f1)
if [ "$got_size" != "$SIZE" ] || [ "$got_sha" != "$SHA256" ]; then
    echo "fetch-intel-dfp: $TARBALL does not match the pin" >&2
    echo "  size   $got_size (want $SIZE)" >&2
    echo "  sha256 $got_sha (want $SHA256)" >&2
    exit 1
fi
if [ ! -d "$SRC" ]; then
    tar -xzf "$TARBALL" -C "$TREE"
fi
# The trig functions' argument-reduction table (295 KB, one copy each in sin, cos and tan) is
# only read, but is declared without const, so a firmware build would place it in RAM. Made
# const here so it goes to flash; any write to it would now fail to compile. Idempotent.
for f in sin cos tan; do
    sed -i 's/^static BID_UINT384 bid_decimal128_moduli\[\] =/static const BID_UINT384 bid_decimal128_moduli[] =/' \
        "$SRC/LIBRARY/src/bid128_$f.c"
    grep -q '^static const BID_UINT384 bid_decimal128_moduli\[\] =' "$SRC/LIBRARY/src/bid128_$f.c" || {
        echo "fetch-intel-dfp: bid128_$f.c: moduli table not found for the const patch" >&2; exit 1; }
done
# The library's makefile does not track flags: a build with other flags starts clean.
STAMP="$SRC/.abacus-flags"
FLAGS="$CC ${OPTS[*]} ${CROSS[*]:-}"
if [ "$(cat "$STAMP" 2>/dev/null)" != "$FLAGS" ]; then
    make -s -C "$SRC/LIBRARY" clean >/dev/null 2>&1 || true
    printf '%s' "$FLAGS" > "$STAMP"
fi
make -s -C "$SRC/LIBRARY" -j"$(nproc 2>/dev/null || echo 4)" CC="$CC" "${OPTS[@]}" "${CROSS[@]}" >/dev/null
cp "$SRC/LIBRARY/libbid.a" "$OUT/libbid.a"
echo "fetch-intel-dfp: verified $SHA256, built ${OUT#"$ROOT"/}/libbid.a"
