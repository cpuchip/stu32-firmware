#!/usr/bin/env python3
"""veckey_fuzz: random vectors typed through the calculator's keys, unit 010.

    python3 tests/veckey_fuzz.py SEED COUNT > f; build/vectors f

Each sequence types vectors of 1 to 3 elements (mixed magnitudes, near-cancelling pairs,
Pythagorean multiples) and applies + - x / or ABS, against an exact decimal model (python decimal
at 500 digits, each result rounded once to 34 digits half to even, ABS by decimal's correctly
rounded square root): every result "=", with the stack, LAST X and the INVALID DATA cases checked.
Needs only the standard library.
"""
import random
import sys
from decimal import Decimal as D, Context, ROUND_HALF_EVEN

C34 = Context(prec=34, rounding=ROUND_HALF_EVEN, Emax=6144, Emin=-6143)
W = Context(prec=500, Emax=999999, Emin=-999999)
R = random.Random(int(sys.argv[1]))
N = int(sys.argv[2])


def rnum(lo_e=-20, hi_e=20, zero=0.05):
    if R.random() < zero:
        return "0"
    sign = "-" if R.random() < 0.45 else ""
    nd = R.randint(1, R.choice([2, 8, 34]))
    digits = str(R.randint(1, 9)) + "".join(str(R.randint(0, 9)) for _ in range(nd - 1))
    pt = R.randint(1, min(nd, 4))
    m = digits[:pt] + ("." + digits[pt:] if pt < nd else "")
    e = R.randint(lo_e, hi_e) if R.random() < 0.35 else 0
    return sign + m + (f"E{e}" if e else "")


def keys_for(v):
    """The keystrokes that type v as d.ddd E e, so that only significant digits are typed (a
    leading 0 would count against the 34-digit entry limit, unit 001)."""
    d = W.create_decimal(v)
    if d == 0:
        return ["0"]
    sign, digits, _ = d.normalize(W).as_tuple()
    m = str(digits[0]) + ("." + "".join(map(str, digits[1:])) if len(digits) > 1 else "")
    e = d.adjusted()
    out = [m] + (["CHS"] if sign else [])
    if e:
        out += ["E", str(abs(e))] + (["CHS"] if e < 0 else [])
    return out


def vkeys(v):
    out = ["["]
    for i, x in enumerate(v):
        if i:
            out.append(",")
        out += keys_for(x)
    return out


def dz(s):
    return W.create_decimal(s)


def sx(d):
    return str(C34.plus(d))


def vtext(v):
    return "[" + ",".join(sx(dz(x)) for x in v) + "]"


def near(v):
    """a vector close to -v: the sum cancels to the last digits"""
    out = []
    for x in v:
        d = dz(x)
        if d == 0:
            out.append(rnum())
            continue
        tweak = W.scaleb(D(R.randint(-99, 99)), d.adjusted() - R.randint(20, 33))
        out.append(str(C34.plus(W.add(W.minus(d), tweak))))
    return out


def pythagorean():
    m, n = R.randint(2, 40), R.randint(1, 39)
    m, n = max(m, n + 1), min(m, n)
    k = W.scaleb(D(R.randint(1, 999)), R.randint(-10, 10))
    if R.random() < 0.5:
        return [str(W.multiply(k, D(m * m - n * n))), str(W.multiply(k, D(2 * m * n)))]
    a, b, c = R.choice([(2, 3, 6), (1, 4, 8), (2, 6, 9), (4, 4, 7), (6, 6, 7)])
    return [str(W.multiply(k, D(a))), str(W.multiply(k, D(b))), str(W.multiply(k, D(c)))]


def rvec(n=None):
    n = n or R.choice([1, 2, 3, 3])
    return [rnum() for _ in range(n)]


def fits(d):
    return d == 0 or (-6143 <= d.adjusted() <= 6144)


out = []
for i in range(N):
    vid = f"V{i:05d}"
    kind = R.choice(["add", "add", "near", "dot", "dot", "scale", "div", "abs", "pyth", "bad"])
    if kind in ("add", "near"):
        a = rvec()
        b = near(a) if kind == "near" else rvec(len(a))
        op = R.choice(["+", "-"])
        if kind == "near" and op == "-":
            b = [str(W.minus(dz(x))) if dz(x) != 0 else "0" for x in b]
        r = [C34.add(dz(x), dz(y)) if op == "+" else C34.subtract(dz(x), dz(y)) for x, y in zip(a, b)]
        keys = vkeys(a) + ["ENTER"] + vkeys(b) + [op]
        exp = "X=[" + ",".join(str(x) for x in r) + "] Y=0 L=" + vtext(b)
    elif kind == "dot":
        n = R.choice([1, 2, 3])
        a, b = rvec(n), rvec(n)
        if R.random() < 0.3:
            b = [rnum(0, 0) for _ in range(n)]
            a[-1] = str(C34.minus(W.multiply(dz(a[0]), dz(b[0]))))   # makes cancellation likely
        s = D(0)
        for x, y in zip(a, b):
            s = W.add(s, W.multiply(dz(x), dz(y)))
        keys = vkeys(a) + ["ENTER"] + vkeys(b) + ["*"]
        exp = "X=" + sx(s) + " L=" + vtext(b)
    elif kind == "scale":
        a, k = rvec(), rnum()
        first = R.random() < 0.5
        r = [C34.multiply(dz(x), dz(k)) for x in a]
        keys = (keys_for(k) + ["ENTER"] + vkeys(a) if first else vkeys(a) + ["ENTER"] + keys_for(k)) + ["*"]
        exp = "X=[" + ",".join(str(x) for x in r) + "]"
    elif kind == "div":
        a, k = rvec(), rnum(zero=0.1)
        keys = vkeys(a) + ["ENTER"] + keys_for(k) + ["/"]
        if dz(k) == 0:
            exp = "ERR X=0 Y=" + vtext(a)
        else:
            r = [C34.divide(dz(x), dz(k)) for x in a]
            exp = "X=[" + ",".join(str(x) for x in r) + "] L=" + sx(dz(k))
    elif kind in ("abs", "pyth"):
        a = pythagorean() if kind == "pyth" else rvec()
        s = D(0)
        for x in a:
            s = W.add(s, W.multiply(dz(x), dz(x)))
        keys = vkeys(a) + ["ABS"]
        exp = "X=" + str(C34.sqrt(s)) + " L=" + vtext(a)
    else:
        a = rvec(R.choice([2, 3]))
        b = R.choice([rvec(5 - len(a)), [rnum()]])
        op = R.choice(["+", "-", "*", "/"])
        keys = vkeys(a) + ["ENTER"] + (vkeys(b) if len(b) > 1 else keys_for(b[0])) + [op]
        if len(b) == 1 and op in ("*", "/"):
            continue        # a vector times or over a number is fine, not a refusal
        exp = "ERR"
    if any(t for t in exp.split() if t.startswith("X=") and "E" in t):
        bad = False
        for t in exp.replace("[", " ").replace("]", " ").replace(",", " ").replace("=", " ").split():
            try:
                if not fits(D(t)):
                    bad = True
            except Exception:
                pass
        if bad:
            continue
    # vectors are 35s mode's (unit 041 A3, decision 58)
    out.append(f"{vid} | ours: random vectors ({kind}) | MODE35 {' '.join(keys)} | {exp}")
print("# veckey_fuzz: generated by tests/veckey_fuzz.py")
print("\n".join(out))
