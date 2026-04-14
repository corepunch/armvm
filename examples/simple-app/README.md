# simple-app example

This example shows how to embed **armvm** in a host application and expose custom host functions to ARM32 code via the VM's syscall (interrupt) interface.

## What it does

1. **`input.c`** — a small C program that calls two host-provided functions:
   - `print_string(const char *s)` — prints a string
   - `add_numbers(int a, int b) → int` — adds two integers and returns the result

2. **`main.c`** — the host application that:
   - Registers `print_string` and `add_numbers` in the assembler's symbol table (syscall IDs 1 and 2)
   - Reads and compiles the ARMv7 assembly file at runtime using `compile_buffer`
   - Creates a VM instance with a custom `syscall_handler`
   - Executes the compiled bytecode and prints the return value

## How external calls work

When `clang -arch armv7` compiles `input.c`, a call like `print_string("...")` becomes `bl _print_string` in the ARM assembly.  The armvm assembler strips the leading underscore and looks up `print_string` in its symbol table.  If it finds the name at index *N* it emits a special external-call instruction encoding *N*.  At runtime the VM calls `syscall_handler(vm, N)`, which dispatches to the matching host function.

```
input.c  →  bl _print_string
assembler: symbols[1]="print_string"  →  OP_BEXT | 1
VM:        syscall_handler(vm, 1)  →  host_print_string(vm)
```

## Requirements

- macOS with Xcode command-line tools (`clang -arch armv7` support)
- GCC or Clang for building the host application

## Build and run

```bash
# from the repository root — build armvm-compiler first
make

# then build and run the example
make -C examples/simple-app run
```

Expected output:

```
Running ARM32 program...
Hello from ARM VM!
Program returned: 42
```
