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
CORE := core/calc.c core/fmt.c core/fn.c core/bigint.c core/const.c core/xnum.c core/stat.c
HDRS := core/abn.h core/calc.h core/fmt.h core/fn.h core/bigint.h core/const.h core/xnum.h core/stat.h
ENGINE := build/abn_intel.o $(INTEL_LIB)
LIBS := -lm

.PHONY: all test accuracy clean
all: build/vectors build/fmt_vectors build/fmt_drive build/fn_drive build/abn_test build/diff_test

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

build/fmt_drive: core/fmt.c $(HDRS) $(ENGINE) tests/fmt_drive.c
	$(CC) $(CFLAGS) -Icore -o $@ core/fmt.c tests/fmt_drive.c $(ENGINE) $(LIBS)

build/fn_drive: core/fn.c core/bigint.c $(HDRS) $(ENGINE) tests/fn_drive.c
	$(CC) $(CFLAGS) -Icore -o $@ core/fn.c core/bigint.c tests/fn_drive.c $(ENGINE) $(LIBS)

build/abn_test: $(HDRS) $(ENGINE) tests/abn_test.c tests/norm.h tests/cases.h
	$(CC) $(CFLAGS) -Icore -o $@ tests/abn_test.c $(ENGINE) $(LIBS)

build/diff_test: $(HDRS) $(ENGINE) tests/diff_test.c tests/dref.c tests/dref.h tests/norm.h tests/cases.h
	$(CC) $(CFLAGS) -Icore -Itests -o $@ tests/diff_test.c tests/dref.c $(ENGINE) $(LIBS)

test: all
	./build/abn_test
	./build/diff_test
	./build/vectors work/001-vectors.txt
	./build/vectors work/003-vectors.txt
	./build/vectors work/004-vectors.txt
	./build/vectors work/005-vectors.txt
	./build/vectors work/006-vectors.txt
	./build/fmt_vectors work/002-vectors.txt
	python3 tests/fmt_fuzz.py build/fmt_drive 20000

# Needs mpmath (not part of `make test`): the unit 003/004 functions against mpmath at 150 digits,
# and random statistics data sets against the exact reference.
accuracy: build/fn_drive build/vectors
	python3 tests/fn_fuzz.py build/fn_drive
	python3 tests/stat_fuzz.py 1 1500 > build/stat_fuzz.txt
	./build/vectors build/stat_fuzz.txt

clean:
	rm -rf build
