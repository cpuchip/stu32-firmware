"""stat_ref: unit 006 reference (soroban), for tests/stat_fuzz.py: replays rule 2's roundings with Python
decimal and evaluates rule 3 exactly with Fraction (needs mpmath for the square roots)."""
from decimal import Decimal as D, Context, ROUND_HALF_EVEN
from fractions import Fraction as F
import mpmath as mp
mp.mp.dps = 150
C34 = Context(prec=34, rounding=ROUND_HALF_EVEN, Emax=6144, Emin=-6143)
W = Context(prec=400, rounding=ROUND_HALF_EVEN)

def r34(q):
    """A Fraction (or Decimal) rounded once to 34 digits."""
    if isinstance(q, F):
        n, d = q.numerator, q.denominator
        return C34.divide(D(n), D(d)) if False else C34.plus(W.divide(D(n), D(d))) if d != 1 else C34.plus(D(n))
    return C34.plus(q)

class Reg:
    def __init__(self):
        self.r = [D(0)] * 6          # n, sx, sy, sx2, sy2, sxy
    def acc(self, x, y, remove=False):
        x, y = D(x), D(y)
        s = -1 if remove else 1
        n, sx, sy, sx2, sy2, sxy = self.r
        # every product in the wide context: Decimal's default context would round to 28 digits
        m = lambda *a: W.multiply(W.multiply(a[0], a[1]), a[2]) if len(a) == 3 else W.multiply(a[0], a[1])
        S = D(s)
        self.r = [C34.plus(W.add(n, S)), C34.plus(W.add(sx, m(S, x))), C34.plus(W.add(sy, m(S, y))),
                  C34.plus(W.add(sx2, m(S, x, x))), C34.plus(W.add(sy2, m(S, y, y))), C34.plus(W.add(sxy, m(S, x, y)))]
    def fr(self):
        return [F(v) for v in self.r]

def mean(R, y=False):
    n, sx, sy, *_ = R.fr(); return (sy if y else sx) / n
def wmean(R):
    n, sx, sy, sx2, sy2, sxy = R.fr(); return sxy / sy
def var(R, y=False, pop=False):
    n, sx, sy, sx2, sy2, sxy = R.fr()
    s, s2 = (sy, sy2) if y else (sx, sx2)
    num = n * s2 - s * s
    return num / (n * n if pop else n * (n - 1))
def slope(R):
    n, sx, sy, sx2, sy2, sxy = R.fr(); return (n * sxy - sx * sy) / (n * sx2 - sx * sx)
def yint(R):
    n, sx, sy, sx2, sy2, sxy = R.fr(); return (sy * sx2 - sx * sxy) / (n * sx2 - sx * sx)
def corr2(R):
    n, sx, sy, sx2, sy2, sxy = R.fr()
    c = n * sxy - sx * sy
    return c, c * c / ((n * sx2 - sx * sx) * (n * sy2 - sy * sy))
def yhat(R, x):
    n, sx, sy, sx2, sy2, sxy = R.fr(); x = F(D(x))
    return (x * (n * sxy - sx * sy) + (sy * sx2 - sx * sxy)) / (n * sx2 - sx * sx)
def xhat(R, y):
    n, sx, sy, sx2, sy2, sxy = R.fr(); y = F(D(y))
    return (y * (n * sx2 - sx * sx) - (sy * sx2 - sx * sxy)) / (n * sxy - sx * sy)
def sqrt36(q, sign=1):
    v = mp.sqrt(mp.mpf(q.numerator) / mp.mpf(q.denominator)) * sign
    return mp.nstr(v, 36, min_fixed=-30, max_fixed=30).replace('e', 'E')

def sqrt34(q, sign=1):
    """sqrt(q) for a Fraction q >= 0, correctly rounded to 34 digits (half to even), as text: the
    integer root of floor(q 10^2s) with a sticky bit, rounded by hand (unit 010b: the core's
    roots became exact)."""
    import math
    if q == 0:
        return "0"
    e = int(mp.floor(mp.log10(mp.sqrt(mp.mpf(q.numerator) / mp.mpf(q.denominator)))))
    s = 40 - e                                   # the root carries about 41 digits
    if s >= 0:
        N, Dn = q.numerator * 10 ** (2 * s), q.denominator
    else:
        N, Dn = q.numerator, q.denominator * 10 ** (-2 * s)
    Q, rem = divmod(N, Dn)
    r = math.isqrt(Q)
    sticky = rem != 0 or r * r != Q
    t = str(r)
    keep, rest = int(t[:34]), t[34:]
    half = "5" + "0" * (len(rest) - 1)
    if rest > half or (rest == half and (sticky or keep % 2)):
        keep += 1
    v = W.scaleb(D(keep), len(rest) - s)
    return str(C34.plus(v.copy_negate() if sign < 0 else v))
