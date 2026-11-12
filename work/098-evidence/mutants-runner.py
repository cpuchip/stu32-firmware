#!/usr/bin/env python3
"""Unit 098's planted faults (mutants-predicted.txt): each applied alone to firmware/screen.c, vectors, screen_test,
app_test and standin_replay deleted and rebuilt (a stale binary is never a pass), every run read (a non-zero exit with
nothing printed is red), the file restored.

    python3 work/098-evidence/mutants-runner.py SCRATCH M0 M1 ...      (SCRATCH/screen098.c: the original)
"""
import os, re, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'screen098.c')
F = 'firmware/screen.c'
M = {
 'M0': [("    bool neg_unused_098;", None)],
 'M1': [("        for (int i = 0; i < 4; i++) level_op_text(c, k, i, out->levelop[i], sizeof out->levelop[i]);    /* unit 098 */",
         "        for (int i = 0; i < 4; i++) if (i != 1) level_op_text(c, k, i, out->levelop[i], sizeof out->levelop[i]);")],
 'M2': [("    if (level_op_text(c, k, i, t, sizeof t)) screen_text(f, NUM_X + 2, k->lvl_bot[i] - 14, 2, t, true);",
         "    if (level_op_text(c, k, i, t, sizeof t)) screen_text(f, NUM_X + 2, k->lvl_bot[i] - 13, 2, t, true);")],
 'M3': [("        if (k->xe_top || !k->x_letter || k->xk != SCREEN_VALUE) return false;",
         "        if (!k->x_letter || k->xk != SCREEN_VALUE) return false;")],
 'M4': [("        cells = k->x.n + (k->xmark && k->x.n < LINE_MAX_CP);   /* (as draw_xline returns) */",
         "        cells = k->x.n;")],
 'M5': [("        if (i == 1 && k->prompt) return false;\n", "")],
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/app_test', 'build/screen_test', 'build/standin_replay']


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, env=env)


def fails(cmd, pat):
    r = run(cmd)
    out = [l for l in (r.stdout + r.stderr).split('\n') if re.search(pat, l)]
    if r.returncode != 0 and not out:
        out = [f'exit {r.returncode} with no failure printed']
    return out, r


for name in sys.argv[2:]:
    open(F, 'w').write(open(ORIG).read())
    for o, n in M[name]:
        if n is None: continue                      # (M0: the control, nothing changed)
        t = open(F).read()
        assert t.count(o) == 1, (name, o[:50])
        open(F, 'w').write(t.replace(o, n))
    for x in BINS:
        if os.path.exists(x): os.remove(x)
    t0 = time.time()
    b = run('make ' + ' '.join(BINS) + ' 2>&1')
    stale = [x for x in BINS if not os.path.exists(x) or os.path.getmtime(x) < t0 - 1]
    red = []
    if stale:
        red.append('BUILD FAILED (' + ','.join(stale) + '): ' + '|'.join(l for l in b.stdout.split('\n') if 'error' in l)[:300])
    else:
        for vf in ('tests/098-soroban.txt', 'tests/hmark-abacus.txt', 'tests/hmark-A-abacus.txt'):
            v, _ = fails('./build/vectors ' + vf, r'^FAIL ')
            ids = sorted(set(re.findall(r'^FAIL (\w+):', '\n'.join(v), re.M)))
            if ids: red.append(os.path.basename(vf) + ':' + ','.join(ids))
            elif v: red.append(os.path.basename(vf) + ': ' + v[0][:100])
        at, _ = fails('./build/app_test', r'^app_test: (?!\d+/\d+ checks)(?!082: )(?!090 4a: )')
        if at: red.append(f'app_test {len(at)}: ' + ' || '.join(x[:90] for x in at[:4]))
        st, _ = fails('./build/screen_test tests/frames', r'differs|^screen_test: (?!\d+/\d+ checks)(?!\d\d-)')
        if st:
            fr = sorted(set(re.findall(r'frame (\d\d)-', '\n'.join(st))))
            other = [x for x in st if 'differs' not in x]
            red.append(f'screen_test frames {",".join(fr) or "none"}; {len(other)} checks' +
                       (': ' + ' || '.join(x[13:90] for x in other[:12]) if other else ''))
        sr, _ = fails('./build/standin_replay device/standin/*.txt', r'FAIL')
        if sr: red.append(f'replay {len(sr)}: ' + ' || '.join(x[:110] for x in sr[:3]))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
open(F, 'w').write(open(ORIG).read())
assert open(F).read() == open(ORIG).read(), F
print('restored:', F, 'as the original given (SCRATCH/screen098.c)')
