#!/usr/bin/env python3
"""Unit 089's planted faults (mutants-predicted.txt): each applied to the tree alone, the four test binaries built (their
times checked against the run's start, so a failed build is never a pass), the four runs read, the file restored.

    python3 work/089-evidence/mutants-runner.py SCRATCH M1 M2 ...    (SCRATCH holds GAMES_REPOS's g077 and the .orig copies)
"""
import os, re, shutil, subprocess, sys, time

S = sys.argv[1]
M = {
 'M1': ('core/rows.c', "        for (int j = 0; j < ns && !dup; j++) dup = seen[j] == cand[i];",
                       "        for (int j = 0; j < ns && !dup; j++) dup = dup && seen[j] == cand[i];"),
 'M2': ('core/rows.c', "        for (int lv = 3; lv >= 0; lv--)                         /* the deepest level read first (R1) */",
                       "        for (int lv = 0; lv <= 3; lv++)"),
 'M3': ('core/calc.c', "        ab_rows_num(c);                         /* unit 089 (R3): the typed number's row, before the op reads it */\n", ""),
 'M4': ('core/rows.c', "    stale(c, run);\n    r->on = false;", "    (void)run;\n    r->on = false;"),
 'M5': ('core/rows.c', "    c->rows_len = ab_rows_visible_len(c);\n    while", "    while"),
 'M6': ('core/state.c', "        if (nh || c->row_top) {", "        if (0) {"),
 'M7': ('core/state.c', "for (int i = 0; i < nh && !keep; i++) keep = held[i] == sr;", "for (int i = 0; i < nh && !keep; i++) keep = keep && held[i] == sr;"),
 'M8': ('core/rows.c', "    if (!r->on || !c->alg_eval || !s) return;", "    if (r->on || !r->on) return;"),
 'M9': ('core/rows.c', "    ab_rowop o = {0, 0, AB_ROWSRC_NONE, AB_ROWK_NONE};\n    (void)arg;",
                       "    ab_rowop o = {0, 0, AB_ROWSRC_NONE, AB_ROWK_NONE};\n    (void)arg;\n    if (op == AB_PCT) return (ab_rowop){1, 1, AB_ROWSRC_NONE, AB_ROWK_OP};"),
 'M10': ('core/rows.c', "    case AB_PI: case AB_CONST:", "    case AB_CONST:"),
 'M11': ('core/rows.c', "    if (s > AB_ROW_SERIAL_LAST) {\n        restart(c);", "    if (s == 0) {\n        restart(c);"),
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/rows_test', 'build/app_test', 'build/expr_test']


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, env=env)


def fails(cmd, pat):
    r = run(cmd)
    out = [l for l in (r.stdout + r.stderr).split('\n') if re.search(pat, l)]
    if r.returncode != 0 and not out:           # (a crash prints nothing it buffered: red, never a pass; M3's first run)
        out = [f'exit {r.returncode} with no failure printed']
    return out


for name in sys.argv[2:]:
    f, o, n = M[name]
    orig = open(os.path.join(S, 'orig', os.path.basename(f))).read()
    cnt = orig.count(o)
    assert cnt >= 1, name
    if name != 'M7':
        assert cnt == 1, name
    open(f, 'w').write(orig.replace(o, n))
    t0 = time.time()
    b = run('make ' + ' '.join(BINS) + ' 2>&1')
    stale = [x for x in BINS if os.path.getmtime(x) < t0 - 1]
    red = []
    if stale:
        red.append('BUILD FAILED (' + ','.join(stale) + '): ' + '|'.join(l for l in b.stdout.split('\n') if 'error' in l)[:300])
    else:
        v = fails('./build/vectors tests/089-soroban.txt', r'^FAIL ')
        ids = sorted(set(re.findall(r'^FAIL (\w+):', '\n'.join(v), re.M)))
        if ids: red.append('089:' + ','.join(ids))
        rt = fails('./build/rows_test', r'^FAIL')
        if rt: red.append(f'rows_test {len(rt)}: ' + ' || '.join(x[:110] for x in rt[:4]))
        at = fails('./build/app_test', r'^app_test: (?!\d+/\d+ checks)(?!082: )')
        if at: red.append(f'app_test {len(at)}: ' + ' || '.join(x[:110] for x in at[:4]))
        et = fails('./build/expr_test', r'FAIL')
        if et: red.append(f'expr_test {len(et)}: ' + ' || '.join(x[:110] for x in et[:3]))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
    open(f, 'w').write(orig)
for f in ('core/rows.c', 'core/calc.c', 'core/state.c'):
    assert open(f).read() == open(os.path.join(S, 'orig', os.path.basename(f))).read(), f
print('restored: core/rows.c, core/calc.c, core/state.c as 7a30b0f')
