#!/usr/bin/env python3
"""fmt_fuzz: the core's formatter against a reference written separately from unit 002's text.

The reference uses Python's decimal module for every rounding (ROUND_HALF_UP, which is half
away from zero) and Python's own fixed-point formatting, so it shares no code and little
method with core/fmt.c. Both format the same random values; any difference is printed.

    tests/fmt_fuzz.py build/fmt_drive [count] [seed]

It also checks itself against work/002-vectors.txt first, so it is fit to judge.
"""
import random
import subprocess
import sys
from decimal import Decimal, Context, ROUND_HALF_UP

CTX = Context(prec=20000, rounding=ROUND_HALF_UP, Emax=10**7, Emin=-10**7)
DIGITS = 34


def round_sig(v, k):
    """v rounded to k significant digits, half away from zero."""
    if v == 0:
        return Decimal(0)
    return Context(prec=k, rounding=ROUND_HALF_UP, Emax=10**7, Emin=-10**7).plus(v)


def round_dec(v, q):
    """v rounded to q decimals, half away from zero."""
    return v.quantize(Decimal(1).scaleb(-q), context=CTX)


def group(intpart, sepchar):
    out = []
    for i, ch in enumerate(reversed(intpart)):
        if i and i % 3 == 0:
            out.append(sepchar)
        out.append(ch)
    return "".join(reversed(out))


def plain(v, q, radix, sep):
    """v (already rounded to q decimals) in plain notation with exactly q decimals."""
    s = format(v.copy_abs(), "f")   # copy_abs is exact; abs() would round to 28 digits
    ip, _, fp = s.partition(".")
    fp = (fp + "0" * q)[:q]
    ip = ip.lstrip("0") or "0"
    if sep:
        ip = group(ip, "." if radix == "," else ",")
    text = ip + (radix + fp if q > 0 else "")
    return ("-" if v < 0 else "") + text


def coefficient(r):
    return "".join(str(d) for d in r.as_tuple().digits).lstrip("0")


def sci_text(r, k, radix):
    """r rounded to k+1 digits: one digit, radix, k digits, exponent."""
    if r == 0:
        return "0" + (radix + "0" * k if k else "") + "E0"
    ds = (coefficient(r) + "0" * (k + 1))[: k + 1]
    s = ds[0] + (radix + ds[1:] if k else "") + "E" + str(r.adjusted())
    return ("-" if r < 0 else "") + s


def eng_text(r, k, radix):
    if r == 0:
        return "0" + (radix + "0" * k if k else "") + "E0"
    e = (r.adjusted() // 3) * 3
    m = r.adjusted() - e + 1
    ds = (coefficient(r) + "0" * (k + 4))[: max(k + 1, m)]
    ip, fp = ds[:m], ds[m:]
    s = ip + (radix + fp if fp else "") + "E" + str(e)
    return ("-" if r < 0 else "") + s


def fits(s, w):
    return s is not None and len(s) <= w


def ref_sci(v, n, w, radix):
    for k in range(n, -1, -1):
        s = sci_text(round_sig(v, k + 1), k, radix)
        if fits(s, w):
            return s
    return None


def ref_eng(v, n, w, radix):
    for k in range(n, -1, -1):
        s = eng_text(round_sig(v, k + 1), k, radix)
        if fits(s, w):
            return s
    return None


def sig_shown(r, q):
    """Significant digits a plain form with q decimals shows, zeros included."""
    return 0 if r == 0 else r.adjusted() + q + 1


def ref_fix(v, n, w, radix, sep):
    r = round_dec(v, n)
    if not (v != 0 and r == 0) and not (r != 0 and sig_shown(r, n) > DIGITS):
        s = plain(r, n, radix, sep)
        if fits(s, w):
            return s
    return ref_sci(v, n, w, radix)


def decimals(v):
    e = v.normalize(CTX).as_tuple().exponent
    return max(0, -e)


def ref_all(v, w, radix, sep):
    if v == 0:
        return "0" if w >= 1 else None
    v = v.normalize(CTX)
    s = plain(v, decimals(v), radix, sep)
    if fits(s, w):
        return s
    want = min(len(coefficient(v)), 12)
    for q in range(decimals(v), -1, -1):
        r = round_dec(v, q)
        if not fits(plain(r, q, radix, sep), w):
            continue
        if sig_shown(r, q) < want:
            break
        r = r.normalize(CTX) if r != 0 else r
        return plain(r, decimals(r), radix, sep)
    for k in range(len(coefficient(v)) - 1, -1, -1):
        r = round_sig(v, k + 1).normalize(CTX)
        ds = coefficient(r)
        s = ds[0] + (radix + ds[1:] if len(ds) > 1 else "") + "E" + str(r.adjusted())
        s = ("-" if r < 0 else "") + s
        if fits(s, w):
            return s
    return None


def reference(value, mode, n, w, radix, sep):
    v = Decimal(value)
    if v == 0:
        v = Decimal(0)
    if mode == "FIX":
        return ref_fix(v, n, w, radix, sep)
    if mode == "SCI":
        return ref_sci(v, n, w, radix)
    if mode == "ENG":
        return ref_eng(v, n, w, radix)
    return ref_all(v, w, radix, sep)


def vectors(path):
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        f = [x.strip() for x in line.split("|")]
        mode, *rest = f[3].split()
        n = int(rest[0]) if rest else 0
        opts = dict(o.split("=", 1) for o in f[4].split())
        yield (f[0], f[2], mode, n, int(opts.get("w", 22)), opts.get("radix", "."),
               opts.get("sep", "on") == "on", f[5])


def random_value(rnd):
    shape = rnd.random()
    nd = rnd.randint(1, DIGITS)
    if shape < 0.25:      # runs of nines: carries
        ds = "9" * nd
    elif shape < 0.45:    # a five at the end: halves
        ds = str(rnd.randint(1, 9)) + "".join(rnd.choice("0123456789") for _ in range(nd - 2)) + "5"
        ds = ds[:nd] if nd > 1 else "5"
    else:
        ds = str(rnd.randint(1, 9)) + "".join(rnd.choice("0123456789") for _ in range(nd - 1))
    span = rnd.choice([3, 12, 40, 40, 6143])
    lead = rnd.randint(-span, span)
    lead = max(-6143, min(6144, lead))
    sign = "-" if rnd.random() < 0.4 else ""
    if rnd.random() < 0.03:
        return "0"
    return "%s%s.%sE%d" % (sign, ds[0], ds[1:] or "0", lead)


def main():
    drive = sys.argv[1]
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 20000
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 2026
    vec_path = sys.argv[4] if len(sys.argv) > 4 else "work/002-vectors.txt"

    bad = 0
    for vid, value, mode, n, w, radix, sep, want in vectors(vec_path):
        got = reference(value, mode, n, w, radix, sep)
        if got != want:
            bad += 1
            print("REFERENCE FAILS %s: expected %r, got %r" % (vid, want, got))
    if bad:
        print("fmt_fuzz: the reference fails the unit's vectors; not fit to judge")
        return 1

    rnd = random.Random(seed)
    cases = []
    for _ in range(count):
        mode = rnd.choice(["FIX", "SCI", "ENG", "ALL"])
        n = rnd.choice([0, 1, 2, 3, 4, 4, 6, 9, 11, 12, 20, 33])
        w = rnd.choice([6, 8, 10, 12, 14, 14, 16, 22, 22, 30, 40])
        cases.append((random_value(rnd), mode, n, w, rnd.choice([".", ","]), rnd.random() < 0.7))
    feed = "".join("%s %s %d %d %s %d\n" % (v, m, n, w, r, 1 if s else 0)
                   for v, m, n, w, r, s in cases)
    out = subprocess.run([drive], input=feed, capture_output=True, text=True, check=True)
    lines = out.stdout.splitlines()
    fails = 0
    for (v, m, n, w, r, s), got in zip(cases, lines):
        want = reference(v, m, n, w, r, s)
        want = "<none>" if want is None else want
        if got != want:
            fails += 1
            if fails <= 20:
                print("DIFF %s %s %d w=%d radix=%s sep=%s: core %r, reference %r"
                      % (v, m, n, w, r, s, got, want))
    if len(lines) != len(cases):
        print("fmt_fuzz: driver returned %d lines for %d cases" % (len(lines), len(cases)))
        return 1
    print("fmt_fuzz: reference passes the vectors; core and reference agree on %d/%d"
          % (count - fails, count))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
