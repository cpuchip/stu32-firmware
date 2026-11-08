#!/usr/bin/env python3
"""Unit 090's planted faults (mutants-predicted.txt): each applied to the tree alone, the six test binaries built (their
times checked against the run's start, so a failed build is never a pass), every run read (a non-zero exit with nothing
printed is red, 089's lesson), the file restored.

    python3 work/090-evidence/mutants-runner.py SCRATCH M1 M2 ...    (SCRATCH holds GAMES_REPOS's g077 and orig090/)
"""
import os, re, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig090')
M = {
 'M0': ('core/rows.c', "/* Unit 089: the worksheet's rows", "/* Unit 089: the worksheet's rows"),     # (the control: no fault)
 'M1': ('firmware/screen.c', "        if (h) continue;\n        matched++;", "        if (h) (void)0;\n        matched++;"),
 'M2': ('firmware/screen.c', "while (n > (k->prompt ? 2 : 1) && lv[n - 1]->bare) n--;",
        "while (n > (k->prompt ? 2 : 1) && lv[n - 1]->row == 0 && lv[n - 1]->kind == AB_REAL && abn_is_zero(&lv[n - 1]->re)) n--;"),
 'M3': ('core/calc.c', "        ab_rows_bare_line(c, (ab_op)op, arg);   /* unit 090 (B′): a level this line made isn't bare, whatever its value */\n", ""),
 'M4': ('core/calc.c', "    ab_rows_bare_key(c, op, arg, st);           /* unit 090 (B′), every mode */\n", ""),
 'M5': ('core/state.c', "        if (c->stu && m) {", "        if (c->stu && m && 0) {"),
 'M6': ('core/rows.c', "    cand[0] = r->pre_var[arg];\n    cand[1] = r->pre[0];", "    cand[0] = r->pre[0];\n    cand[1] = r->pre_var[arg];"),
 'M7': ('core/rows.c', "        if (g16(c->rows + at + 6) == AB_ROW_OP_ALG) found = s;\n    }\n    return found;\n}",
        "        if (g16(c->rows + at + 6) == AB_ROW_OP_ALG) found = s;\n    }\n    {\n        uint32_t ring[16];\n        int n = 0;\n"
        "        for (uint32_t at = 0; at < end; at += g16(c->rows + at))\n"
        "            if (g16(c->rows + at + 6) == AB_ROW_OP_ALG) ring[n++ % 16] = g32(c->rows + at + 2);\n"
        "        if (n > 16 && found < ring[n % 16]) return 0;\n    }\n    return found;\n}"),
 'M8': ('firmware/screen.c', "    if (drawn == matched && c->rows_dropped && used < avail) {",
        "    if ((drawn == matched || 1) && c->rows_dropped && used < avail) {"),
 'M9': ('firmware/screen.c', "    k->page = c->stu && !c->prgm", "    k->page = !c->prgm"),
 'M10': ('core/rows.c', "    case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:      /* (unit 090: was a move) */\n"
         "        o.src = AB_ROWSRC_VAR, o.kind = AB_ROWK_LETTER;\n",
         "    case AB_STOADD: case AB_STOSUB: case AB_STOMUL: case AB_STODIV:\n        o.kind = AB_ROWK_MOVE;\n"),
 'M11': ('core/calc.c', "    ab_rows_clear_all(c);                           /* unit 090: the page too, as MEMORY CLEAR (abacus #14047) */\n", ""),
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/rows_test', 'build/app_test', 'build/expr_test', 'build/screen_test', 'build/store_test']
FILES = ('core/rows.c', 'core/calc.c', 'core/state.c', 'firmware/screen.c')


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
    for x in BINS:                              # (run 1: a screen.c fault rebuilt only what uses it, and the binaries
        if os.path.exists(x): os.remove(x)      #  that don't kept their times: read as failed builds. Every one rebuilt)
    t0 = time.time()
    b = run('make ' + ' '.join(BINS) + ' 2>&1')
    stale = [x for x in BINS if os.path.getmtime(x) < t0 - 1]
    red = []
    if stale:
        red.append('BUILD FAILED (' + ','.join(stale) + '): ' + '|'.join(l for l in b.stdout.split('\n') if 'error' in l)[:300])
    else:
        for tag, vf in (('090', 'tests/090-soroban.txt'), ('089', 'tests/089-soroban.txt'), ('t029', 'tests/029-soroban.txt'),
                        ('w029', 'work/029-vectors.txt')):
            v = fails('./build/vectors ' + vf, r'^FAIL ')
            ids = sorted(set(re.findall(r'^FAIL (\w+):', '\n'.join(v), re.M)))
            if ids: red.append(tag + ':' + ','.join(ids))
            elif v: red.append(tag + ': ' + v[0][:100])
        rt = fails('./build/rows_test', r'^FAIL')
        if rt: red.append(f'rows_test {len(rt)}: ' + ' || '.join(x[:100] for x in rt[:4]))
        at = fails('./build/app_test', r'^app_test: (?!\d+/\d+ checks)(?!082: )(?!090 4a: )')
        if at: red.append(f'app_test {len(at)}: ' + ' || '.join(x[:100] for x in at[:4]))
        et = fails('./build/expr_test', r'FAIL')
        if et: red.append(f'expr_test {len(et)}: ' + ' || '.join(x[:100] for x in et[:3]))
        st = fails('./build/screen_test tests/frames', r'differs|^screen_test: (?!\d+/\d+ checks)(?!\d\d-)')
        if st:
            fr = sorted(set(re.findall(r'frame (\d\d)-', '\n'.join(st))))
            other = [x for x in st if 'differs' not in x]
            red.append(f'screen_test frames {",".join(fr)}' + (f'; checks {len(other)}: ' + ' || '.join(x[:90] for x in other[:5]) if other else ''))
        so = fails('./build/store_test', r'^store_test: (?!.*\d+/\d+ checks)(?!power cut)(?!migration)')
        so += [l for l in run('./build/store_test').stdout.split('\n') if re.search(r'settings (\d+)/(\d+)', l) and
               re.search(r'settings (\d+)/(\d+)', l).group(1) != re.search(r'settings (\d+)/(\d+)', l).group(2)]
        if so: red.append(f'store_test {len(so)}: ' + ' || '.join(x[:100] for x in so[:3]))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
    open(f, 'w').write(orig)
for f in FILES:
    assert open(f).read() == open(os.path.join(ORIG, os.path.basename(f))).read(), f
print('restored:', ', '.join(FILES), 'as the originals given (SCRATCH/orig090)')
