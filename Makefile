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
CORE := core/calc.c core/fmt.c core/fn.c core/bigint.c core/const.c core/xnum.c core/stat.c core/frac.c core/base.c core/cplx.c core/hp.c core/sbig.c core/arena.c core/eqn.c core/lin.c core/state.c core/msg.c core/list.c
HDRS := core/abn.h core/calc.h core/fmt.h core/fn.h core/bigint.h core/const.h core/xnum.h core/stat.h core/frac.h core/base.h core/cplx.h core/hp.h core/sbig.h core/bigint_impl.h core/xnum_impl.h core/arena.h core/eqn.h core/lin.h core/state.h
ENGINE := build/abn_intel.o $(INTEL_LIB)
LIBS := -lm

.PHONY: all test accuracy all-checks clean device-host
# make accuracy needs a python with mpmath: make all-checks PYTHON=...
PYTHON ?= python3
KEYV := $(filter-out work/002-vectors.txt work/016-vectors.txt work/017-vectors.txt work/018-vectors.txt,$(filter-out %-fmt-vectors.txt,$(wildcard work/*-vectors.txt))) tests/021-soroban.txt tests/019-soroban.txt tests/003-soroban.txt tests/018-soroban.txt tests/029-soroban.txt
all: build/standin_test build/app_test build/keymap_test build/power_test build/screen_test build/keys_test build/ls027_test build/store_test build/load_fuzz build/vectors build/fmt_vectors build/fmt_drive build/fn_drive build/cx_drive build/xnum_test build/abn_test build/diff_test

# every build/ target is order-only on the directory: a fresh clone has none (abacus #3803)
build:
	mkdir -p build

$(INTEL_LIB):
	CC="$(CC)" scripts/fetch-intel-dfp.sh

# The library's headers are its own; -isystem keeps our warning flags off them.
build/abn_intel.o: core/abn_intel.c core/abn.h $(INTEL_LIB) | build
	@mkdir -p build
	$(CC) $(CFLAGS) $(BID_FLAGS) -isystem $(INTEL_SRC) -c -o $@ core/abn_intel.c

SCREEN := firmware/screen.c firmware/font5x7.c
build/keymap_test: $(CORE) $(HDRS) $(ENGINE) $(SCREEN) firmware/keymap.c firmware/keymap.h firmware/keys.c firmware/keys.h firmware/screen.h tests/keymap_test.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -o $@ $(CORE) $(SCREEN) firmware/keymap.c firmware/keys.c tests/keymap_test.c $(ENGINE) $(LIBS)

APP := firmware/app.c firmware/keymap.c firmware/keys.c firmware/power.c firmware/settings.c firmware/piezo.c
build/app_test: $(CORE) $(HDRS) $(ENGINE) $(SCREEN) $(APP) firmware/app.h firmware/keymap.h firmware/power.h firmware/screen.h tests/app_test.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -o $@ $(CORE) $(SCREEN) $(APP) tests/app_test.c $(ENGINE) $(LIBS)

build/standin_test: $(CORE) $(HDRS) $(ENGINE) $(SCREEN) $(APP) firmware/standin.c firmware/standin.h firmware/app.h firmware/keymap.h firmware/screen.h tests/standin_test.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -o $@ $(CORE) $(SCREEN) $(APP) firmware/standin.c tests/standin_test.c $(ENGINE) $(LIBS)

build/power_test: firmware/gauge.c firmware/gauge.h firmware/power.c firmware/power.h firmware/pins.c firmware/pins.h firmware/piezo.c firmware/piezo.h tests/power_test.c | build
	$(CC) $(CFLAGS) -Ifirmware -o $@ firmware/gauge.c firmware/power.c firmware/pins.c firmware/piezo.c tests/power_test.c -lm

build/screen_test: $(CORE) $(HDRS) $(ENGINE) $(SCREEN) firmware/screen.h firmware/font5x7.h firmware/ls027.h tests/screen_test.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -o $@ $(CORE) $(SCREEN) tests/screen_test.c $(ENGINE) $(LIBS)

build/keys_test: firmware/keys.c firmware/keys.h tests/keys_test.c | build
	$(CC) $(CFLAGS) -Ifirmware -o $@ firmware/keys.c tests/keys_test.c

build/ls027_test: firmware/ls027.c firmware/ls027.h tests/ls027_test.c | build
	$(CC) $(CFLAGS) -Ifirmware -o $@ firmware/ls027.c tests/ls027_test.c

build/store_test: $(CORE) $(HDRS) $(ENGINE) firmware/store.c firmware/store.h firmware/settings.c firmware/settings.h firmware/piezo.c tests/store_test.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -o $@ $(CORE) firmware/store.c firmware/settings.c firmware/piezo.c tests/store_test.c $(ENGINE) $(LIBS)

build/load_fuzz: $(CORE) $(HDRS) $(ENGINE) tests/load_fuzz.c | build
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/load_fuzz.c $(ENGINE) $(LIBS)

build/vectors: $(CORE) $(HDRS) $(ENGINE) tests/vectors.c tests/norm.h firmware/store.c firmware/store.h | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -o $@ $(CORE) firmware/store.c tests/vectors.c $(ENGINE) $(LIBS)

build/fmt_vectors: $(CORE) $(HDRS) $(ENGINE) tests/fmt_vectors.c | build
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/fmt_vectors.c $(ENGINE) $(LIBS)

build/fmt_drive: core/fmt.c core/fn.c core/hp.c core/frac.c core/bigint.c core/arena.c core/sbig.c core/base.c $(HDRS) $(ENGINE) tests/fmt_drive.c | build
	$(CC) $(CFLAGS) -Icore -o $@ core/fmt.c core/fn.c core/hp.c core/frac.c core/bigint.c core/arena.c core/sbig.c core/base.c tests/fmt_drive.c $(ENGINE) $(LIBS)

build/cx_drive: core/cplx.c core/hp.c core/sbig.c core/xnum.c core/bigint.c core/arena.c $(HDRS) $(ENGINE) tests/cx_drive.c | build
	$(CC) $(CFLAGS) -Icore -o $@ core/cplx.c core/hp.c core/sbig.c core/xnum.c core/bigint.c core/arena.c tests/cx_drive.c $(ENGINE) $(LIBS)

build/xnum_test: core/xnum.c core/bigint.c core/arena.c $(HDRS) $(ENGINE) tests/xnum_test.c | build
	$(CC) $(CFLAGS) -Icore -o $@ core/xnum.c core/bigint.c core/arena.c tests/xnum_test.c $(ENGINE) $(LIBS)

build/fn_drive: core/fn.c core/hp.c core/bigint.c core/arena.c core/sbig.c $(HDRS) $(ENGINE) tests/fn_drive.c | build
	$(CC) $(CFLAGS) -Icore -o $@ core/fn.c core/hp.c core/bigint.c core/arena.c core/sbig.c tests/fn_drive.c $(ENGINE) $(LIBS)

build/abn_test: $(HDRS) $(ENGINE) tests/abn_test.c tests/norm.h tests/cases.h | build
	$(CC) $(CFLAGS) -Icore -o $@ tests/abn_test.c $(ENGINE) $(LIBS)

build/diff_test: $(HDRS) $(ENGINE) tests/diff_test.c tests/dref.c tests/dref.h tests/norm.h tests/cases.h | build
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
	./build/vectors work/016-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/016-vectors.txt
	./build/vectors work/017-vectors.txt
	./build/vectors work/018-vectors.txt
	./build/vectors work/021-vectors.txt
	./build/vectors work/024-vectors.txt
	./build/vectors work/024b-vectors.txt
	./build/vectors work/025-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/025-vectors.txt
	./build/vectors work/026-vectors.txt
	./build/vectors work/027-vectors.txt
	./build/vectors work/028-vectors.txt
	./build/vectors work/029-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/029-vectors.txt
	./build/vectors work/029b-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/029b-vectors.txt
	./build/vectors work/030-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/030-vectors.txt
	./build/vectors work/030b-vectors.txt
	./build/vectors work/031-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/031-vectors.txt
	./build/vectors work/031b-vectors.txt
	./build/vectors work/032-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/032-vectors.txt
	./build/vectors work/035-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/035-vectors.txt
	./build/vectors tests/021-soroban.txt
	./build/vectors tests/019-soroban.txt
	./build/vectors tests/003-soroban.txt
	./build/vectors tests/018-soroban.txt
	./build/vectors tests/029-soroban.txt
	./build/vectors tests/golden-v1.txt
	./build/load_fuzz 20000
	./build/store_test
	./build/ls027_test
	./build/keys_test
	./build/power_test
	./build/keymap_test
	./build/app_test
	@mkdir -p build/standin
	./build/standin_test tests/frames/standin.crc
	python3 scripts/gen_keymap.py --check
	@mkdir -p build/frames
	./build/screen_test tests/frames
	python3 scripts/gen_font.py --check
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
# Every check, as CI would run them: make test, then make accuracy (abacus #3984: a change that only
# accuracy's loops catch slipped past make test, ac8ad8f). Refuses to run without mpmath, since the
# fuzzers would otherwise skip and pass quietly.
all-checks:
	@$(PYTHON) -c "import mpmath" 2>/dev/null || { echo "all-checks: $(PYTHON) has no mpmath (make all-checks PYTHON=...)"; exit 1; }
	$(MAKE) test
	$(MAKE) accuracy PYTHON=$(PYTHON)
	@echo "all-checks: make test and make accuracy passed"

accuracy: build/fn_drive build/cx_drive build/vectors
	$(PYTHON) tests/fn_fuzz.py build/fn_drive
	$(PYTHON) tests/cx_fuzz.py build/cx_drive
	$(PYTHON) tests/range_fuzz.py build/fn_drive 1 30
	$(PYTHON) tests/pow_fuzz.py build/fn_drive 1 3400
	$(PYTHON) tests/cxkey_fuzz.py 1 5000 > build/cxkey_fuzz.txt
	./build/vectors build/cxkey_fuzz.txt
	$(PYTHON) tests/solve_fuzz.py 1 1500 > build/solve_fuzz.txt
	./build/vectors build/solve_fuzz.txt
	$(PYTHON) tests/stat_fuzz.py 1 1500 > build/stat_fuzz.txt
	./build/vectors build/stat_fuzz.txt
	$(PYTHON) tests/frac_fuzz.py 1 3000 > build/frac_fuzz.txt
	./build/fmt_vectors build/frac_fuzz.txt
	$(PYTHON) tests/base_fuzz.py 1 5000 > build/base_fuzz.txt
	./build/vectors build/base_fuzz.txt
	# unit 016: every key-vector file with a save and load after every key, and run in slices
	for f in $(KEYV) work/016-vectors.txt work/017-vectors.txt; do AB_SAVELOAD=1 AB_STORE=1 ./build/vectors $$f || exit 1; done
	for n in 1 7 1000; do for f in $(KEYV); do AB_SLICE=$$n ./build/vectors $$f || exit 1; done; done
	./build/load_fuzz 100000 7

# The on-chip test app (tests/device), built and run on the host: the same runners and bundle
# the chip gets. The bundle is compiled without -Wpedantic (its string literals are long).
DEVICE_RUNNERS := tests/vectors.c tests/fmt_vectors.c tests/diff_test.c tests/dref.c firmware/store.c
build/device/bundle.c: tests/device/bundle.py $(wildcard work/*-vectors.txt) tests/eqn_fuzz.py tests/prog_fuzz.py tests/veckey_fuzz.py | build
	@mkdir -p build/device
	python3 tests/device/bundle.py $@
build/device/bundle.o: build/device/bundle.c tests/device/main/bundle.h | build
	$(CC) -std=c11 -O0 -Itests/device/main -c -o $@ $<
build/device/abn_test.o: tests/abn_test.c tests/norm.h tests/cases.h core/abn.h | build
	@mkdir -p build/device
	$(CC) $(CFLAGS) -DAB_DEVICE -Dmain=abn_test_main -Icore -Itests -c -o $@ $<
build/device/xnum_test.o: tests/xnum_test.c $(HDRS) | build
	@mkdir -p build/device
	$(CC) $(CFLAGS) -DAB_DEVICE -Dmain=xnum_test_main -Icore -Itests -c -o $@ $<
build/device_host: tests/device/main/device_main.c $(DEVICE_RUNNERS) $(CORE) $(HDRS) $(ENGINE) build/device/bundle.o build/device/abn_test.o build/device/xnum_test.o | build
	$(CC) $(CFLAGS) -DAB_DEVICE -Icore -Ifirmware -Itests -Itests/device/main -o $@ tests/device/main/device_main.c $(DEVICE_RUNNERS) $(CORE) build/device/bundle.o build/device/abn_test.o build/device/xnum_test.o $(ENGINE) $(LIBS)
device-host: build/device_host
	./build/device_host

clean:
	rm -rf build
