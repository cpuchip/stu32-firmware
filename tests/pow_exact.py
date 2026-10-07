"""pow_exact: y^x and the x-th root of y correctly rounded, digit for digit, against mpmath at 130 digits (unit 034's
review: a result formed to a fixed precision and rounded once is not correctly rounded near a rounding midpoint;
fn.c's Ziv loop forms it again until the rounding is sure). Run on build/fn_drive and on build/fn_drive_p37, whose
first precision is 37 digits, so that many roundings are unsure and the loop must decide them (without the loop that
build misrounds about a third of these cases).

Usage: python tests/pow_exact.py <fn_drive> [seed] [cases]. Needs mpmath.
"""
import decimal
import random
import subprocess
import sys

import mpmath as mp

mp.mp.dps = 130
D = decimal.Context(prec=34, rounding=decimal.ROUND_HALF_EVEN, Emax=6144, Emin=-6143)
drive = sys.argv[1] if len(sys.argv) > 1 else "build/fn_drive"
R = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
N = int(sys.argv[3]) if len(sys.argv) > 3 else 4000


def num(lo, hi):
    return f"{R.randint(1, 9)}.{''.join(str(R.randint(0, 9)) for _ in range(33))}E{R.randint(lo, hi)}"


cases = []                                      # (name, x, y, truth)
for _ in range(N):
    y, x = num(-20, 20), f"{R.uniform(-30, 30):.30f}"
    cases.append(("POW", x, y, mp.power(mp.mpf(y), mp.mpf(x))))
for _ in range(N // 4):
    y, x = num(-50, 50), f"{R.uniform(1.5, 40):.25f}"
    cases.append(("XROOT", x, y, mp.power(mp.mpf(y), 1 / mp.mpf(x))))
out = subprocess.run([drive], input="\n".join(f"{n} RAD {x} {y}" for n, x, y, _ in cases) + "\n",
                     capture_output=True, text=True, check=True).stdout.split("\n")
bad, first = 0, None
for (n, x, y, t), g in zip(cases, out):
    w = D.create_decimal(mp.nstr(t, 115, min_fixed=-mp.inf, max_fixed=mp.inf))     # past the deciding digit
    if g.startswith("ERR") or D.create_decimal(g) != w:
        bad += 1
        first = first or f"{n} y={y} x={x}: got {g}, want {w}"
print(f"pow_exact {drive}: {len(cases)} cases, {len(cases) - bad} correctly rounded" + (f"  FAIL\n    {first}" if bad else ""))
sys.exit(1 if bad else 0)
