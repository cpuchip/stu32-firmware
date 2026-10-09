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
# Casimir (casim's computer algebra, CAS 004), its src/ at CASIM_PIN (scripts/fetch-casim.sh): built with no frame
# above 1 KB, as casim builds it; linked wherever the core is, and the core's CAS step built in (AB_CASIM)
CASIM_O := build/casim.o
ENGINE := build/abn_intel.o $(CASIM_O) $(INTEL_LIB)
CFLAGS += -DAB_CASIM -Ibuild/casim
LIBS := -lm

.PHONY: all test accuracy all-checks clang-check clean device-host
# make accuracy needs a python with mpmath: make all-checks PYTHON=...
PYTHON ?= python3
KEYV := $(filter-out work/002-vectors.txt work/016-vectors.txt work/017-vectors.txt work/018-vectors.txt,$(filter-out %-fmt-vectors.txt,$(wildcard work/*-vectors.txt))) tests/021-soroban.txt tests/019-soroban.txt tests/003-soroban.txt tests/018-soroban.txt tests/029-soroban.txt tests/034-soroban.txt tests/harness-soroban.txt tests/046-soroban.txt tests/047-soroban.txt tests/050-soroban.txt tests/054-soroban.txt tests/fc-soroban.txt
all: build/standin_test build/app_test build/keymap_test build/power_test build/screen_test build/keys_test build/ls027_test build/draw_test build/store_test build/load_fuzz build/vectors build/fmt_vectors build/fmt_drive build/fn_drive build/cx_drive build/xnum_test build/abn_test build/diff_test build/bi_drive build/hp_drive build/deep_test build/eqn_depth build/eqn_stack build/stackkeys

# every build/ target is order-only on the directory: a fresh clone has none (abacus #3803)
build:
	mkdir -p build

$(INTEL_LIB):
	CC="$(CC)" scripts/fetch-intel-dfp.sh

# The library's headers are its own; -isystem keeps our warning flags off them.
build/casim/.pin build/casim/casim.c build/casim/casim.h: CASIM_PIN
	scripts/fetch-casim.sh
$(CASIM_O): build/casim/.pin build/casim/casim.c build/casim/casim.h core/abn.h | build
	$(CC) $(CFLAGS) -Wframe-larger-than=1024 -Icore -c -o $@ build/casim/casim.c

build/abn_intel.o: core/abn_intel.c core/abn.h $(INTEL_LIB) | build
	@mkdir -p build
	$(CC) $(CFLAGS) $(BID_FLAGS) -isystem $(INTEL_SRC) -c -o $@ core/abn_intel.c

SCREEN := firmware/screen.c firmware/font5x7.c firmware/draw.c
build/keymap_test: $(CORE) $(HDRS) $(ENGINE) $(SCREEN) firmware/keymap.c firmware/keymap.h firmware/keys.c firmware/keys.h firmware/screen.h tests/keymap_test.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -o $@ $(CORE) $(SCREEN) firmware/keymap.c firmware/keys.c tests/keymap_test.c $(ENGINE) $(LIBS)

APP := firmware/app.c firmware/apps.c firmware/keymap.c firmware/keys.c firmware/power.c firmware/settings.c firmware/piezo.c
# The games (games.list): scripts/gen_games.py exports each at its pin into build/games and writes
# build/games/games.mk (GAMES_C, GAMES_O, and a rule a game: its manifest's flags and its own include directories, always
# -Wframe-larger-than=1024 (an error by CFLAGS' -Werror; the spelling clang takes too), every object depending on the export's stamp, since git archive gives
# files their commit's time) and build/games/games.h (GAMES_DECLARE, GAMES_ENTRIES for apps.c).
build/games/games.mk: games.list scripts/gen_games.py
	python3 scripts/gen_games.py games.list build/games
build/games/.stamp build/games/games.h: build/games/games.mk
ifeq ($(filter clean,$(MAKECMDGOALS)),)       # (clean needs no games, nor a clone to make them)
include build/games/games.mk
endif
build/app_test: $(CORE) $(HDRS) $(ENGINE) $(SCREEN) $(APP) $(GAMES_O) build/games/games.h firmware/app.h firmware/keymap.h firmware/power.h firmware/screen.h tests/app_test.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -Ibuild/games -o $@ $(CORE) $(SCREEN) $(APP) $(GAMES_O) tests/app_test.c $(ENGINE) $(LIBS)

# unit 052: the key sequences to the deepest stacks, for scripts/resources.py's board measurements
build/stackkeys: $(CORE) $(HDRS) $(ENGINE) $(SCREEN) $(APP) $(GAMES_O) build/games/games.h firmware/app.h firmware/keymap.h tests/stackkeys.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -Ibuild/games -o $@ $(CORE) $(SCREEN) $(APP) $(GAMES_O) tests/stackkeys.c $(ENGINE) $(LIBS)

build/standin_test: $(CORE) $(HDRS) $(ENGINE) $(SCREEN) $(APP) $(GAMES_O) build/games/games.h firmware/standin.c firmware/standin.h firmware/app.h firmware/keymap.h firmware/screen.h tests/standin_test.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -Ibuild/games -o $@ $(CORE) $(SCREEN) $(APP) $(GAMES_O) firmware/standin.c tests/standin_test.c $(ENGINE) $(LIBS)

build/power_test: firmware/gauge.c firmware/gauge.h firmware/power.c firmware/power.h firmware/pins.c firmware/pins.h firmware/piezo.c firmware/piezo.h tests/power_test.c | build
	$(CC) $(CFLAGS) -Ifirmware -o $@ firmware/gauge.c firmware/power.c firmware/pins.c firmware/piezo.c tests/power_test.c -lm

build/screen_test: $(CORE) $(HDRS) $(ENGINE) $(SCREEN) firmware/screen.h firmware/font5x7.h firmware/ls027.h tests/screen_test.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -o $@ $(CORE) $(SCREEN) tests/screen_test.c $(ENGINE) $(LIBS)

build/keys_test: firmware/keys.c firmware/keys.h tests/keys_test.c | build
	$(CC) $(CFLAGS) -Ifirmware -o $@ firmware/keys.c tests/keys_test.c

build/draw_test: firmware/draw.c firmware/draw.h firmware/font5x7.c tests/draw_test.c | build
	$(CC) $(CFLAGS) -Ifirmware -o $@ firmware/draw.c firmware/font5x7.c tests/draw_test.c

build/ls027_test: firmware/ls027.c firmware/ls027.h tests/ls027_test.c | build
	$(CC) $(CFLAGS) -Ifirmware -o $@ firmware/ls027.c tests/ls027_test.c

build/store_test: $(CORE) $(HDRS) $(ENGINE) firmware/store.c firmware/store.h firmware/settings.c firmware/settings.h firmware/piezo.c tests/store_test.c | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -o $@ $(CORE) firmware/store.c firmware/settings.c firmware/piezo.c tests/store_test.c $(ENGINE) $(LIBS)

build/load_fuzz: $(CORE) $(HDRS) $(ENGINE) tests/load_fuzz.c | build
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/load_fuzz.c $(ENGINE) $(LIBS)

build/vectors: $(CORE) $(HDRS) $(ENGINE) tests/vectors.c tests/norm.h firmware/store.c firmware/store.h | build
	$(CC) $(CFLAGS) -Icore -Ifirmware -o $@ $(CORE) firmware/store.c tests/vectors.c $(ENGINE) $(LIBS)

# unit 047: the parser's depth guard at the deepest site, on a guarded thread stack; the units the parser reaches
# (tests/eqn_depth.py's rule); the stack a check parse uses, painted
build/deep_test: $(CORE) $(HDRS) $(ENGINE) tests/deep_test.c | build
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/deep_test.c $(ENGINE) $(LIBS) -lpthread

build/eqn_depth: $(CORE) $(HDRS) $(ENGINE) tests/eqn_depth.c | build
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/eqn_depth.c $(ENGINE) $(LIBS)

build/eqn_stack: $(CORE) $(HDRS) $(ENGINE) tests/eqn_stack.c | build
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/eqn_stack.c $(ENGINE) $(LIBS) -lpthread

build/fmt_vectors: $(CORE) $(HDRS) $(ENGINE) tests/fmt_vectors.c | build
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/fmt_vectors.c $(ENGINE) $(LIBS)

build/fmt_drive: core/fmt.c core/fn.c core/hp.c core/frac.c core/bigint.c core/arena.c core/sbig.c core/xnum.c core/base.c $(HDRS) $(ENGINE) tests/fmt_drive.c | build
	$(CC) $(CFLAGS) -Icore -o $@ core/fmt.c core/fn.c core/hp.c core/frac.c core/bigint.c core/arena.c core/sbig.c core/xnum.c core/base.c tests/fmt_drive.c $(ENGINE) $(LIBS)

build/cx_drive: core/cplx.c core/hp.c core/sbig.c core/xnum.c core/bigint.c core/arena.c $(HDRS) $(ENGINE) tests/cx_drive.c | build
	$(CC) $(CFLAGS) -Icore -o $@ core/cplx.c core/hp.c core/sbig.c core/xnum.c core/bigint.c core/arena.c tests/cx_drive.c $(ENGINE) $(LIBS)

# unit 045: bigint_impl.h's arithmetic, both its types, for tests/bi_fuzz.py (Python's exact integers the oracle)
build/bi_drive: core/bigint.c core/sbig.c core/xnum.c core/arena.c $(HDRS) $(ENGINE) tests/bi_drive.c | build
	$(CC) $(CFLAGS) -Icore -o $@ core/bigint.c core/sbig.c core/xnum.c core/arena.c tests/bi_drive.c $(ENGINE) $(LIBS)
# unit 045's review: hp_div's quotient exact, truncated to p digits (tests/hp_div_fuzz.py, Python's Fraction the oracle)
build/hp_drive: core/hp.c core/sbig.c core/xnum.c core/bigint.c core/arena.c $(HDRS) $(ENGINE) tests/hp_drive.c | build
	$(CC) $(CFLAGS) -Icore -o $@ core/hp.c core/sbig.c core/xnum.c core/bigint.c core/arena.c tests/hp_drive.c $(ENGINE) $(LIBS)
build/xnum_test: core/xnum.c core/bigint.c core/arena.c $(HDRS) $(ENGINE) tests/xnum_test.c | build
	$(CC) $(CFLAGS) -Icore -o $@ core/xnum.c core/bigint.c core/arena.c tests/xnum_test.c $(ENGINE) $(LIBS)

build/fn_drive: core/fn.c core/hp.c core/bigint.c core/arena.c core/sbig.c core/xnum.c $(HDRS) $(ENGINE) tests/fn_drive.c | build
	$(CC) $(CFLAGS) -Icore -o $@ core/fn.c core/hp.c core/bigint.c core/arena.c core/sbig.c core/xnum.c tests/fn_drive.c $(ENGINE) $(LIBS)
# unit 034 review: y^x's first precision lowered so that roundings fall near midpoints; the Ziv loop must keep
# them correctly rounded (tests/pow_exact.py); unit 044: the one-input functions' too (ZIV_FIRST, tests/fn_fuzz.py)
build/fn_drive_p37: core/fn.c core/hp.c core/bigint.c core/arena.c core/sbig.c core/xnum.c $(HDRS) $(ENGINE) tests/fn_drive.c | build
	$(CC) $(CFLAGS) -DPOW_FIRST=37 -DZIV_FIRST=37 -Icore -o $@ core/fn.c core/hp.c core/bigint.c core/arena.c core/sbig.c core/xnum.c tests/fn_drive.c $(ENGINE) $(LIBS)

build/abn_test: $(HDRS) $(ENGINE) tests/abn_test.c tests/norm.h tests/cases.h | build
	$(CC) $(CFLAGS) -Icore -o $@ tests/abn_test.c $(ENGINE) $(LIBS)

build/diff_test: $(HDRS) $(ENGINE) tests/diff_test.c tests/dref.c tests/dref.h tests/norm.h tests/cases.h | build
	$(CC) $(CFLAGS) -Icore -Itests -o $@ tests/diff_test.c tests/dref.c $(ENGINE) $(LIBS)

test: all
	./build/abn_test
	./build/diff_test
	./build/xnum_test
	python3 tests/bi_fuzz.py build/bi_drive 1 200
	python3 tests/hp_div_fuzz.py build/hp_drive 1 5000
	./build/cx_drive < tests/cx_ties.in | diff - tests/cx_ties.out    # 045 review: exact ties of negative powers
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
	./build/vectors work/032b-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/032b-vectors.txt
	./build/vectors work/035-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/035-vectors.txt
	./build/vectors work/039-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/039-vectors.txt
	./build/vectors work/040-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/040-vectors.txt
	./build/vectors work/042-sweep-vectors.txt
	./build/vectors work/040c-red-vectors.txt
	./build/vectors work/040d-vectors.txt
	./build/vectors work/041a-vectors.txt
	./build/vectors work/041b-vectors.txt
	./build/vectors work/041c-vectors.txt
	./build/vectors work/041d-vectors.txt
	./build/vectors work/044-vectors.txt
	./build/vectors work/cas004-vectors.txt
	./build/vectors work/033-vectors.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors work/033-vectors.txt
	./build/vectors tests/021-soroban.txt
	./build/vectors tests/019-soroban.txt
	./build/vectors tests/003-soroban.txt
	./build/vectors tests/018-soroban.txt
	./build/vectors tests/029-soroban.txt
	./build/vectors tests/034-soroban.txt
	./build/vectors tests/harness-soroban.txt
	./build/vectors tests/046-soroban.txt
	./build/vectors tests/047-soroban.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors tests/047-soroban.txt
	./build/vectors tests/048-soroban.txt    # (no save/load pass: the report and the notes are not in the image)
	./build/vectors tests/050-soroban.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors tests/050-soroban.txt
	./build/vectors tests/054-soroban.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors tests/054-soroban.txt
	./build/vectors tests/fc-soroban.txt
	AB_SAVELOAD=1 AB_STORE=1 ./build/vectors tests/fc-soroban.txt
	./build/deep_test
	python3 tests/eqn_depth_fuzz.py build/eqn_depth 1 3000
	python3 tests/eqn_stack_fuzz.py build/eqn_stack 1 2000
	./build/vectors tests/038-soroban.txt
	./build/vectors tests/golden-v1.txt
	./build/load_fuzz 20000
	bash tests/gen_games_test.sh
	./build/store_test
	./build/ls027_test
	./build/draw_test
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
# The second compiler (docs/toolchain.md, basecamp #5436): every C file of the core, the firmware, the tests and
# Casimir compiled with clang-18 -Werror in a pinned image, compile only (gcc ships). primer's wasm build is clang, and
# found what gcc passes (draw_test's argument order, casim's -Wsign-compare). The planted control must fail, or the
# flags are not biting.
CLANG_IMAGE := abacus-clang:18
CLANG_FLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Wsign-compare -DAB_CASIM -Ibuild/casim -Icore -Ifirmware \
               -Ibuild/games -Itests
clang-check: build/casim/.pin build/games/games.h
	@docker image inspect $(CLANG_IMAGE) >/dev/null 2>&1 || docker build -q -t $(CLANG_IMAGE) tools/clang
	@docker run --rm -v "$(CURDIR):/w" -w /w --user "$$(id -u):$$(id -g)" $(CLANG_IMAGE) sh -c '\
	  if clang-18 $(CLANG_FLAGS) -c -o /dev/null tools/clang/control.c 2>/dev/null; then \
	    echo "clang-check: the planted control compiled: the flags do not bite"; exit 1; fi; \
	  n=0; bad=0; \
	  for f in core/*.c firmware/*.c tests/*.c build/casim/casim.c; do \
	    x=""; [ "$$f" = core/abn_intel.c ] && x="$(BID_FLAGS) -isystem $(INTEL_SRC)"; \
	    n=$$((n + 1)); clang-18 $(CLANG_FLAGS) $$x -c -o /dev/null $$f || bad=$$((bad + 1)); \
	  done; \
	  if [ -f build/wamr/.pin ]; then n=$$((n + 1)); clang-18 $(CLANG_FLAGS) -DSTU_WASM=1 -Ifirmware/wasm \
	    -isystem build/wamr/core/iwasm/include -c -o /dev/null firmware/wasm/wasm_app.c || bad=$$((bad + 1)); \
	  else echo "clang-check: no build/wamr (scripts/fetch-wamr.sh), so firmware/wasm/wasm_app.c is not compiled"; fi; \
	  echo "clang-check: $$n files, $$bad failing (the control refused)"; [ $$bad = 0 ]'

all-checks:
	@$(PYTHON) -c "import mpmath" 2>/dev/null || { echo "all-checks: $(PYTHON) has no mpmath (make all-checks PYTHON=...)"; exit 1; }
	$(MAKE) test
	$(MAKE) accuracy PYTHON=$(PYTHON)
	$(MAKE) clang-check
	@echo "all-checks: make test, make accuracy and make clang-check passed"

accuracy: build/fn_drive build/fn_drive_p37 build/cx_drive build/vectors
	$(PYTHON) tests/fn_fuzz.py build/fn_drive
	$(PYTHON) tests/cx_fuzz.py build/cx_drive
	$(PYTHON) tests/range_fuzz.py build/fn_drive 1 30
	$(PYTHON) tests/pow_fuzz.py build/fn_drive 1 3400
	$(PYTHON) tests/pow_exact.py build/fn_drive 1 4000
	$(PYTHON) tests/pow_exact.py build/fn_drive_p37 2 4000
	$(PYTHON) tests/fn_fuzz.py build/fn_drive_p37 300 44     # unit 044: the loop keeps every rounding sure
	$(PYTHON) tests/fn_edge.py build/fn_drive 25 1            # unit 044 review: the range's edges, the status too
	$(PYTHON) tests/polar_fuzz.py build/fn_drive 1 1500 --rec
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
	for n in 1 7 1000; do AB_SLICE=$$n ./build/vectors tests/048-soroban.txt || exit 1; done    # (048: slices only)
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
