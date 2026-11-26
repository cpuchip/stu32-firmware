#!/usr/bin/env python3
"""PROTO 106: every plain vector run make test makes, PROTO_DIFF=1; each screen line whose drawn string isn't its text,
grouped by the kind of difference. Run from the prototype tree: census.py OUT"""
import os, re, subprocess, sys, collections
out = open(sys.argv[1], 'w')
mk = subprocess.run(['make', '-n', 'test'], capture_output=True, text=True).stdout
runs, seen = [], set()
for line in mk.split('\n'):
    line = re.sub(r'\s+#.*$', '', line.strip())
    m = re.match(r'^((?:[A-Z_]+=\S+\s+)*)\./build/vectors\s+(\S+)$', line)
    if m and line not in seen:
        seen.add(line); runs.append((dict(kv.split('=', 1) for kv in m.group(1).split()), m.group(2)))
kinds = collections.Counter(); ex = {}
nvec = 0
for env, f in runs:
    e = dict(os.environ, PROTO_DIFF='1', **env)
    r = subprocess.run(['./build/vectors', f], capture_output=True, text=True, errors='replace', env=e)
    cur = None
    for l in r.stderr.split('\n'):
        if l.startswith('PROTOVEC '): cur = l[9:]; nvec += 1
        elif l.startswith('PROTODIFF '):
            m = re.match(r'PROTODIFF text=\[(.*)\] drawn=\[(.*)\]$', l)
            t, d = m.group(1), m.group(2)
            # the kind: which characters differ
            k = []
            if '⟮' in d: k.append('pair')
            if '̂' in d or '̄' in d: k.append('mark')
            if not k: k.append('other')
            key = '+'.join(k)
            kinds[key] += 1
            ex.setdefault(key, []).append((cur, t, d))
print(f'{len(runs)} vector runs, {nvec} vectors read, {sum(kinds.values())} lines whose drawn string differs from the text', file=out)
for k, n in kinds.most_common():
    print(f'== {k}: {n} lines', file=out)
    for cur, t, d in ex[k][:12]: print(f'  {cur}: text [{t}] drawn [{d}]', file=out)
out.close()
