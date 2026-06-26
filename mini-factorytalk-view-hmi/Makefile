# =====================================================================
# mini-factorytalk-view-hmi — Makefile
# =====================================================================

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -O2 -g -D_USE_MATH_DEFINES -Wno-unused-parameter -Wno-misleading-indentation
LDFLAGS  := -lm
INCDIR   := include
SRCDIR   := src
TSTDIR   := tests
EXDIR    := examples
BNDIR    := benches

SRCS     := $(SRCDIR)/ftview_common.c $(SRCDIR)/ftview_tag.c $(SRCDIR)/ftview_alarm.c $(SRCDIR)/ftview_security.c $(SRCDIR)/ftview_graphics.c $(SRCDIR)/ftview_datalog.c $(SRCDIR)/ftview_communication.c
OBJS     := $(SRCS:.c=.o)

TESTS    := $(TSTDIR)/test_tagdb $(TSTDIR)/test_alarm $(TSTDIR)/test_security $(TSTDIR)/test_graphics_comm
EXAMPLES := $(EXDIR)/ex01_motor_control_hmi $(EXDIR)/ex02_pid_faceplate $(EXDIR)/ex03_alarm_management
BENCHS   := $(BNDIR)/bench_tagdb

.PHONY: all test examples clean check lines help

all: $(TESTS) $(EXAMPLES)

test: $(TESTS)
	@echo "=== Running Tests ==="
	@fail=0; for t in $(TESTS); do echo ""; ./$$t || fail=1; done; echo ""; \
	if [ $$fail -eq 0 ]; then echo "ALL TESTS PASSED"; else echo "SOME TESTS FAILED"; fi; exit $$fail

examples: $(EXAMPLES)

$(TSTDIR)/test_tagdb: $(TSTDIR)/test_tagdb.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^ $(LDFLAGS)

$(TSTDIR)/test_alarm: $(TSTDIR)/test_alarm.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^ $(LDFLAGS)

$(TSTDIR)/test_security: $(TSTDIR)/test_security.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^ $(LDFLAGS)

$(TSTDIR)/test_graphics_comm: $(TSTDIR)/test_graphics_comm.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^ $(LDFLAGS)

$(EXDIR)/ex01_motor_control_hmi: $(EXDIR)/ex01_motor_control_hmi.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^ $(LDFLAGS)

$(EXDIR)/ex02_pid_faceplate: $(EXDIR)/ex02_pid_faceplate.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^ $(LDFLAGS)

$(EXDIR)/ex03_alarm_management: $(EXDIR)/ex03_alarm_management.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $^ $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/ftview_common.h
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $<

check:
	@echo "=== Safety Check ==="
	@grep -rn "_fn[0-9]" --include="*.c" --include="*.h" . || true
	@grep -rn "_aux[0-9]" --include="*.c" --include="*.h" . || true
	@grep -rn "by trivial" --include="*.lean" . || true
	@grep -rn "\bsorry\b" --include="*.lean" . || true
	@echo "=== Done ==="

lines:
	@echo "include/ lines:"; find include -name "*.h" -exec cat {} + | wc -l
	@echo "src/ lines (.c+.lean):"; find src \( -name "*.c" -o -name "*.lean" \) -exec cat {} + | wc -l
	@echo "include/ + src/ total:"; find include src \( -name "*.h" -o -name "*.c" -o -name "*.lean" \) -exec cat {} + | wc -l

clean:
	rm -f $(OBJS) $(TESTS) $(EXAMPLES)

help:
	@echo "Targets: all test examples clean check lines"
