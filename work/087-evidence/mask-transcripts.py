#!/usr/bin/env python3
"""Unit 087: the README's transcript mask (items 1-9; item 10, the order of talk.py's PASS line, is shown as a
moved pair and named in the README), applied to base/ and new/, and the diff printed. Run from the repo root."""
import difflib, re
def mask(lines):
    out, after_run, after_load = [], False, False
    for l in lines:
        if re.match(r'^(I|W|E) \(\d+\) ', l):
            continue                                                        # 1. ESP-IDF's boot log
        l = re.sub(r'ver=0\.8[67] core=[0-9a-f]{7} id=[0-9a-f]+', 'ver=V core=C id=I', l)  # 2.
        l = re.sub(r'heap internal \d+ \(largest block \d+\)', 'heap internal H (largest block B)', l)  # 3.
        l = re.sub(r'heap=\d+', 'heap=H', l)
        l = re.sub(r'calc stack 32768, then \d+', 'calc stack 32768, then H', l)
        l = re.sub(r'^Saved PC:0x[0-9a-f]+', 'Saved PC:P', l)               # 4.
        l = re.sub(r'ok, \d+ ms', 'ok, N ms', l)                            # 5.
        if l.startswith(('key ', 'wipe:', 'load: no image', 'send ')):
            after_load = False                                              # 6 ends at the first key or wipe
        if re.match(r'^load: \d+ bytes ok', l):                             # 6.
            l, after_load = 'load: N bytes ok', True
        elif after_load and l.startswith('state:'):
            l = re.sub(r'^state: X=\S+ stop=\S+ lines=\d+', 'state: X=L stop=L lines=L', l)
        elif after_load and l.startswith('eqn:'):
            l, after_load = re.sub(r'drawn=.*$', 'drawn=L', l), False
        if 'run: started' in l:
            after_run = True
        if after_run and l.startswith('state:'):
            l = re.sub(r'X=\d+', 'X=COUNT', l)                             # 7.
        l = re.sub(r'^.*ESP-ROM:', 'ESP-ROM:', l)                           # 8.
        l = re.sub(r'ulp-edges=\d+', 'ulp-edges=U', l)                      # 9.
        out.append(l)
    return out
for t in ('standin', 'reva'):
    for f in ('smoke', 'accept'):
        a, b = (mask(open(f'work/087-evidence/transcripts/{c}/{t}-{f}.log', errors='replace').read().split('\n'))
                for c in ('base', 'new'))
        d = [x for x in difflib.unified_diff(a, b, lineterm='', n=0) if x[:1] in '+-' and x[:3] not in ('+++', '---')]
        print(f'{t} {f}: {len(a)}/{len(b)} lines, {len(d)} differing')
        for x in d:
            print('   ', x[:150])
