#!/usr/bin/env python3
"""Unit 095's planted faults (mutants-predicted.txt): each applied alone, vectors, app_test, help_test and standin_replay
deleted and rebuilt (090's lesson: a stale binary is never a pass), every run read (a non-zero exit with nothing printed
is red), the file restored.

    python3 work/094-evidence/mutants-runner.py SCRATCH P0 P1 P2 ...    (SCRATCH holds GAMES_REPOS's g077 and orig095/)
"""
import os, re, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig095')
COPY = "        if (op == AB_ENTER) {                   /* unit 094: the copy"
M = {
 'T0': ('core/calc.c', "/* ---- unit 095: show steps", "/* ---- unit 095: show steps"),
 'T1': ('core/calc.c', "    ab_steps_clear(c);                                  /* unit 095: no steps kept (the tail's) */\n", ""),
 'T2': ('core/calc.c', "    abn_status st = step_page(c, 0);\n    if (st == ABN_OK && c->step_n <= 0) c->step_open = false;",
        "    abn_status st = step_page(c, 0);\n    for (int k = c->step_n - 1; st == ABN_OK && k >= 0; k--) st = step_page(c, k);\n"
        "    if (st == ABN_OK && c->step_n <= 0) c->step_open = false;"),
 'T3': ('core/calc.c', "casim_step(&rq, k, CASIM_STEPS_ONE_LINE, &in, out, sizeof out, ab_cas_arena, sizeof ab_cas_arena);",
        "casim_step(&rq, k, 0, &in, out, sizeof out, ab_cas_arena, sizeof ab_cas_arena);"),
 'T4': ('core/calc.c', "    c->step_op = 0;                             /* unit 095: a new CAS op replaces the steps' request (a refusal: none) */\n    c->step_ans[0] = '\\0';\n", ""),
 'T5': ('core/calc.c', "    if (c->step_op == CASIM_SOLVE + 1) return c->cas_rep[0] != '\\0';",
        "    if (c->step_op == CASIM_SOLVE + 1) return true;"),
 'T6': ('core/calc.c', "    c->step_open = false;\n    return op == AB_CANCEL || op == AB_KBKSP;\n}",
        "    c->step_open = false;\n    if (op == AB_CANCEL) c->cas_rep[0] = '\\0';\n    return op == AB_CANCEL || op == AB_KBKSP;\n}"),
 'T7': ('core/calc.c', "    if (c->intr) {\n        c->intr = false;\n        c->step_open = false;\n        return ABN_INTERRUPTED;\n    }\n", ""),
 'T8': ('core/calc.c', "        *st = step_page(c, (c->step_k + (op == AB_SST ? 1 : n - 1)) % n);",
        "        *st = step_page(c, op == AB_SST ? (c->step_k + 1) % n : (c->step_k > 0 ? c->step_k - 1 : 0));"),
 'T9': ('core/calc.c', "        if (c->eqn_typing || c->eqn_cur < 0 || !c->step_ans[0] || strcmp(c->eqn[c->eqn_cur], c->step_ans) != 0)",
        "        if (c->eqn_typing || c->eqn_cur < 0 || !c->step_ans[0])"),
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/app_test', 'build/screen_test', 'build/standin_replay']
FILES = ('core/calc.c',)
VF = (('095', 'tests/095-soroban.txt'), ('094', 'tests/094-soroban.txt'), ('048', 'tests/048-soroban.txt'))


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
        st = fails('./build/screen_test tests/frames', r'differs|^screen_test: (?!\d+/\d+ checks)(?!\d\d-)')
        if st:
            fr = sorted(set(re.findall(r'frame (\d\d)-', '\n'.join(st))))
            other = [x for x in st if 'differs' not in x]
            red.append(f'screen_test frames {",".join(fr)}' + (f'; checks: ' + ' || '.join(x[:80] for x in other[:3]) if other else ''))
        sr = fails('./build/standin_replay device/standin/*.txt', r'FAIL')
        if sr: red.append(f'replay {len(sr)}: ' + ' || '.join(x[:110] for x in sr[:3]))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
    open(f, 'w').write(orig)
for f in FILES:
    assert open(f).read() == open(os.path.join(ORIG, os.path.basename(f))).read(), f
print('restored:', ', '.join(FILES), 'as the originals given (SCRATCH/orig095)')
