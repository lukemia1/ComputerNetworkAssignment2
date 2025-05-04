###############################################################################
# Makefile for Simplex SR Emulator Assignment
# - Compiles your sr.c alongside the provided emulator.c
# - Defines BIDIRECTIONAL (0: simplex, 1: bidirectional)
###############################################################################

# ----------------------------------------------------------------------------
#  Compiler and flags
# ----------------------------------------------------------------------------
CC      = gcc
CFLAGS  = -std=c99 -Wall -Werror -DBIDIRECTIONAL=0 -I.

# ----------------------------------------------------------------------------
#  Targets and object files
# ----------------------------------------------------------------------------
TARGET  = simulator
OBJS    = sr.o emulator.o

# ----------------------------------------------------------------------------
#  Default rule: build the simulator executable
# ----------------------------------------------------------------------------
all: $(TARGET)

# Link step: build the final executable from object files
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Compile your selective-repeat implementation
sr.o: sr.c sr.h
	$(CC) $(CFLAGS) -c sr.c

# Compile the provided emulator (autograder) code
emulator.o: emulator.c emulator.h sr.h
	$(CC) $(CFLAGS) -c emulator.c

# ----------------------------------------------------------------------------
#  Clean up build artifacts
# ----------------------------------------------------------------------------
clean:
	rm -f $(OBJS) $(TARGET)

# ----------------------------------------------------------------------------
#  Phony targets
# ----------------------------------------------------------------------------
.PHONY: all clean
