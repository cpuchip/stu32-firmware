#!/usr/bin/env python3
"""solve_fuzz: SOLVE and integrate on random programmed functions, unit 013.

    python3 tests/solve_fuzz.py SEED COUNT > f; build/vectors f      (needs mpmath)

SOLVE: polynomials built from chosen roots (Horner programs), and e^x - c, ln x - c, sin x - c,
x - cos x, with guesses around exactly one root. SOLVE finds the root of f as the calculator
computes it (34 digits); how far that lies from the true root is f's conditioning, so the result
must be within 8 units of the true root plus 4 eps M / |f'(r)|, M the size of the terms whose
rounding f carries (eps = 1E-33; for Horner, 2 deg eps times the sum of |c_i| |r|^i). Integrate: polynomials, e^(kx),
sin, 1/(1+x^2), sqrt(x) at random display settings (FIX, SCI, ENG, ALL); the uncertainty in Y must
cover the true error (%COVER) and stay within 1.15 times the display-format uncertainty U the
unit's rule 2 defines (Y#0,bound: Y is U plus an error estimate refined below U/10). Exact values: mpmath at 60 digits.

Unit 015: each case also has an equation twin, the same function as a listed equation whose
operations come in the same order, solved or integrated from Equation mode. A SOLVE twin's root must
be the program's, bit for bit (SAVEX, then %SAMEX). An integral's twin meets the program's own
expectations instead: since unit 044 (rule 5, abacus #4897) a program integrand runs correctly rounded,
since it can store, and an equation integrand keeps the engine's 34-digit path, so their last digits
may differ within the integral's uncertainty.
"""
import random
import sys

try:
    import mpmath as mp
except ImportError:
    print("solve_fuzz: mpmath is not installed; skipped", file=sys.stderr)
    sys.exit(0)

mp.mp.dps = 60
R = random.Random(int(sys.argv[1]))
N = int(sys.argv[2])


def s36(x):
    return mp.nstr(x, 36, min_fixed=-30, max_fixed=30).replace("e", "E")


EPS = mp.mpf("1e-33")


def tol_for(r, M, dfr):
    """8 units of r's 34th digit, plus the evaluation error 4 eps M moved by 1/|f'(r)|"""
    u = mp.mpf(10) ** (mp.floor(mp.log10(abs(r))) - 33) if r != 0 else mp.mpf("1e-6000")
    return 8 * u + 4 * EPS * M / abs(dfr)


def lit(v):
    """keystrokes for a short decimal v (string)"""
    neg = v.startswith("-")
    return v.lstrip("-") + (" CHS" if neg else "")


def etxt(v):
    """a short decimal v (string) as equation text"""
    return v.replace("e", "E")


def twin(tag, eq, setup, act, prog_keys, desc, expect=None):
    """the equation twin: setup is what the program keys do before FN= (guesses or limits); expect, the
    program's own expectations in place of its bit-for-bit result (an integral: unit 044)"""
    if expect:
        out.append(f"{tag}{n:05d} | ours: the equation twin of {desc} | EQ:{eq} C {setup} EQN {act} | {expect}")
    else:
        out.append(f"{tag}{n:05d} | ours: the equation twin of {desc} | EQ:{eq} C {setup} EQN {act} SAVEX {prog_keys} | %SAMEX=same")


def dec(lo, hi, places):
    v = mp.mpf(R.randint(int(lo * 10**places), int(hi * 10**places))) / 10**places
    return v, mp.nstr(v, 20, strip_zeros=True) if v != 0 else "0"


out = []
n = 0
while n < N:
    kind = R.random()
    if kind < 0.35:
        # polynomial from roots
        deg = R.randint(1, 4)
        roots = sorted(set(dec(-9, 9, R.choice([0, 1, 2]))[0] for _ in range(deg)))
        if len(roots) < deg:
            continue
        lead = mp.mpf(R.choice([1, 2, -1, 3, -2]))
        coef = [lead]
        for r in roots:                       # multiply by (x - r)
            coef = [a - r * b for a, b in zip(coef + [0], [0] + coef)]
        if any(abs(c) > 1e12 for c in coef):
            continue
        target = R.choice(roots)
        i = roots.index(target)
        lo = roots[i - 1] if i > 0 else target - 5
        hi = roots[i + 1] if i + 1 < len(roots) else target + 5
        g1 = target - (target - lo) * mp.mpf(R.uniform(0.1, 0.45))
        g2 = target + (hi - target) * mp.mpf(R.uniform(0.1, 0.45))
        g1s, g2s = mp.nstr(g1, 6), mp.nstr(g2, 6)
        body = f"{lit(mp.nstr(coef[0], 20))}"
        for c in coef[1:]:
            body += f" RCL:X * {lit(mp.nstr(c, 20))} +"
        keys = f"PRGM CLPGM LBL:F {body} RTN PRGM {lit(g1s)} STO:X {lit(g2s)} FN=:F SOLVE:X"
        M = 2 * len(coef) * sum(abs(c) * abs(target) ** (len(coef) - 1 - j) for j, c in enumerate(coef))
        dfr = sum((len(coef) - 1 - j) * c * target ** (len(coef) - 2 - j) for j, c in enumerate(coef[:-1]))
        out.append(f"S{n:05d} | ours: a polynomial of degree {deg}, root {mp.nstr(target, 10)} | {keys} | "
                   f"X#{s36(target)},{mp.nstr(tol_for(target, M, dfr), 6)}")
        eq = etxt(mp.nstr(coef[0], 20))
        for c in coef[1:]:
            eq = f"({eq})×X+{etxt(mp.nstr(c, 20))}"
        twin("T", eq, f"{lit(g1s)} STO:X {lit(g2s)}", "SOLVE:X", keys, "a polynomial")
    elif kind < 0.5:
        which = R.choice(["exp", "ln", "sin", "cos"])
        if which == "exp":
            cval, cs = dec(0.5, 50, 2)
            prog, root, g = f"RCL:X EXP {lit(cs)} -", mp.log(cval), (mp.log(cval) - 1, mp.log(cval) + 0.7)
            M, dfr = cval, cval
        elif which == "ln":
            cval, cs = dec(-2, 3, 2)
            prog, root, g = f"RCL:X LN {lit(cs)} -", mp.exp(cval), (mp.exp(cval) * 0.6, mp.exp(cval) * 1.5)
            M, dfr = max(abs(cval), 1), 1 / mp.exp(cval)
        elif which == "sin":
            cval, cs = dec(-0.9, 0.9, 2)
            prog, root, g = f"RCL:X SIN {lit(cs)} -", mp.asin(cval), (mp.asin(cval) - 0.3, mp.asin(cval) + 0.3)
            M, dfr = 1, mp.cos(mp.asin(cval))
        else:
            prog, root, g = "RCL:X RCL:X COS -", mp.findroot(lambda x: x - mp.cos(x), 0.7), (0.2, 1.3)
            M, dfr = 2, 1 + mp.sin(root)
        keys = f"PRGM CLPGM LBL:F {prog} RTN PRGM RAD {lit(mp.nstr(g[0], 6))} STO:X {lit(mp.nstr(g[1], 6))} FN=:F SOLVE:X"
        out.append(f"S{n:05d} | ours: {which} equation | {keys} | X#{s36(root)},{mp.nstr(tol_for(root, M, dfr), 6)}")
        eq = {"exp": f"EXP(X)-{etxt(cs)}" if which == "exp" else "", "ln": f"LN(X)-{etxt(cs)}" if which == "ln" else "",
              "sin": f"SIN(X)-{etxt(cs)}" if which == "sin" else "", "cos": "X-COS(X)"}[which]
        twin("T", eq, f"RAD {lit(mp.nstr(g[0], 6))} STO:X {lit(mp.nstr(g[1], 6))}", "SOLVE:X", keys, f"the {which} case")
    else:
        which = R.choice(["poly", "exp", "sin", "atan", "sqrt"])
        a_, as_ = dec(-3, 1, 1)
        b_, bs_ = dec(1.5, 6, 1)
        if which == "sqrt":
            a_, as_ = mp.mpf(0), "0"
        if which == "poly":
            c2, c2s = dec(-3, 3, 1)
            c1, c1s = dec(-3, 3, 1)
            c0, c0s = dec(-3, 3, 1)
            prog = f"{lit(c2s)} RCL:X * {lit(c1s)} + RCL:X * {lit(c0s)} +"
            eq = f"({etxt(c2s)}×X+{etxt(c1s)})×X+{etxt(c0s)}"
            f = lambda x: c2 * x**2 + c1 * x + c0
        elif which == "exp":
            k, ks = dec(-1.5, 1.5, 1)
            prog = f"RCL:X {lit(ks)} * EXP"
            eq = f"EXP(X×{etxt(ks)})"
            f = lambda x: mp.exp(k * x)
        elif which == "sin":
            prog = "RCL:X SIN"
            eq = "SIN(X)"
            f = mp.sin
        elif which == "atan":
            prog = "RCL:X SQ 1 + INV"
            eq = "INV(SQ(X)+1)"
            f = lambda x: 1 / (1 + x**2)
        else:
            prog = "RCL:X SQRT"
            eq = "SQRT(X)"
            f = mp.sqrt
        exact = mp.quad(f, [a_, b_])
        absint = mp.quad(lambda x: abs(f(x)), mp.linspace(a_, b_, 9))
        fmt = R.choice(["FIX", "SCI", "ENG", "ALL"])
        digits = R.randint(2, 8) if fmt != "ALL" else 11
        if fmt == "FIX":
            U = mp.mpf(10) ** -digits * abs(b_ - a_)
        else:
            U = mp.mpf(10) ** -digits * absint
        setting = "ALL" if fmt == "ALL" else f"{fmt}{digits}"
        keys = f"PRGM CLPGM LBL:F {prog} RTN PRGM RAD {setting} FN=:F {lit(as_)} ENTER {lit(bs_)} INTEG:X"
        bound = mp.mpf("1.15") * U + mp.mpf("1e-30")     # Y = U + an error estimate below U/10
        expect = f"%COVER={mp.nstr(exact, 40)} Y#0,{mp.nstr(bound, 10)}"
        out.append(f"I{n:05d} | ours: integral of {which} on [{as_}, {bs_}] in {setting} | {keys} | {expect}")
        twin("J", eq, f"RAD {setting} {lit(as_)} ENTER {lit(bs_)}", "INTEG:X", keys, f"the integral of {which}", expect)
    n += 1
print("# solve_fuzz: generated by tests/solve_fuzz.py; mpmath " + mp.__version__)
print("\n".join(out))
