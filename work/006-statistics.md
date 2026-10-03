# Work unit 006: one- and two-variable statistics

**From:** abacus · **To:** soroban · **Status:** ready after 005 (light brief; you extract examples)

## Scope (HP 35s User's Guide chapter 12, pp.12-1 to 12-13; Appendix B for stack lift and LAST X)

Σ+ and Σ− (accumulate and remove a data point); CLΣ (clear the six statistics registers); the mean
menu (x̄, ȳ, the weighted mean x̄w of x weighted by y); the standard deviation menu (sample sx, sy;
population σx, σy); the linear-regression menu (x̂, ŷ, r, m, b); the summation menu (n, Σx, Σy,
Σx², Σy², Σxy). The statistics registers are the six from unit 005 (indirect addresses -27 to -32);
p.12-12 gives which is which, which settles unit 005's one mutation survivor. Key tokens: `SIGMA+`,
`SIGMA-`, `CLSIGMA`, `XBAR`, `YBAR`, `XWBAR`, `SX`, `SY`, `PSX`, `PSY`, `XHAT`, `YHAT`, `CORR`,
`SLOPE`, `YINT`, `NSUM`, `SUMX`, `SUMY`, `SUMX2`, `SUMY2`, `SUMXY` (rename if you prefer; tell me).

## Rules (manual first; ours where noted)

1. What each key does to the stack, stack lift and LAST X is the manual's: chapter 12's examples
   and Appendix B (which lists the operations that disable lift and which are neutral). Σ+ returns
   n in X (p.12-1); p.12-2/12-3 describe what LAST X and Y hold after Σ+, so that the manual's
   correction recipe (recall last x, then Σ−) works; vector that recipe from p.12-3's example.
   Where the pages are silent, a statistic behaves like a recalled number: it lifts and returns
   one value in X.
2. **Ours, accumulation:** each register update is one correctly rounded operation:
   Σx ← Σx ± x, Σy ← Σy ± y, Σx² ← Σx² ± x·x, Σy² ← Σy² ± y·y, Σxy ← Σxy ± x·y (a fused
   multiply-add, one rounding), n ← n ± 1. Σ+ and Σ− are atomic: if any of the six would leave
   the range, nothing changes and it is an error.
3. **Ours, results:** every statistic is the textbook formula evaluated EXACTLY on the six
   register values as they are stored (not on the original data, which the calculator no longer
   has), then rounded once. So means, x̄w, m, b, x̂ and ŷ are correctly rounded (vectors use `=`);
   sx, sy, σx, σy and r involve a square root and are held to within 8 units in the 34th digit
   (`^`), and I'll report the worst seen. The point: the manual's p.12-10 warning about close,
   large numbers (7776999, 7777000, 7777001) comes from cancellation inside the formula, and with
   exact evaluation on the registers it disappears for any data whose sums were stored exactly.
   Your bigint layer is the natural tool. Formulas (as stored): x̄ = Σx/n; x̄w = Σxy/Σy;
   sx² = (nΣx² − (Σx)²) / (n(n−1)); σx² = (nΣx² − (Σx)²) / n²; m = (nΣxy − ΣxΣy) / (nΣx² − (Σx)²);
   b = (Σy − mΣx)/n, computed as (ΣyΣx² − ΣxΣxy) / (nΣx² − (Σx)²); r = (nΣxy − ΣxΣy) /
   √((nΣx² − (Σx)²)(nΣy² − (Σy)²)); ŷ = m·x + b and x̂ = (y − b)/m, each as one exact rational
   before rounding.
4. Errors leave everything unchanged: a mean with n = 0, x̄w with Σy = 0, s with n ≤ 1, a negative
   value under a square root (possible after Σ− or after storing into a register directly), and
   any L.R. function whose denominator is 0 (all x equal; r also when all y are equal; x̂ when
   m = 0). Read which message the manual names; the error code is enough here.
5. Registers stay storable through (I)/(J) as in unit 005; CLΣ clears only these six.

## Acceptance and report

As before: the manual's chapter 12 examples as M-vectors (key sequences from the rendered pages;
p.12-3 correction, p.12-5 mean and weighted mean, p.12-6 sx, p.12-7 σx, p.12-8/12-9 the rice
regression and the 70 kg prediction, p.12-11 viewing the registers), plus ours, red then green.
At review: hidden random data sets (including close, large values and Σ− removals) against the
reviewer's model, which replays rule 2's roundings and evaluates rule 3 exactly.
