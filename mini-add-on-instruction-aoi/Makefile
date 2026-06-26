# Makefile for mini-add-on-instruction-aoi
# Rockwell Studio 5000 Add-On Instruction (AOI) Engineering Module
# Knowledge: L1-L7 Complete, L8-L9 Partial

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -I./include
CFLAGS += -Wno-misleading-indentation -Wno-unused-parameter -Wno-unused-function
LDFLAGS = -lm

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TEST_SRC = $(wildcard tests/*.c)
TEST_BIN = $(TEST_SRC:.c=.test)
EXAMPLE_SRC = $(wildcard examples/*.c)
EXAMPLE_BIN = $(EXAMPLE_SRC:.c=.out)

.PHONY: all clean test examples run-examples check-lines

all: $(OBJ)
	@echo "=== Build Complete ==="

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tests/%.test: tests/%.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LDFLAGS)

examples/%.out: examples/%.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LDFLAGS)

test: $(OBJ)
	@if [ -z "$(TEST_SRC)" ]; then echo "No test files found."; else \
		$(MAKE) $(TEST_BIN) && echo "=== Running Tests ===" && \
		for t in $(TEST_BIN); do echo ">>> $$t"; ./$$t || exit 1; done && \
		echo "=== All Tests Passed ==="; fi

examples: $(OBJ)
	@if [ -z "$(EXAMPLE_SRC)" ]; then echo "No example files found."; else \
		$(MAKE) $(EXAMPLE_BIN) && echo "=== Examples Built ==="; fi

run-examples: examples
	@for e in $(EXAMPLE_BIN); do echo ">>> $$e"; ./$$e || exit 1; done

clean:
	rm -f src/*.o tests/*.test examples/*.out

check-lines:
	@total=$$(find . \( -path "*/include/*.h" -o -path "*/src/*.c" \) -exec cat {} + | wc -l); \
	echo "Total include+src lines: $$total"; \
	if [ "$$total" -ge 3000 ]; then echo "LINE COUNT: PASS (>=3000)"; else echo "LINE COUNT: FAIL (<3000)"; fi
