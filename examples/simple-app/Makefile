# Makefile for the simple-app example
#
# Requires macOS with Xcode command-line tools.
# clang -arch armv7 cross-compilation is only supported on Apple platforms.

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

# compiler.c provides compile_buffer, vm_create, vm_shutdown, and the
# symbol table.  Its main() is renamed so ours takes precedence; it must be
# compiled separately so the define does not affect any other translation unit.
COMPILER_SRC = $(ARMVM_DIR)/compiler.c
COMPILER_OBJ = $(OBJDIR)/compiler.o

TARGET     = simple-app
ASM_OUTPUT = input.s

.PHONY: all run clean

all: $(TARGET) $(ASM_OUTPUT)

$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile input.c to ARMv7 assembly using the Apple clang cross-compiler
$(ASM_OUTPUT): input.c
	clang -S -arch armv7 -target armv7-apple-darwin -marm -O1 $< -o $@

# Compile compiler.c in isolation with -Dmain=_unused_main so only its
# main() is renamed and our main.c is left untouched.
$(COMPILER_OBJ): $(COMPILER_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(ARMVM_DIR) -Dmain=_unused_main -c $< -o $@

# Build the example runner: main.c + armvm sources + pre-compiled compiler.o
$(TARGET): main.c $(COMPILER_OBJ) $(ARMVM_SRCS)
	$(CC) $(CFLAGS) -I$(ARMVM_DIR) \
	    main.c $(ARMVM_SRCS) $(COMPILER_OBJ) \
	    $(LDFLAGS) -o $(TARGET)

# Build everything and run the example
run: all
	./$(TARGET) $(ASM_OUTPUT)

clean:
	rm -rf $(OBJDIR) $(TARGET) $(ASM_OUTPUT)
