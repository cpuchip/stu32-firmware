#!/bin/bash
# unit 106's 32-bit leg (abacus #18028): screen_test built -m32 (ILP32) in ubuntu:24.04 with gcc-multilib, from a worktree
set -euo pipefail
trap 'chown -R "$HOST_ID" /w' EXIT
apt-get update -qq >/dev/null && apt-get install -y -qq gcc-multilib make python3 >/dev/null 2>&1
cd /w
gcc -m32 -dM -E -x c /dev/null | grep -E '__ILP32__|__SIZEOF_POINTER__|__i386__' | sort
for m in "-m32" "-m32 -malign-double"; do
  gcc $m -std=c11 -DAB_CASIM -Ibuild/casim -Icore -Ifirmware -o /tmp/probe probe.c && echo "[$m] $(/tmp/probe)"
done
M32="${M32:--m32 -malign-double}"
echo "the screen_test leg: gcc $M32"
SRC=third_party/intel-dfp/IntelRDFPMathLib20U5
[ -d "$SRC" ] || tar -xzf third_party/intel-dfp/IntelRDFPMathLib20U5.tar.gz -C third_party/intel-dfp
make -s -C "$SRC/LIBRARY" clean >/dev/null 2>&1 || true
make -s -C "$SRC/LIBRARY" -j8 CC="gcc $M32" _HOST_ARCH=x86 CALL_BY_REF=0 GLOBAL_RND=0 GLOBAL_FLAGS=0 UNCHANGED_BINARY_FLAGS=0 >/dev/null 2>&1
mkdir -p build/intel && cp "$SRC/LIBRARY/libbid.a" build/intel/libbid.a && touch build/intel/libbid.a
echo "libbid: $(file -b "$SRC/LIBRARY/bid64_add.o" 2>/dev/null || objdump -f "$SRC/LIBRARY/bid64_add.o" | grep format)"
# the native build's own three command lines (make -n -B in the main tree), cc made gcc -m32: make itself needs the
# games' generated makefile, which this container has no clone for
sed "s/^gcc -m32 /gcc $M32 /" m32-cmds.sh > /tmp/cmds.sh && bash -e /tmp/cmds.sh >/tmp/b.log 2>&1 || { tail -20 /tmp/b.log; exit 3; }
echo "screen_test: $(file -b build/screen_test 2>/dev/null | cut -c1-40 || true)"
cat > /tmp/sz.c <<'C'
#include <stdio.h>
#include "screen.h"
int main(void) { printf("ILP32 sizes: screen_line %zu, screen_page %zu, the page beyond its lines %zu, a line beyond its strings %zu\n",
  sizeof(screen_line), sizeof(screen_page), sizeof(screen_page) - 4 * sizeof(screen_line), sizeof(screen_line) - 2 * (size_t)SCREEN_TEXT_MAX); return 0; }
C
gcc $M32 -std=c11 -Icore -Ifirmware -o /tmp/sz /tmp/sz.c && /tmp/sz
set +e
./build/screen_test tests/frames > /tmp/st.out 2>&1; rc=$?
grep -E '106|checks,' /tmp/st.out; echo "screen_test rc=$rc"
