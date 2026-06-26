CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I./include -O2
LDFLAGS = -lm

SRCDIR = src
INCDIR = include
TESTDIR = tests
EXAMPLEDIR = examples

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:.c=.o)
TEST_SRCS = $(wildcard $(TESTDIR)/*.c)
TEST_BINS = $(TEST_SRCS:.c=.test)
EXAMPLE_SRCS = $(wildcard $(EXAMPLEDIR)/*.c)
EXAMPLE_BINS = $(EXAMPLE_SRCS:.c=.exe)

.PHONY: all clean test examples

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TESTDIR)/%.test: $(TESTDIR)/%.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)
	./$@

$(EXAMPLEDIR)/%.exe: $(EXAMPLEDIR)/%.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

test: $(TEST_BINS)

examples: $(EXAMPLE_BINS)

clean:
	rm -f $(SRCDIR)/*.o
	rm -f $(TESTDIR)/*.test
	rm -f $(EXAMPLEDIR)/*.exe
