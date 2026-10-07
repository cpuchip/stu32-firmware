"""polar_fuzz: ->POL's r and theta against mpmath at 120 digits (unit 034). Both must be correctly rounded: the
engine's 34 digits equal to the true value rounded half to even, digit for digit, in DEG, RAD and GRAD.
With --rec, ->REC (r, theta to x, y) is judged the same way on each case's r and theta (034 rule 4: it was not
correctly rounded in RAD, nor for tiny angles; now it is).

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
    return D.create_decimal(mp.nstr(t, 115, min_fixed=-mp.inf, max_fixed=mp.inf).replace("+", ""))   # past a
                                                                    # near-midpoint's deciding digit


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
    for _ in range(N // 3):                     # 14 to 20 digits apart: r a few units above the larger part
        e = R.randint(-3000, 3000)
        a, b = num34(e), num34(e - R.randint(14, 20))
        out.append(("r just above", R.choice(modes), *((a, b) if R.random() < 0.5 else (b, a))))
    for _ in range(N // 10):                    # theta below the range (0 there, unit 026) and at its edge
        ye = R.randint(-6143, -6043)            # y a number; x sets theta from about 1E-6243 to 1E-5943
        out.append(("theta tiny", R.choice(modes), num34(R.randint(-100, 100), sign=False), num34(ye)))
    for mode in modes:                          # the axes and diagonals themselves
        for x, y in [("1", "0"), ("-1", "0"), ("0", "1"), ("0", "-1"), ("3", "3"), ("-3", "3"), ("-3", "-3"),
                     ("3", "-3"), ("2.5E100", "0"), ("0", "-7E-200")]:
            out.append(("axes, diagonals", mode, x, y))
    return out


def convergents(t, lim=10 ** 34):
    """the continued fraction's convergents p/q of t below lim"""
    x, p0, q0, p1, q1, out = t, 0, 1, 1, 0, []
    for _ in range(200):
        a = int(mp.floor(x))
        p0, q0, p1, q1 = p1, q1, a * p1 + p0, a * q1 + q0
        if p1 >= lim or q1 >= lim:
            break
        out.append((p1, q1))
        if x == a:
            break
        x = 1 / (x - a)
    return out


def midpoint_cases():
    """(mode, x, y) whose theta lies extremely near a rounding midpoint: x/y a convergent of cot(midpoint) (the
    034 review's construction)"""
    out = []
    with mp.workdps(250):
        for _ in range(N // 5):
            mode = R.choice(list(UNIT))
            base = mp.mpf(R.uniform(0.05, {"RAD": 1.5, "DEG": 89.0, "GRAD": 99.0}[mode]))
            e = int(mp.floor(mp.log10(base)))
            mid = (mp.floor(base / mp.mpf(10) ** (e - 33)) + mp.mpf("0.5")) * mp.mpf(10) ** (e - 33)
            th = mid if UNIT[mode] is None else mid * mp.pi / UNIT[mode]
            for p, q in convergents(mp.tan(th))[-3:]:
                if q >= 10 ** 25:
                    out.append((mode, str(q), str(p)))
                    out.append((mode, str(-q), str(-p)))
    return out


def rec_midpoint_cases():
    """(mode, r, theta) with r cos theta extremely near a midpoint: r found by an Ostrowski-style walk on cos
    theta's convergents (the 034 review's construction)"""
    out = []
    with mp.workdps(300):
        for _ in range(N // 5):
            mode = R.choice(list(UNIT))
            t = f"{R.uniform(0.1, 1.4):.33f}" if mode == "RAD" else f"{R.uniform(1, 80):.32f}"
            c = mp.cos(mp.mpf(t)) if UNIT[mode] is None else mp.cospi(mp.mpf(t) / UNIT[mode])
            rv, d = 0, mp.mpf("0.5")
            for p, q in convergents(c):
                th = q * c - p
                b = int(mp.nint(d / th))
                if rv + abs(b) * q >= 10 ** 34 or rv + b * q < 0:
                    b = 0
                rv, d = rv + b * q, d - b * th
            if rv >= 10 ** 33:
                out.append((mode, str(rv), t))
    return out


def in_range(x, y):
    r = mp.sqrt(mp.mpf(x) ** 2 + mp.mpf(y) ** 2)
    return r < TOP and abs(rounded(r)) > 0


def main():
    drive = args[0] if args else "build/fn_drive"
    cs = [c for c in cases() if in_range(c[2], c[3])] + [("near a midpoint", m, x, y) for m, x, y in midpoint_cases()]
    lines = [f"TOPOL {m} {x} {y}" for _, m, x, y in cs]
    out = subprocess.run([drive], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout.split("\n")
    count, miss, at = {}, {}, {}
    for (reg, mode, x, y), got in zip(cs, out):
        count[reg] = count.get(reg, 0) + 1
        want_r = rounded(mp.sqrt(mp.mpf(x) ** 2 + mp.mpf(y) ** 2))
        th = angle(y, x, mode)
        want_t = rounded(th) if abs(th) >= mp.mpf("1e-6143") else decimal.Decimal(0)   # below the range: 0
        parts = got.split()
        bad = []
        if len(parts) != 2 or got.startswith("ERR"):
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
            rl.append((mode, str(r), str(rounded(th)) if abs(th) >= mp.mpf("1e-6143") else "0"))   # ->POL's theta
        rl += rec_midpoint_cases()
        rl += [("RAD", num34(R.randint(-50, 50), sign=False), num34(R.randint(1, 130))) for _ in range(N // 5)]
        far = [("RAD", num34(0, sign=False), num34(R.randint(170, 6000))) for _ in range(N // 10)]
        rl += far
        out = subprocess.run([drive], input="\n".join(f"TOREC {m} {r} {t}" for m, r, t in rl) + "\n",
                             capture_output=True, text=True, check=True).stdout.split("\n")
        n = exact = 0
        per = {}
        far_n = far_ok = 0
        for (mode, r, t), got in zip(rl, out):
            isfar = (mode, r, t) in far
            if UNIT[mode] is None:
                with mp.workdps(120 + max(0, int(mp.log10(abs(mp.mpf(t)) + 1)))):    # big angles reduced exactly
                    cs, sn = +mp.cos(mp.mpf(t)), +mp.sin(mp.mpf(t))
            else:                               # exact at the unit's multiples (cos 90 = 0, not a pi residue)
                cs, sn = mp.cospi(mp.mpf(t) / UNIT[mode]), mp.sinpi(mp.mpf(t) / UNIT[mode])
            wx = rounded(mp.mpf(r) * cs) if cs != 0 else decimal.Decimal(0)
            wy = rounded(mp.mpf(r) * sn) if sn != 0 else decimal.Decimal(0)
            parts = got.split()
            if isfar:                           # past hp's pi (|theta| > 1E169): reported, not judged
                far_n += 1
                far_ok += len(parts) == 2 and not got.startswith("ERR") and D.create_decimal(parts[0]) == wx and \
                    D.create_decimal(parts[1]) == wy
                continue
            n += 1
            good = (len(parts) == 2 and not got.startswith("ERR") and D.create_decimal(parts[0]) == wx and
                    D.create_decimal(parts[1]) == wy)
            exact += good
            pm = per.setdefault(mode, [0, 0])
            pm[0] += 1
            pm[1] += good
        ok = ok and exact == n
        print(f"->REC            {n:6d} cases  {exact:6d} correctly rounded  ("
              + ", ".join(f"{m} {g}/{k}" for m, (k, g) in sorted(per.items())) + ")" + ("" if exact == n else "  FAIL"))
        print(f"->REC past 1E169 {far_n:6d} cases  {far_ok:6d} correctly rounded  (reported: the engine's cosine and sine times r)")
    sys.exit(0 if ok else 1)


main()
