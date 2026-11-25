#!/usr/bin/env python3
"""obj_section: whether a section that moved in the linked map moved in its object too (abacus #17284).

On Xtensa the linker relaxes calls and literal loads, so a function's linked size depends on where everything else
lands: three repins (1.7.0, 1.8.0, 1.9.0) moved calc.c's functions in the map with calc.c unchanged, and each time the
object's own section was byte for byte the same. D68 (scripts/resources.py static) reports this for every moved
loaded section outside the sources the commits change: "object identical" (layout, not code) or "object changed".
A report, not a pass or fail.

    scripts/obj_section.py OBJ SECTION...           each section's size and sha256 (with its relocations: .rela<name>)
    scripts/obj_section.py moved BUILD_A BUILD_B [--target BASENAME ...]
                                                    every loaded (section, object) whose size moved between the two
                                                    builds' maps, with its verdict; a target's lines only counted
"""
import glob
import hashlib
import os
import re
import struct
import sys

SKIP = ('.debug', '.comment', '.xtensa.info', '.xt.')


def sections(path):
    """a relocatable ELF32 object's sections: {name: bytes} (NOBITS sections as b'', their size as a decimal tag)"""
    d = open(path, 'rb').read()
    shoff, = struct.unpack_from('<I', d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2E)
    sh = [struct.unpack_from('<IIIIIIIIII', d, shoff + i * shentsize) for i in range(shnum)]
    strtab = d[sh[shstrndx][4]:sh[shstrndx][4] + sh[shstrndx][5]]
    out = {}
    for s in sh:
        name = strtab[s[0]:strtab.index(b'\0', s[0])].decode()
        out[name] = d[s[4]:s[4] + s[5]] if s[1] != 8 else b'NOBITS %d' % s[5]
    return out


def loaded(map_path):
    """the map's loaded sections: {(section, object): bytes}, address non-zero, debug and comment sections left out"""
    lines = open(map_path, errors='replace').read().split('\n')
    i = next(n for n, l in enumerate(lines) if l.startswith('Linker script and memory map'))
    d, name = {}, None
    for l in lines[i:]:
        m = re.match(r'^ (\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)$', l)
        if m:
            name, addr, sz, obj = m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4)
        else:
            m1 = re.match(r'^ (\.\S+)$', l)
            if m1:
                name = m1.group(1)
                continue
            m2 = re.match(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.obj\)?)$', l)
            if not (m2 and name):
                continue
            addr, sz, obj = int(m2.group(1), 16), int(m2.group(2), 16), m2.group(3)
        if addr == 0 or name.startswith(SKIP):
            continue
        d[(name, obj)] = d.get((name, obj), 0) + sz
    return d


def member(obj):
    """a map's object (esp-idf/main/libmain.a(calc.c.obj), or a path) as (its directory in the build, its file name)"""
    m = re.match(r'^(.*)/[^/]+\.a\(([^)]+)\)$', obj)
    if m:
        return m.group(1), m.group(2)
    return os.path.dirname(obj), os.path.basename(obj)


def find(build, obj):
    """the object's file under the build directory, or None when it isn't there (a toolchain library) or isn't one"""
    where, base = member(obj)
    if where.startswith('/'):
        return None
    hits = glob.glob(os.path.join(build, where, '**', base), recursive=True)
    return hits[0] if len(hits) == 1 else None


def verdict(build_a, build_b, section, obj):
    fa, fb = find(build_a, obj), find(build_b, obj)
    if not fa or not fb:
        return 'object not in the build tree'
    sa, sb = sections(fa), sections(fb)
    keys = (section, '.rela' + section)
    return 'object identical' if all(sa.get(k) == sb.get(k) for k in keys) and section in sa else 'object changed'


def map_of(build):
    maps = glob.glob(os.path.join(build, 'abacus_*.map'))
    return maps[0] if len(maps) == 1 else None


def moved(build_a, build_b, targets=()):
    """[(section, object, size A, size B, verdict)] for each moved loaded section outside the targets (object file
    names without .obj, e.g. 'calc.c'), and the count of the targets' moved lines"""
    ma, mb = map_of(build_a), map_of(build_b)
    if not ma or not mb:
        return None, 0
    a, b = loaded(ma), loaded(mb)
    rows, skipped = [], 0
    for k in sorted(set(a) | set(b)):
        if a.get(k, 0) == b.get(k, 0):
            continue
        base = member(k[1])[1]
        if base.endswith('.obj') and base[:-4] in targets:
            skipped += 1
            continue
        rows.append((k[0], k[1], a.get(k, 0), b.get(k, 0), verdict(build_a, build_b, k[0], k[1])))
    return rows, skipped


def main(argv):
    if len(argv) >= 3 and argv[0] == 'moved':
        t = argv[argv.index('--target') + 1:] if '--target' in argv else []
        rows, skipped = moved(argv[1], argv[2], tuple(t))
        if rows is None:
            sys.exit('obj_section: a build has no single abacus_*.map')
        for s, o, x, y, v in rows:
            print(f'{s} {o}: {x} -> {y} ({y - x:+}): {v}')
        print(f'({skipped} moved lines of the targets not listed)')
        return
    if len(argv) < 2:
        sys.exit(__doc__)
    secs = sections(argv[0])
    for want in argv[1:]:
        body = secs.get(want)
        if body is None:
            print(f'{want}: none')
        else:
            print(f'{want}: {len(body)} B, sha256 {hashlib.sha256(body).hexdigest()[:16]}')


if __name__ == '__main__':
    main(sys.argv[1:])
