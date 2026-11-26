#!/usr/bin/env python3
"""PROTO 105: a frame's lit band as dots, downsampled by its scale. dots.py FRAME.png SCALE [Y0 Y1]
The band: the rows between Y0 and Y1 (default: the tallest run of lit rows below the status band)."""
import sys, zlib, struct
def load(p):
    b = open(p, 'rb').read()
    i, idat, w, h = 8, b'', 0, 0
    while i < len(b):
        n, = struct.unpack('>I', b[i:i+4]); t = b[i+4:i+8]; d = b[i+8:i+8+n]
        if t == b'IHDR': w, h = struct.unpack('>II', d[:8])
        if t == b'IDAT': idat += d
        i += 12 + n
    raw = zlib.decompress(idat); rb = 1 + (w + 7) // 8
    return [[not (raw[y*rb + 1 + x//8] >> (7 - x % 8) & 1) for x in range(w)] for y in range(h)], w, h
px, w, h = load(sys.argv[1]); s = int(sys.argv[2])
if len(sys.argv) > 4: y0, y1 = int(sys.argv[3]), int(sys.argv[4])
else:
    lit = [any(r) for r in px]; runs, y = [], 24
    while y < h:
        if lit[y]:
            z = y
            while z < h and lit[z]: z += 1
            runs.append((z - y, y, z)); y = z
        else: y += 1
    _, y0, y1 = max(runs)
cols = [x for x in range(w) if any(px[y][x] for y in range(y0, y1))]
x0, x1 = min(cols), max(cols) + 1
for y in range(y0, y1, s):
    print(''.join('#' if px[y][x] else '.' for x in range(x0, x1, s)).rstrip('.') or '.')
