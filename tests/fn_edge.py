"""fn_edge: unit 044's review, finding 2. The correctly rounded functions at the range's edges against mpmath, the
exact value rounded once as decimal128 rounds (34 digits, subnormals to 1E-6176: the engine's convention, whose
multiply gives 1E-6143 x 0.99...96 as 1E-6143): OVERFLOW past the largest, UNDERFLOW when the rounding is nonzero and
below 1E-6143, the value otherwise.

    fn_edge.py DRIVE N SEED     N arguments per function, edge and spread; exit 1 on any wrong
"""
import random, subprocess, sys
import mpmath as mp
from decimal import Decimal, Context, ROUND_HALF_EVEN
mp.mp.dps = 120
C = Context(prec=34, rounding=ROUND_HALF_EVEN, Emax=999999, Emin=-999999)
DRIVE, N, R = sys.argv[1], int(sys.argv[2]), random.Random(int(sys.argv[3]))
LN10 = mp.log(10)
def near(v, spread):            # a 34-digit argument near v
    w = v * (1 + mp.mpf(R.uniform(-1, 1)) * spread)
    return str(C.plus(Decimal(mp.nstr(w, 50))))
F = {'EXP': (mp.exp, [mp.mpf(6145) * LN10, mp.mpf(-6143) * LN10]),
     'TENX': (lambda x: mp.power(10, x), [mp.mpf(6145), mp.mpf(-6143)]),
     'SINH': (mp.sinh, [mp.mpf(6145) * LN10 + mp.log(2)]),
     'COSH': (mp.cosh, [mp.mpf(6145) * LN10 + mp.log(2)]),
     'TODEG': (lambda x: x * 180 / mp.pi, [mp.mpf('1E6145') * mp.pi / 180]),
     'TORAD': (lambda x: x * mp.pi / 180, [mp.mpf('1E6145') * 180 / mp.pi, mp.mpf('1E-6143') * 180 / mp.pi])}
rows = [('TORAD', '5.729577951308232087679815481410514E-6142')]
for name, (f, cs) in F.items():
    for c in cs:
        for spread in ('1E-30', '1E-33', '1E-34', '1E-36'):
            for _ in range(N):
                a = near(c, mp.mpf(spread))
                if Decimal('1E-6143') <= Decimal(a).copy_abs() < Decimal('1E6145'):    # typeable
                    rows.append((name, a))
out = subprocess.run([DRIVE], input=''.join(f"{n} RAD {x}\n" for n, x in rows), capture_output=True,
                     text=True).stdout.split('\n')
bad = 0
for (n, x), got in zip(rows, out):
    if not got or got[0] not in '+-E':
        bad += 1
        print('ODD OUTPUT', n, x, repr(got))
        continue
    v = F[n][0](mp.mpf(x))
    D = Context(prec=34, rounding=ROUND_HALF_EVEN, Emax=6144, Emin=-6143, traps=[])
    r = D.plus(Decimal(mp.nstr(v, 60)))
    want = 'OVERFLOW' if r.is_infinite() else 'UNDERFLOW' if r != 0 and r.copy_abs() < Decimal('1E-6143') else r
    if isinstance(want, str):
        ok = got == {'OVERFLOW': 'ERR 22', 'UNDERFLOW': 'ERR 23'}[want]
    else:
        ok = not got.startswith('ERR') and Decimal(got) == want
    if not ok:
        bad += 1
        if bad <= 8: print('WRONG', n, x, 'got', got, 'want', want)
print(f'fn_edge: {len(rows)} arguments, {bad} wrong')
sys.exit(1 if bad or len(out) - 1 != len(rows) else 0)
