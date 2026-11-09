#!/usr/bin/env python3
"""mark_table (unit 096, work/mark-exact-rounded.md): the mark's census, judged. Runs every case of the checked-in
table (scripts/gen_mark_table.py, mpmath 1.3.0; its sha256 checked first) through build/mark_drive and compares.
  - false exact (the core marks exact where the true value isn't a 34-digit decimal, or where its value differs from
    the true one): a hard fail, every one;
  - false rounded (marked rounded where it's exact): allowed up to BOUND per op, 0 for any op not named (the mark errs
    toward ≈; the bounds are the measured counts at the build, so a change that loses an exact case fails);
  - a refused case (status not 0) is counted, not judged.
stdlib only.        python3 tests/mark_table.py build/mark_drive tests/mark-table.txt"""
import hashlib, subprocess, sys
from collections import Counter
from decimal import Decimal
# XROOT and POW at an index or exponent that isn't a whole number's reciprocal (y^(1/0.25) = y^4, 4^2.5 = 32): the
# back-check is scoped to roots of index 1 to 99 (work/mark-exact-rounded.md, its question 2, accepted)
BOUND = {'XROOT': 60, 'POW': 8}
drive, table = sys.argv[1], sys.argv[2]
raw = open(table, encoding='utf-8').read()
head, body = raw.split('\n', 2)[:2], raw.split('\n', 2)[2]
want = head[1].split()[-1]
got = hashlib.sha256(body.encode()).hexdigest()
if got != want:
    sys.exit(f'mark_table: {table} body sha256 {got}, its header says {want}')
cases = [l.split(' | ') for l in body.splitlines()]
out = subprocess.run([drive], input='\n'.join(c[0] for c in cases) + '\n', capture_output=True, text=True,
                     check=True).stdout.splitlines()
if len(out) != len(cases):
    sys.exit(f'mark_table: {len(cases)} cases, {len(out)} answers')
fe, fr, refused, good = [], Counter(), 0, 0
for (case, verdict), ans in zip(cases, out):
    st, bit, val = ans.split()
    if st != '0':
        refused += 1
        continue
    v = verdict.split()
    if bit == 'E' and (v[0] == 'R' or Decimal(val) != Decimal(v[1])):
        fe.append(f'{case}: marked exact, {val}; true {verdict}')
    elif bit == 'R' and v[0] == 'E':
        fr[case.split()[0]] += 1
    else:
        good += 1
print(f'mark_table: {len(cases)} cases (table sha256 {want[:12]}...): {good} marked right, {sum(fr.values())} false '
      f'rounded, {len(fe)} false exact, {refused} refused')
if fr:
    print('  false rounded by op: ' + ', '.join(f'{k} {n}' for k, n in sorted(fr.items(), key=lambda t: -t[1])))
for l in fe[:40]:
    print('  FALSE EXACT ' + l)
over = [f'{k} {n} (bound {BOUND.get(k, 0)})' for k, n in sorted(fr.items()) if n > BOUND.get(k, 0)]
if over:
    print('  FALSE ROUNDED OVER THE BOUND: ' + ', '.join(over))
sys.exit(1 if fe or over else 0)
