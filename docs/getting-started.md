---
layout: page
title: Getting Started
permalink: /getting-started/
nav_order: 1
---

# Getting Started

This page walks you through building armvm, running the test suite, and executing your first ARM32 program.

## Prerequisites

| Tool | Notes |
|---|---|
| GCC or Clang | Any recent version for the host |
| GNU Make | Standard `make` |
| clang `-arch armv7` | **macOS only** — required only for the `simple-app` example |

## Building

### Using Make (recommended)

```bash
# Clone the repository
git clone https://github.com/corepunch/armvm
cd armvm

# Build the armvm-compiler executable
make

# Run all unit tests
make test

# Remove build artifacts
make clean
```

The default `make` target produces the `armvm-compiler` executable in the repository root.

### Manual build

```bash
cd armvm
gcc -Wall -O2 -o ../armvm-compiler \
    armvm.c compiler.c armcomp.c expr.c memory.c libpvm.c -lm
```

### Using Xcode

```bash
xcodebuild -project armvm.xcodeproj -scheme armvm
```

## Your first program

### Step 1 — write ARM assembly

```bash
cat > hello.s << 'EOF'
.globl _main
_main:
    mov r0, #42
    bx lr
EOF
```

### Step 2 — compile to bytecode

```bash
./armvm-compiler -o hello.bin hello.s
```

The compiler produces two files:

| File | Contents |
|---|---|
| `hello.bin` | Executable bytecode with a 12-byte ORCA header |
| `hello.bin_d` | Debug symbol table (line-number ↔ bytecode offset mapping) |

### Step 3 — run it

Write a small host program that loads and executes the bytecode:

```c
#include <stdio.h>
#include <stdlib.h>
#include "armvm/vm.h"

static DWORD no_syscall(LPVM vm, DWORD id) { return 0; }

int main(void) {
    FILE *fp = fopen("hello.bin", "rb");
    fseek(fp, 0, SEEK_END);
    DWORD size = (DWORD)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    BYTE *prog = malloc(size);
    fread(prog, size, 1, fp);
    fclose(fp);

    LPVM vm = vm_create(no_syscall, 64*1024, 64*1024, prog, size);
    free(prog);

    execute(vm, 0);          /* start at offset 0 */
    printf("r0 = %u\n", vm->r[0]);   /* prints: r0 = 42 */
    vm_shutdown(vm);
    return 0;
}
```

## Running the tests

```bash
make test
```

The test binary (`build/armtest`) runs 11 unit tests and prints a summary:

```
ARM VM Test Suite
=================

✓ testMOV
✓ testLSL
✓ testLSR
✓ testPUSH
✓ testCMP
✓ testCMP2
✓ testBL
✓ testLDR
✓ testLDR2
✓ testADD
✓ testPopPC

=================
Test Results: 11/11 passed
```

## Generating compatible assembly from C

### macOS / Xcode

```bash
# Compile a C file to ARMv7 assembly
clang -S -arch armv7 -target armv7-apple-darwin -O1 input.c -o input.s
```

### GCC cross-compiler (Linux)

```bash
arm-linux-gnueabi-gcc -S -march=armv7-a -marm -O2 input.c -o input.s
```

> **Note**: GCC may generate directives that armvm does not recognise. Unknown
> directives are ignored, so this is usually harmless, but verify the output.

## simple-app example (macOS)

The `examples/simple-app/` directory contains a full end-to-end demonstration
that cross-compiles a C source file, injects custom host functions via the
syscall interface, and runs the result in the VM.

```bash
# Build armvm first, then run the example
make
make -C examples/simple-app run
```

See [Examples](examples) for a detailed walkthrough.
