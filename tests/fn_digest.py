#!/usr/bin/env python3
"""fn_digest: a fixed 100,000 calls of the core's functions (tests/fn_drive), their results digested (unit 045: hp
and the limb arithmetic sped up, so every correctly rounded result, being unique, must come back byte for byte, and
the engine's and the exact paths with it). The calls are a seeded mix across each function's domain, its near-ties
and its edges; no oracle here, only the record: compare a digest before and after a change, and with OUT the
lines themselves to see which moved.

    python3 tests/fn_digest.py DRIVE [OUT [CX_DRIVE]]

Three records: the 100,000 calls above; "more", 30,000 calls of every other fn_drive function (gamma at non-integers,
the hyperbolic and logarithmic helpers, the unit conversions, the integer parts...), since hp's last digits feed the
paths that round without the Ziv check too; and with CX_DRIVE, "complex", 20,000 calls of unit 009's functions.
"""
import hashlib
import random
import subprocess
import sys

R = random.Random(45)


def num(lo, hi, nd=None):
    nd = nd or R.choice([1, 3, 12, 20, 34])
    d = str(R.randint(1, 9)) + "".join(str(R.randint(0, 9)) for _ in range(nd - 1))
    s = "-" if R.random() < 0.5 else ""
    return f"{s}{d[0]}.{d[1:]}E{R.randint(lo, hi)}" if nd > 1 else f"{s}{d}E{R.randint(lo, hi)}"


def pos(lo, hi, nd=None):
    return num(lo, hi, nd).lstrip("-")


def beside(head, rep):
    k = R.randint(1, 30)
    return head + rep * k + "".join(str(R.randint(0, 9)) for _ in range(R.randint(0, 32 - k))) + str(R.randint(1, 9))


GEN = [
    ("EXP", "RAD", lambda: num(-30, 3)), ("LN", "RAD", lambda: pos(-6000, 6000)),
    ("LN", "RAD", lambda: R.choice([beside("1.", "0"), beside("0.", "9")])), ("TENX", "RAD", lambda: num(-30, 3)),
    ("LOG", "RAD", lambda: pos(-6000, 6000)), ("SINH", "RAD", lambda: num(-30, 3)), ("COSH", "RAD", lambda: num(-30, 3)),
    ("TANH", "RAD", lambda: num(-30, 1)), ("ASINH", "RAD", lambda: num(-30, 6000)), ("ACOSH", "RAD", lambda: pos(0, 6000)),
    ("ACOSH", "RAD", lambda: beside("1.", "0")), ("ATANH", "RAD", lambda: num(-30, -1)),
    ("ATANH", "RAD", lambda: R.choice(["", "-"]) + beside("0.", "9")), ("TORAD", "RAD", lambda: num(-6000, 6000)),
    ("TODEG", "RAD", lambda: num(-6000, 6000)), ("SQRT", "RAD", lambda: pos(-6000, 6000)),
    ("POW", "RAD", lambda: f"{pos(-20, 20)} {num(-2, 2)}"), ("POW", "RAD", lambda: f"{R.randint(-9, 9)} {R.randint(-60, 60)}"),
    ("XROOT", "RAD", lambda: f"{pos(-300, 300)} {R.choice([2, 3, 5, -2, 7])}"), ("FACT", "RAD", lambda: str(R.randint(0, 3000))),
    ("NCR", "RAD", lambda: f"{R.randint(0, 3000)} {R.randint(0, 3000)}"),
    ("NPR", "RAD", lambda: f"{R.randint(0, 3000)} {R.randint(0, 300)}"),
    ("TOPOL", "RAD", lambda: f"{num(-20, 20)} {num(-20, 20)}"), ("TOREC", "DEG", lambda: f"{pos(-20, 20)} {num(-3, 3)}"),
]
for m in ("RAD", "DEG", "GRAD"):
    GEN += [(f, m, lambda: num(-30, 4)) for f in ("SIN", "COS", "TAN")]
    GEN += [(f, m, lambda: num(-6000, -1)) for f in ("SIN", "TAN")]     # the tiny angles (044 review)
    GEN += [("ASIN", m, lambda: num(-30, -1)), ("ACOS", m, lambda: num(-30, -1)), ("ATAN", m, lambda: num(-30, 6000))]
GEN += [(f, "RAD", lambda: num(30, 163)) for f in ("SIN", "COS", "TAN")]

lines = []
while len(lines) < 100000:
    f, m, g = R.choice(GEN)
    lines.append(f"{f} {m} {g()}")


def record(name, drive, lines, out_path):
    out = subprocess.run([drive], input="\n".join(lines) + "\n", capture_output=True, text=True).stdout
    if out_path:
        with open(out_path, "w") as fh:
            fh.write("".join(f"{a} -> {b}\n" for a, b in zip(lines, out.split("\n"))))
    print(f"fn_digest: {name}{len(lines)} calls, {out.count(chr(10))} results, digest "
          f"{hashlib.sha256(out.encode()).hexdigest()[:16]}")


OUT = sys.argv[2] if len(sys.argv) > 2 else None
record("", sys.argv[1], lines, OUT)

R = random.Random(4501)                     # (a stream of its own: the record above stays as it was)
MORE = [("FACT", lambda: num(-3, 3)), ("FACT", lambda: f"{R.randint(-170, 3000)}.5"), ("EXPM1", lambda: num(-30, 3)),
        ("LN1P", lambda: num(-30, -1)), ("TWOX", lambda: num(-20, 4)), ("LOG2", lambda: pos(-6000, 6000)),
        ("SQ", lambda: num(-3000, 3000)), ("INV", lambda: num(-6000, 6000)), ("HMS", lambda: num(-6, 6)),
        ("HMSINV", lambda: num(-6, 6)), ("IP", lambda: num(-40, 40)), ("FP", lambda: num(-40, 40)),
        ("INTG", lambda: num(-40, 40)), ("PCT", lambda: f"{num(-50, 50)} {num(-50, 50)}"),
        ("PCTCHG", lambda: f"{num(-30, 30)} {num(-30, 30)}"), ("IDIV", lambda: f"{R.randint(-10**12, 10**12)} {R.randint(1, 10**6)}"),
        ("RMDR", lambda: f"{num(-30, 30)} {num(-30, 30)}")]
MORE += [(u, lambda: num(-300, 300)) for u in ("TOKG", "TOLB", "TOC", "TOF", "TOCM", "TOIN", "TOL", "TOGAL", "TOKM", "TOMILE")]
more = []
while len(more) < 30000:
    f, g = R.choice(MORE)
    more.append(f"{f} RAD {g()}")
record("more ", sys.argv[1], more, OUT + ".more" if OUT else None)
if len(sys.argv) > 3:
    def cnum():
        return num(-30, 4)
    CX = [(f, 1) for f in ("INV", "EXP", "LN", "SIN", "COS", "TAN", "ABS")] + [(f, 2) for f in ("ADD", "SUB", "MUL", "DIV", "POW")]
    cx = []
    while len(cx) < 20000:
        f, k = R.choice(CX)
        cx.append(f + "".join(f" {cnum()} {cnum()}" for _ in range(k)))
    record("complex ", sys.argv[3], cx, OUT + ".cx" if OUT else None)
