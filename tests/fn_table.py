#!/usr/bin/env python3
"""fn_table (unit 097, abacus #14274, #14276): a table of "NAME x TRUE" lines (TRUE in decimal or exponent form, mpmath's
at 60 digits) run through build/fn_drive ("NAME DEG x"), each answer's error in units of the 34th digit of TRUE, held to
the house bar (8 units, unit 003 amendment 1). "NAME x ERR" passes only on a refusal (fn_drive's "ERR status"); a refusal
where TRUE is a number fails. # lines and blank lines are skipped. stdlib only (decimal), so it runs in make test.

    python3 tests/fn_table.py build/fn_drive TABLE              the check: exit 1 on any line past the bar
    python3 tests/fn_table.py build/fn_drive TABLE --control    the bar's own control: one line's TRUE moved by 1,000
                                                                units in a copy must fail (exit 0 when it does)
"""
import subprocess, sys
from decimal import Decimal, getcontext
getcontext().prec = 120
BAR = Decimal(8)
drive, table = sys.argv[1], sys.argv[2]
control = '--control' in sys.argv[3:]
cases = []
for n, l in enumerate(open(table, encoding='utf-8'), 1):
    l = l.strip()
    if not l or l.startswith('#'): continue
    f = l.split()
    if len(f) != 3: sys.exit(f'fn_table: {table}:{n}: not "NAME x TRUE": {l!r}')
    cases.append((n, f[0], f[1], f[2]))
if control:                                         # the first value line's TRUE moved by 1,000 units of its 34th digit
    for i, (n, name, x, t) in enumerate(cases):
        if t != 'ERR' and Decimal(t) != 0:
            v = Decimal(t)
            cases[i] = (n, name, x, str(v + Decimal(1000) * Decimal(10) ** (v.adjusted() - 33)))
            break
out = subprocess.run([drive], input=''.join(f'{c[1]} DEG {c[2]}\n' for c in cases), capture_output=True, text=True,
                     check=True).stdout.split('\n')
bad, worst, byname = [], {}, {}
for (n, name, x, t), got in zip(cases, out):
    byname[name] = byname.get(name, 0) + 1
    if t == 'ERR':
        if not got.startswith('ERR'): bad.append(f'{table}:{n}: {name} {x}: wanted a refusal, got {got}')
        continue
    if got.startswith('ERR') or got == 'BAD':
        bad.append(f'{table}:{n}: {name} {x}: {got}, wanted {t}')
        continue
    v, g = Decimal(t), Decimal(got)
    u = abs(g - v) if v == 0 else abs(g - v) / Decimal(10) ** (v.adjusted() - 33)
    if u > worst.get(name, (Decimal(-1),))[0]: worst[name] = (u, x)
    if u > BAR: bad.append(f'{table}:{n}: {name} {x}: {got}, {u:.2f} units off {t}')
if len(out) < len(cases): sys.exit(f'fn_table: {len(cases)} cases, {len(out)} answers')
w = ', '.join(f'{k} {worst[k][0]:.2f} (x = {worst[k][1]})' for k in sorted(worst))
if control:
    print(f'fn_table --control: {table} with one TRUE moved 1,000 units: ' + ('failed, as it must' if bad else 'PASSED: the bar checks nothing'))
    sys.exit(0 if bad else 1)
print(f'fn_table: {table}: {len(cases)} lines ({", ".join(f"{k} {v}" for k, v in sorted(byname.items()))}), '
      f'{len(cases) - len(bad)} within {BAR} units; the worst: {w}')
for b in bad[:40]: print('  FAIL ' + b)
sys.exit(1 if bad else 0)
