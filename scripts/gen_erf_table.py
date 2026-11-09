#!/usr/bin/env python3
"""gen_erf_table (unit 097, work/normal-distribution.md revision 1): soroban's own accuracy table for ERF, ERFC, NCDF
(Phi), NCDFC (Q) and NCDFINV (Phi^-1), in the form of abacus's frozen herf-table: lines "NAME x TRUE", TRUE from mpmath
pinned at 1.3.0 (refused otherwise) at 200 digits, written to 60 significant digits in mpmath's form; "NAME x ERR" where
the function refuses x. tests/fn_table.py reads it through build/fn_drive with the 8-unit bar.

    <venv with mpmath==1.3.0>/bin/python scripts/gen_erf_table.py > tests/erf-table.txt
"""
import random, sys
import mpmath
from mpmath import mp, mpf, erf, erfc, sqrt
if mpmath.__version__ != '1.3.0':
    sys.exit('gen_erf_table: mpmath 1.3.0 is the pin (abacus #14264), this is ' + mpmath.__version__)
mp.dps = 200
PHI = lambda z: erfc(-z / sqrt(2)) / 2
Q = lambda z: erfc(z / sqrt(2)) / 2
def phiinv(p):
    """Phi^-1(p) at 200 digits: through erfinv in the middle; in the tails by Newton on log Phi (relative, so p down to
    1E-6000 converges; findroot's tolerance is absolute, which a tiny p meets anywhere), then checked: Phi(z) gives p back
    to 1E-150 relative (the generator refuses otherwise)"""
    if mpf('1e-30') < p < 1 - mpf('1e-30'):
        z = -sqrt(2) * mpmath.erfinv(1 - 2 * p)
    else:
        lo = p if p < 0.5 else 1 - p
        z = -sqrt(2 * mpmath.log(1 / lo))
        for _ in range(200):
            f = mpmath.log(Q(-z)) - mpmath.log(lo)            # log Phi(z) - log lo, z < 0
            d = mpmath.exp(-z * z / 2) / sqrt(2 * mpmath.pi) / Q(-z)   # (log Phi)' = phi / Phi
            z2 = z - f / d
            if abs(z2 - z) < mpf(10) ** -185: z = z2; break
            z = z2
        if p > 0.5: z = -z
    back = erfc(-z / sqrt(2)) / 2
    assert abs(back - p) / p < mpf(10) ** -150, ('phiinv', p)
    return z
F = {'ERF': erf, 'ERFC': erfc, 'NCDF': PHI, 'NCDFC': Q}
R = random.Random(97)
def r34(lo, hi):
    """a random decimal of at most 34 significant digits in [lo, hi]"""
    v = mpf(lo) + (mpf(hi) - mpf(lo)) * mpf(R.random())
    return mpmath.nstr(v, R.randint(3, 34), strip_zeros=True, min_fixed=-30, max_fixed=30).replace('e', 'E')
lines = []
def add(name, x, val=None):
    x = x.replace('e', 'E')
    if val is None:
        lines.append(f'{name} {x} ERR')
    else:
        lines.append(f'{name} {x} {mpmath.nstr(val, 60, min_fixed=-5, max_fixed=5)}')
fixed = ['0', '0.5', '1', '1.5', '1.96', '1.645', '2', '2.33', '2.5', '3', '4', '5', '6', '0.001', '1E-10', '1E-30', '1E-60']
for name in ('ERF', 'ERFC', 'NCDF', 'NCDFC'):
    for x in fixed + ['-' + v for v in fixed if v != '0']:
        add(name, x, F[name](mpf(x)))
for z in ['7.04', '8', '10', '13.4', '20', '24.86', '27', '30', '36.61', '38.27', '38.5', '38.86']:
    for name in ('NCDF', 'NCDFC'):
        add(name, z, F[name](mpf(z)))
        add(name, '-' + z, F[name](-mpf(z)))
for x in ['10', '13.4', '20', '26.5']:
    add('ERFC', x, erfc(mpf(x)))
for _ in range(120):
    x = r34(-6, 6); add('ERF', x, erf(mpf(x)))
    x = r34(-6, 26.5); add('ERFC', x, erfc(mpf(x)))
    z = r34(-38.5, 38.5); add('NCDF', z, PHI(mpf(z)))
    z = r34(-38.5, 38.5); add('NCDFC', z, Q(mpf(z)))
ps = ['1E-100', '1E-50', '1E-20', '1E-10', '0.000001', '0.001', '0.01', '0.025', '0.05', '0.0999999', '0.1', '0.2',
      '0.25', '0.3', '0.4', '0.45', '0.4999999', '0.5', '0.5000000001', '0.6', '0.75', '0.8', '0.9', '0.9000001', '0.95',
      '0.975', '0.99', '0.999', '0.999999', '0.9999999999', '1E-1000', '1E-6000']
for p in ps:
    add('NCDFINV', p, phiinv(mpf(p)))
for _ in range(120):
    k = R.random()
    p = r34(0, 1) if k < 0.6 else ('1E-' + str(R.randint(2, 300)) if k < 0.8 else '0.' + '9' * R.randint(2, 30) + str(R.randint(0, 8)))
    if mpf(p) <= 0 or mpf(p) >= 1: continue
    add('NCDFINV', p, phiinv(mpf(p)))
for p in ['0', '1', '1.5', '-0.1']:
    add('NCDFINV', p)
sys.stdout.write('# unit 097: soroban\'s erf/normal-distribution table (scripts/gen_erf_table.py), mpmath %s at 200 digits, TRUE to 60\n# %d lines\n' % (mpmath.__version__, len(lines)) + '\n'.join(lines) + '\n')
