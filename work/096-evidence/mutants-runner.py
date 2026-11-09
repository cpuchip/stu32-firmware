#!/usr/bin/env python3
"""Unit 096's planted faults (mutants-predicted.txt): each applied alone (one or more replacements), vectors, mark_drive,
expr_test, screen_test and standin_replay deleted and rebuilt (090's lesson: a stale binary is never a pass), every run
read (a non-zero exit with nothing printed is red), the files restored.

    python3 work/096-evidence/mutants-runner.py SCRATCH R0 R1 ...    (SCRATCH holds GAMES_REPOS's g077 and orig096/)
"""
import os, re, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig096')
M = {
 'R0': [('core/calc.c', "    return abn_cmp(&acc, base) == 0;", "    return abn_cmp(&acc, base) == 0;")],
 'R1': [('core/abn_intel.c', "if (fl & BID_INEXACT_EXCEPTION) rounded_seen = true;", "")],
 'R2': [('core/calc.c', "    abn_round_note();                           /* (096: the largest number stands in: rounded) */\n", "")],
 'R3': [('core/calc.c', "        abn_round_note();                       /* (096: flushed to 0: rounded) */\n", "")],
 'R4': [('core/fn.c', "        abn_round_note();   /* (096: a value no rounding call reported) */\n        *out = abn_is_negative(x) ? abn_from_int(-1) : one();",
         "        *out = abn_is_negative(x) ? abn_from_int(-1) : one();")],
 'R5': [('core/calc.c', "    return abn_cmp(&acc, base) == 0;", "    return true;")],
 'R6': [('core/fn.c', "            if (v && abn_cmp(x, &k) == 0) {",
         "            abn dd, ad, eps = K(\"1E-9\");\n            abn_sub(&dd, x, &k);\n            ad = abn_abs(&dd);\n"
         "            if (v && abn_cmp(&ad, &eps) < 0) {")],
 'R7': [('firmware/screen.c', "k->xmark = c->stu && !ab_is_sym(&c->x)", "k->xmark = !ab_is_sym(&c->x)"),
        ('firmware/screen.c', "    k->xmark = k->xmark && k->page;             /* (096: STU's home only) */\n", "")],
 'R8': [('core/fn.c', "    if (st == ABN_OK && f != f_tenx && !(f == f_ln && arg == 1) && !abn_is_zero(x) && !abn_is_zero(out))\n        abn_round_note();\n", "    (void)f, (void)arg;\n")],
 'R9': [('core/calc.c', "    if (c->running || c->active) v.rounded = 1;    /* (096: a letter a program or a solver writes: an untracked path) */\n", "")],
 'R10': [('core/state.c', "            if (r->version >= 25) {\n                unsigned m = r8(r);", "            if (0) {\n                unsigned m = r8(r);")],
 'R11': [('core/state.c', "    if (c->stu && !r.had_rounded) {", "    if (0) {")],
 'R12': [('core/calc.c', "        else if (o.src == AB_ROWSRC_VAR) in = true;     /* (an indirect register keeps no mark: rounded) */\n", "")],
 'R13': [('core/calc.c', "    nv.rounded = abn_round_seen();\n", "")],
 'R14': [('core/calc.c', "        if ((op == AB_ENTER || op == AB_CASEVAL) && !ab_cas_report(c)) c->x.rounded = 1;", "        ;")],
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/abn_intel.o', 'build/vectors', 'build/mark_drive', 'build/expr_test', 'build/screen_test', 'build/standin_replay']
FILES = ('core/abn_intel.c', 'core/calc.c', 'core/fn.c', 'core/state.c', 'firmware/screen.c')


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
        v, _ = fails('./build/vectors tests/096-soroban.txt', r'^FAIL ')
        ids = sorted(set(re.findall(r'^FAIL (\w+):', '\n'.join(v), re.M)))
        if ids: red.append('096:' + ','.join(ids))
        elif v: red.append('096: ' + v[0][:100])
        r = run('python3 tests/mark_table.py build/mark_drive tests/mark-table.txt')
        m = re.search(r'(\d+) false exact', r.stdout)
        if r.returncode != 0:
            byop = {}
            for op in re.findall(r'FALSE EXACT (\w+) ', r.stdout): byop[op] = byop.get(op, 0) + 1
            red.append(f'census {m.group(1) if m else "?"} false exact' + (f' (first 40 by op: {byop})' if byop else '') +
                       ('; ' + re.search(r'FALSE ROUNDED OVER.*', r.stdout).group(0) if 'OVER THE BOUND' in r.stdout else ''))
        et, _ = fails('./build/expr_test', r'^FAIL')
        if et: red.append(f'expr_test {len(et)}: ' + ' || '.join(x[:90] for x in et[:4]))
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
print('restored:', ', '.join(FILES), 'as the originals given (SCRATCH/orig096)')
