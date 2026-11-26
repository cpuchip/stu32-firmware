#!/usr/bin/env python3
"""Unit 105's planted faults (mutants-predicted.txt; 104's runner with 105's faults and files): each applied alone, the binaries deleted and rebuilt (a stale
binary is never a pass), every run read, the files restored and compared, and the restored tree built again at the end.

    python3 work/105-evidence/mutants-runner.py SCRATCH M0 M1 ...
"""
import os, re, shutil, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig105')
SCREEN, LIST, FONT = 'firmware/screen.c', 'core/list.c', 'firmware/font5x7.c'
FILES = (SCREEN, LIST, FONT)
PAIR_O = "    {0x27EE, {0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01}},\n"
PAIR_C = "    {0x27EF, {0x10, 0x08, 0x08, 0x08, 0x08, 0x08, 0x10}},\n"
M = {
 'M0': [(SCREEN, "        out[0] = 0x27EE;", "        out[0] = 0x27EE;")],
 'M1': [(SCREEN, "        out[0] = 0x27EE;", "        out[0] = '(';"),
        (SCREEN, "        out[2] = 0x27EF | TOK_MORE;", "        out[2] = ')' | TOK_MORE;")],
 'M2': [(SCREEN, "            if (op == AB_EQLINE && arg >= 0 && arg < AB_PEQN_MAX) {",
                 "            if (0 && op == AB_EQLINE && arg >= 0 && arg < AB_PEQN_MAX) {")],
 'M3': [(FONT, PAIR_O, "    {0x27EE, {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},\n"),
        (FONT, PAIR_C, "    {0x27EF, {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},\n")],
 'M4': [(FONT, PAIR_C, "")],
 'M5': [(SCREEN, "            char ch = i == 0 ? '(' : i == 2 ? ')' : (char)(o[i] & CP_MASK);",
                 "            char ch = (char)(o[i] & CP_MASK);")],
 'M6': [(LIST, "    return best >= 0 && c->lmode[best] ? c->lmode[best] == 35 : c->m35 && !c->stu;   /* 099: STU's is (i) */",
               "    (void)best;\n    return true;")],
 'M7': [(SCREEN, "    return &comp.x;\n}", "    return &comp.y;\n}")],
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/screen_test', 'build/standin_replay']
VEC = ['tests/105-soroban.txt', 'tests/104-soroban.txt', 'tests/h104-abacus.txt', 'tests/103-soroban.txt',
       'tests/h103-abacus.txt']


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
        r = run('./build/screen_test tests/frames')
        out = r.stdout + r.stderr
        m = re.findall(r'(\d+)/(\d+) checks', out)
        short = sum(int(b) - int(a) for a, b in m)
        fails = [l.replace('screen_test: ', '') for l in out.split('\n')
                 if 'differs' in l or ('screen_test: ' in l and re.search(r'10[45]|lacks|alike', l))]
        if short or r.returncode or [l for l in out.split('\n') if 'differs' in l]:
            red.append(f'screen_test rc {r.returncode}, {short} short: ' + ' || '.join(x[:70] for x in fails[:8]))
        r = run('./build/standin_replay device/standin/*.txt')
        if r.returncode:
            red.append('replay: ' + ' | '.join(l.replace('standin_replay: device/standin/', '')[:60]
                                               for l in (r.stdout + r.stderr).split('\n') if ': FAIL' in l))
        r = run('python3 tests/glyph_census.py')
        if r.returncode:
            red.append('glyph_census: ' + ' '.join(l.split()[2] for l in r.stdout.split('\n') if 'FAIL' in l))
        r = run('python3 scripts/gen_font.py --check')
        if r.returncode:
            red.append('gen_font --check')
        r = run('python3 scripts/glyph_frames.py')
        if r.returncode:
            red.append('glyph_frames: ' + r.stdout.strip().split('\n')[-1].replace('glyph_frames: ', ''))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
restore()
for f in FILES:
    assert open(f, encoding='utf-8').read() == open(os.path.join(ORIG, os.path.basename(f)), encoding='utf-8').read(), f
for x in BINS:
    if os.path.exists(x): os.remove(x)
run('make ' + ' '.join(BINS) + ' 2>&1')
