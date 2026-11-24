#!/usr/bin/env python3
"""The 1.7.0 repin (kept for 1.8.0 unchanged): casim.c.obj's loaded size in two of D68's maps (sections with a non-zero address, debug and
comment sections left out), and every loaded (section, object) whose size differs. Usage: casim-size.py BASE NEW"""
import re, sys
SKIP = ('.debug', '.comment', '.xtensa.info', '.xt.')
def read(p):
    L = open(p).read().split("\n")
    i = next(n for n, l in enumerate(L) if l.startswith("Linker script and memory map"))
    d, name = {}, None
    for l in L[i:]:
        m = re.match(r"^ (\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)$", l)
        if m:
            name, addr, sz, obj = m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4)
        else:
            m1 = re.match(r"^ (\.\S+)$", l)
            if m1:
                name = m1.group(1)
                continue
            m2 = re.match(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.obj\)?)$", l)
            if not (m2 and name):
                continue
            addr, sz, obj = int(m2.group(1), 16), int(m2.group(2), 16), m2.group(3)
        if addr == 0 or name.startswith(SKIP):
            continue
        d[(name, obj)] = d.get((name, obj), 0) + sz
    return d
a, b = read(sys.argv[1]), read(sys.argv[2])
ca = sum(v for (n, o), v in a.items() if o.endswith('(casim.c.obj)'))
cb = sum(v for (n, o), v in b.items() if o.endswith('(casim.c.obj)'))
print(f"casim.c.obj loaded: {ca:,} -> {cb:,} ({cb - ca:+,})")
for k in sorted(set(a) | set(b)):
    if a.get(k, 0) != b.get(k, 0):
        print(f"{k[0]} {k[1]}: {a.get(k, 0)} -> {b.get(k, 0)} ({b.get(k, 0) - a.get(k, 0):+})")
