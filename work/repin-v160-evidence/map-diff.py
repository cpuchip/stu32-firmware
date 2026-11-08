#!/usr/bin/env python3
"""The repin's linked-section diff: every (section, object) whose size differs between two of D68's Rev A maps, and
whether casim's steps functions are linked or discarded. Usage: map-diff.py BASE_MAP NEW_MAP"""
import re, sys
FN = ("casim_step", "do_ssteps", "ls_steps")
def read(p):
    L = open(p).read().split("\n")
    i = next(n for n, l in enumerate(L) if l.startswith("Linker script and memory map"))
    disc = {f for l in L[:i] for f in FN if re.match(r"^ \.text\.%s(\s|$)" % f, l)}
    d, name = {}, None
    for l in L[i:]:
        m = re.match(r"^ (\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)$", l)
        if m:
            name, obj, sz = m.group(1), m.group(4), int(m.group(3), 16)
        else:
            m1 = re.match(r"^ (\.\S+)$", l)
            if m1:
                name = m1.group(1)
                continue
            m2 = re.match(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.obj\)?)$", l)
            if not (m2 and name):
                continue
            obj, sz = m2.group(3), int(m2.group(2), 16)
        d[(name, obj)] = d.get((name, obj), 0) + sz
    return d, disc
(a, da), (b, db) = read(sys.argv[1]), read(sys.argv[2])
for p, disc in ((sys.argv[1], da), (sys.argv[2], db)):
    print(f"{p}: discarded {sorted(disc)}; kept {[f for f in FN if any(n == '.text.' + f for n, _ in (a if p == sys.argv[1] else b))]}")
for k in sorted(set(a) | set(b)):
    if a.get(k, 0) != b.get(k, 0):
        load = "" if k[0].startswith(".debug") else "   <- loaded"
        print(f"{k[0]} {k[1]}: {a.get(k, 0)} -> {b.get(k, 0)} ({b.get(k, 0) - a.get(k, 0):+d}){load}")
