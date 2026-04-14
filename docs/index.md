---
layout: home
title: armvm
---

**armvm** is a self-contained ARMv7 assembler and virtual machine written in C.
It lets you compile ARM32 assembly at runtime and execute the resulting bytecode on any host architecture — x86, x86_64, ARM64, or anything else.

## Key features

| Feature | Details |
|---|---|
| **ARMv7 32-bit** | Full ARM UAL instruction set (data processing, memory, branches, multiply) |
| **Cross-architecture** | Host can be x86-64, ARM64, RISC-V, or any other platform |
| **Runtime assembler** | Compile `.s` files to bytecode inside your process with `compile_buffer` |
| **Syscall interface** | Register host C functions; ARM code calls them via `bl _funcname` |
| **Configurable memory** | Separate stack and heap with sizes chosen at VM creation time |
| **Zero dependencies** | Pure C, only requires the C standard library |

## Quick start

```bash
# Build the assembler/compiler
git clone https://github.com/corepunch/armvm
cd armvm
make

# Run the test suite
make test

# (macOS only) Run the end-to-end example
make -C examples/simple-app run
```

Expected output for the example:

```
Running ARM32 program...
Hello from ARM VM!
Program returned: 42
```

## Navigation

- [Getting Started](getting-started) — build, install, and run your first program
- [Assembly Reference](assembly-reference) — instructions, directives, and condition codes
- [API Reference](api-reference) — C API for embedding armvm in your application
- [Examples](examples) — complete worked examples including host function integration
- [Architecture](architecture) — VM internals, bytecode format, and memory model
