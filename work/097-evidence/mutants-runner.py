#!/usr/bin/env python3
"""Unit 097's planted faults (mutants-predicted.txt): each applied alone (one or more replacements), vectors, mark_drive,
expr_test, screen_test and standin_replay deleted and rebuilt (090's lesson: a stale binary is never a pass), every run
read (a non-zero exit with nothing printed is red), the files restored.

    python3 work/097-evidence/mutants-runner.py SCRATCH J0 J1 ...
"""
import os, re, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig097')
M = {
 'J0': [('core/fn.c', "    bool neg = abn_cmp(p, &half) > 0;", "    bool neg = abn_cmp(p, &half) > 0;")],
 'J1': [('core/fn.c', "    return nd_half_erfc(out, upper ? 1 : -1);",
         "    if (upper) {\n        abn lo1, o1 = one();\n        abn_status s1 = nd_half_erfc(&lo1, -1);\n"
         "        if (s1 != ABN_OK) return s1;\n        return abn_sub(out, &o1, &lo1);\n    }\n    return nd_half_erfc(out, -1);")],
 'J2': [('core/fn.c', "    return nd_half_erfc(out, upper ? 1 : -1);", "    return nd_half_erfc(out, 1);")],
 'J3': [('core/fn.c', "            if (centre) {\n                hp_set_int(&k, 2);",
         "            if (!hp_from_abn(&step, &x)) return ABN_RANGE;\n            if (centre) {\n                hp_set_int(&k, 2);")],
 'J4': [('core/fn.c', "    if (abn_cmp(p, &zero) <= 0 || abn_cmp(p, &onev) >= 0) return ABN_DOMAIN;", "    (void)zero;")],
 'J5': [('core/calc.c', "    if (op >= AB_ERF && op <= AB_NCDFINV && !c->stu) return ABN_STU_ONLY;", "")],
 'J6': [('core/calc.c', "    const char *nd = nd_name_in(text);", "    const char *nd = nd_name_in(\"\");")],     # (run 1: NULL left nd_name_in unused: -Werror)
 'J7': [('core/state.c', "        if (op_code((ab_op)op) >= V1_N_096 && w.ver < 26) w.ver = 26;   /* (097: a code an older build lacks) */", "")],
 'J8': [('core/state.c', "        if (code >= V1_N_096 && w.ver < 26) w.ver = 26;                  /* (097) */", "")],
 'J9': [('core/fn.c', "    if (!hp_from_abn(E, &u0) || !hp_add(A, D, E, true, ND_P)) return ABN_RANGE;   /* A = r = u - u0 */",
         "    if (!hp_from_abn(E, &u0)) return ABN_RANGE;\n    hp_set_int(A, 0);")],
 'J10': [('core/fn.c', "    bool neg = abn_cmp(p, &half) > 0;", "    abn hi9 = K(\"0.9\");\n    bool neg = abn_cmp(p, &hi9) > 0;")],
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/fn_drive', 'build/vectors', 'build/app_test', 'build/screen_test', 'build/standin_replay']
FILES = ('core/calc.c', 'core/fn.c', 'core/state.c')


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, env=env)


def fails(cmd, pat):
    r = run(cmd)
    out = [l for l in (r.stdout + r.stderr).split('\n') if re.search(pat, l)]
    if r.returncode != 0 and not out:
        out = [f'exit {r.returncode} with no failure printed']
    return out, r


for name in sys.argv[2:]:
    for f in FILES:
        open(f, 'w').write(open(os.path.join(ORIG, os.path.basename(f))).read())
    for f, o, n in M[name]:
        t = open(f).read()
        assert t.count(o) == 1, (name, o[:40])
        open(f, 'w').write(t.replace(o, n))
    for x in BINS:
        if os.path.exists(x): os.remove(x)
    t0 = time.time()
    b = run('make ' + ' '.join(BINS) + ' 2>&1')
    stale = [x for x in BINS if not os.path.exists(x) or os.path.getmtime(x) < t0 - 1]
    red = []
    if stale:
        red.append('BUILD FAILED (' + ','.join(stale) + '): ' + '|'.join(l for l in b.stdout.split('\n') if 'error' in l)[:300])
    else:
        for tag, pre in (('097', ''), ('097 save/load', 'AB_SAVELOAD=1 AB_STORE=1 ')):
            v, _ = fails(pre + './build/vectors tests/097-soroban.txt', r'^FAIL ')
            ids = sorted(set(re.findall(r'^FAIL (\w+):', '\n'.join(v), re.M)))
            if ids: red.append(tag + ':' + ','.join(ids))
            elif v: red.append(tag + ': ' + v[0][:100])
        r = run('python3 tests/fn_table.py build/fn_drive tests/erf-table.txt')
        if r.returncode != 0:
            byname = {}
            for nm in re.findall(r'FAIL \S+: (\w+) ', r.stdout): byname[nm] = byname.get(nm, 0) + 1
            m = re.search(r'(\d+) lines .*?, (\d+) within', r.stdout)
            red.append(f'fn_table {int(m.group(1)) - int(m.group(2)) if m else "?"} past the bar (first 40 by name: {byname})')
        at, _ = fails('./build/app_test', r'^app_test: (?!\d+/\d+ checks)(?!082: )(?!090 4a: )')
        if at: red.append(f'app_test {len(at)}: ' + ' || '.join(x[:90] for x in at[:4]))
        st, _ = fails('./build/screen_test tests/frames', r'differs|^screen_test: (?!\d+/\d+ checks)(?!\d\d-)')
        if st:
            fr = sorted(set(re.findall(r'frame (\d\d)-', '\n'.join(st))))
            other = [x for x in st if 'differs' not in x]
            red.append(f'screen_test frames {",".join(fr)}' + (f'; checks: ' + ' || '.join(x[:80] for x in other[:3]) if other else ''))
        sr, _ = fails('./build/standin_replay device/standin/*.txt', r'FAIL')
        if sr: red.append(f'replay {len(sr)}: ' + ' || '.join(x[:110] for x in sr[:3]))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
for f in FILES:
    open(f, 'w').write(open(os.path.join(ORIG, os.path.basename(f))).read())
for f in FILES:
    assert open(f).read() == open(os.path.join(ORIG, os.path.basename(f))).read(), f
print('restored:', ', '.join(FILES), 'as the originals given (SCRATCH/orig097)')
