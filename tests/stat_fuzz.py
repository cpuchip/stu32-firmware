#!/usr/bin/env python3
"""stat_fuzz: random data sets typed through the calculator, every statistic checked against
tests/stat_ref.py. Writes a vector file to stdout: python3 tests/stat_fuzz.py SEED COUNT > f; build/vectors f
(needs mpmath; `make accuracy` runs it)."""
import random, sys
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stat_ref import *
R0 = random.Random(int(sys.argv[1]))
N = int(sys.argv[2])
def rnum(kind):
    """A decimal string: mantissa digits, optional point, optional exponent."""
    if kind == "close":
        return str(R0.randint(10**12, 10**12 + 50))
    sign = "-" if R0.random() < 0.4 else ""
    nd = R0.randint(1, 10 if kind == "small" else 34)
    digits = str(R0.randint(1, 9)) + "".join(str(R0.randint(0, 9)) for _ in range(nd - 1))
    pt = R0.randint(1, nd)
    m = digits[:pt] + ("." + digits[pt:] if pt < nd else "")
    e = R0.randint(-20, 20) if kind == "wide" else 0
    return sign + m + (f"E{e}" if e else "")

def keys_for(v):
    """The keystrokes that type v: mantissa, E and exponent, CHS where negative."""
    neg = v.startswith("-")
    v = v.lstrip("-")
    if "E" in v:
        m, e = v.split("E")
        out = [m] + (["CHS"] if neg else []) + ["E", e.lstrip("-")] + (["CHS"] if e.startswith("-") else [])
    else:
        out = [v] + (["CHS"] if neg else [])
    return out

lines = []
for t in range(N):
    kind = R0.choice(["close", "small", "wide", "any"])
    pts = [(rnum(kind), rnum(kind)) for _ in range(R0.randint(2, 8))]
    R = Reg(); toks = []
    for x, y in pts:
        toks += keys_for(y) + ["ENTER"] + keys_for(x) + ["SIGMA+"]
        R.acc(x, y)
    if R0.random() < 0.3:          # remove one point again
        x, y = pts[R0.randrange(len(pts))]
        toks += keys_for(y) + ["ENTER"] + keys_for(x) + ["SIGMA-"]
        R.acc(x, y, remove=True)
    k = " ".join(toks)
    n, sx, sy, sx2, sy2, sxy = R.fr()
    def emit(stat, val):
        lines.append(f"Z{t}{stat.split()[-1]} | fuzz | {k} {stat} | {val}")
    try:
        if n != 0: emit("XBAR", "X=" + str(r34(mean(R)))); emit("YBAR", "X=" + str(r34(mean(R, True))))
        if sy != 0: emit("XWBAR", "X=" + str(r34(wmean(R))))
        Dx = n * sx2 - sx * sx; Dy = n * sy2 - sy * sy
        if n > 1 and Dx >= 0: emit("SX", "X=" + sqrt34(var(R)))
        if n != 0 and Dx >= 0: emit("PSX", "X=" + sqrt34(var(R, pop=True)))
        if Dx != 0:
            emit("SLOPE", "X=" + str(r34(slope(R)))); emit("YINT", "X=" + str(r34(yint(R))))
            emit("0 YHAT", "X=" + str(r34(yhat(R, "0"))))
        if Dx > 0 and Dy > 0:
            c, rr = corr2(R); emit("CORR", "X=" + sqrt34(rr, 1 if c > 0 else -1) if c != 0 else "X=0")
    except ZeroDivisionError:
        pass
print("\n".join(lines))
