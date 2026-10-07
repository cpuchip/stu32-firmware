#!/usr/bin/env python3
"""cxkey_fuzz: random complex numbers typed through the calculator's keys, unit 009.

    python3 tests/cxkey_fuzz.py SEED COUNT > f; build/vectors f      (needs mpmath)

Each sequence types complex (and real) operands as x CI y or r CA a and applies one operation:
+ - x / (exact decimal reference, each part correctly rounded: "="), y^x, 1/x, LN, e^x, SIN, COS,
TAN, ABS, ARG (mpmath at 150 digits, "^": normwise, unit 009 rule 4), CHS, recall arithmetic, or
polar entry alone; the stack and LAST X are checked as well. `make accuracy` runs it.
"""
import random
import sys
from decimal import Decimal as D, Context, ROUND_HALF_EVEN

try:
    import mpmath as mp
except ImportError:
    print("cxkey_fuzz: mpmath is not installed; skipped", file=sys.stderr)
    sys.exit(0)

mp.mp.dps = 150
C34 = Context(prec=34, rounding=ROUND_HALF_EVEN)
W = Context(prec=500)
R = random.Random(int(sys.argv[1]))
N = int(sys.argv[2])


def rnum(lo_e=-6, hi_e=6, zero=0.06):
    if R.random() < zero:
        return "0"
    sign = "-" if R.random() < 0.45 else ""
    nd = R.randint(1, R.choice([3, 12, 34]))
    digits = str(R.randint(1, 9)) + "".join(str(R.randint(0, 9)) for _ in range(nd - 1))
    pt = R.randint(1, min(nd, 3))     # at most three integer digits before the exponent
    m = digits[:pt] + ("." + digits[pt:] if pt < nd else "")
    e = R.randint(lo_e, hi_e) if R.random() < 0.3 else 0
    return sign + m + (f"E{e}" if e else "")


def keys_for(v):
    neg = v.startswith("-")
    v = v.lstrip("-")
    if "E" in v:
        m, e = v.split("E")
        out = [m] + (["CHS"] if neg else []) + ["E", e.lstrip("-")] + (["CHS"] if e.startswith("-") else [])
    else:
        out = [v] + (["CHS"] if neg else [])
    return out


def ckeys(re, im):
    return keys_for(re) + ["CI"] + keys_for(im)


def dz(s):
    return W.create_decimal(s)


def sx(d):
    return str(C34.plus(d))


def s36(x):
    return mp.nstr(x, 36, min_fixed=-30, max_fixed=30).replace("e", "E")


def cz(z):
    return s36(mp.re(z)) + "i" + s36(mp.im(z))


def operand(complex_=True, lo=-6, hi=6):
    if complex_:
        return (rnum(lo, hi), rnum(lo, hi))
    return (rnum(lo, hi), None)


def typed(o):
    return ckeys(o[0], o[1]) if o[1] is not None else keys_for(o[0])


def val(o):
    """The register text of a typed operand (exact: at most 34 digits)."""
    if o[1] is None:
        return sx(dz(o[0]))
    return sx(dz(o[0])) + "i" + sx(dz(o[1]))


def mpc(o):
    return mp.mpc(mp.mpf(o[0]), mp.mpf(o[1] or "0"))


def exact_op(op, a, b):
    ar, ai = dz(a[0]), dz(a[1] or "0")
    br, bi = dz(b[0]), dz(b[1] or "0")
    if op == "+":
        return C34.add(ar, br), C34.add(ai, bi)
    if op == "-":
        return C34.subtract(ar, br), C34.subtract(ai, bi)
    if op == "*":
        return (C34.plus(W.subtract(W.multiply(ar, br), W.multiply(ai, bi))),
                C34.plus(W.add(W.multiply(ar, bi), W.multiply(ai, br))))
    den = W.add(W.multiply(br, br), W.multiply(bi, bi))
    if den == 0:
        return None
    return (C34.divide(W.add(W.multiply(ar, br), W.multiply(ai, bi)), den),
            C34.divide(W.subtract(W.multiply(ai, br), W.multiply(ar, bi)), den))


def xr(z):
    """The expectation for a transcendental complex result: normwise; an error beyond the range
    (the larger part above the largest number: a complex overflow stays an error, unit 026 with
    abacus #4198); 0i0 when the whole number is below the smallest (unit 026 rule 4: each part
    underflows to 0)."""
    m = max(abs(mp.re(z)), abs(mp.im(z)))
    if m > mp.mpf("9.999999999999999999999999999999999e6144"):
        return "ERR"
    if m != 0 and m < mp.mpf("1e-6143"):
        return "X=0i0"
    return "X^" + cz(z)


def cx_text(re, im):
    return f"{sx(re)}i{sx(im)}"


out = []
for i in range(N):
    kind = R.choice(["bin", "bin", "bin", "pow", "one", "one", "one", "polar", "chs", "rcl"])
    vid = f"K{i:05d}"
    if kind in ("bin", "pow"):
        cx1, cx2 = R.choice([(True, True), (True, False), (False, True)])
        if kind == "pow":
            a, b = operand(cx1, -2, 2), operand(cx2, -1, 1)
            if R.random() < 0.3:
                b = (str(R.randint(-12, 12)), None if R.random() < 0.7 and a[1] is not None else "0")
            op = "POW"
        else:
            a, b = operand(cx1), operand(cx2)
            op = R.choice(["+", "-", "*", "/"])
        keys = typed(a) + ["ENTER"] + typed(b) + [op]
        if op == "POW":
            za, zb = mpc(a), mpc(b)
            if za == 0:
                exp = "X=0i0" if zb.real > 0 else "ERR"
            else:
                w = zb * mp.log(za)
                if abs(w.real) > 14000 or abs(w.imag) > 1e30:
                    continue
                exp = xr(mp.exp(w))
        else:
            r = exact_op(op, a, b)
            exp = "ERR" if r is None else "X=" + cx_text(*r)
        if exp == "ERR":
            exp += " X=" + val(b) + " Y=" + val(a)
        else:
            exp += " Y=0 L=" + val(b)
        src = "ours: complex " + op
    elif kind == "one":
        f = R.choice(["INV", "LN", "EXP", "SIN", "COS", "TAN", "ABS", "ARG"])
        a = operand(True, -4, 2)
        mode = R.choice(["DEG", "RAD", "GRAD"])
        z = mpc(a)
        keys = [mode] + typed(a) + [f]
        if f == "INV":
            r = exact_op("/", ("1", "0"), a)
            exp = "ERR" if r is None else "X=" + cx_text(*r)
        elif f == "LN":
            exp = "ERR" if z == 0 else xr(mp.log(z))
        elif f == "EXP":
            exp = xr(mp.exp(z))
        elif f in ("SIN", "COS", "TAN"):
            exp = xr({"SIN": mp.sin, "COS": mp.cos, "TAN": mp.tan}[f](z))
        elif f == "ABS":
            exp = "X^" + s36(abs(z))
        else:
            k = {"DEG": 180 / mp.pi, "RAD": 1, "GRAD": 200 / mp.pi}[mode]
            exp = "X^" + s36(mp.atan2(z.imag, z.real) * k) if z != 0 else "X=0"
        exp += (" X=" + val(a)) if exp == "ERR" else (" L=" + val(a))
        src = "ours: complex " + f
    elif kind == "polar":
        mode = R.choice(["DEG", "RAD", "GRAD"])
        r, ang = rnum(-4, 4, 0), rnum(-1, 2)
        k = {"DEG": mp.pi / 180, "RAD": 1, "GRAD": mp.pi / 200}[mode]
        zz = mp.mpf(r) * mp.expj(mp.mpf(ang) * k)
        keys = [mode] + keys_for(r) + ["CA"] + keys_for(ang) + ["ENTER"]
        exp = "X^" + cz(zz) + " Y^" + cz(zz)
        src = "ours: polar entry"
    elif kind == "chs":
        a = operand(True)
        keys = typed(a) + ["ENTER", "CHS"]
        exp = "X=" + cx_text(C34.minus(dz(a[0])) if dz(a[0]) != 0 else D(0), C34.minus(dz(a[1])) if dz(a[1]) != 0 else D(0))
        exp += " Y=" + val(a)
        src = "ours: CHS of a complex number"
    else:
        a, b = operand(R.random() < 0.7), operand(True)
        op = R.choice(["+", "-", "*", "/"])
        tok = {"+": "RCL+:B", "-": "RCL-:B", "*": "RCL*:B", "/": "RCL/:B"}[op]
        keys = typed(b) + ["STO:B", "CLX"] + typed(a) + [tok]
        r = exact_op(op, a, b)
        exp = ("ERR X=" + val(a)) if r is None else ("X=" + cx_text(*r) + " L=" + val(a) + " @B=" + val(b))
        src = "ours: recall arithmetic with a complex variable"
    # one-value complex numbers are 35s mode's (unit 041 A1, decision 58)
    out.append(f"{vid} | {src} | MODE35 {' '.join(keys)} | {exp}")

print("# cxkey_fuzz: generated by tests/cxkey_fuzz.py; mpmath " + mp.__version__ + " at 150 digits")
print("\n".join(out))
