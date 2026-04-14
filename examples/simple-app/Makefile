# Makefile for the simple-app example
#
# Requires macOS with Xcode command-line tools.
# clang -arch armv7 cross-compilation is only supported on Apple platforms.

CC       = gcc
CFLAGS   = -Wall -Wextra -O2
LDFLAGS  = -lm

ARMVM_DIR = ../../armvm

# armvm library sources (everything except compiler.c, which has its own main)
ARMVM_SRCS = \
	$(ARMVM_DIR)/armvm.c \
	$(ARMVM_DIR)/armcomp.c \
	$(ARMVM_DIR)/expr.c \
	$(ARMVM_DIR)/memory.c \
	$(ARMVM_DIR)/libpvm.c

# compiler.c provides compile_buffer, vm_create, vm_shutdown, and the
# symbol table.  We rename its main() so ours takes precedence.
COMPILER_SRC = $(ARMVM_DIR)/compiler.c

TARGET     = simple-app
ASM_OUTPUT = input.s

.PHONY: all run clean

all: $(TARGET) $(ASM_OUTPUT)

# Compile input.c to ARMv7 assembly using the Apple clang cross-compiler
$(ASM_OUTPUT): input.c
	clang -S -arch armv7 -target armv7-apple-darwin -O1 $< -o $@

# Build the example runner, linking against the armvm library sources.
# -Dmain=_unused_main renames main() in compiler.c so our main() wins.
$(TARGET): main.c $(COMPILER_SRC) $(ARMVM_SRCS)
	$(CC) $(CFLAGS) \
	    -I$(ARMVM_DIR) \
	    -Dmain=_unused_main \
	    main.c $(COMPILER_SRC) $(ARMVM_SRCS) \
	    $(LDFLAGS) -o $(TARGET)

# Build everything and run the example
run: all
	./$(TARGET) $(ASM_OUTPUT)

clean:
	rm -f $(TARGET) $(ASM_OUTPUT)
