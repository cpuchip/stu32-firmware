#!/bin/bash
# unit 106's 32-bit leg, the run alone: build/frames made first, as make test's recipe does (Makefile: mkdir -p build/frames)
set -euo pipefail
trap 'chown -R "$HOST_ID" /w' EXIT
apt-get update -qq >/dev/null && apt-get install -y -qq libc6-i386 >/dev/null 2>&1
cd /w && mkdir -p build/frames
set +e
./build/screen_test tests/frames > /tmp/st.out 2>&1; rc=$?
grep -E '106|checks,|differs|cannot' /tmp/st.out | head -20; echo "screen_test rc=$rc"
