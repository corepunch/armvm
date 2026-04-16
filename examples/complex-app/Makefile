# Makefile for the complex-app example
#
# On macOS with Xcode command-line tools, 'make all' also regenerates
# complex.s from input.c using clang's ARMv7 cross-compiler.  On Linux
# (or any host without clang -arch armv7), 'make app' builds and runs
# the pre-generated complex.s that is already checked into the repository.

CC       = gcc
CFLAGS   = -Wall -Wextra -O2
LDFLAGS  = -lm

ARMVM_DIR = ../../armvm
OBJDIR    = build

# armvm library sources (everything except compiler.c, which has its own main)
ARMVM_SRCS = \
	$(ARMVM_DIR)/armvm.c \
	$(ARMVM_DIR)/armcomp.c \
	$(ARMVM_DIR)/expr.c \
	$(ARMVM_DIR)/memory.c \
	$(ARMVM_DIR)/libpvm.c

# compiler.c is compiled in isolation with -Dmain=_unused_main so that
# compile_buffer() and avm_loadbuffer() are available to link against
# without conflicting with our own main().
COMPILER_SRC = $(ARMVM_DIR)/compiler.c
COMPILER_OBJ = $(OBJDIR)/compiler.o

TARGET     = complex-app
ASM_OUTPUT = complex.s

.PHONY: all app run clean

# 'all' attempts to regenerate the assembly from input.c (macOS only).
all: $(TARGET) $(ASM_OUTPUT)

# 'app' builds the host runner against the pre-generated assembly.
app: $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

# Regenerate ARM assembly from input.c using the Apple clang cross-compiler.
# This target only works on macOS with Xcode command-line tools installed.
$(ASM_OUTPUT): input.c
	clang -S -arch armv7 -target armv7-apple-darwin -marm -O1 $< -o $@

# Compile compiler.c in isolation so its main() is renamed.
$(COMPILER_OBJ): $(COMPILER_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(ARMVM_DIR) -Dmain=_unused_main -c $< -o $@

# Build the host runner.
$(TARGET): main.c $(COMPILER_OBJ) $(ARMVM_SRCS)
	$(CC) $(CFLAGS) -I$(ARMVM_DIR) \
	    main.c $(ARMVM_SRCS) $(COMPILER_OBJ) \
	    $(LDFLAGS) -o $(TARGET)

# Build and run using the pre-generated complex.s.
run: app
	./$(TARGET) $(ASM_OUTPUT)

clean:
	rm -rf $(OBJDIR) $(TARGET)
