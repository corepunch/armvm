# Makefile for armvm - ARM assembler and virtual machine
# Supports Linux and Mac

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lm

# Directories
SRCDIR = armvm
TESTDIR = test
OBJDIR = build

# Source files
SRCS = $(SRCDIR)/armvm.c $(SRCDIR)/compiler.c $(SRCDIR)/armcomp.c \
       $(SRCDIR)/expr.c $(SRCDIR)/memory.c $(SRCDIR)/libpvm.c

# Object files
OBJS = $(OBJDIR)/armvm.o $(OBJDIR)/compiler.o $(OBJDIR)/armcomp.o \
       $(OBJDIR)/expr.o $(OBJDIR)/memory.o $(OBJDIR)/libpvm.o

# Test files
TEST_SRCS = $(TESTDIR)/armtest.c
TEST_OBJS = $(TEST_SRCS:$(TESTDIR)/%.c=$(OBJDIR)/test_%.o)

# Output executable
TARGET = armvm-compiler
TEST_TARGET = $(OBJDIR)/armtest

# Default target - build the compiler and VM
all: $(TARGET)

# Create build directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile object files (pattern rule)
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Explicit dependencies for object files
$(OBJDIR)/armvm.o: $(SRCDIR)/armvm.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/compiler.o: $(SRCDIR)/compiler.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/armcomp.o: $(SRCDIR)/armcomp.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/expr.o: $(SRCDIR)/expr.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/memory.o: $(SRCDIR)/memory.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/libpvm.o: $(SRCDIR)/libpvm.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link the main executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)

# Compile test object files
$(OBJDIR)/test_%.o: $(TESTDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c $< -o $@

# Link test executable
# Note: This uses -Dmain=_unused_main to rename the main() in compiler.c when compiling it
# for tests, allowing the test suite's main() to be used instead. This is a simple workaround
# to avoid having to refactor compiler.c. If compiler.c is refactored in the future to separate
# test helper functions from main(), this approach should be updated to link test_program() and
# run_program() from a separate object file.
$(TEST_TARGET): $(TEST_OBJS) $(filter-out $(OBJDIR)/compiler.o,$(OBJS)) $(SRCDIR)/compiler.c | $(OBJDIR)
	$(CC) $(TEST_OBJS) $(filter-out $(OBJDIR)/compiler.o,$(OBJS)) $(SRCDIR)/compiler.c -Dmain=_unused_main $(CFLAGS) $(LDFLAGS) -o $(TEST_TARGET)

# Run tests
test: $(TEST_TARGET)
	@echo "Running tests..."
	@$(TEST_TARGET)

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Phony targets
.PHONY: all test clean
