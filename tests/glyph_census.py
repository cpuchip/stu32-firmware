#!/usr/bin/env python3
"""glyph_census: unit 103 (work/glyphs-steps.md; abacus #17479, #17497), the face's static census.

Every printable code point in the C string literals of core/, firmware/ and Casimir's casim.c, and in the answer column
of Casimir's golden corpus, must be drawable: a glyph in firmware/font5x7.txt, or a mapping screen.c's next_cell makes
(found in that function's body, not taken on this file's word) to a glyph and a combining mark the face has, or
(unit 104) the three cells screen.c's cells_of draws ⒤ and ⒥ as, found in its body. A code
point that is never drawn is named below with its reason. Exit 1 on any other: the screen would draw it as a hollow
box.

    python3 tests/glyph_census.py [--casim build/casim/casim.c] [--corpus build/casim/golden/corpus.txt]
"""
import argparse
import glob
import re
import sys

# Never drawn, each with where it comes from
NOT_DRAWN = {
    0x007B: "help_text.c's {} placeholder, filled before the entry is drawn",
    0x007D: "help_text.c's {} placeholder, filled before the entry is drawn",
    0x007C: "tbook.c's drawn-text export for ABS's bars (screen_drawn's text); the frame draws bars as lines",
}
# Drawn as a letter and a combining mark (screen.c next_cell, unit 103), as x-hat and x-bar are typed
MAPPED = {0x0177: (ord('y'), 0x0302), 0x0233: (ord('y'), 0x0304)}
# Unit 104 (work/indirect-three-cells.md): the indirect tokens, drawn as three cells by screen.c's cells_of, the
# characters each can be drawn as (the letter's case is the model's)
EXPANDED = {0x24A4: "(iI)", 0x24A5: "(J)"}


def c_strings(path):
    src = open(path, encoding='utf-8', errors='replace').read()
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
    src = re.sub(r'//[^\n]*', '', src)
    src = re.sub(r"'(?:[^'\\\n]|\\.)'", "' '", src)          # character constants, so a '"' doesn't open a string
    for m in re.finditer(r'"((?:[^"\\\n]|\\.)*)"', src):
        s, b, i = m.group(1), bytearray(), 0
        while i < len(s):
            if s[i] != '\\':
                b += s[i].encode('utf-8')
                i += 1
                continue
            n = s[i + 1]
            if n == 'x':
                j = i + 2
                while j < len(s) and s[j] in '0123456789abcdefABCDEF':
                    j += 1
                b.append(int(s[i + 2:j], 16) & 0xFF)
                i = j
            elif n in '01234567':
                j = i + 1
                while j < len(s) and j < i + 4 and s[j] in '01234567':
                    j += 1
                b.append(int(s[i + 1:j], 8) & 0xFF)
                i = j
            else:
                b += {'n': b'\n', 't': b'\t', 'r': b'\r', '0': b'\0'}.get(n, n.encode())
                i += 2
        yield b.decode('utf-8', errors='replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--casim', default='build/casim/casim.c')
    ap.add_argument('--corpus', default='build/casim/golden/corpus.txt')
    a = ap.parse_args()
    face = set(int(m, 16) for m in re.findall(r'^U\+([0-9A-Fa-f]{4,5})\b',
                                               open('firmware/font5x7.txt', encoding='utf-8').read(), re.M))
    seen = {}
    files = sorted(f for f in glob.glob('core/*.c') + glob.glob('firmware/*.c') if not f.endswith('font5x7.c'))
    for f in files + [a.casim]:
        for s in c_strings(f):
            for ch in s:
                seen.setdefault(ord(ch), f)
    answers = 0
    for line in open(a.corpus, encoding='utf-8', errors='replace'):
        if line.startswith('#') or '\t' not in line:
            continue
        answers += 1
        for ch in line.rstrip('\n').split('\t')[-1]:
            seen.setdefault(ord(ch), a.corpus)
    # a mapping counts only where screen.c's next_cell makes it: the code point and its mark in that function's body
    sc = open('firmware/screen.c', encoding='utf-8').read()
    m = re.search(r'static uint32_t next_cell\(const char \*\*s\)\n\{(.*?)\n\}', sc, re.S)
    body = m.group(1) if m else ''
    mapped = {cp for cp, (base, mark) in MAPPED.items()
              if re.search(rf'cp == 0x{cp:04X}\).*?0x{mark:04X}', body, re.I)}
    # an expansion counts only where screen.c's cells_of makes it: the code point and each character in that body
    m = re.search(r'static int cells_of\(const char \*\*s, bool upper, uint32_t out\[3\]\)\n\{(.*?)\n\}', sc, re.S)
    ebody = m.group(1) if m else ''
    expanded = {cp for cp, chars in EXPANDED.items()
                if f'0x{cp:04X}' in ebody and all(f"'{ch}'" in ebody for ch in chars)}
    printable = [cp for cp in seen if cp >= 0x20 and cp != 0xFFFD and not 0x0300 <= cp <= 0x036F]
    bad = []
    for cp in sorted(printable):
        if cp in face or cp in NOT_DRAWN:
            continue
        if cp in mapped and all(x in face for x in MAPPED[cp]):
            continue
        if cp in expanded and all(ord(ch) in face for ch in EXPANDED[cp]):
            continue
        bad.append(cp)
    for cp in bad:
        print(f'glyph_census: FAIL U+{cp:04X} {chr(cp)!r} (from {seen[cp]}): no glyph, no mapping, not named as undrawn')
    print(f'glyph_census: {len(printable)} printable code points from {len(files) + 1} sources and {answers} corpus answers, '
          f'{len(face)} glyphs, {len(mapped)} mapped in next_cell, {len(expanded)} drawn as three cells by cells_of, '
          f'{len(NOT_DRAWN)} never drawn; {len(bad)} without a glyph')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
