# Makefile for mini-compactlogix-pointio
# Rockwell CompactLogix Point I/O (1734 series)

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -O2 -g
LDFLAGS  := -lm
INCLUDES := -Iinclude

BUILD_DIR := build

SRC_FILES := src/pointio_module.c src/pointio_digital_io.c src/pointio_analog_io.c \
             src/pointio_connection.c src/pointio_scanner.c src/pointio_fault.c \
             src/pointio_signal_proc.c

TEST_FILES := tests/test_pointio.c

EXAMPLE_FILES := examples/example_digital_io_demo.c \
                 examples/example_analog_scaling.c \
                 examples/example_fault_diagnostics.c

SRC_OBJS  := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))
TEST_BINS := $(patsubst tests/%.c,$(BUILD_DIR)/%,$(TEST_FILES))
EXAMPLE_BINS := $(patsubst examples/%.c,$(BUILD_DIR)/%,$(EXAMPLE_FILES))

.PHONY: all
all: test examples

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/test_pointio: tests/test_pointio.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_digital_io_demo: examples/example_digital_io_demo.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_analog_scaling: examples/example_analog_scaling.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_fault_diagnostics: examples/example_fault_diagnostics.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: test
test: $(TEST_BINS)
	@echo "=== Running Test Suite ==="
	-$(BUILD_DIR)/test_pointio
	@echo "=== Tests Complete ==="

.PHONY: examples
examples: $(EXAMPLE_BINS)
	@echo "=== All examples built ==="

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: lines
lines:
	@find include src -type f \( -name "*.h" -o -name "*.c" \) -exec cat {} + | wc -l
