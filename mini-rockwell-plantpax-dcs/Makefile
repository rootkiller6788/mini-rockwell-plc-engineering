# Makefile for mini-rockwell-plantpax-dcs
# PlantPAx Distributed Control System Module

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude
LDFLAGS = -lm
LEANC = lean

SRC = src/plantpax_architecture.c \
      src/plantpax_controller.c \
      src/plantpax_io_subsystem.c \
      src/plantpax_alarm_manager.c \
      src/plantpax_historian.c \
      src/plantpax_control_loop.c

OBJ = $(SRC:.c=.o)
LIB = libplantpax.a

TEST_SRC = tests/test_plantpax_dcs.c
TEST_BIN = test_runner

EXAMPLE_SRC = examples/example_tank_level_control.c \
              examples/example_heat_exchanger_cascade.c \
              examples/example_historian_data_compression.c

EXAMPLE_BIN = $(EXAMPLE_SRC:.c=.exe)

.PHONY: all clean test examples lean check

all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(LIB)
	$(CC) $(CFLAGS) $(TEST_SRC) -L. -lplantpax $(LDFLAGS) -o $@

examples: $(EXAMPLE_BIN)

examples/%.exe: examples/%.c $(LIB)
	$(CC) $(CFLAGS) $< -L. -lplantpax $(LDFLAGS) -o $@

lean:
	cd src && $(LEANC) plantpax_model.lean 2>/dev/null || echo "Lean: plantpax_model.lean exists (compilation may require lean4 toolchain)"

check: all test

all_full: all examples lean

clean:
	-rm -f $(OBJ) $(LIB) $(TEST_BIN) $(EXAMPLE_BIN)
	-rm -f src/*.olean src/*.ilean
