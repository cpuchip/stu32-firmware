#!/usr/bin/env python3
"""fn_fuzz: unit 003 functions against mpmath, in units of the 34th significant digit.

    python3 tests/fn_fuzz.py build/fn_drive [count-per-case] [seed]      (needs mpmath)

For every function and angle mode, random 34-digit arguments across the domain (including
near-exact angles and huge radian arguments) go through build/fn_drive; each result is compared
with mpmath at 150 digits on the exact decimal argument. Prints the largest error per case and
fails if any exceeds the bar: 8 units (unit 003 amendment 1), 0.5 for sqrt and the arithmetic
of SQ, INV, PCT (PCTCHG is three operations: 8). Not part of `make test` because it needs mpmath; run with `make accuracy`.
"""
import random
import subprocess
import sys

try:
    import mpmath as mp
except ImportError:
    print("fn_fuzz: mpmath is not installed; skipped")
    sys.exit(0)

mp.mp.dps = 150
from decimal import Decimal, getcontext
getcontext().prec = 200


def exact_trig(name, xs, turn):
    """sin/cos/tan of xs degrees or grads: the angle is reduced exactly in decimal first, so
    the true value at a multiple of a quarter turn is exactly 0 (or undefined for tan), not
    the residue mpmath's binary pi would leave."""
    r = Decimal(xs) % Decimal(turn)
    if r < 0:
        r += turn
    half, quarter = Decimal(turn) / 2, Decimal(turn) / 4
    if name in ("SIN", "TAN") and r % half == 0:
        return mp.mpf(0)
    if (r - quarter) % half == 0:
        if name == "COS":
            return mp.mpf(0)
        if name == "TAN":
            return None
    a = mp.mpf(str(r)) * 2 * mp.pi / turn
    return {"SIN": mp.sin, "COS": mp.cos, "TAN": mp.tan}[name](a)
R = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 3)
N = int(sys.argv[2]) if len(sys.argv) > 2 else 300


def num(lo, hi, digits=34):
    x = mp.mpf(lo) + (mp.mpf(hi) - mp.mpf(lo)) * mp.mpf(R.random())
    if x == 0:
        return "0"
    return mp.nstr(x, R.randint(1, digits), min_fixed=-mp.inf, max_fixed=mp.inf, strip_zeros=True)


def logspace(lo_exp, hi_exp, signed=False):
    s = "-" if signed and R.random() < 0.5 else ""
    return s + num(1, 10) + "E" + str(R.randint(lo_exp, hi_exp))


ANG = {"DEG": mp.pi / 180, "RAD": mp.mpf(1), "GRAD": mp.pi / 200}
TURN = {"DEG": 360, "GRAD": 400}

CASES = []  # (name, mode, arg generator, truth(x[, y]), bar in units)
for name, f, gen in [
    ("EXP", mp.exp, lambda: num(-300, 300)), ("LN", mp.log, lambda: logspace(-300, 300)),
    ("TENX", lambda x: mp.power(10, x), lambda: num(-300, 300)),
    ("LOG", mp.log10, lambda: logspace(-300, 300)),
    ("SINH", mp.sinh, lambda: num(-200, 200)), ("COSH", mp.cosh, lambda: num(-200, 200)),
    ("TANH", mp.tanh, lambda: num(-40, 40)), ("ASINH", mp.asinh, lambda: logspace(-30, 30, True)),
    ("ACOSH", mp.acosh, lambda: num(1, 1e6)), ("ATANH", mp.atanh, lambda: num(-0.9999999, 0.9999999))]:
    CASES.append((name, "RAD", gen, f, 8))
for name, f, gen in [("SQRT", mp.sqrt, lambda: logspace(-300, 300)),
                     ("SQ", lambda x: x * x, lambda: logspace(-300, 300, True)),
                     ("INV", lambda x: 1 / x, lambda: logspace(-300, 300, True))]:
    CASES.append((name, "RAD", gen, f, 0.5))
for mode in ("DEG", "RAD", "GRAD"):
    k = ANG[mode]
    if mode == "RAD":
        gens = [lambda: num(-10, 10), lambda: logspace(-30, 30, True)]
    else:
        t = TURN[mode]
        gens = [lambda t=t: num(-2 * t, 2 * t), lambda t=t: logspace(-30, 30, True),
                lambda t=t: str(R.randint(-8, 8) * t // 8) + "." + "9" * R.randint(1, 30),
                lambda t=t: str(R.randint(-8, 8) * t // 8) + ".0" + "0" * R.randint(0, 28) + str(R.randint(1, 9))]
    for g in gens:
        for fname, f in (("SIN", mp.sin), ("COS", mp.cos), ("TAN", mp.tan)):
            if mode == "RAD":
                CASES.append((fname, mode, g, lambda x, f=f: f(x), 8))
            else:
                CASES.append((fname, mode, g, ("exact", fname, TURN[mode]), 8))
    CASES.append(("ASIN", mode, lambda: num(-1, 1), lambda x, k=k: mp.asin(x) / k, 8))
    CASES.append(("ACOS", mode, lambda: num(-1, 1), lambda x, k=k: mp.acos(x) / k, 8))
    CASES.append(("ATAN", mode, lambda: logspace(-30, 30, True), lambda x, k=k: mp.atan(x) / k, 8))
TWO = [("POW", lambda: (logspace(-20, 20), num(-50, 50)), lambda y, x: mp.power(y, x), 8),
       ("POW", lambda: (num(-9, 9), str(R.randint(-60, 60))), lambda y, x: mp.power(y, x), 8),
       ("XROOT", lambda: (logspace(-300, 300), str(R.choice([2, 3, 4, 5, 7, -2, -3]))),
        lambda y, x: mp.root(y, int(x)) if x > 0 else 1 / mp.root(y, -int(x)), 8),
       ("XROOT", lambda: (logspace(-300, 300), num(-20, 20, 6)), lambda y, x: mp.power(y, 1 / x), 8),
       ("PCT", lambda: (logspace(-100, 100, True), logspace(-100, 100, True)), lambda y, x: y * x / 100, 0.5 + 1e-9),
       ("PCTCHG", lambda: (logspace(-30, 30, True), logspace(-30, 30, True)), lambda y, x: (x - y) / y * 100, 8)]   # three roundings: within 8 units


def units(got, truth):
    if truth == 0:
        return mp.mpf(0) if got == 0 else mp.inf
    e = int(mp.floor(mp.log10(abs(truth))))
    if abs(truth) >= mp.mpf(10) ** (e + 1):
        e += 1
    return abs(got - truth) / mp.mpf(10) ** (e - 33)


def main():
    drive = sys.argv[1]
    lines, meta = [], []
    for name, mode, gen, f, bar in CASES:
        for _ in range(N):
            x = gen()
            lines.append(f"{name} {mode} {x}")
            if isinstance(f, tuple):
                meta.append((name, mode, bar, lambda x=x, f=f: exact_trig(f[1], x, f[2]), x))
            else:
                meta.append((name, mode, bar, lambda x=x, f=f: f(mp.mpf(x)), x))
    for name, gen, f, bar in TWO:
        for _ in range(N):
            y, x = gen()
            lines.append(f"{name} RAD {x} {y}")
            meta.append((name, "-", bar, lambda x=x, y=y, f=f: f(mp.mpf(y), mp.mpf(x)), f"{y} {x}"))
    out = subprocess.run([drive], input="\n".join(lines) + "\n", capture_output=True, text=True,
                         check=True).stdout.splitlines()
    worst, bad = {}, 0
    for (name, mode, bar, truth, arg), got in zip(meta, out):
        key = (name, mode)
        try:
            t = truth()
        except (ValueError, ZeroDivisionError):
            t = None
        if got.startswith("ERR"):
            # an error must mean the true value is undefined, complex, infinite or out of range
            ok = t is None or not mp.isfinite(t) or abs(mp.im(t)) > 0 or abs(t) >= mp.mpf(10) ** 6145 \
                or (t != 0 and abs(t) < mp.mpf(10) ** -6143)
            if not ok:
                bad += 1
                print(f"UNEXPECTED ERROR {name} {mode} {arg}: {got}, true value {mp.nstr(t, 20)}")
            continue
        if t is None or (hasattr(t, "imag") and mp.im(t) != 0):
            bad += 1
            print(f"MISSING ERROR {name} {mode} {arg}: got {got}")
            continue
        u = units(mp.mpf(got.replace("E", "e")), mp.re(t))
        w = worst.get(key, (0, ""))
        if u > w[0]:
            worst[key] = (u, arg)
        if u > bar:
            bad += 1
            if bad <= 20:
                print(f"OVER {name} {mode} {arg}: {float(u):.2f} units (bar {bar})")
    for (name, mode), (u, arg) in sorted(worst.items()):
        print(f"{name:6s} {mode:4s} max {float(u):6.3f} units   (at {arg})")
    print(f"fn_fuzz: {len(lines)} cases, {bad} over the bar or wrong in kind")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
