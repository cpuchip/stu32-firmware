# Unit 024's review vectors (copied from abacus docs/evidence/2026-10-06-manual-review-33s-32sii, 2026-10-06)

Five reviewers' vectors from the HP 33s User's Guide and the HP 32SII Owner's Manual, with the 35s guide for
comparison. Every line cites its manual page, or says "ours". Nothing in them comes from another calculator's
source or output; one set's expected values were checked with mpmath.

The format is tests/vectors.c's (`ID | source | keys | expectations`); `fmt-vectors.txt` uses tests/fmt_vectors.c's.
Run: `build/vectors work/024-review/stack/vectors.txt` (and `build/fmt_vectors .../fmt-vectors.txt`).

**Counts on the core at 02ddee9** (abacus reran each):

| Folder | vectors.txt | fmt-vectors.txt |
|---|---|---|
| stack | 164/180 | 44/44 |
| functions | 139/148 | 53/58 |
| cxbasestat | 105/124 | 0/6 |
| eqnsolve | 70/104 | — |
| programs | 105/135 | — |

**Most failures are expected:** divergences between the machines that wait for Michael's rulings, or rulings we made
on purpose. Unit 024's own failures should turn green: K01, K02, K05, K06 (stack), Q16, Q16b, B14 (eqnsolve), B17
(cxbasestat). Every other vector should keep its result: report the counts before and after.
