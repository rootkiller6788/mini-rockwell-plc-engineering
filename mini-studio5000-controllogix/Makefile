# Makefile — mini-studio5000-controllogix
# Rockwell Studio 5000 / ControlLogix Engineering Simulation Library
#
# Knowledge coverage: L1-L8 (L9 documented)
#
# Reference standards:
#   IEC 61131-3:2013, ISA-18.2, ISA-88
#   1756-PM004, 1756-PM005, 1756-PM010, 1756-RM006, MOTION-RM003

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -pedantic -O2 -g
INCLUDES = -I./include
LDFLAGS  = -lm

SRCS     = $(wildcard src/*.c)
OBJS     = $(SRCS:.c=.o)

TARGET   = liblogix.a

.PHONY: all clean check

all: $(TARGET)

$(TARGET): $(OBJS)
	ar rcs $(TARGET) $(OBJS)
	@echo "=== Library $(TARGET) built ==="

src/%.o: src/%.c include/*.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

check:
	@echo "=== Compile check ==="
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SRCS)
	@echo "=== Compile check passed ==="
	@rm -f *.o

clean:
	rm -f $(OBJS) $(TARGET) *.o
