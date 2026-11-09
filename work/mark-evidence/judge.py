"""THROWAWAY (decision 95's mark): cases over the core's real functions, the core's flag (census) against mpmath's truth:
exact means the decimal128 result equals the true value (80 digits)."""
import subprocess, sys, itertools
from mpmath import mp, mpf, sinpi, cospi, sqrt, cbrt, ln, log10, exp, sin, cos, tan, asin, acos, atan, sinh, cosh, tanh, asinh, acosh, atanh, pi, factorial, binomial, floor
mp.dps = 300
d = lambda v: v * pi / 180
r = lambda v: v * 180 / pi
U = {  # name: (f(x, mode))
 'SQRT': lambda x, m: sqrt(x), 'SQ': lambda x, m: x * x, 'INV': lambda x, m: 1 / x, 'LN': lambda x, m: ln(x),
 'LOG': lambda x, m: log10(x), 'EXP': lambda x, m: exp(x), 'TENX': lambda x, m: mpf(10) ** x,
 'SIN': lambda x, m: sinpi(x / 180) if m == 'DEG' else sin(x), 'COS': lambda x, m: cospi(x / 180) if m == 'DEG' else cos(x),
 'TAN': lambda x, m: sinpi(x / 180) / cospi(x / 180) if m == 'DEG' else tan(x),
 'ASIN': lambda x, m: r(asin(x)) if m == 'DEG' else asin(x), 'ACOS': lambda x, m: r(acos(x)) if m == 'DEG' else acos(x),
 'ATAN': lambda x, m: r(atan(x)) if m == 'DEG' else atan(x),
 'SINH': lambda x, m: sinh(x), 'COSH': lambda x, m: cosh(x), 'TANH': lambda x, m: tanh(x), 'ASINH': lambda x, m: asinh(x),
 'ACOSH': lambda x, m: acosh(x), 'ATANH': lambda x, m: atanh(x), 'FACT': lambda x, m: factorial(x),
 'TODEG': lambda x, m: r(x), 'TORAD': lambda x, m: d(x), 'ABS': lambda x, m: abs(x), 'IP': lambda x, m: mpf(int(x)),
 'FP': lambda x, m: x - int(x),
 'TOKG': lambda x, m: x * mpf('0.45359237'), 'TOLB': lambda x, m: x / mpf('0.45359237'), 'TOC': lambda x, m: (x - 32) / mpf('1.8'),
 'TOF': lambda x, m: x * mpf('1.8') + 32, 'TOCM': lambda x, m: x * mpf('2.54'), 'TOIN': lambda x, m: x / mpf('2.54'),
}
B = {  # name: f(y, x): HP order, y the first entered
 'ADD': lambda y, x: y + x, 'SUB': lambda y, x: y - x, 'MUL': lambda y, x: y * x, 'DIV': lambda y, x: y / x,
 'POW': lambda y, x: y ** x, 'XROOT': lambda y, x: y ** (1 / x), 'PCT': lambda y, x: y * x / 100,
 'PCTCHG': lambda y, x: (x - y) / y * 100, 'NCR': lambda y, x: binomial(y, x),
}
XS = ['0', '1', '2', '3', '4', '8', '9', '16', '27', '0.25', '0.5', '2.5', '10', '100', '0.001', '30', '45', '60', '90',
      '180', '150', '0.1', '1.5', '7', '123.456', '1E10', '-1', '-8', '0.75', '1E-20', '64', '12', '20']
YS = ['1', '2', '3', '4', '8', '9', '10', '27', '0.5', '64', '100', '7', '1.5', '20']
import random
R = random.Random(95)
def rnd():
    k = R.choice(['int', 'dec', 'long', 'tiny'])
    if k == 'int': return str(R.randint(-50, 400))
    if k == 'dec': return str(R.randint(-9999, 99999)) + 'E-' + str(R.randint(1, 4))
    if k == 'long': return ''.join(R.choice('123456789') for _ in range(R.randint(10, 34))) + 'E-' + str(R.randint(0, 30))
    return '1E-' + str(R.randint(1, 40))
if len(sys.argv) > 3:
    XS = [rnd() for _ in range(int(sys.argv[3]))]
    YS = [rnd() for _ in range(max(4, int(sys.argv[3]) // 8))]
cases = []
for op in U:
    for m in (['DEG', 'RAD'] if op in ('SIN', 'COS', 'TAN', 'ASIN', 'ACOS', 'ATAN') else ['DEG']):
        for x in XS: cases.append((op, m, x, None))
for op in B:
    for y in YS:
        for x in XS: cases.append((op, 'DEG', x, y))
txt = '\n'.join(f"{o} {m} {x}" + (f" {y}" if y else '') for o, m, x, y in cases) + '\n'
open(sys.argv[2], 'w').write(txt)
out = subprocess.run([sys.argv[1], sys.argv[2]], capture_output=True, text=True).stdout.split('\n')
tally = {'clamped': 0, 'ok_exact': 0, 'ok_rounded': 0, 'false_exact': 0, 'false_rounded': 0, 'error': 0, 'skip': 0}
fe, fr = [], []
for (o, m, x, y), line in zip(cases, out):
    p = line.split()
    if len(p) != 3: tally['skip'] += 1; continue
    st, rnd, res = int(p[0]), p[1] == '1', p[2]
    if st != 0: tally['error'] += 1; continue
    try:
        tv = U[o](mpf(x), m) if y is None else B[o](mpf(y), mpf(x))
        if isinstance(tv, complex) or hasattr(tv, 'imag') and tv.imag != 0: tally['skip'] += 1; continue
        tv = mpf(tv)
    except Exception:
        tally['skip'] += 1; continue
    rv = mpf(res.replace('E', 'e'))
    if res.lstrip('+-') == '9999999999999999999999999999999999E+6111': tally['clamped'] += 1; continue
    exact = (tv == 0 and rv == 0) or (tv != 0 and abs(rv - tv) / abs(tv) < mpf('1e-250'))
    k = ('ok_rounded' if rnd else 'false_exact') if not exact else ('false_rounded' if rnd else 'ok_exact')
    tally[k] += 1
    if k == 'false_exact': fe.append(f"{o} {m} x={x} y={y} -> {res}")
    if k == 'false_rounded': fr.append(f"{o} {m} x={x} y={y} -> {res}")
print(len(cases), 'cases;', tally)
import collections
print('FALSE EXACT by op:', dict(collections.Counter(x.split()[0] + ' ' + x.split()[1] for x in fe)))
print('FALSE EXACT:', *fe[:40], sep='\n  ')
print('NONZERO FALSE EXACT:', *[x for x in fe if not x.endswith('+0E-6176') and not x.endswith('-0E-6176')][:30], sep='\n  ')
print('FALSE ROUNDED (%d):' % len(fr), *fr[:60], sep='\n  ')
# per op: truly exact results, and how many of those the flag called rounded
per = {}
for (o, m, x, y), line in zip(cases, out):
    p = line.split()
    if len(p) != 3 or int(p[0]) != 0: continue
    try:
        tv = U[o](mpf(x), m) if y is None else B[o](mpf(y), mpf(x)); tv = mpf(tv)
    except Exception: continue
    rv = mpf(p[2].replace('E', 'e'))
    exact = (tv == 0 and rv == 0) or (tv != 0 and abs(rv - tv) / abs(tv) < mpf('1e-250'))
    if exact:
        a = per.setdefault(o, [0, 0]); a[0] += 1; a[1] += p[1] == '1'
print('per op, exact results: called rounded / all exact')
print('  ' + ', '.join(f"{o} {v[1]}/{v[0]}" for o, v in sorted(per.items(), key=lambda kv: -kv[1][1]) if v[1]))
