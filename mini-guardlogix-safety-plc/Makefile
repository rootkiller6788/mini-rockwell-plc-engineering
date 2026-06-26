# Makefile for mini-guardlogix-safety-plc
# GuardLogix Safety PLC Library — IEC 61508 / IEC 61511 Implementations
#
# Target: build the static library, run tests, and build examples.
#
# Tested with: GCC (MinGW / Linux), MSVC

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g
CFLAGS  += -Iinclude
LDFLAGS  = -lm

# Source files (excluding .lean formal verification files from build)
SRC_C    = $(wildcard src/*.c)
OBJ      = $(SRC_C:.c=.o)

# Test and example targets
TEST_SRC = tests/test_guardlogix.c
TEST_BIN = test_guardlogix

EXAMPLES = examples/example_estop \
           examples/example_light_curtain \
           examples/example_safety_interlock
EXE_BINS = $(EXAMPLES:.c=)

# Static library
LIBNAME  = libguardlogix.a

# Phony targets
.PHONY: all clean test examples lib check

all: lib test examples

# Build the static library
lib: $(LIBNAME)

$(LIBNAME): $(OBJ)
	@echo "  AR    $@"
	@ar rcs $@ $(OBJ)

%.o: %.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Run all tests
test: $(TEST_BIN)
	@echo "=== Running Unit Tests ==="
	@./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(OBJ)
	@echo "  LINK  $@"
	@$(CC) $(CFLAGS) $(TEST_SRC) $(OBJ) $(LDFLAGS) -o $@

# Build all examples
examples: $(EXE_BINS)

examples/%: examples/%.c $(OBJ)
	@echo "  LINK  $@"
	@$(CC) $(CFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

# Clean build artifacts
clean:
	@echo "Cleaning..."
	@rm -f src/*.o
	@rm -f $(LIBNAME)
	@rm -f $(TEST_BIN)
	@rm -f $(EXE_BINS)
	@rm -f *.exe
	@echo "Clean complete."

# Check: compile everything but don't run
check: all

# Display help
help:
	@echo "Targets:"
	@echo "  make all      — Build library, tests, and examples"
	@echo "  make lib      — Build static library libguardlogix.a"
	@echo "  make test     — Build and run unit tests"
	@echo "  make examples — Build example programs"
	@echo "  make clean    — Remove build artifacts"
	@echo "  make help     — Show this help"
