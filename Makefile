# k_filter — host-side build/test tooling. Nothing here ships to an MCU.
#
#   make            build the example
#   make test       build + run the unit tests
#   make run        build + run the example
#   make analyze    run cppcheck (if installed) over the library
#   make cross      freestanding cross-compile for Cortex-M0+ (needs arm-none-eabi-gcc)
#   make clean

CC      ?= cc
CSTD    ?= -std=c99
WARN    := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
CFLAGS  ?= -O2 -g
INC     := -Iinclude

BUILD   := build
SRC     := src/k_filter.c
# Optional coefficient designers (needs libm) — NOT part of the freestanding core.
DESIGN  := src/k_filter_design.c
# Optional fixed-point (integer) variants for FPU-less parts.
FIXED   := src/k_filter_fixed.c

.PHONY: all test run parity compare bode footprint bench coverage analyze cross clean

all: $(BUILD)/example

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/example: examples/filter_test.c $(SRC) | $(BUILD)
	$(CC) $(CSTD) $(WARN) $(CFLAGS) $(INC) $^ -o $@ -lm

$(BUILD)/test_filters: test/test_filters.c $(SRC) $(DESIGN) $(FIXED) | $(BUILD)
	$(CC) $(CSTD) $(WARN) -Werror $(CFLAGS) $(INC) -Itest $^ -o $@ -lm

test: $(BUILD)/test_filters
	./$(BUILD)/test_filters

run: $(BUILD)/example
	./$(BUILD)/example

# double-precision harness so C-vs-Python parity measures algorithm, not float32.
$(BUILD)/parity_dump: test/parity_dump.c $(SRC) $(DESIGN) | $(BUILD)
	$(CC) $(CSTD) $(WARN) $(CFLAGS) -DKF_USE_DOUBLE $(INC) $^ -o $@ -lm

parity: $(BUILD)/parity_dump
	python3 sim/parity_test.py ./$(BUILD)/parity_dump

# Metrics workbench: rank the smoothing filters on each test signal (pure Python).
compare:
	python3 sim/compare.py --all

# Empirical frequency response (gain/phase) of the frequency-shaping filters.
bode:
	python3 sim/bode.py

$(BUILD)/footprint: test/footprint.c $(SRC) | $(BUILD)
	$(CC) $(CSTD) $(WARN) $(CFLAGS) $(INC) $^ -o $@ -lm

footprint: $(BUILD)/footprint
	./$(BUILD)/footprint

$(BUILD)/bench: test/bench.c $(SRC) $(DESIGN) | $(BUILD)
	$(CC) $(CSTD) $(WARN) -O3 $(INC) $^ -o $@ -lm

bench: $(BUILD)/bench
	./$(BUILD)/bench

# Line/branch coverage of the unit tests (needs gcov/lcov; llvm-cov on macOS).
coverage:
	$(CC) $(CSTD) $(INC) -Itest --coverage -O0 -g \
	  test/test_filters.c $(SRC) $(DESIGN) -o $(BUILD)/cov -lm
	./$(BUILD)/cov >/dev/null
	@command -v gcov >/dev/null 2>&1 && gcov -n -o . src/k_filter.c 2>/dev/null | grep -A1 "k_filter.c" \
	  || echo "gcov not found — coverage data written to *.gcda"

analyze:
	@command -v cppcheck >/dev/null 2>&1 \
	  && cppcheck --enable=all --std=c99 --inline-suppr --quiet \
	       --suppress=missingIncludeSystem $(INC) $(SRC) \
	  || echo "cppcheck not installed — skipping"

# Proves the runtime stays freestanding: no libc/libm, no heap. If anyone adds a
# forbidden dependency, this target stops compiling/linking.
cross:
	@command -v arm-none-eabi-gcc >/dev/null 2>&1 \
	  && arm-none-eabi-gcc $(CSTD) $(WARN) -Werror -Os \
	       -mcpu=cortex-m0plus -mthumb -ffreestanding -fno-stack-protector \
	       -c $(INC) $(SRC) -o $(BUILD)/k_filter_arm.o \
	  && echo "freestanding arm build OK" \
	  || echo "arm-none-eabi-gcc not installed — skipping"

clean:
	rm -rf $(BUILD)
