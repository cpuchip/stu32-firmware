#!/usr/bin/env python3
"""prog_fuzz: random keystroke programs against a reference interpreter, unit 011.

    python3 tests/prog_fuzz.py SEED COUNT > f; build/vectors f

The reference is written from the guide's rules (35s UG ch.13, 14, Appendix B; the unit's notes),
not from core/calc.c: a 4-level stack with the lift flag; a number line lifts when lift is on and
then enables it; ENTER copies and disables; every other line enables lift; tests and FS? skip the
next line when false; ISG/DSE on ccccccc.fffii; XEQ/RTN with a return stack; GTO. Each program is
built from blocks (arithmetic, a do-if-true test, a flag test, an ISG or DSE counted loop, a call to
a subroutine), typed in program entry, run with XEQ A, and the final stack and variables compared
"=" (python decimal, 34 digits, half to even). Standard library only.
"""
import random
import sys
from decimal import Decimal as D, Context, ROUND_HALF_EVEN

C34 = Context(prec=34, rounding=ROUND_HALF_EVEN)
W = Context(prec=500, Emax=999999, Emin=-999999)     # exact; python's default rounds to 28 digits
R = random.Random(int(sys.argv[1]))
N = int(sys.argv[2])
VARS = "BCDEFGH"


def num():
    """a small number: integers, one or two decimals, sometimes negative"""
    v = D(R.randint(0, 99)) if R.random() < 0.6 else C34.divide(D(R.randint(1, 999)), D(R.choice([10, 100])))
    return v.copy_negate() if R.random() < 0.3 and v != 0 else v


def num_keys(v):
    s = format(abs(v), "f")
    return s + (" CHS" if v < 0 else "")


class Machine:
    def __init__(self):
        self.st = [D(0)] * 4          # X Y Z T
        self.lift = True
        self.var = {v: D(0) for v in VARS + "ACIN"}
        self.flags = set()

    def push(self, v):
        if self.lift:
            self.st = [v] + self.st[:3]
        else:
            self.st[0] = v

    def binop(self, f):
        x, y = self.st[0], self.st[1]
        self.st = [C34.plus(f(y, x)), self.st[2], self.st[3], self.st[3]]
        self.lift = True


def loop_step(v, op):
    """ISG / DSE on +-ccccccc.fffii: the new value and whether the next line is skipped"""
    neg = v < 0
    a = v.copy_abs()
    ip = D(int(a))
    frac = W.subtract(a, ip)
    fff = int(W.multiply(frac, 1000))
    ii = int(W.multiply(frac, 100000)) - fff * 100 or 1
    cnt = ip.copy_negate() if neg else ip
    new = W.add(cnt, ii) if op == "ISG" else W.subtract(cnt, ii)
    skip = new > fff if op == "ISG" else new <= fff
    nv = W.add(new.copy_abs(), frac)
    return (nv.copy_negate() if new < 0 else nv), skip


def run(prog, labels, m, budget=100000):
    """prog: list of (op, arg); labels: name -> index of its LBL line"""
    pc, rstack, steps = labels["A"], [], 0
    while steps < budget:
        if pc >= len(prog):
            if rstack:
                pc = rstack.pop()
                continue
            return "done"
        op, arg = prog[pc]
        nxt = pc + 1
        steps += 1
        if op == "NUM":
            m.push(arg)
            m.lift = True
        elif op == "LBL":
            m.lift = True
        elif op == "RTN":
            m.lift = True
            if not rstack:
                return "done"
            nxt = rstack.pop()
        elif op in ("GTO", "XEQ"):
            lab, line = arg
            if op == "XEQ":
                rstack.append(nxt)
            nxt = labels[lab] + line - 1
            m.lift = True
        elif op == "ENTER":
            m.st = [m.st[0]] + m.st[:3]
            m.lift = False
        elif op in "+-*":
            m.binop({"+": W.add, "-": W.subtract, "*": W.multiply}[op])
        elif op == "XY":
            m.st[0], m.st[1] = m.st[1], m.st[0]
            m.lift = True
        elif op == "CHS":
            m.st[0] = m.st[0].copy_negate() if m.st[0] != 0 else D(0)
            m.lift = True
        elif op == "STO":
            m.var[arg] = m.st[0]
            m.lift = True
        elif op == "STO+":
            m.var[arg] = C34.plus(W.add(m.var[arg], m.st[0]))
            m.lift = True
        elif op == "RCL":
            m.push(m.var[arg])
            m.lift = True
        elif op in ("SF", "CF"):
            (m.flags.add if op == "SF" else m.flags.discard)(arg)
            m.lift = True
        elif op == "FS?":
            if arg not in m.flags:
                nxt += 1
            m.lift = True
        elif op == "TEST":
            rel, other = arg
            x = m.st[0]
            y = D(0) if other == "0" else m.st[1]
            t = {"!=": x != y, "<=": x <= y, "<": x < y, ">": x > y, ">=": x >= y, "=": x == y}[rel]
            if not t:
                nxt += 1
            m.lift = True
        elif op in ("ISG", "DSE"):
            m.var[arg], skip = loop_step(m.var[arg], op)
            if skip:
                nxt += 1
            m.lift = True
        else:
            raise ValueError(op)
        pc = nxt
    return "budget"


def arith_block(lines):
    for _ in range(R.randint(1, 4)):
        c = R.random()
        if c < 0.3:
            lines.append(("NUM", num()))
        elif c < 0.45:
            lines.append((R.choice("+-*"), None))
        elif c < 0.55:
            lines.append(("ENTER", None))
        elif c < 0.62:
            lines.append((R.choice(["XY", "CHS"]), None))
        elif c < 0.75:
            lines.append(("STO", R.choice(VARS)))
        elif c < 0.85:
            lines.append(("STO+", R.choice(VARS)))
        else:
            lines.append(("RCL", R.choice(VARS)))


def body(lines, depth, subs):
    for _ in range(R.randint(1, 4)):
        c = R.random()
        if c < 0.4 or depth > 1:
            arith_block(lines)
        elif c < 0.55:
            rel = R.choice(["!=", "<=", "<", ">", ">=", "="])
            lines.append(("TEST", (rel, R.choice("Y0"))))
            lines.append(R.choice([("NUM", num()), ("+", None), ("STO", R.choice(VARS)), ("CHS", None)]))
        elif c < 0.65:
            f = R.randint(0, 4)
            lines.append((R.choice(["SF", "CF"]), f))
            lines.append(("FS?", R.randint(0, 4)))
            lines.append(R.choice([("NUM", num()), ("STO", R.choice(VARS))]))
        elif c < 0.85:
            # a counted loop: counter c.fffii in variable C (outer) or N (inner)
            var = "C" if depth == 0 else "N"
            op = R.choice(["ISG", "DSE"])
            if op == "ISG":
                start, final, step = R.randint(0, 3), R.randint(0, 8), R.randint(1, 3)
            else:
                start, final, step = R.randint(3, 9), R.randint(0, 3), R.randint(1, 3)
            ctl = W.add(W.add(D(start), W.divide(D(final), 1000)), W.divide(D(step), 100000))
            lines.append(("NUM", ctl))
            lines.append(("STO", var))
            top = len(lines)
            body(lines, depth + 1, subs)
            lines.append((op, var))
            lines.append(("GTO", ("@", top)))      # resolved below to label A's line
        elif subs:
            lines.append(("XEQ", (R.choice(subs), 1)))


def make_program():
    subs = [s for s in "BKQ" if R.random() < 0.5]
    main = [("LBL", "A")]
    for v in "CN":
        pass
    body(main, 0, subs)
    main.append(("RTN", None))
    prog = list(main)
    labels = {"A": 0}
    for s in subs:
        labels[s] = len(prog)
        prog.append(("LBL", s))
        arith_block(prog)
        if R.random() < 0.3 and s != subs[-1]:
            prog.append(("XEQ", (subs[subs.index(s) + 1], 1)))
        prog.append(("RTN", None))
    # resolve loop GTOs (indices inside label A, whose LBL is line 0: line = index + 1)
    prog = [(op, ("A", arg[1] + 1)) if op == "GTO" and arg[0] == "@" else (op, arg) for op, arg in prog]
    return prog, labels


def keys(prog, labels):
    """the tokens that key prog in program entry, or None when two number lines are adjacent
    (they cannot be typed apart: the second key would extend the first number)"""
    out = ["PRGM", "CLPGM"]
    prev_num = False
    for op, arg in prog:
        if op in ("NUM", "CHS") and prev_num:
            return None                     # CHS after a number line would join the number too
        prev_num = op == "NUM"
        if op == "NUM":
            out.append(num_keys(arg))
        elif op == "LBL":
            out.append(f"LBL:{arg}")
        elif op in ("GTO", "XEQ"):
            out.append(f"{op}:{arg[0]}{arg[1]:03d}")
        elif op in ("STO", "STO+", "RCL", "SF", "CF", "ISG", "DSE"):
            out.append(f"{op}:{arg}")
        elif op == "FS?":
            out.append(f"FS?:{arg}")
        elif op == "TEST":
            out.append(f"X{arg[0]}{arg[1]}?")
        else:
            out.append(op)
    out.append("PRGM")
    return " ".join(out)


def fmt(v):
    return str(C34.plus(v))


out = []
made = 0
while made < N:
    prog, labels = make_program()
    k = keys(prog, labels)
    if k is None or len(prog) > 400:
        continue
    m = Machine()
    pre = []
    for v in VARS:                        # start the variables at known values
        val = num()
        m.var[v] = val
        pre.append(f"{num_keys(val)} STO:{v}")
    pre.append("CLSTK")
    m.st = [D(0)] * 4
    m.lift = False
    try:
        res = run(prog, labels, m)
    except ArithmeticError:                 # values that leave any sane range: skip the program
        continue
    if res != "done":
        continue
    if any(v.copy_abs() > D("1e30") for v in m.st + list(m.var.values())):
        continue
    exp = " ".join([f"X={fmt(m.st[0])}", f"Y={fmt(m.st[1])}", f"Z={fmt(m.st[2])}", f"T={fmt(m.st[3])}"] +
                   [f"@{v}={fmt(m.var[v])}" for v in VARS + "CN"] + ["%PC=TOP"])
    out.append(f"P{made:05d} | ours: a random program ({len(prog)} lines) | {' '.join(pre)} {k} XEQ:A | {exp}")
    made += 1
print("# prog_fuzz: generated by tests/prog_fuzz.py")
print("\n".join(out))
