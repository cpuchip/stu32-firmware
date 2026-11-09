#!/usr/bin/env python3
"""Unit 094's planted faults (mutants-predicted.txt): each applied alone, vectors, app_test, help_test and standin_replay
deleted and rebuilt (090's lesson: a stale binary is never a pass), every run read (a non-zero exit with nothing printed
is red), the file restored.

    python3 work/094-evidence/mutants-runner.py SCRATCH P0 P1 P2 ...    (SCRATCH holds GAMES_REPOS's g077 and orig094/)
"""
import os, re, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig094')
COPY = "        if (op == AB_ENTER) {                   /* unit 094: the copy"
M = {
 'P0': ('core/rows.c', "/* Unit 089: the worksheet's rows", "/* Unit 089: the worksheet's rows"),
 'P1': ('core/calc.c', COPY, "        if (op == AB_ENTER || op == AB_CASEVAL) {                   /* unit 094: the copy"),
 'P2': ('core/calc.c', COPY, "        if (0) {                   /* unit 094: the copy"),
 'P3': ('core/calc.c', "            *st = cas_plain_number(eq + 1) ? eqn_eval(c, eq + 1",
        "            for (int k = 1; k < n; k++) {\n                char cond[AB_EQN_LEN];\n"
        "                if (ab_cas_row(c, k, cond, sizeof cond) && cas_cond_holds(c, cond) == 0) {\n"
        "                    c->cas_row = (int8_t)k;\n                    *st = ABN_CAS_CONDITION;\n                    return true;\n"
        "                }\n            }\n            *st = cas_plain_number(eq + 1) ? eqn_eval(c, eq + 1"),
 'P4': ('core/calc.c', "            *st = cas_plain_number(eq + 1) ? eqn_eval(c, eq + 1", "            *st = false ? eqn_eval(c, eq + 1"),
 'P5': ('core/rows.c', "            uint32_t link[1] = {c->cas_rep_row};\n            c->x.row = append(c, AB_CASSOLVE, 0, 0, link, c->cas_rep_row ? 1 : 0,",
        "            uint32_t link[1] = {c->y.row};\n            c->x.row = append(c, AB_CASSOLVE, 0, 0, link, c->y.row ? 1 : 0,"),
 'P6': ('core/calc.c', "        for (int k = 1; k < n; k++) {           /* the CAS 038 repin",
        "        for (int k = 1; k < 0; k++) {           /* the CAS 038 repin"),
 'P7': ('firmware/keymap.c', '{KM_OP, AB_CASEVAL, 0, 0, "EVAL"}', '{KM_NONE, 0, 0, 0, ""}'),
 'P8': ('core/calc.c', "    case AB_CASEVAL:                        /* 094 */\n", ""),
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/app_test', 'build/help_test', 'build/standin_replay']
FILES = ('core/rows.c', 'core/calc.c', 'firmware/keymap.c')
VF = (('094', 'tests/094-soroban.txt'), ('048', 'tests/048-soroban.txt'), ('088', 'tests/088-soroban.txt'),
      ('076', 'tests/076-soroban.txt'), ('079', 'tests/079-soroban.txt'), ('h093', 'tests/h093-abacus.txt'))


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, env=env)


def fails(cmd, pat):
    r = run(cmd)
    out = [l for l in (r.stdout + r.stderr).split('\n') if re.search(pat, l)]
    if r.returncode != 0 and not out:
        out = [f'exit {r.returncode} with no failure printed']
    return out


for name in sys.argv[2:]:
    f, o, n = M[name]
    orig = open(os.path.join(ORIG, os.path.basename(f))).read()
    assert orig.count(o) == 1, name
    open(f, 'w').write(orig.replace(o, n))
    for x in BINS:
        if os.path.exists(x): os.remove(x)
    t0 = time.time()
    b = run('make ' + ' '.join(BINS) + ' 2>&1')
    stale = [x for x in BINS if not os.path.exists(x) or os.path.getmtime(x) < t0 - 1]
    red = []
    if stale:
        red.append('BUILD FAILED (' + ','.join(stale) + '): ' + '|'.join(l for l in b.stdout.split('\n') if 'error' in l)[:300])
    else:
        for tag, vf in VF:
            v = fails('./build/vectors ' + vf, r'^FAIL ')
            ids = sorted(set(re.findall(r'^FAIL (\w+):', '\n'.join(v), re.M)))
            if ids: red.append(tag + ':' + ','.join(ids))
            elif v: red.append(tag + ': ' + v[0][:100])
        at = fails('./build/app_test', r'^app_test: (?!\d+/\d+ checks)(?!082: )(?!090 4a: )')
        if at: red.append(f'app_test {len(at)}: ' + ' || '.join(x[:90] for x in at[:6]))
        ht = fails('./build/help_test', r'^help_test: (?!\d+/\d+ checks)(?!the sweep: \d+ states, (\d+) of \1 )')
        if ht: red.append(f'help_test {len(ht)}: ' + ' || '.join(x[:90] for x in ht[:3]))
        sr = fails('./build/standin_replay device/standin/*.txt', r'FAIL')
        if sr: red.append(f'replay {len(sr)}: ' + ' || '.join(x[:110] for x in sr[:3]))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
    open(f, 'w').write(orig)
for f in FILES:
    assert open(f).read() == open(os.path.join(ORIG, os.path.basename(f))).read(), f
print('restored:', ', '.join(FILES), 'as the originals given (SCRATCH/orig094)')
