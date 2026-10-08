#!/usr/bin/env python3
"""fn_digest: a fixed 100,000 calls of the core's functions (tests/fn_drive), their results digested (unit 045: hp
and the limb arithmetic sped up, so every correctly rounded result, being unique, must come back byte for byte, and
the engine's and the exact paths with it). The calls are a seeded mix across each function's domain, its near-ties
and its edges; no oracle here, only the record: compare a digest before and after a change, and with OUT the
lines themselves to see which moved.

    python3 tests/fn_digest.py DRIVE [OUT]
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
out = subprocess.run([sys.argv[1]], input="\n".join(lines) + "\n", capture_output=True, text=True).stdout
if len(sys.argv) > 2:
    with open(sys.argv[2], "w") as fh:
        fh.write("".join(f"{a} -> {b}\n" for a, b in zip(lines, out.split("\n"))))
print(f"fn_digest: {len(lines)} calls, {out.count(chr(10))} results, digest {hashlib.sha256(out.encode()).hexdigest()[:16]}")
