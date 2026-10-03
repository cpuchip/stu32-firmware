#!/usr/bin/env python3
"""eqn_fuzz: random equations against the keyboard path, unit 014.

    python3 tests/eqn_fuzz.py SEED COUNT > f; build/vectors f

Each case is a random expression tree over numbers, the variables A..E, + - × ÷ ^, unary minus,
!, the one-argument functions SQ SQRT INV ABS SIN COS EXP LN IP FP and the two-argument IDIV RMDR
XROOT nCr. It is written two ways: as equation text, with the fewest parentheses the precedence
table allows (p.6-15; left to right within a level: ours), evaluated with XEQ; and as RPN keys for
the same operations in the same order (trees needing at most four stack levels). The two X values
must be identical (%SAMEX): the equation path is the keyboard's functions (unit 014 rule 2).
Cases whose value a float check finds undefined or huge are skipped. Standard library only.
"""
import math
import random
import sys

R = random.Random(int(sys.argv[1]))
N = int(sys.argv[2])
VARS = "ABCDE"
PREC = {"=": 0, "+": 1, "-": 1, "*": 2, "/": 2, "neg": 3, "^": 4}
F1 = {"SQ": "SQ", "SQRT": "SQRT", "INV": "INV", "ABS": "ABS", "SIN": "SIN", "COS": "COS",
      "EXP": "EXP", "LN": "LN", "IP": "IP", "FP": "FP"}
F2 = {"IDIV": "IDIV", "RMDR": "RMDR", "XROOT": "XROOT", "nCr": "NCR"}


def leaf():
    if R.random() < 0.45:
        return ("var", R.choice(VARS))
    v = R.choice([str(R.randint(1, 20)), f"{R.randint(0, 9)}.{R.randint(1, 99)}", f"{R.randint(1, 9)}.{R.randint(0, 9)}E{R.randint(1, 3)}"])
    return ("num", v)


def tree(d):
    if d <= 0 or R.random() < 0.25:
        return leaf()
    c = R.random()
    if c < 0.5:
        return (R.choice(["+", "-", "*", "/", "^"]), tree(d - 1), tree(d - 1))
    if c < 0.62:
        return ("neg", tree(d - 1))
    if c < 0.68:
        return ("!", ("num", str(R.randint(0, 8))))
    if c < 0.9:
        return ("f1", R.choice(list(F1)), tree(d - 1))
    return ("f2", R.choice(list(F2)), tree(d - 1), tree(d - 1))


def text(t, ctx=0, right=False):
    """equation text; ctx the precedence of the parent, right whether t is its right operand"""
    k = t[0]
    if k == "num":
        return t[1]
    if k == "var":
        return t[1]
    if k == "!":
        return text(t[1], 5) + "!"
    if k == "f1":
        return f"{t[1]}({text(t[2])})"
    if k == "f2":
        return f"{t[1]}({text(t[2])},{text(t[3])})"
    if k == "neg":
        inner = text(t[1], PREC["neg"])
        s = "-" + inner
        # unary minus sits below ^ and above × ÷: it needs parentheses as an operand of ^ base or
        # inside a higher level's left side; as a × ÷ operand or exponent it can stand
        need = ctx > PREC["neg"] and not (ctx == PREC["^"] and right)
        return f"({s})" if need else s
    p = PREC[k]
    sym = {"+": "+", "-": "-", "*": "×", "/": "÷", "^": "^"}[k]
    s = text(t[1], p, False) + sym + text(t[2], p, True)
    need = p < ctx or (p == ctx and right)
    return f"({s})" if need else s


def depth(t):
    """stack levels the RPN keys need (Sethi-Ullman style)"""
    k = t[0]
    if k in ("num", "var"):
        return 1
    if k in ("neg", "f1"):
        return depth(t[-1])
    if k == "!":
        return 1
    a, b = depth(t[-2]), depth(t[-1])
    if k == "f2" and t[1] == "XROOT":
        a, b = b, a                     # keyed radicand first (p.6-16)
    return max(a, b + 1)


def keys(t, out):
    """RPN keys; "#" marks a number still being typed (CHS keeps it so), for the ENTER that must
    separate it from a number typed next"""
    k = t[0]
    if k == "num":
        if out and out[-1] == "#":
            out.append("ENTER")
        m, _, e = t[1].partition("E")
        out.append(m)
        if e:
            out += ["E", e]
        out.append("#")
        return
    if k == "var":
        out.append(f"RCL:{t[1]}")
        return
    if k == "neg":
        keys(t[1], out)
        if out[-1] == "#":
            # the typed number's sign, keyed after its mantissa (after the exponent's digits CHS
            # would negate the exponent, unit 001): the entry goes on
            i = len(out) - 2
            if len(out) >= 4 and out[-3] == "E":
                out.insert(len(out) - 3, "CHS")
            else:
                out[-1:] = ["CHS", "#"]
        else:
            out.append("CHS")
        return
    if k == "!":
        keys(t[1], out)
        out.append("FACT")
        return
    if k == "f1":
        keys(t[2], out)
        out.append(F1[t[1]])
        return
    if k == "f2":
        if t[1] == "XROOT":         # XROOT(a, b) is RPN's b ENTER a XROOT (p.6-16)
            keys(t[3], out)
            keys(t[2], out)
        else:
            keys(t[2], out)
            keys(t[3], out)
        out.append(F2[t[1]])
        return
    keys(t[1], out)
    keys(t[2], out)
    out.append({"+": "+", "-": "-", "*": "*", "/": "/", "^": "POW"}[k])


def exp_checked(x):
    r = math.exp(x)
    if r == 0 or r < 1e-300 or r > 1e300:
        raise ValueError                # beyond the range: both paths report it (skipped)
    return r


def fval(t, env):
    k = t[0]
    if k == "num":
        return float(t[1])
    if k == "var":
        return env[t[1]]
    if k == "neg":
        return -fval(t[1], env)
    if k == "!":
        return float(math.factorial(int(t[1][1])))
    if k == "f1":
        x = fval(t[2], env)
        return {"SQ": lambda: x * x, "SQRT": lambda: math.sqrt(x), "INV": lambda: 1 / x, "ABS": lambda: abs(x),
                "SIN": lambda: 0.0 if x % 180 == 0 else math.sin(math.radians(x)),   # exact in DEG (unit 003)
                "COS": lambda: 0.0 if (x - 90) % 180 == 0 else math.cos(math.radians(x)),
                "EXP": lambda: exp_checked(x), "LN": lambda: math.log(x), "IP": lambda: float(int(x)),
                "FP": lambda: x - int(x)}[t[1]]()
    if k == "f2":
        a, b = fval(t[2], env), fval(t[3], env)
        if t[1] in ("IDIV", "RMDR"):
            if a != int(a) or b != int(b) or b == 0:
                raise ValueError
            return float(int(a / b)) if t[1] == "IDIV" else math.fmod(a, b)
        if t[1] == "XROOT":
            if a == 0 or b < 0:
                raise ValueError
            r = b ** (1 / a)
            if r == 0 or r < 1e-300 or r > 1e300:
                raise ValueError
            return r
        if a != int(a) or b != int(b) or a < 0 or b < 0 or a > 60:
            raise ValueError
        return float(math.comb(int(a), int(b)))
    a, b = fval(t[1], env), fval(t[2], env)
    if k == "+":
        return a + b
    if k == "-":
        return a - b
    if k == "*":
        return a * b
    if k == "/":
        return a / b
    if a < 0 and b != int(b):
        raise ValueError
    if a == 0 and b <= 0:
        raise ValueError
    r = a ** b
    if a != 0 and (r == 0 or abs(r) < 1e-300 or abs(r) > 1e300):
        raise ValueError                # beyond the range (both paths report it: skipped)
    return r


out = []
n = 0
while n < N:
    t = tree(R.randint(1, 4))
    if depth(t) > 4:
        continue
    env = {v: R.choice([R.randint(1, 9), R.randint(-9, 9) + R.randint(1, 9) / 10]) for v in VARS}
    try:
        v = fval(t, env)
        if not math.isfinite(v) or abs(v) > 1e300 or (v != 0 and abs(v) < 1e-300):
            continue
    except (ValueError, ZeroDivisionError, OverflowError):
        continue
    txt = text(t)
    if len(txt.encode()) > 180:
        continue
    k = []
    keys(t, k)
    rpn = " ".join(x for x in k if x != "#")
    pre = " ".join(f"{abs(env[x])}{' CHS' if env[x] < 0 else ''} STO:{x}" for x in VARS)
    out.append(f"Q{n:05d} | ours: equation and keyboard agree | {pre} DEG EQ:{txt} XEQ RSALL SAVEX {rpn} | %SAMEX=1")
    n += 1
print("# eqn_fuzz: generated by tests/eqn_fuzz.py")
print("\n".join(out))
