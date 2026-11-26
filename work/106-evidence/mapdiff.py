#!/usr/bin/env python3
"""Unit 106's D68 follow-up: every input section placed in the image (not the map's discarded ones, address 0) whose
size differs between two builds' link maps (scripts/resources.py's
build/res/<sha>/device/<board>/build-res/abacus_<board>.map). mapdiff.py RES OLD NEW BOARD..."""
import re, sys, collections
R, old, new, boards = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
def secs(sha, board):
    t = open(f'{R}/{sha}/device/{board}/build-res/abacus_{board}.map', errors='replace').read().split('\n')
    d = collections.Counter(); i = 0
    while i < len(t):
        m = re.match(r'^ (\.\S+)\s*$', t[i])
        if m and i + 1 < len(t):
            m2 = re.match(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)', t[i + 1])
            if m2 and int(m2.group(1), 16): d[(m.group(1), m2.group(3).split('/')[-1])] += int(m2.group(2), 16); i += 2; continue
            if m2: i += 2; continue
        m = re.match(r'^ (\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)', t[i])
        if m and int(m.group(2), 16): d[(m.group(1), m.group(4).split('/')[-1])] += int(m.group(3), 16)
        i += 1
    return d
for b in boards:
    a, c = secs(old, b), secs(new, b)
    ch = sorted(((k, a.get(k, 0), c.get(k, 0)) for k in set(a) | set(c) if a.get(k, 0) != c.get(k, 0)), key=lambda z: z[2] - z[1])
    loaded = [z for z in ch if not z[0][0].startswith(('.debug', '.xt.', '.comment'))]
    print(f'== {b}: {old} -> {new}, {len(ch)} sections differ; loaded ones {sum(z[2]-z[1] for z in loaded):+} B')
    for (s, o), x, y in ch: print(f'  {s:44} {o:28} {x} -> {y} ({y - x:+})')
