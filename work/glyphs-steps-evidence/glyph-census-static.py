import re, sys, glob, collections
font = set(int(m, 16) for m in re.findall(r'^U\+([0-9A-Fa-f]{4,5})\b', open('firmware/font5x7.txt', encoding='utf-8').read(), re.M))
def c_strings(path):
    src = open(path, encoding='utf-8', errors='replace').read()
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
    src = re.sub(r'//[^\n]*', '', src)
    out = []
    for m in re.finditer(r'"((?:[^"\\\n]|\\.)*)"', src):
        s = m.group(1); b = bytearray(); i = 0
        while i < len(s):
            if s[i] == '\\':
                n = s[i+1]
                if n == 'x':
                    j = i+2
                    while j < len(s) and s[j] in '0123456789abcdefABCDEF': j += 1
                    b.append(int(s[i+2:j], 16) & 0xFF); i = j; continue
                if n in '01234567':
                    j = i+1
                    while j < len(s) and j < i+4 and s[j] in '01234567': j += 1
                    b.append(int(s[i+1:j], 8) & 0xFF); i = j; continue
                b += {'n': b'\n', 't': b'\t', '\\': b'\\', '"': b'"', "'": b"'", 'r': b'\r', '0': b'\0'}.get(n, n.encode()); i += 2; continue
            b += s[i].encode('utf-8'); i += 1
        out.append(b.decode('utf-8', errors='replace'))
    return out
src = collections.defaultdict(set)
for f in sorted(glob.glob('core/*.c') + glob.glob('firmware/*.c') + ['build/casim/casim.c']):
    if f.endswith('font5x7.c'): continue
    for s in c_strings(f):
        for ch in s:
            src[ord(ch)].add(f)
for line in open('build/casim/golden/corpus.txt', encoding='utf-8', errors='replace'):
    for ch in line.rstrip('\n'):
        src[ord(ch)].add('golden')
miss = sorted(cp for cp in src if cp >= 0x20 and cp not in font and cp != 0xFFFD)
print(f'font: {len(font)} code points; emitted (printable, from string literals and the golden corpus): {len([c for c in src if c >= 0x20])}')
for cp in miss:
    fs = sorted(src[cp]); print(f'U+{cp:04X} {chr(cp)!r}: {len(fs)} sources: {", ".join(fs[:6])}')
