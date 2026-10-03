#!/usr/bin/env python3
"""cx_fuzz: unit 009 complex arithmetic and functions against mpmath, normwise.

    python3 tests/cx_fuzz.py build/cx_drive [count-per-case] [seed]      (needs mpmath)

Random complex operands go through build/cx_drive; each result is compared with mpmath at 150
digits on the exact decimal operands. The error is normwise (unit 009 rule 4): the larger part
error, in units of the 34th significant digit of the larger part of the true value. + and -
must be correctly rounded in each part, x, / and 1/z too (each part is formed exactly and
rounded once). The unit's bar for everything else is 8 units; the functions formed to 50 digits
and rounded once (e^z, ln z, sin, cos, tan, z1^z2: core/hp.c) are held here to the half unit
they achieve, so that a lapse in that precision shows (ours); ABS (the engine's root of the
exactly formed sum of squares) to the unit's 8. Prints the largest error per case and fails if
any exceeds its bar. Run with `make accuracy`.
"""
import random
import subprocess
import sys

try:
    import mpmath as mp
except ImportError:
    print("cx_fuzz: mpmath is not installed; skipped")
    sys.exit(0)

mp.mp.dps = 150
R = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 9)
N = int(sys.argv[2]) if len(sys.argv) > 2 else 400


def num(lo, hi, digits=34):
    x = mp.mpf(lo) + (mp.mpf(hi) - mp.mpf(lo)) * mp.mpf(R.random())
    if x == 0:
        return "0"
    return mp.nstr(x, R.randint(1, digits), min_fixed=-mp.inf, max_fixed=mp.inf, strip_zeros=True)


def logspace(lo_exp, hi_exp):
    s = "-" if R.random() < 0.5 else ""
    return s + num(1, 10) + "E" + str(R.randint(lo_exp, hi_exp))


def part(lo, hi):
    """A part: mostly moderate, sometimes zero or an integer, sometimes far from 1."""
    u = R.random()
    if u < 0.06:
        return "0"
    if u < 0.16:
        return str(R.randint(-12, 12))
    if u < 0.30:
        return logspace(-20, 20)
    return num(lo, hi)


def z(lo=-10, hi=10):
    return (part(lo, hi), part(lo, hi))


def near_unit():
    """A point close to the unit circle (ln's real part cancels)."""
    t = mp.mpf(R.random()) * 2 * mp.pi
    r = 1 + mp.mpf(R.choice([-1, 1])) * mp.power(10, -R.randint(1, 30)) * mp.mpf(R.random())
    return (mp.nstr(r * mp.cos(t), 34), mp.nstr(r * mp.sin(t), 34))


def near_one():
    """1 + d1 + d2 i with both small: ln z is small in both parts and neither part dominates."""
    d1 = mp.power(10, -R.randint(1, 33)) * (mp.mpf(R.random()) - 0.5)
    d2 = mp.power(10, -R.randint(1, 33)) * (mp.mpf(R.random()) - 0.5)
    return (mp.nstr(1 + d1, 34), mp.nstr(d2, 34))


def near_pole():
    """tan near (k + 1/2) pi on the real axis."""
    k = R.randint(-5, 5)
    a = (k + mp.mpf(1) / 2) * mp.pi + mp.power(10, -R.randint(1, 30)) * (mp.mpf(R.random()) - 0.5)
    return (mp.nstr(a, 34), mp.nstr(mp.power(10, -R.randint(0, 30)) * mp.mpf(R.random() - 0.5), 34))


def C(p):
    return mp.mpc(mp.mpf(p[0]), mp.mpf(p[1]))


def pw(a, b):
    if a == 0:
        return mp.mpc(0) if b.real > 0 else None
    return mp.exp(b * mp.log(a))


ONE = {"INV": lambda w: 1 / w if w != 0 else None, "EXP": mp.exp,
       "LN": lambda w: mp.log(w) if w != 0 else None, "SIN": mp.sin, "COS": mp.cos, "TAN": mp.tan,
       "ABS": lambda w: mp.mpc(abs(w), 0)}
TWO = {"ADD": lambda a, b: a + b, "SUB": lambda a, b: a - b, "MUL": lambda a, b: a * b,
       "DIV": lambda a, b: a / b if b != 0 else None, "POW": pw}
EXACT = {"ADD", "SUB", "MUL", "DIV", "INV"}

CASES = []  # (label, name, generator)
for name in ("ADD", "SUB", "MUL", "DIV"):
    CASES.append((name, name, lambda: z() + z()))
    CASES.append((name + " wide", name, lambda: (logspace(-300, 300), logspace(-300, 300),
                                                logspace(-300, 300), logspace(-300, 300))))
CASES += [("INV", "INV", lambda: z()), ("ABS", "ABS", lambda: z()),
          ("EXP", "EXP", lambda: (num(-300, 300), part(-1e6, 1e6))),
          ("EXP small", "EXP", lambda: z(-3, 3)),
          ("LN", "LN", lambda: z()), ("LN wide", "LN", lambda: (logspace(-3000, 3000), logspace(-3000, 3000))),
          ("LN near |z|=1", "LN", near_unit), ("LN near 1", "LN", near_one),
          ("SIN", "SIN", lambda: z()), ("COS", "COS", lambda: z()),
          ("SIN large re", "SIN", lambda: (num(-1e6, 1e6), num(-3, 3))),
          ("TAN", "TAN", lambda: z()), ("TAN near pole", "TAN", near_pole),
          ("TAN large im", "TAN", lambda: (num(-10, 10), num(-9000, 9000))),
          ("POW integer", "POW", lambda: z(-3, 3) + (str(R.randint(-60, 60)), "0")),
          ("POW big integer", "POW", lambda: (num(0.9, 1.1), num(-0.1, 0.1), str(R.randint(-100000, 100000)), "0")),
          ("POW real", "POW", lambda: z(-5, 5) + (num(-6, 6), "0")),
          ("POW complex", "POW", lambda: z(-5, 5) + z(-3, 3)),
          ("POW real base", "POW", lambda: (num(-20, 20), "0") + z(-3, 3))]


def unit_of(w):
    m = max(abs(w.real), abs(w.imag))
    return mp.power(10, mp.floor(mp.log10(m)) - 33) if m != 0 else None


def main():
    drive = sys.argv[1] if len(sys.argv) > 1 else "build/cx_drive"
    worst_all = True
    for label, name, gen in CASES:
        args = [gen() for _ in range(N)]
        text = "".join(name + " " + " ".join(a) + "\n" for a in args)
        out = subprocess.run([drive], input=text, capture_output=True, text=True, check=True).stdout.split("\n")
        worst, at, errs = 0, None, 0
        for a, line in zip(args, out):
            if name in ONE:
                truth = ONE[name](C(a))
            else:
                truth = TWO[name](C(a[:2]), C(a[2:]))
            if truth is not None and abs(truth) != 0:
                m = max(abs(truth.real), abs(truth.imag))
                if m > mp.mpf("9.99e6144") or m < mp.mpf("1e-6143"):
                    truth = "range"
            if line.startswith("ERR"):
                if truth is None or truth == "range":
                    continue
                errs += 1
                if errs <= 3:
                    print(f"  {label}: {a} gave {line}, expected {mp.nstr(truth, 20)}")
                continue
            if truth is None or truth == "range":
                errs += 1
                if errs <= 3:
                    print(f"  {label}: {a} gave {line}, expected an error")
                continue
            g = [mp.mpf(t) for t in line.split()]
            if name in EXACT:
                # each part correctly rounded: within half a unit of its own 34th digit
                e = 0
                for gv, tv in ((g[0], truth.real), (g[1], truth.imag)):
                    if tv == 0:
                        e = max(e, 0 if gv == 0 else mp.inf)
                    else:
                        e = max(e, abs(gv - tv) / mp.power(10, mp.floor(mp.log10(abs(tv))) - 33))
            else:
                u = unit_of(truth)
                e = 0 if u is None else max(abs(g[0] - truth.real), abs(g[1] - truth.imag)) / u
            if e > worst:
                worst, at = e, a
        bar = 8 if name == "ABS" else 0.5 + 1e-9 if name in EXACT else 0.5 + 1e-6
        ok = worst <= bar and errs == 0
        worst_all &= ok
        print(f"{label:18s} worst {mp.nstr(worst, 4):>10s} units (bar {bar:g}){'' if ok else '  FAIL'}"
              f"{'' if errs == 0 else f'  {errs} error mismatches'}{'' if ok else f'  at {at}'}")
    sys.exit(0 if worst_all else 1)


main()
