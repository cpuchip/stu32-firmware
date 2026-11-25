#!/usr/bin/env python3
"""glyph_frames: unit 103's drawn census (work/glyphs-steps.md; abacus #17497 ruling 2).

Every key of every plain vector run make test makes draws its frame (build/vectors with AB_DRAWN_CENSUS: screen_render
with the default ui after each key), and each frame that draws a hollow box (a character the face lacks) is printed.
Run at a unit that touches the face or the screen, and at its acceptance; not a make test step (it doubles the
vectors' time). tests/glyph_census.py, the static census, is the make test step.

    python3 scripts/glyph_frames.py              # build/vectors must be built; exit 1 if any frame drew a box
"""
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


def runs():
    """the plain vector runs make test makes: ./build/vectors FILE, with its environment prefix if any"""
    out = subprocess.run(['make', '-n', 'test'], cwd=ROOT, capture_output=True, text=True).stdout
    seen, cmds = set(), []
    for line in out.split('\n'):
        line = re.sub(r'\s+#.*$', '', line.strip())
        m = re.match(r'^((?:[A-Z_]+=\S+\s+)*)\./build/vectors\s+(\S+)$', line)
        if m and line not in seen:
            seen.add(line)
            env = dict(kv.split('=', 1) for kv in m.group(1).split())
            cmds.append((env, m.group(2)))
    return cmds


def main():
    if not os.path.exists(os.path.join(ROOT, 'build', 'vectors')):
        sys.exit('glyph_frames: build/vectors is not built (make build/vectors)')
    cmds = runs()
    boxes, frames = 0, []
    for env, f in cmds:
        e = dict(os.environ, **env, AB_DRAWN_CENSUS='1')
        r = subprocess.run(['./build/vectors', f], cwd=ROOT, env=e, capture_output=True, text=True)
        for line in r.stdout.split('\n'):
            m = re.match(r'^DRAWN (\S+) (\d+)$', line)
            if m:
                boxes += int(m.group(2))
                frames.append(f'{m.group(1)}: {m.group(2)}')
    for x in frames[:40]:
        print(f'glyph_frames: a box drawn: {x}')
    print(f'glyph_frames: {len(cmds)} vector runs, every key\'s frame drawn; {len(frames)} frames drew {boxes} hollow boxes')
    return 1 if boxes else 0


if __name__ == '__main__':
    sys.exit(main())
