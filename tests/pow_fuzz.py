"""pow_fuzz: y^x and the x-th root of y against mpmath at 120 digits (unit 028). Each case's error is in
units of the 34th significant digit of the true value; the bar is 8 (units 003 and 021). Every case's true
value is in range, so an error status is a failure too. Exact cases must come out exact.

Regions (unit 028's acceptance): bases within 1E-33 to 1E-3 of 1 with exponents up to 1E40; bases near
the range's edges; integer and half-integer exponents; negative bases with integer exponents; random
pairs; the root's same regions; (unit 038) integer exponents from -64 to 64 on any 34-digit base.

Usage: python tests/pow_fuzz.py build/fn_drive [seed] [cases per region]. Needs mpmath (make accuracy).
"""
import random
import subprocess
import sys

import mpmath as mp

mp.mp.dps = 120
seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
N = int(sys.argv[3]) if len(sys.argv) > 3 else 2500
R = random.Random(seed)
BAR = 8
TOP, BOT = mp.mpf("9.999999999999999999999999999999999e6144"), mp.mpf("1e-6143")


def digits(n):
    return "".join(str(R.randint(0, 9)) for _ in range(n))


def num34(lo, hi, sign=False):
    """a 34-digit number 10^[lo, hi)"""
    e = R.randint(lo, hi - 1)
    s = "-" if sign and R.random() < 0.5 else ""
    return f"{s}{R.randint(1, 9)}.{digits(33)}E{e}"


def near_one_str():
    k = R.randint(3, 33)
    tail = (f"{R.randint(1, 9)}" + digits(33 - k))[: 34 - k]
    if R.random() < 0.5:
        return "1." + "0" * (k - 1) + tail                          # 1 + d
    return "0." + "9" * (k - 1) + tail if k > 1 else "0." + tail    # 1 - d (about)


def big_exponent_for(y):
    """an exponent up to 1E40 in size keeping y^x in range"""
    ly = abs(mp.log(mp.mpf(y)))
    if ly == 0:
        return "1"
    lim = mp.mpf(14000) / ly                                         # |x ln y| < 14000: in range
    u = min(mp.log10(lim), 40)
    e = R.uniform(0, float(u))
    m = mp.power(10, e)
    s = "-" if R.random() < 0.5 else ""
    return s + mp.nstr(m, 30, min_fixed=-mp.inf, max_fixed=mp.inf)


def in_range(t):
    return t is not None and t != 0 and BOT * mp.mpf("1.0000001") < abs(t) < TOP


def pow_true(y, x):
    y, x = mp.mpf(y), mp.mpf(x)
    if y < 0:
        if x != mp.floor(x):
            return None
        return (-1) ** int(x % 2) * mp.power(-y, x)
    return mp.power(y, x)


def root_true(y, x):
    y, x = mp.mpf(y), mp.mpf(x)
    if x == 0:
        return None
    if y < 0:
        if x != mp.floor(x) or int(x) % 2 == 0:
            return None
        return -mp.power(-y, 1 / x)
    return mp.power(y, 1 / x)


def cases():
    out = []                                    # (region, name, x, y, truth)
    for _ in range(N):                          # near 1, exponents up to 1E40
        y = near_one_str()
        x = big_exponent_for(y)
        out.append(("near 1", "POW", x, y, pow_true(y, x)))
    for _ in range(N):                          # bases near the edges, small exponents keeping range
        y = num34(6000, 6144) if R.random() < 0.5 else num34(-6143, -6000)
        x = f"{R.uniform(-1, 1):.20f}"
        out.append(("edge bases", "POW", x, y, pow_true(y, x)))
    for _ in range(N):                          # integer and half-integer exponents
        y = num34(-5, 6)
        ly = abs(float(mp.log(mp.mpf(y)))) or 1
        n = R.randint(2, int(min(14000 / ly, 10 ** 6)))
        n = -n if R.random() < 0.5 else n
        x = str(n) if R.random() < 0.6 else f"{n}.5"
        out.append(("integer / half", "POW", x, y, pow_true(y, x)))
    for _ in range(N):                          # negative bases, integer exponents
        y = "-" + num34(-5, 6)
        ly = abs(float(mp.log(-mp.mpf(y)))) or 1
        n = R.randint(1, int(min(14000 / ly, 10 ** 6)))
        n = -n if R.random() < 0.5 else n
        out.append(("negative base", "POW", str(n), y, pow_true(y, n)))
    for _ in range(N):                          # random pairs
        y = num34(-300, 300)
        ly = abs(float(mp.log(mp.mpf(y)))) or 1
        x = f"{R.uniform(-1, 1) * min(14000 / ly, 1e6):.25g}"
        out.append(("random pairs", "POW", x, y, pow_true(y, x)))
    for _ in range(N):                          # unit 038: integer exponents up to 64 either way, any base
        y = num34(-90, 90, sign=True)
        n = R.choice([k for k in range(-64, 65) if k not in (0, 1)])
        out.append(("integer to 64", "POW", str(n), y, pow_true(y, n)))
    for _ in range(N):                          # the root: near 1 and large x, edges, odd roots of negatives
        k = R.random()
        if k < 0.4:
            y = near_one_str()
            x = f"{R.uniform(1, 10 ** R.randint(0, 40)):.30g}"
        elif k < 0.7:
            y = num34(6000, 6144) if R.random() < 0.5 else num34(-6143, -6000)
            x = str(R.choice([2, 3, 5, 7, 1.5, 0.75, 10]))
        else:
            y = "-" + num34(-50, 50)
            x = str(R.choice([1, 3, 5, 7, 9, 11, 101]))
        out.append(("root", "XROOT", x, y, root_true(y, x)))
    return out


EXACT = [("POW", "10", "2", "1024"), ("POW", "5", "3", "243"), ("POW", "3", "-2", "-8"), ("POW", "-3", "10", "0.001"),
         ("POW", "2", "1.1", "1.21"), ("POW", "0.5", "4", "2"), ("POW", "0.5", "6.25", "2.5"),
         ("POW", "4", "-1.5", "5.0625"), ("XROOT", "3", "27", "3"), ("XROOT", "3", "-8", "-2"),
         ("XROOT", "2", "1.44", "1.2"), ("POW", "34", "2", "17179869184"),
         ("POW", "70", "3", "2503155504993241601315571986085849"),           # 34 digits, exact
         ("POW", "40", "7", "6366805760909027985741435139224001"),
         ("POW", "69", "-3", "-834385168331080533771857328695283"),
         ("POW", "100", "2", "1267650600228229401496703205376"),
         ("POW", "-40", "5", "1.099511627776E-28")]                          # 1/5^40 = 2^40 / 10^40


def main():
    drive = sys.argv[1] if len(sys.argv) > 1 else "build/fn_drive"
    cs = [c for c in cases() if in_range(c[4])]
    lines = [f"{n} RAD {x} {y}" for _, n, x, y, _ in cs] + [f"{n} RAD {x} {y}" for n, x, y, _ in EXACT]
    out = subprocess.run([drive], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout.split("\n")
    worst, count, errs, at = {}, {}, {}, {}
    for (reg, n, x, y, t), got in zip(cs, out):
        count[reg] = count.get(reg, 0) + 1
        if got.startswith("ERR") or got == "BAD":
            errs[reg] = errs.get(reg, 0) + 1
            at.setdefault(reg + " err", f"{n} y={y} x={x}: {got}")
            continue
        u = mp.power(10, mp.floor(mp.log10(abs(t))) - 33)
        e = abs(mp.mpf(got) - t) / u
        if e > worst.get(reg, -1):
            worst[reg], at[reg] = e, f"{n} y={y} x={x}: got {got}, true {mp.nstr(t, 40)}"
    ok = True
    for reg in count:
        w = worst.get(reg, 0)
        bad = w > BAR or errs.get(reg)
        ok = ok and not bad
        print(f"{reg:16s} {count[reg]:6d} cases  worst {mp.nstr(w, 4):>10s} units{'  ' + str(errs[reg]) + ' errors' if errs.get(reg) else ''}"
              f"{'  FAIL' if bad else ''}")
        if bad:
            print("    " + at.get(reg, "") + ("\n    " + at[reg + ' err'] if reg + ' err' in at else ""))
    nex = 0
    for (n, x, y, want), got in zip(EXACT, out[len(cs):]):
        if got.startswith("ERR") or mp.mpf(got) != mp.mpf(want):
            nex += 1
            print(f"    exact {n} y={y} x={x}: got {got}, want {want}")
    print(f"exact cases     {len(EXACT) - nex}/{len(EXACT)} exact")
    sys.exit(0 if ok and nex == 0 else 1)


main()
