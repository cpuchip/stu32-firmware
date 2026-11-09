"""abacus's hidden table for unit 097 (the normal distribution), 2026-11-09.
Each line is NAME x TRUE, TRUE from mpmath at 60 significant digits (computed at 200), for soroban's tests/fn_table.py
(#14274): the build's result is judged in units of its 34th digit, with a bar of 8. NAME x ERR expects a refusal.
Arguments are exact decimals of at most 34 digits, so the driver reads exactly what's here."""
import sys, os, random
from decimal import Decimal, getcontext
import mpmath as mp
sys.stdout.reconfigure(encoding='utf-8')
mp.mp.dps = 200
getcontext().prec = 60
OUT = sys.argv[1]

SQ2 = mp.sqrt(2)


def f(name, x):
    x = mp.mpf(x)
    if name == 'ERF':
        return mp.erf(x)
    if name == 'ERFC':
        return mp.erfc(x)
    if name == 'NCDF':
        return mp.erfc(-x / SQ2) / 2
    if name == 'NCDFC':
        return mp.erfc(x / SQ2) / 2
    if name == 'NCDFINV':
        return SQ2 * mp.erfinv(2 * x - 1)
    raise ValueError(name)


def s60(v):
    return mp.nstr(v, 60, min_fixed=-1, max_fixed=-1) if v != 0 else '0'


R = random.Random(97)


def rnd(lo, hi):
    """a 34-digit decimal in [lo, hi), exact as text"""
    d = Decimal(lo) + (Decimal(hi) - Decimal(lo)) * Decimal(R.randrange(10 ** 33)) / Decimal(10 ** 33)
    return format(d.quantize(Decimal(1).scaleb(-30)) if abs(d) >= 1 else d.quantize(Decimal(1).scaleb(-33)), 'f')


L = ["# abacus's hidden table for unit 097 (the normal distribution), 2026-11-09: NAME x TRUE (mpmath 1.3.0 at 200 digits,",
     "# written to 60), or NAME x ERR for a refusal. Judged by tests/fn_table.py at 8 units of the 34th digit (#14274).",
     "# Built by herfgen.py (seed 97), from mpmath, never from the firmware's code."]
fixed = {
    'ERF': ['0', '0.5', '1', '-1', '2', '3.5', '-4.25', '6', '0.000001', '1E-30'],
    'ERFC': ['0', '0.5', '1', '-1', '2', '5', '10', '20', '26.5', '-3'],
    'NCDF': ['0', '1', '-1', '1.96', '-1.96', '2.326347874040841', '3', '-3', '-8.5', '-20', '-27', '-38.5', '7.04', '-7.04'],
    'NCDFC': ['0', '1', '1.96', '3', '8.5', '20', '27', '38.5', '-2'],
    'NCDFINV': ['0.5', '0.975', '0.025', '0.95', '0.05', '0.999999', '0.000001', '1E-10', '1E-100', '0.84134474606854293'],
}
for name, xs in fixed.items():
    for x in xs:
        L.append(f"{name} {x} {s60(f(name, x))}")
for name, lo, hi, n in [('ERF', -6, 6, 20), ('ERFC', -3, 27, 20), ('NCDF', -39, 39, 30), ('NCDFC', -39, 39, 30)]:
    for _ in range(n):
        x = rnd(lo, hi)
        L.append(f"{name} {x} {s60(f(name, x))}")
for _ in range(20):
    p = rnd(0, 1)
    if Decimal(p) == 0:
        continue
    L.append(f"NCDFINV {p} {s60(f('NCDFINV', p))}")
for x in ['0', '1', '1.5', '-0.1']:
    L.append(f"NCDFINV {x} ERR")
open(OUT, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
print('lines', len(L))
