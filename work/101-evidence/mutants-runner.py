#!/usr/bin/env python3
"""Unit 101's planted faults (mutants-predicted.txt): each applied alone, the binaries deleted and rebuilt (a stale
binary is never a pass), every run read, the files restored and compared, and the restored tree built again at the end
(unit 100's lesson: the last mutant's binaries were left in build/).

    python3 work/101-evidence/mutants-runner.py SCRATCH M0 A1 B1 ...
"""
import os, re, shutil, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig101')
EQN, SCREEN = 'core/eqn.c', 'firmware/screen.c'
FILES = (EQN, SCREEN)
M = {
 'M0': [(EQN, "    p.ans = ans;\n", "    p.ans = ans;\n")],
 'A1': [(SCREEN, "        k->xexpr = false;                       /* unit 101:", "        (void)0;                                /* unit 101:")],
 'B1': [(EQN, "    return say_text(c, text, AB_MODEL_STU, true, out, size);", "    return say_text(c, text, AB_MODEL_STU, false, out, size);")],
 'B2': [(EQN, "    if ((p->c->alg_eval || p->ans) && at(p, \"ANS\")) {", "    if (p->ans && at(p, \"ANS\")) {")],
 'B3': [(EQN, "    return say_text(c, text, model, false, out, size);", "    return say_text(c, text, model, true, out, size);")],
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/eqn_say_test']
VEC = ['tests/101-soroban.txt', 'tests/029-soroban.txt', 'tests/089-soroban.txt', 'tests/090-soroban.txt',
       'tests/096-soroban.txt', 'work/029-vectors.txt', 'work/029b-vectors.txt', 'work/031-vectors.txt',
       'work/032b-vectors.txt']


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
        for corpus in ('tests/101-alg-corpus.txt', 'tests/086-corpus.txt'):
            r = run('./build/eqn_say_test ' + corpus)
            fails = [l.replace('eqn_say_test: FAIL ', '') for l in r.stdout.split('\n') if 'FAIL ' in l]
            if fails or r.returncode:
                red.append(os.path.basename(corpus) + f' rc {r.returncode}: ' + ' || '.join(x[:90] for x in fails[:6]))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
restore()
for f in FILES:
    assert open(f, encoding='utf-8').read() == open(os.path.join(ORIG, os.path.basename(f)), encoding='utf-8').read(), f
for x in BINS:
    if os.path.exists(x): os.remove(x)
run('make ' + ' '.join(BINS) + ' 2>&1')
