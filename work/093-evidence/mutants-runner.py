#!/usr/bin/env python3
"""Unit 093's planted faults (mutants-predicted.txt): each applied alone, vectors, app_test and screen_test deleted and
rebuilt (090's lesson: a stale binary is never a pass), every run read (a non-zero exit with nothing printed is red),
the file restored.

    python3 work/093-evidence/mutants-runner.py SCRATCH M0 N1 N2 ...    (SCRATCH holds GAMES_REPOS's g077 and orig093/)
"""
import os, re, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig093')
M = {
 'M0': ('core/rows.c', "/* Unit 089: the worksheet's rows", "/* Unit 089: the worksheet's rows"),
 'N1': ('core/calc.c', "    if (ab_rows_window(c, &top, &bot) && top && p < top) c->sel_top = p;",
        "    if (ab_rows_window(c, &top, &bot) && top && p < top && 0) c->sel_top = p;"),
 'N2': ('core/rows.c', "    uint32_t s = append(c, AB_COPY, 0, 0, link, 1, NULL, 0, &c->x);",
        "    uint32_t s = append(c, AB_COPY, 0, 0, link, 0, NULL, 0, &c->x);"),
 'N3': ('core/calc.c', "        abn_status st = sel_copy(c, c->sel_row);    /* a verb: COPY, then the verb on X */",
        "        abn_status st = ABN_OK;\n        sel_clear(c);"),
 'N4': ('core/calc.c', "    case AB_CANCEL: case AB_KBKSP:\n        sel_clear(c);\n        return ABN_OK;\n    case AB_ENTER: case AB_COPY:",
        "    case AB_ENTER: case AB_COPY:"),
 'N5': ('core/calc.c', "        if (c->rows_dropped && !c->sel_drop) c->sel_drop = true, c->sel_top = c->sel_row;\n        return;",
        "        c->sel_row = ab_rows_page_prev(c, 0), c->sel_top = 0;\n        return;"),
 'N6': ('core/calc.c', "        c->sel_row = c->sel_entry, c->sel_level = c->sel_entry_level, c->sel_top = 0, c->sel_drop = false;\n",
        ""),
 'N7': ('core/calc.c', "    case AB_ENTER: case AB_COPY:\n        return sel_copy(c, c->sel_row);",
        "    case AB_COPY:\n        return sel_copy(c, c->sel_row);"),
 'N8': ('firmware/screen.c', "            if (sr == links[j] && k->pg[i].mark == '-') k->pg[i].mark = 'L';",
        "            if (sr == links[j] && k->pg[i].mark == '-' && 0) k->pg[i].mark = 'L';"),
 'N9': ('core/calc.c', "    case AB_K4: case AB_K6: case AB_SEL: case AB_ROWEDIT:",
        "    case AB_K4: case AB_K6: case AB_SEL: case AB_ROWEDIT: case AB_K0: case AB_K1: case AB_K3: case AB_K7: case AB_K9:"),
 'N10': ('core/calc.c', "    c->sel_view = 0;                            /* (unit 093: a row's view, too) */\n", ""),
}
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/vectors', 'build/app_test', 'build/screen_test']
FILES = ('core/rows.c', 'core/calc.c', 'firmware/screen.c')


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
        for tag, vf in (('093', 'tests/093-soroban.txt'), ('090', 'tests/090-soroban.txt'), ('h090', 'tests/h090-abacus.txt')):
            v = fails('./build/vectors ' + vf, r'^FAIL ')
            ids = sorted(set(re.findall(r'^FAIL (\w+):', '\n'.join(v), re.M)))
            if ids: red.append(tag + ':' + ','.join(ids))
            elif v: red.append(tag + ': ' + v[0][:100])
        at = fails('./build/app_test', r'^app_test: (?!\d+/\d+ checks)(?!082: )(?!090 4a: )')
        if at: red.append(f'app_test {len(at)}: ' + ' || '.join(x[:100] for x in at[:4]))
        st = fails('./build/screen_test tests/frames', r'differs|^screen_test: (?!\d+/\d+ checks)(?!\d\d-)')
        if st:
            fr = sorted(set(re.findall(r'frame (\d\d)-', '\n'.join(st))))
            other = [x for x in st if 'differs' not in x]
            red.append(f'screen_test frames {",".join(fr)}' + (f'; checks {len(other)}: ' + ' || '.join(x[:90] for x in other[:5]) if other else ''))
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
    open(f, 'w').write(orig)
for f in FILES:
    assert open(f).read() == open(os.path.join(ORIG, os.path.basename(f))).read(), f
print('restored:', ', '.join(FILES), 'as the originals given (SCRATCH/orig093)')
