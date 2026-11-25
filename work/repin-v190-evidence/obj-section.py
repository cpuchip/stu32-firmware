#!/usr/bin/env python3
"""A relocatable ELF32 object's named section: its size and sha256 (the repin's check that calc.c's code didn't change).
Usage: obj-section.py OBJ SECTION..."""
import hashlib, struct, sys
d = open(sys.argv[1], 'rb').read()
shoff, = struct.unpack_from('<I', d, 0x20); shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2E)
sh = [struct.unpack_from('<IIIIIIIIII', d, shoff + i * shentsize) for i in range(shnum)]
strtab = d[sh[shstrndx][4]:sh[shstrndx][4] + sh[shstrndx][5]]
name = lambda o: strtab[o:strtab.index(b'\0', o)].decode()
for want in sys.argv[2:]:
    for s in sh:
        if name(s[0]) == want:
            body = d[s[4]:s[4] + s[5]] if s[1] != 8 else b''
            print(f"{want}: {s[5]} B, sha256 {hashlib.sha256(body).hexdigest()[:16]}")
