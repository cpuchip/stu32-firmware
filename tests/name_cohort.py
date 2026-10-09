#!/usr/bin/env python3
"""name_cohort (unit 058, abacus #5784): two namedrive builds, before and after a change to name matching, on the same
inputs, compared line for line. The inputs: every name and every prefix of one; every name followed by each byte 1-255
(but newline); each byte before a name, alone, after X and between 2 and a call; operands (2, X, (X), 2.5, ), π) right
before each name, bare, called and in Y=; and random strings over the names' bytes with digits and operators.

    python3 tests/name_cohort.py OLD_DRIVE NEW_DRIVE SEED N       exit 1 on any difference
"""
import random
import re
import subprocess
import sys

OLD, NEW, SEED, N = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
t = open("core/eqn.c", "rb").read().decode("utf-8", "surrogateescape")
body = t[t.index("static const fname NAMES[] = {"):]
body = body[:body.index("};")]
names = [bytes("".join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))), "latin-1").decode("unicode_escape").encode("latin-1")
         for m in re.finditer(r'\{((?:"(?:[^"\\]|\\.)*"\s*)+),\s*K', body)]
lines = set()
for n in names:
    for i in range(1, len(n) + 1):
        lines.add(n[:i])
    for b in range(1, 256):
        if b != 10:
            lines.update({n + bytes([b]), bytes([b]) + n, b"X" + bytes([b]) + n, b"2" + bytes([b]) + n + b"(X)"})
    for o in (b"2", b"X", b"(X)", b"2.5", b")", b"\xcf\x80"):
        lines.update({o + n, o + n + b"(X)", o + n + b"(2:3)", o + n + b"(2,3)", b"Y=" + o + n + b"(X)"})
alpha = sorted(set(b"".join(names)) | set(b"0123456789XYAB+-^()=.,"))
R = random.Random(SEED)
for _ in range(N):
    lines.add(bytes(R.choice(alpha) for _ in range(R.randint(1, 24))))
data = b"".join(l + b"\n" for l in sorted(lines))
old = subprocess.run([OLD], input=data, capture_output=True, check=True).stdout.split(b"\n")
new = subprocess.run([NEW], input=data, capture_output=True, check=True).stdout.split(b"\n")
bad = sum(1 for a, b in zip(old, new) if a != b) + abs(len(old) - len(new))
print(f"name_cohort: {len(lines)} inputs ({len(names)} names), {bad} differing")
sys.exit(1 if bad else 0)
