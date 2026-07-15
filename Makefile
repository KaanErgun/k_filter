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

.PHONY: all test run parity analyze cross clean

all: $(BUILD)/example

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/example: examples/filter_test.c $(SRC) | $(BUILD)
	$(CC) $(CSTD) $(WARN) $(CFLAGS) $(INC) $^ -o $@ -lm

$(BUILD)/test_filters: test/test_filters.c $(SRC) | $(BUILD)
	$(CC) $(CSTD) $(WARN) -Werror $(CFLAGS) $(INC) -Itest $^ -o $@ -lm

test: $(BUILD)/test_filters
	./$(BUILD)/test_filters

run: $(BUILD)/example
	./$(BUILD)/example

# double-precision harness so C-vs-Python parity measures algorithm, not float32.
$(BUILD)/parity_dump: test/parity_dump.c $(SRC) | $(BUILD)
	$(CC) $(CSTD) $(WARN) $(CFLAGS) -DKF_USE_DOUBLE $(INC) $^ -o $@ -lm

parity: $(BUILD)/parity_dump
	python3 sim/parity_test.py ./$(BUILD)/parity_dump

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
