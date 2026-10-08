#!/usr/bin/env python3
"""bi_fuzz: bigint_impl.h's arithmetic (bigint, 760 limbs; sbig, 72) against Python's exact integers (unit 045: the
limb steps sped up, every result unchanged). Random operands and the carries' hard cases (limbs of 999999999, powers
of ten and their neighbours, operands at the capacity), each operation's room rule as the code states it.

    python3 tests/bi_fuzz.py build/bi_drive SEED N      N cases per operation and type; exit 1 on any difference
"""
import hashlib
import random
import subprocess
import sys

sys.set_int_max_str_digits(100000)
DRIVE, R, N = sys.argv[1], random.Random(int(sys.argv[2])), int(sys.argv[3])
CAP = {"W": 760, "S": 72}


def limbs(x):
    return 0 if x == 0 else (len(str(x)) + 8) // 9


def num(cap, most=None):
    """a non-negative integer of up to `most` limbs (default the capacity), often an awkward one"""
    most = most or cap
    k = R.random()
    nd = R.randint(1, 9 * most)
    if k < 0.1:
        return 0
    if k < 0.25:
        return 10 ** R.randint(0, 9 * most - 1) - R.choice([0, 1])            # 10^j or 99...9
    if k < 0.35:
        return 10 ** R.randint(1, 9 * most - 1) + R.randint(-3, 3) % 10       # beside a power of ten
    if k < 0.45:
        return int("999999999" * R.randint(1, most))                            # every limb 999999999
    if k < 0.55:
        return R.randint(0, 2**32)
    return int(str(R.randint(1, 9)) + "".join(str(R.randint(0, 9)) for _ in range(nd - 1)))


def u32():
    """a 32-bit operand; for DIVU, divmod_u32's own edges too: the last divisor its 32-bit step takes (57358) and the
    first it does not, the powers of ten and of two, 10^9's odd factor"""
    return R.choice([0, 1, 9, 10, 999999999, 1000000000, 2**32 - 1, R.randint(0, 2**32 - 1), R.randint(1, 1000),
                     57358, 57359, 1024, 1953125, 10 ** R.randint(1, 9), 2 ** R.randint(1, 31), R.randint(57000, 60000)])


cases = []
for t, cap in CAP.items():
    for _ in range(N):
        a, b = num(cap), num(cap)
        cases.append((t, "MUL", a, b, "OVF" if a and b and limbs(a) + limbs(b) > cap else str(a * b)))
        a, b = num(cap), num(cap, cap // 2)
        cases.append((t, "MUL", a, b, "OVF" if a and b and limbs(a) + limbs(b) > cap else str(a * b)))
        a, b = num(cap), num(cap)
        cases.append((t, "ADD", a, b, "OVF" if limbs(a + b) > cap else str(a + b)))
        a, b = sorted([num(cap), num(cap)], reverse=True)
        cases.append((t, "SUB", a, b, str(a - b)))
        a, v = num(cap), u32()
        cases.append((t, "MULU", a, v, "OVF" if limbs(a * v) > cap else str(a * v)))
        a, v = num(cap), u32()
        cases.append((t, "ADDU", a, v, "OVF" if limbs(a + v) > cap else str(a + v)))
        a, v = R.choice([num(cap), R.randint(0, 2**33)]), u32()
        cases.append((t, "SUBU", a, v, "NEG" if a < v else str(a - v)))
        a, k = num(cap), R.randint(0, 40)
        cases.append((t, "MULP", a, k, str(a) if a == 0 else "OVF" if limbs(a) + k // 9 > cap or
                      limbs(a * 10**k) > cap else str(a * 10**k)))
        a, k = num(cap), R.randint(0, 9 * cap + 9)
        cases.append((t, "DIVP", a, k, str(a // 10**k)))
        a, d = num(cap), max(1, u32())
        cases.append((t, "DIVU", a, d, f"{a // d} {a % d}"))
        a, d = num(cap, 25), max(1, num(cap, R.randint(1, 20)))                 # long division: about 45 digits
        cases.append((t, "DIV", a, d, f"{a // d} {int(a % d != 0)}"))
        a, b = num(cap), R.choice([num(cap), None])
        b = a if b is None else b
        cases.append((t, "CMP", a, b, str((a > b) - (a < b))))
        a = num(cap)
        cases.append((t, "NDIG", a, 0, str(len(str(a)) if a else 0)))

inp = "".join(f"{t} {op} {a} {b}\n" for t, op, a, b, _ in cases)
out = subprocess.run([DRIVE], input=inp, capture_output=True, text=True).stdout.split("\n")
bad = 0
for (t, op, a, b, want), got in zip(cases, out):
    if got != want:
        bad += 1
        if bad <= 8:
            print(f"WRONG {t} {op} {str(a)[:40]}.. {str(b)[:40]}..: got {got[:60]} want {want[:60]}")
if len(out) - 1 != len(cases):
    bad += 1
    print(f"bi_fuzz: {len(out) - 1} output lines for {len(cases)} cases")
print(f"bi_fuzz: {len(cases)} cases, {bad} wrong; output digest {hashlib.sha256('\n'.join(out).encode()).hexdigest()[:16]}")
sys.exit(1 if bad else 0)
