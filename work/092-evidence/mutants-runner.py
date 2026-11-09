#!/usr/bin/env python3
"""Unit 092's planted faults (mutants-predicted.txt; Q4's text and Q7, Q8 for run 3, the store at 8e259ef): each applied alone to firmware/store.c, store_test and store_sim
deleted and rebuilt (a stale binary is never a pass), every run read (a non-zero exit with nothing printed is red), the
file restored.

    python3 work/092-evidence/mutants-runner.py SCRATCH Q0 Q1 ...    (SCRATCH holds GAMES_REPOS's g077 and orig092/)
"""
import os, re, subprocess, sys, time

S = sys.argv[1]
ORIG = os.path.join(S, 'orig092')
HDR_LAST = '''    if (ring_io(f, true, at, AB_STORE_HEADER, (void *)image, len) != 0) return -1;
    memset(h, 0xFF, sizeof h);
    memcpy(h, "ABST", 4);
    put32(h + 4, seq);
    put32(h + 8, len);
    put32(h + 12, ab_crc32(image, len));
    put32(h + 16, ab_crc32(h, 16));
    if (f->write(f->ctx, at * f->sector, h, sizeof h) != 0) return -1;     /* the header last: it commits the save */'''
HDR_FIRST = '''    memset(h, 0xFF, sizeof h);
    memcpy(h, "ABST", 4);
    put32(h + 4, seq);
    put32(h + 8, len);
    put32(h + 12, ab_crc32(image, len));
    put32(h + 16, ab_crc32(h, 16));
    if (f->write(f->ctx, at * f->sector, h, sizeof h) != 0) return -1;
    if (ring_io(f, true, at, AB_STORE_HEADER, (void *)image, len) != 0) return -1;'''
M = {
 'Q0': ("/* store: the state image in a ring", "/* store: the state image in a ring"),
 'Q1': ("    uint32_t at = nw.good ? (nw.at + span(f, nw.len)) % n : 0, k = span(f, len);",
        "    uint32_t at = 0, k = span(f, len);"),
 'Q2': ("    uint32_t seq = (nw.good ? nw.seq : 0) + 1;",
        "    if (at + k > n) at = 0;\n    uint32_t seq = (nw.good ? nw.seq : 0) + 1;"),
 'Q3': ("            if (k < below && (!any || k > key)) best = h, key = k, any = true;",
        "            if (k < below && !any) best = h, key = k, any = true;"),
 'Q4': (HDR_LAST, HDR_FIRST),
 'Q5': ("    if (len == 0 || len > f->slot_size - AB_STORE_HEADER) return -1;",
        "    if (len == 0 || len > 2 * f->slot_size - AB_STORE_HEADER) return -1;"),
 'Q7': ("    if (!header_at(f, f->ring->at, &h) || h.seq != f->ring->seq || h.len != f->ring->len || h.crc != f->ring->crc) {",
        "    h.seq = f->ring->seq, h.len = f->ring->len, h.crc = f->ring->crc, h.at = f->ring->at;\n    if (0) {"),
 'Q8': ("    if (f->ring) f->ring->valid = false;        /* (until this save is whole) */\n", ""),
 'Q6': ("        if (payload_good(f, &best)) {", "        if (payload_good(f, &best) || 1) {"),   # (run 1's "if (1)" didn't compile: payload_good unused)
}
SECTIONS = [('A', 6, 23), ('B', 24, 36), ('C', 37, 79), ('D', 80, 96), ('E', 97, 109), ('F', 110, 123), ('G', 124, 137)]
env = dict(os.environ, GAMES_REPOS=S + '/g077', CASIM_REPO='/home/claude/code/stuffleberry/workspace/projects-internal/casim')
BINS = ['build/store_test', 'build/store_sim']
F = 'firmware/store.c'


def run(cmd, t=900):
    try:
        return subprocess.run(cmd, shell=True, capture_output=True, text=True, env=env, timeout=t)
    except subprocess.TimeoutExpired:
        return None


def short_test(out):
    bad = []
    for l in out.split('\n'):
        m = re.search(r'(\d+)/(\d+)', l)
        w = re.search(r'erased (\d+) to (\d+) times \((\d+) to (\d+) allowed\)', l)
        if 'FAIL' in l or (m and int(m.group(1)) < int(m.group(2))) or (w and (int(w.group(1)) < int(w.group(3)) or int(w.group(2)) > int(w.group(4)))):
            bad.append(l.replace('store_test: ', '')[:70])
    return bad


for name in sys.argv[2:]:
    o, n = M[name]
    orig = open(os.path.join(ORIG, 'store.c')).read()
    assert orig.count(o) == 1, name
    open(F, 'w').write(orig.replace(o, n))
    for x in BINS:
        if os.path.exists(x): os.remove(x)
    t0 = time.time()
    b = run('make ' + ' '.join(BINS) + ' 2>&1')
    stale = [x for x in BINS if not os.path.exists(x) or os.path.getmtime(x) < t0 - 1]
    red = []
    if stale:
        red.append('BUILD FAILED (' + ','.join(stale) + ')')
    else:
        r = run('./build/store_test')
        if r is None: red.append('store_test TIMEOUT')
        else:
            st = short_test(r.stdout + r.stderr)
            if st: red.append('store_test: ' + ' || '.join(st))
            elif r.returncode: red.append(f'store_test: exit {r.returncode} with nothing short')
        r = run('./build/store_sim tests/092-soroban-store.txt')
        if r is None: red.append('store_sim TIMEOUT')
        else:
            lines = sorted({int(x) for x in re.findall(r'^FAIL line (\d+):', r.stdout, re.M)})
            secs = [s for s, a, z in SECTIONS if any(a <= x <= z for x in lines)]
            if secs: red.append('store_sim ' + ''.join(secs) + ' (lines ' + ','.join(map(str, lines)) + ')')
            elif r.returncode: red.append(f'store_sim: exit {r.returncode}, no FAIL line')
    print(name, 'RED:' if red else 'SURVIVED', ' ## '.join(red), flush=True)
    open(F, 'w').write(orig)
assert open(F).read() == open(os.path.join(ORIG, 'store.c')).read()
print('restored:', F, 'as the original given (SCRATCH/orig092)')
