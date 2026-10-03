#!/usr/bin/env bash
# Fetches the Intel Decimal Floating-Point Math Library from Intel's distribution at
# netlib (and nowhere else), verifies the pinned size and sha256, and builds libbid.a.
#
#   scripts/fetch-intel-dfp.sh            -> build/intel/libbid.a, headers in third_party/
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
SRC="$DEST/IntelRDFPMathLib20U5"
CC="${CC:-cc}"

mkdir -p "$DEST" "$ROOT/build/intel"
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
    tar -xzf "$TARBALL" -C "$DEST"
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
make -s -C "$SRC/LIBRARY" -j"$(nproc 2>/dev/null || echo 4)" CC="$CC" "${OPTS[@]}" >/dev/null
cp "$SRC/LIBRARY/libbid.a" "$ROOT/build/intel/libbid.a"
echo "fetch-intel-dfp: verified $SHA256, built build/intel/libbid.a"
