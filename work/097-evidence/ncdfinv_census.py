#!/usr/bin/env python3
"""Unit 097's measurement before Phi^-1 is kept (revision 1: the house bar over (0, 1), its tails included): 5,000 p
(seed 970) in five shapes - uniform 34-digit p in (0, 1); 1E-k for k to 6000; p near 0.5 (0.5 +- 1E-k); 1 - 1E-k; and
34-digit p near the branch points 0.1 and 0.9 - through build/fn_drive's NCDFINV, against mpmath 1.3.0 at 200 digits (the
reference Newton on log Phi in the tails, checked by putting it back through Phi). Prints the worst units by shape.
    <mpmath 1.3.0 venv>/bin/python work/097-evidence/ncdfinv_census.py build/fn_drive"""
import random, subprocess, sys
import mpmath
from mpmath import mp, mpf, erfc, sqrt
assert mpmath.__version__ == '1.3.0'
mp.dps = 200
Q = lambda z: erfc(z / sqrt(2)) / 2
def phiinv(p):
    if mpf('1e-30') < p < 1 - mpf('1e-30'):
        z = -sqrt(2) * mpmath.erfinv(1 - 2 * p)
    else:
        lo = p if p < 0.5 else 1 - p
        z = -sqrt(2 * mpmath.log(1 / lo))
        for _ in range(200):
            f = mpmath.log(Q(-z)) - mpmath.log(lo)
            d = mpmath.exp(-z * z / 2) / sqrt(2 * mpmath.pi) / Q(-z)
            z2 = z - f / d
            if abs(z2 - z) < mpf(10) ** -185: z = z2; break
            z = z2
        if p > 0.5: z = -z
    assert abs(erfc(-z / sqrt(2)) / 2 - p) / p < mpf(10) ** -150
    return z
R = random.Random(970)
def d34(v): return mpmath.nstr(v, 34, min_fixed=-30, max_fixed=30).replace('e', 'E')
cases = []
for _ in range(1000): cases.append(('uniform', d34(mpf(R.random()) * (1 - mpf(10) ** -30) + mpf(10) ** -31)))
for _ in range(1000): cases.append(('1E-k', f'{R.randint(1, 9)}E-{R.randint(2, 6000)}'))
for _ in range(1000):
    k = R.randint(2, 33); cases.append(('near 0.5', d34(mpf('0.5') + R.choice([-1, 1]) * R.randint(1, 9) * mpf(10) ** -k)))
for _ in range(1000):
    k = R.randint(2, 33); cases.append(('1 - 1E-k', d34(1 - R.randint(1, 9) * mpf(10) ** -k)))
for _ in range(1000):
    c = R.choice(['0.1', '0.9']); k = R.randint(5, 33)
    cases.append(('near 0.1, 0.9', d34(mpf(c) + R.choice([-1, 1]) * R.randint(1, 99) * mpf(10) ** -k)))
out = subprocess.run([sys.argv[1]], input=''.join(f'NCDFINV DEG {p}\n' for _, p in cases), capture_output=True,
                     text=True, check=True).stdout.split('\n')
worst, n, over = {}, {}, 0
for (shape, p), got in zip(cases, out):
    n[shape] = n.get(shape, 0) + 1
    t = phiinv(mpf(p))
    if got.startswith('ERR'): print('REFUSED', p, got); over += 1; continue
    g = mpf(got)
    u = abs(g - t) / mpf(10) ** (mpmath.floor(mpmath.log10(abs(t))) - 33)
    if u > worst.get(shape, (-1,))[0]: worst[shape] = (u, p)
    if u > 8: over += 1; print('OVER', p, got, mpmath.nstr(u, 4))
for s in n: print(f'{s}: {n[s]} p, the worst {mpmath.nstr(worst[s][0], 3)} units (p = {worst[s][1]})')
print(f'{len(cases)} p, {over} past 8 units or refused')
