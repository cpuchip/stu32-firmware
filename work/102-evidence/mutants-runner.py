#!/usr/bin/env python3
"""Unit 102's planted faults (mutants-predicted.txt): each applied alone, the binaries deleted and rebuilt (a stale
binary is never a pass), every run read, the files restored and compared, and the restored tree built again at the end
(unit 100's lesson: the last mutant's binaries were left in build/).

    python3 work/101-evidence/mutants-runner.py SCRATCH M0 M1 ...
"""
import os, re, shutil, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig102')
SCREEN = 'firmware/screen.c'
FILES = (SCREEN,)
M = {
 'M0': [(SCREEN, "    char kind = '-';\n", "    char kind = '-';\n")],
 'M1': [(SCREEN, "kind = left[0] ? 'A' : '-';", "kind = left[0] ? 'O' : '-';")],
 'M2': [(SCREEN, "    if (k->pg[i].part == 2) left[0] = '\\0', kind = '-';", "    if (k->pg[i].part == 2) left[0] = '\\0';")],
 'M3': [(SCREEN, "        return 'D';", "        return '-';")],
 'M4': [(SCREEN, "ab_op_text(c, (int)r.op, r.arg, left, lsz) <= 0) left[0] = '\\0';", "ab_op_text(c, (int)r.op, r.arg, left, lsz) <= 0) left[0] = '\\0', kind = 'O';")],
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/app_test']
VEC = ['tests/102-soroban.txt', 'tests/090-soroban.txt', 'tests/093-soroban.txt', 'tests/100-soroban.txt']


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, env=env)


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
        r = run('./build/app_test')
        out = r.stdout + r.stderr
        m = re.findall(r'(\d+)/(\d+) checks', out)
        short = sum(int(b) - int(a) for a, b in m)
        fails = [l for l in out.split('\n') if re.search(r'^app_test: (102|100)', l) and 'census:' not in l]
        cen = [l for l in out.split('\n') if 'kind census' in l]
        if short or r.returncode:
            red.append(f'app_test rc {r.returncode}, {short} short: ' + ' || '.join(x[:110] for x in fails[:4] + cen))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
restore()
for f in FILES:
    assert open(f, encoding='utf-8').read() == open(os.path.join(ORIG, os.path.basename(f)), encoding='utf-8').read(), f
for x in BINS:
    if os.path.exists(x): os.remove(x)
run('make ' + ' '.join(BINS) + ' 2>&1')
