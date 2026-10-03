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
CORE := core/calc.c
HDRS := core/abn.h core/calc.h
ENGINE := build/abn_intel.o $(INTEL_LIB)
LIBS := -lm

.PHONY: all test clean
all: build/vectors build/abn_test build/diff_test

$(INTEL_LIB):
	CC="$(CC)" scripts/fetch-intel-dfp.sh

# The library's headers are its own; -isystem keeps our warning flags off them.
build/abn_intel.o: core/abn_intel.c core/abn.h $(INTEL_LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) $(BID_FLAGS) -isystem $(INTEL_SRC) -c -o $@ core/abn_intel.c

build/vectors: $(CORE) $(HDRS) $(ENGINE) tests/vectors.c tests/norm.h
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/vectors.c $(ENGINE) $(LIBS)

build/abn_test: $(HDRS) $(ENGINE) tests/abn_test.c tests/norm.h tests/cases.h
	$(CC) $(CFLAGS) -Icore -o $@ tests/abn_test.c $(ENGINE) $(LIBS)

build/diff_test: $(HDRS) $(ENGINE) tests/diff_test.c tests/dref.c tests/dref.h tests/norm.h tests/cases.h
	$(CC) $(CFLAGS) -Icore -Itests -o $@ tests/diff_test.c tests/dref.c $(ENGINE) $(LIBS)

test: all
	./build/abn_test
	./build/diff_test
	./build/vectors work/001-vectors.txt

clean:
	rm -rf build
