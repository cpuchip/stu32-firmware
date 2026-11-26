#!/usr/bin/env python3
"""Unit 106's planted faults (mutants-predicted.txt; 105's runner with 106's faults and files): each applied alone, the binaries deleted and rebuilt (a stale
binary is never a pass), every run read, the files restored and compared, and the restored tree built again at the end.

    python3 work/106-evidence/mutants-runner.py SCRATCH M0 M1 ...
"""
import os, re, shutil, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig106')
SCREEN = 'firmware/screen.c'
FILES = (SCREEN,)
LOOP = "    for (int i = 0; i < l->n; i++) {\n        char b[8];\n        int m = cell_u8(l->cp[i], b);"
M = {
 'M0': [(SCREEN, LOOP, LOOP)],
 'M1': [(SCREEN, "    o->drawn[u] = '\\0';\n", "    o->drawn[u] = '\\0';\n    snprintf(o->drawn, sizeof o->drawn, \"%s\", l->u8);\n")],
 'M2': [(SCREEN, "    for (int i = 0; i < l->n; i++) {\n        char b[8];", "    for (int i = 0; i < l->n && i != l->cursor; i++) {\n        char b[8];")],
 'M3': [(SCREEN, "    for (int i = 0; i < l->n; i++) {\n        char b[8];", "    for (int i = 0; i < l->n && i < 31; i++) {\n        char b[8];")],
 'M4': [(SCREEN, "    out->t.cursor = out->z.cursor = out->y.cursor = out->x.cursor = -1;\n", "")],
 'M5': [(SCREEN, "        int m = cell_u8(l->cp[i], b);", "        int m = cell_u8(l->cp[i] & ~(0x7Fu << MARK_SHIFT), b);")],
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/screen_test', 'build/app_test']
VEC = ['tests/106-soroban.txt', 'tests/105-soroban.txt', 'tests/h105-abacus.txt', 'tests/h104-abacus.txt']


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, errors='replace', env=env)   # (after run 1: M5's stray bytes broke the decode)


def restore():
    for f in FILES:
        shutil.copyfile(os.path.join(ORIG, os.path.basename(f)), f)


os.makedirs(ORIG, exist_ok=True)
for f in FILES:
    shutil.copyfile(f, os.path.join(ORIG, os.path.basename(f)))
for name in sys.argv[2:]:
    restore()
    for f, o, n in M[name]:
        t = open(f, encoding='utf-8').read()
        assert t.count(o) == 1, (name, o[:60])
        open(f, 'w', encoding='utf-8').write(t.replace(o, n))
    if name != 'M0':
        assert any(open(f, encoding='utf-8').read() != open(os.path.join(ORIG, os.path.basename(f)), encoding='utf-8').read()
                   for f in FILES), name + ': no change applied'
    for x in BINS:
        if os.path.exists(x): os.remove(x)
    t0 = time.time()
    b = run('make ' + ' '.join(BINS) + ' 2>&1')
    stale = [x for x in BINS if not os.path.exists(x) or os.path.getmtime(x) < t0 - 1]
    red = []
    if stale:
        red.append('BUILD FAILED (' + ','.join(stale) + '): ' + '|'.join(l for l in b.stdout.split('\n') if 'error' in l)[:300])
    else:
        for v in VEC:
            r = run('./build/vectors ' + v)
            ids = sorted(set(re.findall(r'^FAIL (\w+):', r.stdout + r.stderr, re.M)))
            if ids: red.append(os.path.basename(v) + ':' + ','.join(ids))
            elif r.returncode: red.append(os.path.basename(v) + f': exit {r.returncode} with no failure printed')
        r = run('./build/screen_test tests/frames')
        out = r.stdout + r.stderr
        m = re.findall(r'(\d+)/(\d+) checks', out)
        short = sum(int(b) - int(a) for a, b in m)
        fails = [l.replace('screen_test: ', '') for l in out.split('\n')
                 if 'differs' in l or ('screen_test: ' in l and re.search(r'106|lacks', l))]
        if short or r.returncode or [l for l in out.split('\n') if 'differs' in l]:
            red.append(f'screen_test rc {r.returncode}, {short} short: ' + ' || '.join(x[:70] for x in fails[:8]))
        r = run('./build/app_test')
        m = re.findall(r'(\d+)/(\d+) checks', r.stdout + r.stderr)
        if r.returncode or any(a != b for a, b in m):
            red.append(f'app_test rc {r.returncode}: ' + ' '.join(f'{a}/{b}' for a, b in m))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
restore()
for f in FILES:
    assert open(f, encoding='utf-8').read() == open(os.path.join(ORIG, os.path.basename(f)), encoding='utf-8').read(), f
for x in BINS:
    if os.path.exists(x): os.remove(x)
run('make ' + ' '.join(BINS) + ' 2>&1')
