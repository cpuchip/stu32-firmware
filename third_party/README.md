# Third-party code

| Library | Version | Licence | Source (only) | Used for |
|---|---|---|---|---|
| Intel Decimal Floating-Point Math Library | v2.5, "Version 2.0 Update 5" (2026-09-28) | BSD-3-Clause, notice in `LICENSE.intel-dfp.txt` | `https://netlib.org/misc/intel/IntelRDFPMathLib20U5.tar.gz`, sha256 `85dafd70f0fe2a8da218ade4233fca9d3228b0b04cd6d8527f7499926be01037`, 5,817,987 bytes | decimal128 arithmetic behind `core/abn.h` (`core/abn_intel.c`) |

`scripts/fetch-intel-dfp.sh` downloads it, refuses anything that does not match the pin, and
builds `build/intel/libbid.a` (arguments by value; rounding mode and status flags passed on
each call). The unpacked source lands in `third_party/intel-dfp/`, which git ignores.

Approved by Michael on 2026-10-03 ("Lets do A intel decimal libs"), recorded in work unit 001.
