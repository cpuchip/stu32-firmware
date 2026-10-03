# Abacus core: portable C11, no platform calls. `make test` runs every check.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
CORE := core/abn_dec.c core/calc.c
HDRS := core/abn.h core/calc.h

.PHONY: all test clean
all: build/vectors build/abn_test

build/vectors: $(CORE) $(HDRS) tests/vectors.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/vectors.c

build/abn_test: $(CORE) $(HDRS) tests/abn_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Icore -o $@ $(CORE) tests/abn_test.c

test: all
	./build/abn_test
	./build/vectors work/001-vectors.txt

clean:
	rm -rf build
