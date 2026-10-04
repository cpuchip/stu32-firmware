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
    CASES.append((name, "RAD", gen, f, 0.5 + 1e-9))   # exact ties round to even: 0.5 exactly
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
       ("PCTCHG", lambda: (logspace(-30, 30, True), logspace(-30, 30, True)), lambda y, x: (x - y) / y * 100, 8),   # three roundings: within 8 units
       # floored, the remainder with the divisor's sign (unit 017 rule 5): exact, so correctly rounded;
       # the truth in Decimal, since binary mpf leaves a residue where the remainder is exactly 0
       ("IDIV", lambda: (str(R.randint(-10**12, 10**12)), str(R.choice([-1, 1]) * R.randint(1, 10**6))),
        lambda y, x: mp.floor(y / x), 0.5 + 1e-9),
       ("RMDR", lambda: (str(R.randint(-10**12, 10**12)), str(R.choice([-1, 1]) * R.randint(1, 10**6))),
        lambda y, x: floored_rmdr(y, x), 0.5 + 1e-9),
       ("RMDR", lambda: (num(-1000, 1000, 8), num(-50, 50, 6)), lambda y, x: floored_rmdr(y, x), 0.5 + 1e-9)]


def floored_rmdr(y, x):
    """y - x floor(y / x) exactly: the arguments are decimals of at most 34 digits."""
    dy, dx = Decimal(mp.nstr(y, 50)), Decimal(mp.nstr(x, 50))
    q = (dy / dx).to_integral_value(rounding="ROUND_FLOOR")
    return mp.mpf(str(dy - dx * q))


import math


def hms_truth(x):
    """Exact H.MMSSss of decimal hours x (Decimal arithmetic at 200 digits)."""
    d = Decimal(x)
    a = d.copy_abs()
    h = int(a)
    rem = (a - h) * 60
    m = int(rem)
    sec = (rem - m) * 60
    r = h + Decimal(m) / 100 + sec / 10000
    return mp.mpf(str(r.copy_negate() if d < 0 else r))


def hmsinv_truth(x):
    d = Decimal(x)
    a = d.copy_abs()
    h = int(a)
    f = (a - h) * 100
    mm = int(f)
    ss = (f - mm) * 100
    r = (h * 3600 + mm * 60 + ss) / 3600
    return mp.mpf(str(r.copy_negate() if d < 0 else r))


EXACT_UNITS = {"TOKG": ("*", "0.45359237"), "TOLB": ("/", "0.45359237"), "TOCM": ("*", "2.54"),
               "TOIN": ("/", "2.54"), "TOL": ("*", "3.785411784"), "TOGAL": ("/", "3.785411784"),
               "TOKM": ("*", "1.609344"), "TOMILE": ("/", "1.609344")}


def unit_truth(name, x):
    d = Decimal(x)
    if name == "TOC":
        return mp.mpf(str((5 * d - 160) / 9))
    if name == "TOF":
        return mp.mpf(str((9 * d + 160) / 5))
    op, k = EXACT_UNITS[name]
    return mp.mpf(str(d * Decimal(k) if op == "*" else d / Decimal(k)))


def units(got, truth):
    if truth == 0:
        return mp.mpf(0) if got == 0 else mp.inf
    e = int(mp.floor(mp.log10(abs(truth))))
    if abs(truth) >= mp.mpf(10) ** (e + 1):
        e += 1
    return abs(got - truth) / mp.mpf(10) ** (e - 33)


# unit 004: conversions correctly rounded; factorial, nCr, nPr exact then rounded once
for name in ("TOKG", "TOLB", "TOC", "TOF", "TOCM", "TOIN", "TOL", "TOGAL", "TOKM", "TOMILE"):
    CASES.append((name, "RAD", lambda: logspace(-300, 300, True), ("unit", name), 0.5 + 1e-9))
CASES.append(("HMS", "RAD", lambda: num(-1e6, 1e6), ("hms",), 8))
CASES.append(("HMS", "RAD", lambda: logspace(-20, 20, True), ("hms",), 8))
CASES.append(("HMSINV", "RAD", lambda: str(R.randint(-9999, 9999)) + "." + "%02d%02d" % (R.randint(0, 59), R.randint(0, 59)) + str(R.randint(0, 10**R.randint(0, 25))), ("hmsinv",), 8))
CASES.append(("TODEG", "RAD", lambda: logspace(-300, 300, True), lambda x: x * 180 / mp.pi, 8))
CASES.append(("TORAD", "RAD", lambda: logspace(-300, 300, True), lambda x: x * mp.pi / 180, 8))
CASES.append(("FACT", "RAD", lambda: str(R.randint(0, 2123)), lambda x: mp.mpf(math.factorial(int(x))), 0.5 + 1e-9))
# unit 021: x! of a non-integer is gamma(x + 1), the poles beside the negative integers included;
# e^x - 1 and ln(1 + x) where x is tiny; 2^x and log2 x, with their exact powers of two
def near_negint():
    k = str(R.randint(1, 60))
    if R.random() < 0.5:
        return "-" + k + ".0" + "0" * R.randint(0, 28) + str(R.randint(1, 9))
    return "-" + str(int(k) - 1) + "." + "9" * R.randint(1, 30)


for gen in (lambda: num(-1, 1), lambda: num(-200, 200), lambda: num(-2300, 2300, 12),
            lambda: logspace(-40, -1, True), near_negint):
    CASES.append(("FACT", "RAD", gen, lambda x: mp.gamma(x + 1), 8))
for name, f, gens in [
        ("EXPM1", mp.expm1, (lambda: num(-60, 60), lambda: logspace(-300, 5, True))),
        ("LN1P", mp.log1p, (lambda: num(-1, 1), lambda: logspace(-300, 300), lambda: logspace(-300, -1, True))),
        ("TWOX", lambda x: mp.power(2, x), (lambda: num(-300, 300), lambda: logspace(-40, 4, True))),
        ("LOG2", lambda x: mp.log(x, 2), (lambda: logspace(-300, 300), lambda: str(Decimal(2) ** R.randint(-48, 112))))]:
    for gen in gens:
        CASES.append((name, "RAD", gen, f, 8))


# unit 021's review: tiny arguments, where the engine underflows inside (below about 1E-3000)
def tiny_arg():
    return logspace(-6140, -2900, True)


for name, f in [("EXP", mp.exp), ("TENX", lambda x: mp.power(10, x)), ("TWOX", lambda x: mp.power(2, x)),
                ("SINH", mp.sinh), ("COSH", mp.cosh), ("TANH", mp.tanh), ("ASINH", mp.asinh),
                ("ATANH", mp.atanh), ("EXPM1", mp.expm1), ("LN1P", mp.log1p)]:
    CASES.append((name, "RAD", tiny_arg, f, 8))
for mode in ("DEG", "RAD", "GRAD"):
    k = ANG[mode]
    for fname, f in (("ASIN", mp.asin), ("ACOS", mp.acos), ("ATAN", mp.atan)):
        CASES.append((fname, mode, tiny_arg, lambda x, f=f, k=k: f(x) / k, 8))
    for fname, f in (("SIN", mp.sin), ("COS", mp.cos), ("TAN", mp.tan)):
        CASES.append((fname, mode, tiny_arg, lambda x, f=f, k=k: f(x * k), 8))


def prob_truth(f, y, x):
    """nCr, nPr: r > n or n >= 10^16 is INVALID DATA (p.F-2, ruled #3778): no true value"""
    if int(x) > int(y) or int(y) >= 10**16:
        raise ValueError
    return mp.mpf(f(int(y), int(x)))


TWO += [("NCR", lambda: (str(R.randint(0, 3000)), str(R.randint(0, 1500))),
         lambda y, x: prob_truth(math.comb, y, x), 0.5 + 1e-9),
        ("NPR", lambda: (str(R.randint(0, 2500)), str(R.randint(0, 900))),
         lambda y, x: prob_truth(math.perm, y, x), 0.5 + 1e-9),
        ("NCR", lambda: (str(R.randint(1, 10**R.randint(10, 33))), str(R.randint(0, 40))),
         lambda y, x: prob_truth(math.comb, y, x), 0.5 + 1e-9)]
POLAR = []
for mode in ("DEG", "RAD", "GRAD"):
    k = ANG[mode]
    POLAR.append(("TOPOL", mode, lambda: (logspace(-50, 50, True), logspace(-50, 50, True)),
                  lambda x, y, k=k: (mp.hypot(x, y), mp.atan2(y, x) / k)))
    POLAR.append(("TOREC", mode, lambda: (logspace(-50, 50), num(-720, 720)),
                  lambda r, t, k=k, mode=mode: (r * (mp.cos(t * k) if mode == "RAD" else exact_trig("COS", mp.nstr(t, 40), TURN[mode])),
                                                 r * (mp.sin(t * k) if mode == "RAD" else exact_trig("SIN", mp.nstr(t, 40), TURN[mode])))))


def main():
    drive = sys.argv[1]
    lines, meta = [], []
    for name, mode, gen, f, bar in CASES:
        for _ in range(N):
            x = gen()
            lines.append(f"{name} {mode} {x}")
            if isinstance(f, tuple) and f[0] == "unit":
                meta.append((name, mode, bar, lambda x=x, f=f: unit_truth(f[1], x), x))
            elif isinstance(f, tuple) and f[0] == "hms":
                meta.append((name, mode, bar, lambda x=x: hms_truth(x), x))
            elif isinstance(f, tuple) and f[0] == "hmsinv":
                meta.append((name, mode, bar, lambda x=x: hmsinv_truth(x), x))
            elif isinstance(f, tuple):
                meta.append((name, mode, bar, lambda x=x, f=f: exact_trig(f[1], x, f[2]), x))
            else:
                meta.append((name, mode, bar, lambda x=x, f=f: f(mp.mpf(x)), x))
    for name, gen, f, bar in TWO:
        for _ in range(N):
            y, x = gen()
            lines.append(f"{name} RAD {x} {y}")
            meta.append((name, "-", bar, lambda x=x, y=y, f=f: f(mp.mpf(y), mp.mpf(x)), f"{y} {x}"))
    npolar = 0
    for name, mode, gen, f in POLAR:
        for _ in range(N):
            a, b = gen()
            lines.append(f"{name} {mode} {a} {b}")
            for part in (0, 1):
                meta.append((name + ("-r" if part == 0 else "-t") if name == "TOPOL" else name + ("-x" if part == 0 else "-y"),
                             mode, 8, lambda a=a, b=b, f=f, part=part: f(mp.mpf(a), mp.mpf(b))[part], f"{a} {b}"))
            npolar += 1
    raw = subprocess.run([drive], input="\n".join(lines) + "\n", capture_output=True, text=True,
                         check=True).stdout.splitlines()
    out = []
    for line in raw:
        if " " in line and not line.startswith("ERR"):
            out.extend(line.split())
        elif line.startswith("ERR") and len(out) + 1 < len(meta) and meta[len(out)][0][:5] in ("TOPOL", "TOREC"):
            out.extend([line, line])
        else:
            out.append(line)
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
        if key not in worst or u > w[0]:        # an exact case still gets its line
            worst[key] = (u, arg)
        if u > bar:
            bad += 1
            if bad <= 20:
                print(f"OVER {name} {mode} {arg}: {float(u):.2f} units (bar {bar})")
    for (name, mode), (u, arg) in sorted(worst.items()):
        print(f"{name:6s} {mode:4s} max {float(u):6.3f} units   (at {arg})")
    print(f"fn_fuzz: {len(meta)} checks, {bad} over the bar or wrong in kind")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
