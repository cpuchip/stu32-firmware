#!/usr/bin/env python3
"""base_fuzz: random integers typed in mixed bases, base arithmetic with overflow, and logic, against
Python integers in 36-bit two's complement (unit 008). Writes a key vector file to stdout:
python3 tests/base_fuzz.py SEED COUNT > f; build/vectors f (standard library only; make accuracy)."""
import random, sys
R = random.Random(int(sys.argv[1])); N = int(sys.argv[2])
B = 36; MASK = (1 << B) - 1; MAX = (1 << (B - 1)) - 1; MIN = -(1 << (B - 1))
def clamp(v): return (min(max(v, MIN), MAX), v > MAX or v < MIN)
def sgn(u): u &= MASK; return u - (1 << B) if u >> (B - 1) else u
def tz(a, b): q = abs(a) // abs(b); return q if (a >= 0) == (b >= 0) else -q
digs = "0123456789ABCDEF"
def typed_ok(v, base, mode_base):
    """the digits of |v| in `base`, read in the current mode's base while typing, stay in range"""
    u, s = abs(v), ""
    while True:
        s = digs[u % base] + s; u //= base
        if u == 0: break
    return int(s, mode_base) <= MAX
def keys_in(v, base):   # type |v| in `base` with its suffix, CHS for negatives
    u, s = abs(v), ""
    while True:
        s = digs[u % base] + s; u //= base
        if u == 0: break
    ks = " ".join("H" + ch if ch in "ABCDEF" else ch for ch in s)
    return ks + " SUF:" + {16: "h", 8: "o", 2: "b", 10: "d"}[base] + (" CHS" if v < 0 else "")
out = []
for t in range(N):
    mode = R.choice(["HEX", "OCT", "BIN", "DEC"])
    a = R.choice([R.randint(-1000, 1000), R.randint(MIN // 2, MAX // 2), R.randint(-MAX, MAX)])
    b = R.choice([R.randint(-1000, 1000), R.randint(MIN // 2, MAX // 2), R.randint(-MAX, MAX), R.randint(1, 64)])
    op = R.choice(["+", "-", "*", "/", "AND", "OR", "XOR", "NAND", "NOR", "NOT"])
    if op in "+-*/" and mode == "DEC": mode = R.choice(["HEX", "OCT", "BIN"])
    if op == "/" and b == 0: b = 7
    # a suffix can only reread digits the current mode lets you type (unit 008 rule 5)
    typeable = {"HEX": [16, 10, 8, 2], "DEC": [10, 8, 2], "OCT": [8, 2], "BIN": [2]}[mode]
    mb = {"HEX": 16, "DEC": 10, "OCT": 8, "BIN": 2}[mode]
    pick = lambda v: R.choice([bb for bb in typeable if typed_ok(v, bb, mb)] or [mb])
    ka, kb = keys_in(a, pick(a)), keys_in(b, pick(b))
    clamped = False
    if op == "+": r, clamped = clamp(a + b)
    elif op == "-": r, clamped = clamp(a - b)
    elif op == "*": r, clamped = clamp(a * b)
    elif op == "/": r, clamped = clamp(tz(a, b))
    elif op == "NOT": r = sgn(~b)
    else:
        f = {"AND": lambda: a & b, "OR": lambda: a | b, "XOR": lambda: a ^ b, "NAND": lambda: ~(a & b), "NOR": lambda: ~(a | b)}[op]
        r = sgn(f())
    keys = f"{mode} {ka} ENTER {kb} {op}" if op != "NOT" else f"{mode} {kb} {op}"
    out.append(f"Z{t} | fuzz | {keys} | {'ERR ' if clamped else ''}X={r}")
print("\n".join(out))
