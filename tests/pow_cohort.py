#!/usr/bin/env python3
"""pow_cohort: yˣ for integer x on two fn_drive builds, compared as engine text (coefficient and exponent, so the
cohort as well as the value), case for case (unit 050, abacus #5305). Aimed at the edges, not only at random 34-digit
bases: n from -66 to 66 (past POW_XNUM_MAX on both sides), bases of 1 to 34 digits and with trailing zeros, powers of
ten, 1, -1, +0, -0, negative bases (odd and even n), 0 to negative n, and bases whose powers land at the overflow and
underflow edges and among the subnormals.

    python3 tests/pow_cohort.py REF_DRIVE NEW_DRIVE SEED N       exit 1 on any difference
"""
import random
import subprocess
import sys

REF, NEW, SEED, N = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
R = random.Random(SEED)


def digits(k):
    return str(R.randint(1, 9)) + "".join(str(R.randint(0, 9)) for _ in range(k - 1))


def base():
    g = R.random()
    if g < 0.45:                                    # a full 34-digit base, any exponent the power can bear
        d, e = digits(34), R.randint(-200, 200)
    elif g < 0.65:                                  # a shorter one
        d, e = digits(R.randint(1, 33)), R.randint(-60, 60)
    elif g < 0.75:                                  # trailing zeros in the coefficient
        d, e = digits(R.randint(1, 20)) + "0" * R.randint(1, 14), R.randint(-30, 30)
    elif g < 0.85:                                  # 1, -1, 0, -0, a power of ten
        return R.choice(["1", "-1", "0", "-0", "1E" + str(R.randint(-40, 40)), "10", "1.000000000000000000000000000000000"])
    else:                                           # near the range's edges once raised (overflow, underflow, subnormal)
        d = digits(R.choice([1, 2, 34]))
        e = R.choice([6144, -6143, -6176, 3072, -3088, 2048, -2058, 1536, 96, -96]) // R.choice([1, 2, 3, 4, 32, 64]) - len(d)
    s = ("-" if R.random() < 0.25 else "") + d + "E" + str(e)
    return s


def exponent():
    g = R.random()
    if g < 0.7:
        return R.randint(-66, 66)
    return R.choice([0, 1, 2, -1, -2, 3, -3, 63, 64, 65, -63, -64, -65])


cases = [(base(), exponent()) for _ in range(N)]
cases += [(b, n) for b in ("0", "-0", "1", "-1", "2", "-2.5", "9.999999999999999999999999999999999E6144",
                           "1E-6176", "1.000000000000000000000000000000001", "0.9999999999999999999999999999999999")
          for n in (-65, -64, -3, -2, -1, 0, 1, 2, 3, 64, 65)]
# Unit 054 (abacus #5546), named: the skip's bound |n|(d-1)+1 > 34 at its edge. d = 12, n = 3 is exactly 34:
# 1.00000000001 cubed is exact (the least such base), 2.15443469003 cubed the greatest exact one and 2.15443469004
# cubed the first that is not; d = 13, n = 3 (37) is skipped; trailing zeros typed leave d at 12. With d = 1 the bound
# never skips: 2^65 to 2^112 and 3^65 to 3^71 are exact past POW_XNUM_MAX, where a d·n bound would send them to the
# high-precision route.
NAMED = [("1.00000000001", 3), ("1.00000000001", -3), ("-1.00000000001", 3), ("1.00000000002", 3),
         ("2.15443469003", 3), ("2.15443469004", 3), ("1.000000000001", 3), ("1.0000000000100", 3),
         ("100000000001E-11", 3), ("1.00000000001", 2), ("1.1", 33), ("1.1", 34)]
NAMED += [("2", n) for n in (34, 35, 64, 65, 66, 100, 112, 113, -65, -100)]
NAMED += [("3", n) for n in (65, 71, 72)] + [("0.2", 100), ("2E-50", 100), ("-2", 101), ("20", 100)]
cases += NAMED
lines = "".join(f"POW RAD {n} {b}\n" for b, n in cases)     # fn_drive's POW takes the exponent first (x), then y


def run(drive):
    return subprocess.run([drive], input=lines, capture_output=True, text=True).stdout.split("\n")


ref, new = run(REF), run(NEW)
bad = 0
for (b, n), r, w in zip(cases, ref, new):
    if r != w:
        bad += 1
        if bad <= 10:
            print(f"DIFFER {b} ^ {n}: ref {r!r}, new {w!r}")
if len(ref) != len(new) or len(ref) - 1 != len(cases):
    bad += 1
    print(f"pow_cohort: {len(ref) - 1} and {len(new) - 1} results for {len(cases)} cases")
errs = sum(1 for r in ref if r.startswith("ERR"))
print(f"pow_cohort: {len(cases)} cases ({errs} errors on the reference), {bad} differing")
sys.exit(1 if bad else 0)
