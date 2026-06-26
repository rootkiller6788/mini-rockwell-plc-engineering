# Makefile for mini-rockwell-motion-kinetix
# Rockwell Kinetix Motion Control – Servo Drives and CIP Motion

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -O2 -g
LDFLAGS  := -lm
INCLUDES := -Iinclude

BUILD_DIR := build

SRC_FILES := src/axis_state.c src/motion_planner.c src/servo_tuning.c src/cip_motion.c src/kinetix_drive.c src/motion_advanced.c

TEST_FILES := tests/test_axis_state.c tests/test_motion_planner.c tests/test_servo_tuning.c

EXAMPLE_FILES := examples/example_point_to_point.c examples/example_camming.c examples/example_servo_tuning.c

BENCH_FILES := benches/bench_motion_perf.c

DEMO_FILES := demos/demo_trajectory.c

SRC_OBJS  := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))
TEST_BINS := $(patsubst tests/%.c,$(BUILD_DIR)/%,$(TEST_FILES))
EXAMPLE_BINS := $(patsubst examples/%.c,$(BUILD_DIR)/%,$(EXAMPLE_FILES))
BENCH_BINS := $(patsubst benches/%.c,$(BUILD_DIR)/%,$(BENCH_FILES))
DEMO_BINS := $(patsubst demos/%.c,$(BUILD_DIR)/%,$(DEMO_FILES))

.PHONY: all
all: test examples benches demos

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/test_axis_state: tests/test_axis_state.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_motion_planner: tests/test_motion_planner.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_servo_tuning: tests/test_servo_tuning.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_point_to_point: examples/example_point_to_point.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_camming: examples/example_camming.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_servo_tuning: examples/example_servo_tuning.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: test
test: $(TEST_BINS)
	@echo "=== Running All Tests ==="
	-$(BUILD_DIR)/test_axis_state
	-$(BUILD_DIR)/test_motion_planner
	-$(BUILD_DIR)/test_servo_tuning
	@echo "=== All Tests Done ==="

$(BUILD_DIR)/bench_motion_perf: benches/bench_motion_perf.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/demo_trajectory: demos/demo_trajectory.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: examples
examples: $(EXAMPLE_BINS)
	@echo "=== All examples built ==="

.PHONY: benches
benches: $(BENCH_BINS)
	@echo "=== All benches built ==="

.PHONY: demos
demos: $(DEMO_BINS)
	@echo "=== All demos built ==="

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: lines
lines:
	@find include src -type f \( -name "*.h" -o -name "*.c" \) -exec cat {} + | wc -l
