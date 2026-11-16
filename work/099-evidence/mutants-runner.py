#!/usr/bin/env python3
"""Unit 099's planted faults (mutants-predicted.txt): each applied alone, the binaries deleted and rebuilt (a stale
binary is never a pass), every run read (a non-zero exit with nothing printed is red), the files restored and compared.

    python3 work/099-evidence/mutants-runner.py SCRATCH M0 M1 ...
"""
import os, re, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig099')
CALC, STATE, APP, LIST = 'core/calc.c', 'core/state.c', 'firmware/app.c', 'core/list.c'
M = {
 'M0': [(CALC, "    if (ab_rec_model(c) == AB_MODEL_STU) {\n        /* unit 099 (work/stu-indirect.md, decision 100)",
              "    if (ab_rec_model(c) == AB_MODEL_STU) {\n        /* unit 099 (work/stu-indirect.md, decision 100)")],
 'M1': [(CALC, "    if (code != AB_VAR_IND_I && code != AB_VAR_IND_J) return ABN_DOMAIN;\n    if (ab_rec_model(c) == AB_MODEL_STU) {",
              "    if (code != AB_VAR_IND_I && code != AB_VAR_IND_J) return ABN_DOMAIN;\n    if (0) {")],
 'M2': [(CALC, "        if (ix->kind != AB_REAL || !int_in(&ix->re, 0, AB_IND_MAX, &k)) return ABN_NO_REGISTER;\n        if (for_read && k > c->ind_top) return ABN_NO_REGISTER;",
              "        if (ix->kind != AB_REAL || !int_in(&ix->re, -26, AB_IND_MAX, &k)) return ABN_NO_REGISTER;\n"
              "        if (k < 0) { out->val = &c->var[-k - 1]; return ABN_OK; }\n        if (for_read && k > c->ind_top) return ABN_NO_REGISTER;")],
 'M3': [(CALC, "!int_in(&ix->re, 0, AB_IND_MAX, &k)) return ABN_NO_REGISTER;", "!int_in(&ix->re, 0, AB_IND_MAX - 1, &k)) return ABN_NO_REGISTER;")],
 'M4': [(CALC, "!int_in(&ix->re, 0, AB_IND_MAX, &k)) return ABN_NO_REGISTER;", "!int_in(&ix->re, 0, AB_IND_MAX + 1, &k)) return ABN_NO_REGISTER;")],
 'M5': [(STATE, "    if (r.version < 27) {", "    if (0) {")],
 'M6': [(STATE, "        if (stu && w.ver < 27) w.ver = 27;", "        (void)stu;")],
 'M7': [(CALC, "        return ABN_OK;\n    }\n    if (ab_rec_model(c) == AB_MODEL_STU) {\n        /* unit 099 (decision 102, (b))",
              "        return ABN_OK;\n    }\n    if (0) {\n        /* unit 099 (decision 102, (b))")],
 'M8': [(APP, "                if (ab_rec_model(a->c) == AB_MODEL_STU && arg == AB_VAR_IND_I &&",
              "                if (0 && arg == AB_VAR_IND_I &&")],
 'M9': [(LIST, "        line_m35 = best >= 0 && c->lmode[best] ? c->lmode[best] == 35 : c->m35 && !c->stu;   /* 099: STU's is (i) */",
              "        line_m35 = best >= 0 && c->lmode[best] ? c->lmode[best] != 33 : c->m35;   /* 099: STU's is (i) */"),
         (LIST, "    line_m35 = c->m35 && !c->stu;          /* (unit 099: STU's index is (i), as the 33s's) */",
              "    line_m35 = c->stu || c->m35;          /* (unit 099: STU's index is (i), as the 33s's) */")],
 'M10': [(APP, "            if (ab_rec_model(a->c) != 35 && l->kind != KL_LETTER) {", "            if (!ab_rec35(a->c) && l->kind != KL_LETTER) {")],
}
FILES = (CALC, STATE, APP, LIST)
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/app_test', 'build/expr_test', 'build/rows_test', 'build/store_test']
VEC = ['tests/099-soroban.txt', 'work/029-vectors.txt', 'tests/029-soroban.txt', 'tests/096-soroban.txt']


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, env=env)


def restore():
    for f in FILES:
        open(f, 'w').write(open(os.path.join(ORIG, os.path.basename(f))).read())


for name in sys.argv[2:]:
    restore()
    for f, o, n in M[name]:
        t = open(f).read()
        assert t.count(o) == 1, (name, o[:50])
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
        for v in VEC:
            r = run('./build/vectors ' + v)
            ids = sorted(set(re.findall(r'^FAIL (\w+):', r.stdout + r.stderr, re.M)))
            if ids: red.append(os.path.basename(v) + ':' + ','.join(ids))
            elif r.returncode: red.append(os.path.basename(v) + f': exit {r.returncode} with no failure printed')
        for t in ('app_test', 'expr_test', 'rows_test', 'store_test'):
            r = run('./build/' + t)
            out = r.stdout + r.stderr
            m = re.findall(r'(\d+)/(\d+) checks', out)
            bad = sum(int(b) - int(a) for a, b in m)
            fails = [l for l in out.split('\n') if re.search(r'FAIL|differs', l)]
            if bad or fails or r.returncode:
                red.append(f'{t} rc {r.returncode}, {bad} short' + (': ' + ' || '.join(x[:110] for x in fails[:3]) if fails else ''))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
restore()
for f in FILES:
    assert open(f).read() == open(os.path.join(ORIG, os.path.basename(f))).read(), f
print('restored:', ', '.join(FILES), 'as the originals given (SCRATCH/orig099), compared byte for byte')
