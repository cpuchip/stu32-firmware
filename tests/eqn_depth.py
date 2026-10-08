#!/usr/bin/env python3
"""eqn_depth: unit 047's rule, the parser's recursion depth of an equation, for scanning vector sets (abacus #5153).

The units an equation's parse reaches: +8 for each group "(", vector "[" or function argument list open around the
point, +1 for each step of an STU power chain a^b^c being read (right to left), +2 for a unary minus group (one or
more minuses) before an operand while that operand is read; an exponent's minus costs none.
More than 153 is NESTED TOO DEEP (work/047). In 33s and 35s equations powers read left to right, a loop, so only
the groups count.

This is the parser's own grammar (core/eqn.c), syntax only, as the core's check parse reads it: its names, their
models and the 33s's hidden ones are read from core/eqn.c, so a name added there is seen here. tests/eqn_depth.c
measures the same with the parser itself; tests/eqn_depth_fuzz.py checks the two agree.

    python3 tests/eqn_depth.py MODEL TEXT...        print each text's units (MODEL 33, 35 or STU)
    python3 tests/eqn_depth.py --scan FILE...       vector files: each EQ:, the model in force; print those past 153
"""
import os
import re
import sys

LIMIT = 153
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


def _c_string(lit):
    """the bytes of adjacent C string literals"""
    out = bytearray()
    for s in re.findall(r'"((?:[^"\\]|\\.)*)"', lit):
        i = 0
        while i < len(s):
            if s[i] == "\\":
                if s[i + 1] == "x":
                    j = i + 2
                    while j < len(s) and s[j] in "0123456789abcdefABCDEF" and j < i + 4:
                        j += 1
                    out.append(int(s[i + 2:j], 16))
                    i = j
                    continue
                out += {"n": b"\n", "\\": b"\\", '"': b'"'}.get(s[i + 1], s[i + 1].encode())
                i += 2
            else:
                out += s[i].encode()
                i += 1
    return bytes(out)


def _load():
    src = open(os.path.join(ROOT, "core", "eqn.c"), encoding="utf-8").read()
    block = src[src.index("static const fname NAMES[] = {"):]
    block = block[:block.index("\n};")]
    names = [(_c_string(m.group(1)), m.group(2), m.group(3))
             for m in re.finditer(r'\{((?:"(?:[^"\\]|\\.)*"\s*)+),\s*(K[012]),\s*(F_\w+)\}', block)]
    nm = src[src.index("static int name_model"):]
    only33 = [_c_string(x) for x in re.findall(r'"(?:[^"\\]|\\.)*"', nm[nm.index("ONLY33"):nm.index("};")])]
    nm2 = nm[nm.index("ONLY35"):]
    only35 = [_c_string(x) for x in re.findall(r'"(?:[^"\\]|\\.)*"', nm2[:nm2.index("};")])]
    hid = src[src.index("static bool m33_hidden"):]
    hid = hid[:hid.index("\n}")]
    hidden = set(re.findall(r"id == (F_\w+)", hid))
    rng = re.search(r"id >= (F_\w+) && id <= (F_\w+)", hid)
    enum = re.findall(r"\b(F_\w+)\b", re.search(r"enum\s*\{([^}]*\bF_REGX\b[^}]*)\}", src).group(1))
    if rng:
        hidden |= set(enum[enum.index(rng.group(1)):enum.index(rng.group(2)) + 1])
    ind_i = _c_string(re.search(r"#define IND_I (\"[^\n]*\")", src).group(1))
    ind_j = _c_string(re.search(r"#define IND_J (\"[^\n]*\")", src).group(1))
    return names, only33, only35, hidden, ind_i, ind_j


NAMES, ONLY33, ONLY35, HIDDEN33, IND_I, IND_J = _load()


def name_in(name, m):
    nm = 33 if name in ONLY33 else 35 if name in ONLY35 else 0
    return not (nm == 33 and m != 33) and not (nm == 35 and m == 33)


class Fail(Exception):
    pass


class P:
    def __init__(self, text, model):
        self.s = text.encode() if isinstance(text, str) else text
        self.i = 0
        self.rtl = self.implied = model == "STU"
        self.m33 = model == "33"
        self.depth = self.peak = 0

    def ch(self, k=0):
        return self.s[self.i + k:self.i + k + 1]

    def at(self, tok):
        if self.s.startswith(tok, self.i):
            self.i += len(tok)
            return True
        return False

    def descend(self, w):
        self.depth += w
        self.peak = max(self.peak, self.depth)

    def ascend(self, w):
        self.depth -= w


def isdigit(b):
    return b != b"" and b"0" <= b <= b"9"


def based_len(s):
    n = 0
    while n < len(s) and (48 <= s[n] <= 57 or 65 <= s[n] <= 70):
        n += 1
    if n == 0 or n >= len(s):
        return 0
    b = {98: 2, 111: 8, 100: 10, 104: 16}.get(s[n], 0)
    if not b:
        return 0
    for i in range(n):
        d = s[i] - 48 if s[i] <= 57 else s[i] - 55
        if d >= b:
            return 0
    return n + 1


def decimal_len(s):
    n = points = digits = 0
    while n < len(s) and (s[n] == 46 or 48 <= s[n] <= 57):
        if s[n] == 46:
            points += 1
        else:
            digits += 1
        n += 1
    if (digits or points) and s[n:n + 1] == b"E":
        nxt = s[n + 1:n + 2]
        if (nxt == b"-" and isdigit(s[n + 2:n + 3])) or isdigit(nxt):
            n += 1
            if s[n:n + 1] == b"-":
                n += 1
            while n < len(s) and 48 <= s[n] <= 57:
                n += 1
    return n, points, digits


def parse_number(p):
    s = p.s[p.i:]
    n = based_len(s)
    if n > 0:
        p.i += n
        if p.m33:
            raise Fail
        return True
    n, points, digits = decimal_len(s)
    if n == 0:
        return False
    p.i += n
    if not digits or points > 2:
        raise Fail
    if not p.m33 and (p.at(b"i") or p.at("θ".encode())):
        p.at(b"-")
        n2, points, digits = decimal_len(p.s[p.i:])
        if n2 == 0 or not digits or points > 1:
            raise Fail
        p.i += n2
    return True


def parse_prim(p):
    if parse_number(p):
        return
    if p.at(b"("):
        p.descend(8)
        parse_add(p)
        p.ascend(8)
        if not p.at(b")"):
            raise Fail
        return
    if p.at(b"["):
        if p.m33:
            raise Fail
        p.descend(8)
        k = 0
        while True:
            if k == 3:
                raise Fail
            parse_add(p)
            k += 1
            if not p.at(b","):
                break
        p.ascend(8)
        if not p.at(b"]"):
            raise Fail
        return
    s = p.s[p.i:]
    best = None
    for name, kind, fid in NAMES:
        n = len(name)
        if (best is None or n > len(best[0])) and s.startswith(name) and (kind == "K0" or s[n:n + 1] == b"(") and \
                name_in(name, 33 if p.m33 else 35):
            best = (name, kind, fid)
    if best:
        name, kind, fid = best
        p.i += len(name)
        if p.m33 and fid in HIDDEN33:
            raise Fail
        if kind == "K0":
            return
        p.i += 1
        p.descend(8)
        parse_add(p)
        p.ascend(8)
        if fid == "F_RND" and p.m33 and p.at(b":"):
            p.descend(8)
            parse_add(p)
            p.ascend(8)
            if not p.at(b")"):
                raise Fail
            return
        if kind == "K2":
            if not p.at(b":" if p.m33 else b","):
                raise Fail
            p.descend(8)
            parse_add(p)
            p.ascend(8)
        if not p.at(b")"):
            raise Fail
        return
    if p.at(IND_I) or p.at(IND_J):
        return
    c = p.ch()
    if c != b"" and b"A" <= c <= b"Z":
        p.i += 1
        return
    if c == b"i" and p.m33:
        p.i += 1
        return
    raise Fail


def parse_post(p):
    parse_prim(p)
    while p.at(b"!"):
        pass


def parse_parg(p):
    while p.at(b"-"):
        pass
    parse_post(p)


def parse_parg_r(p):
    while p.at(b"-"):
        pass
    parse_pow_r(p)


def parse_pow_r(p):
    parse_post(p)
    if p.at(b"^"):
        p.descend(1)
        parse_parg_r(p)
        p.ascend(1)


def parse_pow(p):
    if p.rtl:
        parse_pow_r(p)
        return
    parse_post(p)
    while p.at(b"^"):
        parse_parg(p)


def parse_neg(p):
    n = 0
    while p.at(b"-"):
        n += 1
    if n:
        p.descend(2)
    parse_pow(p)
    if n:
        p.ascend(2)


def implied_here(p):
    s = p.s[p.i:]
    if s[:1] == b"(":
        return True
    if s[:1] == b"E" and p.i > 0 and (isdigit(p.s[p.i - 1:p.i]) or p.s[p.i - 1:p.i] == b"."):
        return False
    if s[:1] != b"" and b"A" <= s[:1] <= b"Z":
        return True
    if s.startswith(IND_I) or s.startswith(IND_J):
        return True
    if s[:1] in (b"b", b"o", b"d", b"h"):
        return False
    for name, kind, _ in NAMES:
        n = len(name)
        if s.startswith(name) and (kind == "K0" or s[n:n + 1] == b"(") and name_in(name, 33 if p.m33 else 35):
            return True
    return False


def parse_mul(p):
    parse_neg(p)
    while True:
        if p.at("×".encode()) or p.at("÷".encode()):
            pass
        elif p.implied and implied_here(p):
            pass
        else:
            break
        parse_neg(p)


def parse_add(p):
    parse_mul(p)
    while p.at(b"+") or p.at(b"-"):
        parse_mul(p)


def units(text, model):
    """the units the parse reaches (to its first syntax error, where the parse stops)"""
    p = P(text, model)
    try:
        parse_add(p)
        if p.at(b"="):
            parse_add(p)
    except Fail:
        pass
    return p.peak


def too_deep(text, model):
    return units(text, model) > LIMIT


def scan(path):
    """vector files: each EQ:text with the model the core stamps it with, the equations past the limit. The mode in
    force is 35s first (as after a memory clear), then MODE33, MODE35, STU, MODEL:n. In program entry a line takes its
    label's stamp (the mode when the label was keyed, or a model key pressed on the LBL line); after a LOADFILE the
    image's stamps are unknown, so each EQ is scanned in every model."""
    tok_model = {"MODE33": "33", "MODE35": "35", "STU": "STU"}
    arg_model = {"33": "33", "35": "35", "1": "STU"}         # AB_MODEL's argument; 1 is AB_MODEL_STU (core/calc.h)
    out = []
    for n, line in enumerate(open(path, encoding="utf-8"), 1):
        if line.startswith("#") or line.count("|") < 3:
            continue
        model, prgm, label, on_lbl, loaded, lmode = "35", False, None, False, False, {}
        for tok in line.split("|")[2].split():
            m = tok_model.get(tok) or (arg_model.get(tok[6:]) if tok.startswith("MODEL:") else None)
            if m:
                if prgm and on_lbl:
                    lmode[label] = m                        # restamps the label (unit 025 rule 5)
                elif not prgm:
                    model = m
                continue
            on_lbl = False
            if tok == "PRGM":
                prgm = not prgm
            elif tok == "CLPGM":
                lmode, label = {}, None
            elif tok.startswith("LBL:") and prgm:
                label, on_lbl = tok[4:], True
                lmode.setdefault(label, model)
            elif tok.startswith("LOADFILE:"):
                loaded = True
            elif tok.startswith("EQ:"):
                ms = ("33", "35", "STU") if loaded else (lmode.get(label, model) if prgm and label else model,)
                u = max(units(tok[3:], x) for x in ms)
                if u > LIMIT:
                    out.append((path, n, "/".join(ms), u, tok[3:]))
    return out

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--scan":
        hits = [h for f in sys.argv[2:] for h in scan(f)]
        for h in hits:
            print("%s:%d: %s %d units: %s" % h)
        print(f"eqn_depth: {len(hits)} equations past {LIMIT}")
    else:
        for t in sys.argv[2:]:
            u = units(t, sys.argv[1])
            print(u, "TOO_DEEP" if u > LIMIT else "")
