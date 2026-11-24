#!/usr/bin/env python3
"""Unit 100's planted faults (mutants-predicted.txt): each applied alone, the binaries deleted and rebuilt (a stale
binary is never a pass), every run read (a non-zero exit with nothing printed is red), the files restored and compared.

    python3 work/100-evidence/mutants-runner.py SCRATCH M0 M1 ...
"""
import os, re, shutil, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig100')
LIST, SCREEN, ROWS = 'core/list.c', 'firmware/screen.c', 'tests/rows_test.c'
FILES = (LIST, SCREEN, ROWS)
NAMES = ('    [AB_EQDROP] = "\\xe2\\x86\\x92X", [AB_CASSIMP] = "SIMP", [AB_CASEXP] = "EXPND", [AB_CASSOLVE] = "SOLVE", '
         '[AB_SST] = "SST",\n')
def drop(entry):
    return [(LIST, NAMES, NAMES.replace(entry + ', ', '').replace(', ' + entry, ''))]
M = {
 'M0': [(LIST, NAMES, NAMES)],
 'M1': drop('[AB_EQDROP] = "\\xe2\\x86\\x92X"'),
 'M2': drop('[AB_CASSIMP] = "SIMP"'),
 'M3': drop('[AB_CASEXP] = "EXPND"'),
 'M4': drop('[AB_CASSOLVE] = "SOLVE"'),
 'M5': drop('[AB_SST] = "SST"'),
 'M6': [(SCREEN, "    else if (r.op != AB_ROW_OP_NUM && ab_op_text(c, (int)r.op, r.arg, left, lsz) <= 0) left[0] = '\\0';\n",
                 "    else if (r.op != AB_ROW_OP_NUM) ab_op_text(c, (int)r.op, r.arg, left, lsz);\n")],
 'M7': drop('[AB_CASSOLVE] = "SOLVE"') +
       [(ROWS, "    static const int routes[] = {AB_CASSOLVE, AB_EQDROP, AB_COPY};",
               "    static const int routes[] = {AB_EQDROP, AB_COPY};")],
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/app_test', 'build/rows_test', 'build/standin_replay']


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
        r = run('./build/vectors tests/100-soroban.txt')
        ids = sorted(set(re.findall(r'^FAIL (\w+):', r.stdout + r.stderr, re.M)))
        if ids: red.append('100-soroban:' + ','.join(ids))
        elif r.returncode: red.append(f'100-soroban: exit {r.returncode} with no failure printed')
        for t in ('app_test', 'rows_test'):
            r = run('./build/' + t)
            out = r.stdout + r.stderr
            m = re.findall(r'(\d+)/(\d+) checks', out)
            short = sum(int(b) - int(a) for a, b in m)
            fails = [l for l in out.split('\n') if re.search(r'FAIL|100: a row', l)]
            if short or fails or r.returncode:
                red.append(f'{t} rc {r.returncode}, {short} short' + (': ' + ' || '.join(x[:100] for x in fails[:5]) if fails else ''))
        r = run('./build/standin_replay device/standin/arith084.txt device/standin/cas088.txt')
        fails = [re.sub(r'^standin_replay: device/standin/', '', l) for l in r.stdout.split('\n') if 'FAIL: framecrc' in l]
        if fails or r.returncode:
            red.append(f'replay rc {r.returncode}: ' + ' || '.join(fails))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
restore()
for f in FILES:
    assert open(f, encoding='utf-8').read() == open(os.path.join(ORIG, os.path.basename(f)), encoding='utf-8').read(), f
