#!/usr/bin/env python3
"""gen_mark_table (unit 096, work/mark-exact-rounded.md revision 1; abacus #14264 (6)): the mark's census table, made
with mpmath pinned at 1.3.0 (it refuses another version). Each case "OP MODE x [y]" gets its verdict from the true value
alone: E when the true value is a decimal of at most 34 significant digits (at 300 digits and again at 1,500) (it's written beside: the core's result
marked exact must be exactly it), R otherwise. The header carries the body's sha256 (tests/mark_table.py checks it).

    <venv with mpmath==1.3.0>/bin/python scripts/gen_mark_table.py > tests/mark-table.txt
"""
import hashlib, random, sys
import mpmath
from mpmath import mp, mpf, sqrt, ln, log10, exp, sinpi, cospi, sin, cos, tan, asin, acos, atan, sinh, cosh, tanh
from mpmath import asinh, acosh, atanh, pi, factorial, binomial, floor, fabs
if mpmath.__version__ != '1.3.0':
    sys.exit('gen_mark_table: mpmath 1.3.0 is the pin (abacus #14264), this is ' + mpmath.__version__)
mp.dps = 300
r = lambda v: v * 180 / pi
U = {
 'SQRT': lambda x, m: sqrt(x), 'SQ': lambda x, m: x * x, 'INV': lambda x, m: 1 / x, 'LN': lambda x, m: ln(x),
 'LOG': lambda x, m: log10(x), 'EXP': lambda x, m: exp(x), 'TENX': lambda x, m: mpf(10) ** x,
 'SIN': lambda x, m: sinpi(x / 180) if m == 'DEG' else sin(x), 'COS': lambda x, m: cospi(x / 180) if m == 'DEG' else cos(x),
 'TAN': lambda x, m: sinpi(x / 180) / cospi(x / 180) if m == 'DEG' else tan(x),
 'ASIN': lambda x, m: r(asin(x)) if m == 'DEG' else asin(x), 'ACOS': lambda x, m: r(acos(x)) if m == 'DEG' else acos(x),
 'ATAN': lambda x, m: r(atan(x)) if m == 'DEG' else atan(x),
 'SINH': lambda x, m: sinh(x), 'COSH': lambda x, m: cosh(x), 'TANH': lambda x, m: tanh(x), 'ASINH': lambda x, m: asinh(x),
 'ACOSH': lambda x, m: acosh(x), 'ATANH': lambda x, m: atanh(x), 'FACT': lambda x, m: factorial(x),
 'TODEG': lambda x, m: r(x), 'TORAD': lambda x, m: x * pi / 180, 'ABS': lambda x, m: fabs(x),
 'TOKG': lambda x, m: x * mpf('0.45359237'), 'TOLB': lambda x, m: x / mpf('0.45359237'), 'TOC': lambda x, m: (x - 32) / mpf('1.8'),
 'TOF': lambda x, m: x * mpf('1.8') + 32, 'TOCM': lambda x, m: x * mpf('2.54'), 'TOIN': lambda x, m: x / mpf('2.54'),
}
B = {
 'ADD': lambda y, x: y + x, 'SUB': lambda y, x: y - x, 'MUL': lambda y, x: y * x, 'DIV': lambda y, x: y / x,
 'POW': lambda y, x: y ** x, 'XROOT': lambda y, x: y ** (1 / x), 'PCT': lambda y, x: y * x / 100,
 'PCTCHG': lambda y, x: (x - y) / y * 100, 'NCR': lambda y, x: binomial(y, x),
}
XS = ['0', '1', '2', '3', '4', '8', '9', '16', '27', '0.25', '0.5', '2.5', '10', '100', '0.001', '30', '45', '60', '90',
      '180', '150', '0.1', '1.5', '7', '123.456', '1E10', '-1', '-8', '0.75', '1E-20', '64', '12', '20', '1E-50', '1E-35',
      '1000']
YS = ['1', '2', '3', '4', '8', '9', '10', '27', '0.5', '64', '100', '7', '1.5', '20']
R = random.Random(95)
def rnd():
    k = R.choice(['int', 'dec', 'long', 'tiny'])
    if k == 'int': return str(R.randint(-50, 400))
    if k == 'dec': return str(R.randint(-9999, 99999)) + 'E-' + str(R.randint(1, 4))
    if k == 'long': return ''.join(R.choice('123456789') for _ in range(R.randint(10, 34))) + 'E-' + str(R.randint(0, 30))
    return '1E-' + str(R.randint(1, 40))
XR = [rnd() for _ in range(24)]
YR = [rnd() for _ in range(4)]
def exact34(v):
    """the true value as a decimal of at most 34 significant digits ("nEk"), or None"""
    if v == 0: return '0E0'
    e = int(floor(log10(fabs(v))))
    q = v * mpf(10) ** (33 - e)                          # 34 digits before the point
    n = mpmath.nint(q)
    if fabs(q - n) > mpf(10) ** -(mp.dps - 60): return None
    n, k = int(n), e - 33
    while n % 10 == 0:                                   # the fewest digits that hold it
        n //= 10; k += 1
    return f"{n}E{k}"
lines = []
def true_value(o, m, x, y):
    tv = U[o](mpf(x), m) if y is None else B[o](mpf(y), mpf(x))
    if isinstance(tv, mpmath.mpc):
        if tv.imag != 0: raise ValueError
        tv = tv.real
    return mpf(tv)
def case(o, m, x, y=None):
    try:
        tv = true_value(o, m, x, y)
        if not mpmath.isfinite(tv) or (tv != 0 and fabs(tv) < mpf(10) ** -6176): return   # the core refuses or flushes
    except (ZeroDivisionError, ValueError, OverflowError):
        return
    ex = exact34(tv)
    if ex is not None and ex != '0E0':                   # exact at 300 digits: it must hold at 1,500 too
        with mpmath.workdps(1500):
            ex = exact34(true_value(o, m, x, y))
    if ex is not None and o == 'TANH' and fabs(mpf(x)) > 1500:
        ex = None                                        # 1 - |tanh x| = 2/(e^2|x| + 1) > 0, below even 1,500 digits
    if ex is not None:                                   # the decimal128 range: 34 digits from 1E-6176 to below 1E6145
        n, e = ex.split('E')
        e = int(e)
        if ex != '0E0' and (e < -6176 or e + len(n.lstrip('-')) - 1 > 6144): ex = None
    lines.append(f"{o} {m} {x}" + (f" {y}" if y is not None else '') + (f" | E {ex}" if ex else " | R"))
for xs, ys in ((XS, YS), (XR, YR)):
    for o in U:
        for m in (['DEG', 'RAD'] if o in ('SIN', 'COS', 'TAN', 'ASIN', 'ACOS', 'ATAN') else ['DEG']):
            for x in xs: case(o, m, x)
    for o in B:
        for y in ys:
            for x in xs: case(o, 'DEG', x, y)
body = '\n'.join(lines) + '\n'
h = hashlib.sha256(body.encode()).hexdigest()
sys.stdout.write(f"# mark census (unit 096): mpmath {mpmath.__version__} at {mp.dps} digits; {len(lines)} cases\n# sha256 {h}\n" + body)
