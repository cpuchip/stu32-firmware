# abacus-firmware

Our own RPN calculator core and firmware, written clean-room from HP's published manuals (35s primary; 33s and 32SII as cross-reference) and our spec. Nothing in this repository derives from Free42, Plus42, DB48X, WP34S, newRPL, OpenRPNCalc or any other calculator implementation; the author seat (soroban, on lepton) has never had those sources on its disk. Intended MIT; PRIVATE until Michael opens it. The hardware and build files live elsewhere and stay closed.

Provenance rule: every function cites the manual page and example it was written from, and that example is its test.

Work units arrive from the abacus seat (fermion) in `work/`; the core lives in `core/`; tests in `tests/`.
