# Abacus core: portable C11, no platform calls. `make test` runs every check.
#
# The number engine is the Intel Decimal Floating-Point Math Library (decimal128), fetched
# from netlib and verified against a pinned sha256 by scripts/fetch-intel-dfp.sh.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
INTEL_SRC := third_party/intel-dfp/IntelRDFPMathLib20U5/LIBRARY/src
INTEL_LIB := build/intel/libbid.a
BID_FLAGS := -DDECIMAL_CALL_BY_REFERENCE=0 -DDECIMAL_GLOBAL_ROUNDING=0 \
             -DDECIMAL_GLOBAL_EXCEPTION_FLAGS=0
CORE := core/calc.c core/fmt.c core/fn.c core/bigint.c core/const.c core/xnum.c core/stat.c core/frac.c core/base.c core/cplx.c core/hp.c core/sbig.c core/arena.c core/eqn.c core/lin.c core/state.c
HDRS := core/abn.h core/calc.h core/fmt.h core/fn.h core/bigint.h core/const.h core/xnum.h core/stat.h core/frac.h core/base.h core/cplx.h core/hp.h core/sbig.h core/bigint_impl.h core/xnum_impl.h core/arena.h core/eqn.h core/lin.h core/state.h
ENGINE := build/abn_intel.o $(INTEL_LIB)
LIBS := -lm

.PHONY: all test accuracy clean device-host
all: build/vectors build/fmt_vectors build/fmt_drive build/fn_drive build/cx_drive build/xnum_test build/abn_test build/diff_test

$(INTEL_LIB):
	CC="$(CC)" scripts/fetch-intel-dfp.sh

# The library's headers are its own; -isystem keeps our warning flags off them.
build/abn_intel.o: core/abn_intel.c core/abn.h $(INTEL_LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) $(BID_FLAGS) -isystem $(INTEL_SRC) -c -o $@ core/abn_intel.c

build/vectors: $(CORE) $(HDRS) $(ENGINE) tests/vectors.c tests/norm.h
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/vectors.c $(ENGINE) $(LIBS)

build/fmt_vectors: $(CORE) $(HDRS) $(ENGINE) tests/fmt_vectors.c
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/fmt_vectors.c $(ENGINE) $(LIBS)

build/fmt_drive: core/fmt.c core/fn.c core/frac.c core/bigint.c core/arena.c core/sbig.c core/base.c $(HDRS) $(ENGINE) tests/fmt_drive.c
	$(CC) $(CFLAGS) -Icore -o $@ core/fmt.c core/fn.c core/frac.c core/bigint.c core/arena.c core/sbig.c core/base.c tests/fmt_drive.c $(ENGINE) $(LIBS)

build/cx_drive: core/cplx.c core/hp.c core/sbig.c core/xnum.c core/bigint.c core/arena.c $(HDRS) $(ENGINE) tests/cx_drive.c
	$(CC) $(CFLAGS) -Icore -o $@ core/cplx.c core/hp.c core/sbig.c core/xnum.c core/bigint.c core/arena.c tests/cx_drive.c $(ENGINE) $(LIBS)

build/xnum_test: core/xnum.c core/bigint.c core/arena.c $(HDRS) $(ENGINE) tests/xnum_test.c
	$(CC) $(CFLAGS) -Icore -o $@ core/xnum.c core/bigint.c core/arena.c tests/xnum_test.c $(ENGINE) $(LIBS)

build/fn_drive: core/fn.c core/bigint.c core/arena.c core/sbig.c $(HDRS) $(ENGINE) tests/fn_drive.c
	$(CC) $(CFLAGS) -Icore -o $@ core/fn.c core/bigint.c core/arena.c core/sbig.c tests/fn_drive.c $(ENGINE) $(LIBS)

build/abn_test: $(HDRS) $(ENGINE) tests/abn_test.c tests/norm.h tests/cases.h
	$(CC) $(CFLAGS) -Icore -o $@ tests/abn_test.c $(ENGINE) $(LIBS)

build/diff_test: $(HDRS) $(ENGINE) tests/diff_test.c tests/dref.c tests/dref.h tests/norm.h tests/cases.h
	$(CC) $(CFLAGS) -Icore -Itests -o $@ tests/diff_test.c tests/dref.c $(ENGINE) $(LIBS)

test: all
	./build/abn_test
	./build/diff_test
	./build/xnum_test
	./build/vectors work/001-vectors.txt
	./build/vectors work/003-vectors.txt
	./build/vectors work/004-vectors.txt
	./build/vectors work/005-vectors.txt
	./build/vectors work/006-vectors.txt
	./build/vectors work/007-vectors.txt
	./build/fmt_vectors work/007-fmt-vectors.txt
	./build/vectors work/008-vectors.txt
	./build/fmt_vectors work/008-fmt-vectors.txt
	./build/vectors work/009-vectors.txt
	./build/fmt_vectors work/009-fmt-vectors.txt
	./build/vectors work/010-vectors.txt
	./build/fmt_vectors work/010-fmt-vectors.txt
	./build/vectors work/011-vectors.txt
	./build/vectors work/012-vectors.txt
	./build/vectors work/013-vectors.txt
	./build/vectors work/014-vectors.txt
	./build/vectors work/015-vectors.txt
	./build/vectors work/015b-vectors.txt
	python3 tests/eqn_fuzz.py 1 2000 > build/eqn_fuzz.txt
	./build/vectors build/eqn_fuzz.txt
	python3 tests/prog_fuzz.py 1 1000 > build/prog_fuzz.txt
	./build/vectors build/prog_fuzz.txt
	python3 tests/veckey_fuzz.py 1 3000 > build/veckey_fuzz.txt
	./build/vectors build/veckey_fuzz.txt
	./build/fmt_vectors work/002-vectors.txt
	python3 tests/fmt_fuzz.py build/fmt_drive 20000

# Needs mpmath (not part of `make test`): the unit 003/004 functions against mpmath at 150 digits,
# random statistics data sets, fractions and base operations, against exact references.
accuracy: build/fn_drive build/cx_drive build/vectors
	python3 tests/fn_fuzz.py build/fn_drive
	python3 tests/cx_fuzz.py build/cx_drive
	python3 tests/cxkey_fuzz.py 1 5000 > build/cxkey_fuzz.txt
	./build/vectors build/cxkey_fuzz.txt
	python3 tests/solve_fuzz.py 1 1500 > build/solve_fuzz.txt
	./build/vectors build/solve_fuzz.txt
	python3 tests/stat_fuzz.py 1 1500 > build/stat_fuzz.txt
	./build/vectors build/stat_fuzz.txt
	python3 tests/frac_fuzz.py 1 3000 > build/frac_fuzz.txt
	./build/fmt_vectors build/frac_fuzz.txt
	python3 tests/base_fuzz.py 1 5000 > build/base_fuzz.txt
	./build/vectors build/base_fuzz.txt

# The on-chip test app (tests/device), built and run on the host: the same runners and bundle
# the chip gets. The bundle is compiled without -Wpedantic (its string literals are long).
DEVICE_RUNNERS := tests/vectors.c tests/fmt_vectors.c tests/diff_test.c tests/dref.c
build/device/bundle.c: tests/device/bundle.py $(wildcard work/*-vectors.txt) tests/eqn_fuzz.py tests/prog_fuzz.py tests/veckey_fuzz.py
	@mkdir -p build/device
	python3 tests/device/bundle.py $@
build/device/bundle.o: build/device/bundle.c tests/device/main/bundle.h
	$(CC) -std=c11 -O0 -Itests/device/main -c -o $@ $<
build/device/abn_test.o: tests/abn_test.c tests/norm.h tests/cases.h core/abn.h
	@mkdir -p build/device
	$(CC) $(CFLAGS) -DAB_DEVICE -Dmain=abn_test_main -Icore -Itests -c -o $@ $<
build/device/xnum_test.o: tests/xnum_test.c $(HDRS)
	@mkdir -p build/device
	$(CC) $(CFLAGS) -DAB_DEVICE -Dmain=xnum_test_main -Icore -Itests -c -o $@ $<
build/device_host: tests/device/main/device_main.c $(DEVICE_RUNNERS) $(CORE) $(HDRS) $(ENGINE) build/device/bundle.o build/device/abn_test.o build/device/xnum_test.o
	$(CC) $(CFLAGS) -DAB_DEVICE -Icore -Itests -Itests/device/main -o $@ tests/device/main/device_main.c $(DEVICE_RUNNERS) $(CORE) build/device/bundle.o build/device/abn_test.o build/device/xnum_test.o $(ENGINE) $(LIBS)
device-host: build/device_host
	./build/device_host

clean:
	rm -rf build
