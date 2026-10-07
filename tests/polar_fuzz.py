"""polar_fuzz: ->POL's r and theta against mpmath at 120 digits (unit 034). Both must be correctly rounded: the
engine's 34 digits equal to the true value rounded half to even, digit for digit, in DEG, RAD and GRAD.
->REC (r, theta to x, y) is measured and reported, not judged (034 rule 4 asks whether it already is).

Regions (034's acceptance): magnitudes from 1E-6000 to 1E6000 in every quadrant; x and y of very different
sizes; near the axes; near the diagonals; the axes and diagonals themselves; the range's ends (no intermediate
overflow for r).

Usage: python tests/polar_fuzz.py build/fn_drive [seed] [cases per region] [--rec]. Needs mpmath.
"""
import decimal
import random
import subprocess
import sys

import mpmath as mp

mp.mp.dps = 120
args = [a for a in sys.argv[1:] if not a.startswith("--")]
seed = int(args[1]) if len(args) > 1 else 1
N = int(args[2]) if len(args) > 2 else 600
REC = "--rec" in sys.argv
R = random.Random(seed)
D = decimal.Context(prec=34, rounding=decimal.ROUND_HALF_EVEN, Emax=6144, Emin=-6143)
TOP = mp.mpf("9.999999999999999999999999999999999e6144")
UNIT = {"RAD": None, "DEG": mp.mpf(180), "GRAD": mp.mpf(200)}


def digits(n):
    return "".join(str(R.randint(0, 9)) for _ in range(n))


def num34(e, sign=True):
    s = "-" if sign and R.random() < 0.5 else ""
    return f"{s}{R.randint(1, 9)}.{digits(33)}E{e}"


def rounded(t):
    """t rounded to 34 significant digits, half to even, as engine-comparable Decimal"""
    return D.create_decimal(mp.nstr(t, 60, min_fixed=-mp.inf, max_fixed=mp.inf).replace("+", ""))


def angle(y, x, mode):
    th = mp.atan2(mp.mpf(y), mp.mpf(x))
    return th if UNIT[mode] is None else th * UNIT[mode] / mp.pi


def cases():
    out = []                                    # (region, mode, x, y)
    modes = list(UNIT)
    for _ in range(N):                          # both parts of one size, anywhere in the range
        e = R.randint(-6000, 6000)
        out.append(("same size", R.choice(modes), num34(e), num34(e + R.randint(-2, 2))))
    for _ in range(N):                          # sizes apart: theta near an axis
        e = R.randint(-3000, 3000)
        d = R.randint(3, 3000)
        a, b = num34(e), num34(e - d)
        out.append(("near an axis", R.choice(modes), *((a, b) if R.random() < 0.5 else (b, a))))
    for _ in range(N):                          # near the diagonals: |x| and |y| equal to a few digits
        e = R.randint(-300, 300)
        m = f"{R.randint(1, 9)}.{digits(R.randint(0, 20))}"
        x = m + digits(33 - len(m) + 2) + f"E{e}"
        y = m + digits(33 - len(m) + 2) + f"E{e}"
        sx, sy = R.choice("+-"), R.choice("+-")
        out.append(("near a diagonal", R.choice(modes), sx.replace("+", "") + x, sy.replace("+", "") + y))
    for _ in range(N):                          # the range's ends: r near the top, parts near the bottom
        if R.random() < 0.5:
            out.append(("range ends", R.choice(modes), num34(R.randint(6140, 6144)), num34(R.randint(6140, 6144))))
        else:
            out.append(("range ends", R.choice(modes), num34(R.randint(-6143, -6138)), num34(R.randint(-6143, -6138))))
    for _ in range(N):                          # ordinary sizes, short numbers (typed values)
        x = f"{R.choice('+-').replace('+', '')}{R.randint(1, 999)}.{R.randint(0, 99)}"
        y = f"{R.choice('+-').replace('+', '')}{R.randint(1, 999)}.{R.randint(0, 99)}"
        out.append(("typed", R.choice(modes), x, y))
    for mode in modes:                          # the axes and diagonals themselves
        for x, y in [("1", "0"), ("-1", "0"), ("0", "1"), ("0", "-1"), ("3", "3"), ("-3", "3"), ("-3", "-3"),
                     ("3", "-3"), ("2.5E100", "0"), ("0", "-7E-200")]:
            out.append(("axes, diagonals", mode, x, y))
    return out


def in_range(x, y):
    r = mp.sqrt(mp.mpf(x) ** 2 + mp.mpf(y) ** 2)
    return r < TOP and abs(rounded(r)) > 0


def main():
    drive = args[0] if args else "build/fn_drive"
    cs = [c for c in cases() if in_range(c[2], c[3])]
    lines = [f"TOPOL {m} {x} {y}" for _, m, x, y in cs]
    out = subprocess.run([drive], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout.split("\n")
    count, miss, at = {}, {}, {}
    for (reg, mode, x, y), got in zip(cs, out):
        count[reg] = count.get(reg, 0) + 1
        want_r = rounded(mp.sqrt(mp.mpf(x) ** 2 + mp.mpf(y) ** 2))
        th = angle(y, x, mode)
        want_t = rounded(th) if th != 0 else decimal.Decimal(0)
        parts = got.split()
        bad = []
        if len(parts) != 2:
            bad.append(f"got '{got}'")
        else:
            gr, gt = D.create_decimal(parts[0]), D.create_decimal(parts[1])
            if gr != want_r:
                bad.append(f"r {parts[0]} want {want_r}")
            if gt != want_t:
                bad.append(f"theta {parts[1]} want {want_t}")
        if bad:
            miss[reg] = miss.get(reg, 0) + 1
            at.setdefault(reg, f"{mode} x={x} y={y}: " + "; ".join(bad))
    ok = True
    for reg in count:
        m = miss.get(reg, 0)
        ok = ok and m == 0
        print(f"{reg:16s} {count[reg]:6d} cases  {count[reg] - m:6d} correctly rounded{'  FAIL' if m else ''}")
        if m:
            print("    " + at[reg])
    if REC:                                     # ->REC, reported only
        rc = [(m, x, y) for _, m, x, y in cs if m != "RAD" or R.random() < 1]
        rl = []
        for mode, x, y in rc:
            r = rounded(mp.sqrt(mp.mpf(x) ** 2 + mp.mpf(y) ** 2))
            th = angle(y, x, mode)
            rl.append((mode, str(r), str(rounded(th) if th != 0 else 0)))
        out = subprocess.run([drive], input="\n".join(f"TOREC {m} {r} {t}" for m, r, t in rl) + "\n",
                             capture_output=True, text=True, check=True).stdout.split("\n")
        n = exact = 0
        for (mode, r, t), got in zip(rl, out):
            th = mp.mpf(t) if UNIT[mode] is None else mp.mpf(t) * mp.pi / UNIT[mode]
            wx, wy = rounded(mp.mpf(r) * mp.cos(th)), rounded(mp.mpf(r) * mp.sin(th))
            parts = got.split()
            n += 1
            if len(parts) == 2 and D.create_decimal(parts[0]) == wx and D.create_decimal(parts[1]) == wy:
                exact += 1
        print(f"->REC (reported) {n:6d} cases  {exact:6d} correctly rounded")
    sys.exit(0 if ok else 1)


main()
