#!/usr/bin/env python3
"""frac_fuzz: random values, /c settings and formats for Fraction-display mode, each judged by brute
force over every denominator, exactly against the 34-digit value (unit 007 rules 3-5). Writes a
formatting vector file to stdout: python3 tests/frac_fuzz.py SEED COUNT > f; build/fmt_vectors f
(standard library only; `make accuracy` runs it)."""
import random
import sys
from decimal import Decimal as D
from fractions import Fraction as Fr


def frac_text(v, c, fmt):
    """reference: the text and indicator for value string v, maximum denominator c, format P/F/X"""
    x = Fr(D(v)); neg = x < 0; a = abs(x); I = int(a); f = a - I
    if fmt == "P":
        best = None
        for d in range(1, c + 1):
            n0 = (2 * f * d + 1) // 2
            for n in (n0 - 1, n0, n0 + 1):
                if 0 <= n <= d:
                    key = (abs(f - Fr(n, d)), d, -Fr(n, d))
                    if best is None or key < best[0]: best = (key, n, d)
        q = Fr(best[1], best[2]); n, d = q.numerator, q.denominator
    else:
        n, d = int((2 * f * c + 1) // 2), c
        if fmt == "F" and n: q = Fr(n, d); n, d = q.numerator, q.denominator
    ind = (f > Fr(n, d)) - (f < Fr(n, d))
    if n == d: I += 1; n = 0; d = d if fmt == "X" else 1
    shown_neg = neg and not (I == 0 and n == 0 and ind == 0)
    t = ("-" if shown_neg else "") + str(I) + ((f" {n}/{d}") if (n or fmt == "X") else "")
    return t + (" ^" if ind > 0 else " v" if ind < 0 else "")

def main():
    R = random.Random(int(sys.argv[1]))
    lines = []
    for t in range(int(sys.argv[2])):
        nd = R.randint(1, 34)
        digits = str(R.randint(1, 9)) + "".join(str(R.randint(0, 9)) for _ in range(nd - 1))
        v = ("-" if R.random() < .4 else "") + digits + "E" + str(R.randint(-40, 2))
        if len(str(int(abs(D(v))))) > 30:
            continue
        c = R.choice([1, 2, 3, 4, 5, 8, 10, 12, 16, 32, 60, 64, 100, 819, 1000, R.randint(2, 4095), 4095])
        fmt = R.choice("PFX")
        lines.append(f"Z{t} | fuzz | {v} | FRAC {c} {fmt} | w=80 | {frac_text(v, c, fmt)}")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
