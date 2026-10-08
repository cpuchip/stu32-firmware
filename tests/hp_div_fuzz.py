#!/usr/bin/env python3
"""hp_div_fuzz: hp_div against the exact quotient, truncated to p significant digits, with exactly p digits in its
mantissa: what the long division gave and what the paths rounding hp once rely on (unit 045's review: the Newton
reciprocal's first form lost exact quotients, and a negative complex power then rounded an exact tie the wrong way).
Python's Fraction is the oracle; the operands lean on the hard cases (nines, powers of ten, exact multiples).

    python3 tests/hp_div_fuzz.py build/hp_drive SEED N        exit 1 on any difference
"""
import random
import subprocess
import sys
from fractions import Fraction

sys.set_int_max_str_digits(100000)
DRIVE, R, N = sys.argv[1], random.Random(int(sys.argv[2])), int(sys.argv[3])


def digits(n):
    k = R.random()
    if k < 0.15:
        return "9" * n
    if k < 0.25:
        return "1" + "0" * (n - 1)
    if k < 0.35:
        return ("1" + "0" * (n - 2) + "1") if n > 1 else "1"
    if k < 0.45:
        return str(R.randint(1, 9)) + "9" * (n - 1)
    return str(R.randint(1, 9)) + "".join(R.choice("0123456789") for _ in range(n - 1))


def trunc(q, p):
    """q > 0 truncated to p significant digits: (m, e) with 10^(p-1) <= m < 10^p and m 10^e <= q < (m + 1) 10^e"""
    e = len(str(q.numerator // q.denominator)) - p if q >= 1 else -len(str(q.denominator // q.numerator)) - p
    while q >= Fraction(10) ** (e + p):
        e += 1
    while q < Fraction(10) ** (e + p - 1):
        e -= 1
    return int(q / Fraction(10) ** e), e


cases = []
for _ in range(N):
    p = R.choice([1, 2, 9, 10, 14, 20, 34, 38, 55, 60, 64, 65, 70, 105, 155, 185, 220, 229])
    a, b = digits(R.choice([1, 2, 9, 10, 18, 19, 30, R.randint(1, 240)])), digits(R.choice([1, 2, 9, 10, 18, 19, 30, R.randint(1, 240)]))
    if R.random() < 0.3:
        a = str(int(b) * R.randint(1, 1000))                  # an exact quotient
    a, b = ("-" if R.random() < 0.5 else "") + a, ("-" if R.random() < 0.5 else "") + b
    cases.append((p, a, R.randint(-50, 50), b, R.randint(-50, 50)))
out = subprocess.run([DRIVE], input="".join(f"DIV {p} {a} {ea} {b} {eb}\n" for p, a, ea, b, eb in cases),
                     capture_output=True, text=True).stdout.split("\n")
bad = 0
for (p, a, ea, b, eb), got in zip(cases, out):
    q = Fraction(int(a)) * Fraction(10) ** ea / (Fraction(int(b)) * Fraction(10) ** eb)
    m, e = trunc(abs(q), p)
    want = f"{'-' if q < 0 else ''}{m} {e} {p}"
    if got != want:
        bad += 1
        if bad <= 6:
            print(f"WRONG p {p}: {a[:30]}E{ea} / {b[:30]}E{eb}: got {got[:60]} want {want[:60]}")
if len(out) - 1 != len(cases):
    bad += 1
    print(f"hp_div_fuzz: {len(out) - 1} results for {len(cases)} cases")
print(f"hp_div_fuzz: {len(cases)} quotients, {bad} wrong")
sys.exit(1 if bad else 0)
