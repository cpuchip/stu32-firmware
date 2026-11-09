#!/usr/bin/env python3
"""Unit 097: Phi and Q as built (fn.c's fn_ncdf through build/fn_drive's NCDF and NCDFC), over revision 1's 5,132 z: every
0.01 on [-8, 8], 1,500 random on it, the integers 9 to 39 on each side, and 2,000 random 34-digit z in the tails (8 to 39
each side; seed 971 here, the blueprint's were phifix.c's), against mpmath 1.3.0 at 200 digits. The worst units each.
    <mpmath 1.3.0 venv>/bin/python work/097-evidence/ncdf_census.py build/fn_drive"""
import random, subprocess, sys
import mpmath
from mpmath import mp, mpf, erfc, sqrt
assert mpmath.__version__ == '1.3.0'
mp.dps = 200
R = random.Random(971)
d34 = lambda v: mpmath.nstr(v, 34, min_fixed=-30, max_fixed=30)
zs = [str(mpf(k) / 100) if k % 100 else str(k // 100) for k in range(-800, 801)]
zs += [d34(mpf(R.random()) * 16 - 8) for _ in range(1500)]
zs += [str(k) for k in range(9, 40)] + [str(-k) for k in range(9, 40)]
zs += [d34((mpf(R.random()) * 31 + 8) * R.choice([-1, 1])) for _ in range(2000)]
inp = ''.join(f'NCDF DEG {z}\nNCDFC DEG {z}\n' for z in zs)
out = subprocess.run([sys.argv[1]], input=inp, capture_output=True, text=True, check=True).stdout.split('\n')
worst = {'NCDF': (0, ''), 'NCDFC': (0, '')}
over = 0
for i, z in enumerate(zs):
    for j, (name, sign) in enumerate((('NCDF', -1), ('NCDFC', 1))):
        t = erfc(sign * mpf(z) / sqrt(2)) / 2
        g = out[2 * i + j]
        if g.startswith('ERR'): print('REFUSED', name, z, g); over += 1; continue
        u = abs(mpf(g) - t) / mpf(10) ** (mpmath.floor(mpmath.log10(t)) - 33)
        if u > worst[name][0]: worst[name] = (u, z)
        if u > 8: over += 1; print('OVER', name, z, mpmath.nstr(u, 4))
print(f'{len(zs)} z: Phi the worst {mpmath.nstr(worst["NCDF"][0], 3)} units (z = {worst["NCDF"][1]}), '
      f'Q {mpmath.nstr(worst["NCDFC"][0], 3)} (z = {worst["NCDFC"][1]}); {over} past 8 or refused')
