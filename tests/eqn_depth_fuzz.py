#!/usr/bin/env python3
"""eqn_depth_fuzz: tests/eqn_depth.py (the rule in Python, for scanning vector sets) against tests/eqn_depth.c (the
parser itself) on random and adversarial equations up to 191 bytes in each model: the units must agree on every one,
and with them the NESTED TOO DEEP verdict (unit 047).

    python3 tests/eqn_depth_fuzz.py build/eqn_depth SEED N        exit 1 on any difference
"""
import os
import random
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eqn_depth  # noqa: E402

R = random.Random(0)                    # reseeded by the caller (tests/eqn_stack_fuzz.py shares the shapes)
FUNCS = [n.decode() for n, k, _ in eqn_depth.NAMES if k != "K0"]
CONSTS = [n.decode() for n, k, _ in eqn_depth.NAMES if k == "K0"]
TOKENS = ["(", ")", "[", "]", ",", ":", "^", "-", "+", "×", "÷", "!", "=", " ", "X", "A", "B", "2", "3.5",
          "1E-3", "2E", ".", "1Fh", "101b", "i", "2i3", "3θ4", eqn_depth.IND_I.decode(), "REGX", "π"]


def soup():
    out = ""
    while len(out.encode()) < R.randint(1, 191):
        k = R.random()
        out += R.choice(TOKENS) if k < 0.6 else R.choice(FUNCS) + "(" if k < 0.85 else R.choice(CONSTS)
    return out


def operand():
    return R.choice(["X", "A", "2", "3.5", "1E-3", "π", "REGY", "1Fh", eqn_depth.IND_I.decode(), "2i3"])


def nest(model):
    """a structure that recurses: groups, vectors, functions (two-argument ones with their model's separator),
    power chains, unary minuses and implied products, to about 191 bytes"""
    sep = ":" if model == "33" else ","

    def build(budget, depth):
        if budget < 6 or depth > 40:
            return operand()
        k = R.random()
        if k < 0.2:
            return "(" + build(budget - 2, depth + 1) + ")"
        if k < 0.3:
            return "[" + build(budget - 3, depth + 1) + ("," + operand() if R.random() < 0.5 else "") + "]"
        if k < 0.5:
            f = R.choice(FUNCS)
            two = f in ("IDIV", "RMDR", "XROOT", "%CHG", "nCr", "nPr", "AND", "OR", "XOR", "NAND", "NOR", "%",
                        "Cn,r", "Pn,r")
            return f + "(" + build(budget - len(f) - 2, depth + 1) + (sep + operand() if two else "") + ")"
        if k < 0.65:
            return operand() + "^" + build(budget - 3, depth + 1)
        if k < 0.75:
            return "-" * R.randint(1, 5) + build(budget - 5, depth + 1)
        if k < 0.85:
            return operand() + R.choice(["+", "-", "×", "÷", ""]) + build(budget - 3, depth + 1)
        return operand() + "^-" + build(budget - 4, depth + 1)

    t = build(191, 0)
    return t if len(t.encode()) <= 191 else t.encode()[:191].decode("utf-8", "ignore")


def deep(model):
    """the shapes at the limit: n groups or X^( levels around a chain, plus or minus one"""
    n = R.randint(14, 22)
    opener = R.choice(["(", "X^(", "SIN(", "[", "-("])
    close = "]" if opener == "[" else ")"
    core = R.choice(["X", "X^X^X", "X^" * R.randint(1, 40) + "X", "-" * R.randint(1, 60) + "X"])
    t = opener * n + core + close * n
    return t if len(t.encode()) <= 191 else t.encode()[:191].decode("utf-8", "ignore")


def cases(n):
    """n equations: a third token soup, the rest structures that recurse and the shapes at the limit, in each model"""
    out = []
    for _ in range(n):
        m = R.choice(["STU", "35", "33"])
        g = R.random()
        out.append((m, (soup() if g < 0.4 else nest(m) if g < 0.8 else deep(m)).replace("\n", "")))
    return out


if __name__ == "__main__":
    DRIVE, N = sys.argv[1], int(sys.argv[3])
    R.seed(int(sys.argv[2]))
    cs = cases(N)
    out = subprocess.run([DRIVE], input="".join(f"{m} {t}\n" for m, t in cs), capture_output=True,
                         text=True).stdout.split("\n")
    bad = deepn = 0
    for (m, t), got in zip(cs, out):
        u = eqn_depth.units(t, m)
        want = f"{u}{' TOO_DEEP' if u > eqn_depth.LIMIT else ''}"
        deepn += u > eqn_depth.LIMIT
        if got != want:
            bad += 1
            if bad <= 8:
                print(f"DIFFER {m} {t!r}: C {got!r}, Python {want!r}")
    if len(out) - 1 != len(cs):
        bad += 1
        print(f"eqn_depth_fuzz: {len(out) - 1} results for {len(cs)} cases")
    print(f"eqn_depth_fuzz: {len(cs)} equations ({deepn} past {eqn_depth.LIMIT}), {bad} differing")
    sys.exit(1 if bad else 0)
