"""range_fuzz: the functions of units 003, 004 and 021 at the edges of the range (unit 026), against
mpmath. Since unit 026 a function's ABN_OVERFLOW becomes the largest number and its ABN_UNDERFLOW
becomes 0, silently, so a function may report them only when its true result lies beyond the range:
an overflow or underflow in a middle step, with the result in range, would be a wrong answer with no
error. Each case is classified by the true value:

- in range: the function must not report OVERFLOW (22) or UNDERFLOW (23); a number is checked to
  within 1E-20 relative, any other error is allowed (it stays an error, as before 026);
- beyond the largest number: it must report OVERFLOW;
- nonzero below the smallest normal number: UNDERFLOW, or exactly 0;
- within a hair of either edge: skipped (where rounding decides).

Usage: python tests/range_fuzz.py build/fn_drive [seed] [cases per function]. Needs mpmath (make
accuracy).
"""
import decimal
import random
import subprocess
import sys

import mpmath as mp

mp.mp.dps = 80
D = decimal.Context(prec=120, Emax=10**7, Emin=-10**7)    # exact enough for IDIV, PCT, PCTCHG
OVF, UNF = 22, 23
TOP = mp.mpf("9.999999999999999999999999999999999e6144")
TOP_HI = mp.mpf("9.9999999999999999999999999999999995e6144")      # rounds to infinity from here
BOT = mp.mpf("1e-6143")
seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
N = int(sys.argv[3]) if len(sys.argv) > 3 else 300
R = random.Random(seed)


def mant():
    return R.choice(["1", "9", "9.999999999999999999999999999999999", "5", "3.3"] +
                    [f"{R.randint(1, 9)}.{R.randint(0, 10**12)}"])


def edge(sign=True, lo=True, hi=True):
    """A number near either edge of the range, or an ordinary one."""
    k = R.random()
    if hi and k < 0.4:
        e = R.randint(6100, 6144)
    elif lo and k < 0.8:
        e = R.randint(-6143, -6100)
    else:
        e = R.randint(-30, 30)
    s = "-" if sign and R.random() < 0.5 else ""
    return f"{s}{mant()}E{e}"


def ordinary(sign=True):
    s = "-" if sign and R.random() < 0.5 else ""
    return f"{s}{mant()}E{R.randint(-5, 5)}"


ANG = {"RAD": 1, "DEG": mp.pi / 180, "GRAD": mp.pi / 200}
UNITS = {"TOKG": lambda x: x * mp.mpf("0.45359237"), "TOLB": lambda x: x / mp.mpf("0.45359237"),
         "TOC": lambda x: (x - 32) / mp.mpf("1.8"), "TOF": lambda x: x * mp.mpf("1.8") + 32,
         "TOCM": lambda x: x * mp.mpf("2.54"), "TOIN": lambda x: x / mp.mpf("2.54"),
         "TOL": lambda x: x * mp.mpf("3.785411784"), "TOGAL": lambda x: x / mp.mpf("3.785411784"),
         "TOKM": lambda x: x * mp.mpf("1.609344"), "TOMILE": lambda x: x / mp.mpf("1.609344")}


def hms(x):
    """H.MMSS of hours x; beyond 1E30 the minutes and seconds lie below the 34th digit."""
    if abs(x) > mp.mpf("1e30"):
        return x
    a = abs(x)
    h = mp.floor(a)
    m = mp.floor((a - h) * 60)
    s = ((a - h) * 60 - m) * 60
    return mp.sign(x) * (h + m / 100 + s / 10000)


def hmsinv(x):
    if abs(x) > mp.mpf("1e30"):
        return x
    a = abs(x)
    h = mp.floor(a)
    m = mp.floor((a - h) * 100)
    s = ((a - h) * 100 - m) * 100
    return mp.sign(x) * (h + m / 60 + s / 3600)


def odd(x):
    return x == mp.floor(x) and mp.floor(x) % 2 == 1


def pow_(y, x):
    if y < 0 and x != mp.floor(x):
        return None
    if y == 0:
        return None if x <= 0 else mp.mpf(0)
    return mp.sign(y) ** (1 if odd(x) else 0) * mp.power(abs(y), x)


def xroot(y, x):
    if x == 0:
        return None
    if y < 0 and not odd(x):
        return None
    return pow_(y, 1 / x) if y >= 0 else -mp.power(-y, 1 / x)


# (name, argument maker, truth); a maker gives (mode, x) or (mode, x, y), y the calculator's Y
ONE = [
    ("EXP", lambda: R.choice([edge(), str(R.randint(-15000, 15000))]), mp.exp),
    ("TENX", lambda: R.choice([edge(), str(R.randint(-6200, 6200)) + "." + str(R.randint(0, 99))]), lambda x: mp.power(10, x)),
    ("TWOX", lambda: R.choice([edge(), str(R.randint(-21000, 21000)) + ".5"]), lambda x: mp.power(2, x)),
    ("EXPM1", lambda: R.choice([edge(), str(R.randint(-15000, 15000))]), mp.expm1),
    ("SINH", lambda: R.choice([edge(), str(R.randint(-15000, 15000))]), mp.sinh),
    ("COSH", lambda: R.choice([edge(), str(R.randint(-15000, 15000))]), mp.cosh),
    ("TANH", edge, mp.tanh), ("ASINH", edge, mp.asinh), ("ATANH", lambda: edge(hi=False), mp.atanh),
    ("ACOSH", lambda: edge(sign=False, lo=False), mp.acosh),
    ("LN", lambda: edge(sign=False), mp.ln), ("LOG", lambda: edge(sign=False), mp.log10),
    ("LN1P", edge, lambda x: mp.log1p(x) if x > -1 else None),
    ("LOG2", lambda: edge(sign=False), lambda x: mp.log(x, 2)),
    ("SQ", edge, lambda x: x * x), ("SQRT", lambda: edge(sign=False), mp.sqrt),
    ("INV", edge, lambda x: 1 / x),
    ("HMS", edge, hms), ("HMSINV", edge, hmsinv),
    ("TODEG", edge, lambda x: x * 180 / mp.pi), ("TORAD", edge, lambda x: x * mp.pi / 180),
    ("FACT", lambda: R.choice([str(R.randint(0, 3000)), f"{R.randint(-3200, 3200)}.5", edge(hi=False)]),
     lambda x: None if x < 0 and x == mp.floor(x) else mp.gamma(x + 1)),
    ("IP", edge, lambda x: mp.sign(x) * mp.floor(abs(x))), ("INTG", edge, mp.floor),
]
ONE += [(u, edge, f) for u, f in UNITS.items()]
TRIG = [("SIN", mp.sin), ("COS", mp.cos), ("TAN", mp.tan)]
ATRIG = [("ASIN", mp.asin), ("ACOS", mp.acos), ("ATAN", mp.atan)]


def two(f):
    return lambda: R.choice([(edge(), edge()), (edge(), ordinary()), (ordinary(), edge())])


TWO = [
    ("POW", lambda: R.choice([(edge(), ordinary()), (ordinary(), str(R.randint(-7000, 7000))),
                              (ordinary(), f"{R.randint(-7000, 7000)}.5"), (edge(), str(R.randint(-3, 3)))]), pow_),
    ("XROOT", lambda: R.choice([(edge(), ordinary()), (edge(), edge()), (ordinary(), edge()),
                                (edge(), str(R.choice([-3, -1, 1, 3, 2])))]), xroot),
    ("IDIV", two(None), "dec"),
    ("PCT", two(None), "dec"),
    ("PCTCHG", two(None), "dec"),
]


def dec(name, ys, xs):
    """IDIV, PCT and PCTCHG in decimal (mpmath's binary would floor an exact quotient wrongly)."""
    y, x = D.create_decimal(ys), D.create_decimal(xs)
    if name == "IDIV":
        if x == 0:
            return None
        q = D.divide(y, x)
        if abs(q) > decimal.Decimal("1e100"):
            return mp.mpf(str(q))           # the floor is below its 34th digit
        return mp.mpf(str(D.divide_int(y, x) - (1 if (D.remainder(y, x) != 0 and (y < 0) != (x < 0)) else 0)))
    if name == "PCT":
        return mp.mpf(str(D.divide(D.multiply(y, x), 100)))
    if y == 0:
        return None
    return mp.mpf(str(D.multiply(D.divide(D.subtract(x, y), y), 100)))


def cls(t):
    if t is None or mp.isinf(t):
        return "dom"                    # no value (atanh 1: a pole, INVALID here)
    a = abs(t)
    if a == 0:
        return "zero"
    if a > TOP_HI:
        return "over"
    if a < BOT * mp.mpf("0.9999999"):
        return "under"
    if a > TOP or a < BOT * mp.mpf("1.0000001"):
        return "edge"
    return "in"


def main():
    drive = sys.argv[1] if len(sys.argv) > 1 else "build/fn_drive"
    cases = []                          # (label, line, truth)
    for name, mk, f in ONE:
        for _ in range(N):
            x = mk()
            cases.append((name, f"{name} RAD {x}", f(mp.mpf(x))))
    for name, f in TRIG:
        for _ in range(N):
            mode = R.choice(list(ANG))
            x = edge(hi=False)      # a huge angle needs more digits of pi than mpmath carries here
            cases.append((name + " " + mode, f"{name} {mode} {x}", f(mp.mpf(x) * ANG[mode])))
    for name, f in ATRIG:
        for _ in range(N):
            mode = R.choice(list(ANG))
            x = edge(hi=name == "ATAN")
            v = mp.mpf(x)
            t = None if name != "ATAN" and abs(v) > 1 else f(v) / ANG[mode]
            cases.append((name + " " + mode, f"{name} {mode} {x}", t))
    for name, mk, f in TWO:
        for _ in range(N):
            y, x = mk()
            t = dec(name, y, x) if f == "dec" else f(mp.mpf(y), mp.mpf(x))
            cases.append((name, f"{name} RAD {x} {y}", t))
    out = subprocess.run([drive], input="".join(l + "\n" for _, l, _ in cases), capture_output=True,
                         text=True, check=True).stdout.split("\n")
    bad = {}
    seen = {}
    for (label, line, t), got in zip(cases, out):
        c = cls(t)
        seen[label] = seen.get(label, 0) + 1
        err = int(got.split()[1]) if got.startswith("ERR") else None
        why = None
        if c == "in":
            if err in (OVF, UNF):
                why = f"in range ({mp.nstr(t, 8)}) but ERR {err}"
            elif err is None and got != "BAD":
                g = mp.mpf(got)
                if label in ("HMS", "HMSINV"):
                    pass                # the class is what is checked here; their digits are fn_fuzz's
                elif g == 0 and abs(t) < mp.mpf("1e-30") and label[:3] in ("SIN", "COS", "TAN"):
                    pass                # an exact zero of ours (cos 90 degrees), mpmath's is not
                elif abs(g - t) > abs(t) * mp.mpf("1e-20"):
                    why = f"gave {got}, true {mp.nstr(t, 20)}"
        elif c == "over" and err != OVF:
            why = f"true {mp.nstr(t, 8)} beyond the range, gave {got}"
        elif c == "under" and err != UNF and got not in ("0", "-0"):
            why = f"true {mp.nstr(t, 8)} below the range, gave {got}"
        elif c == "zero" and err in (OVF, UNF):
            why = f"true 0, gave ERR {err}"
        if why:
            bad.setdefault(label, []).append(f"{line}: {why}")
    for label in seen:
        b = bad.get(label, [])
        print(f"{label:12s} {seen[label]:5d} cases  {'ok' if not b else f'{len(b)} FAIL'}")
        for m in b[:3]:
            print("    " + m)
    sys.exit(1 if bad else 0)


main()
