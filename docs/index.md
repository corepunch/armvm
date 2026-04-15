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
| **Runtime assembler** | Compile `.s` files to bytecode inside your process with `avm_loadbuffer` |
| **Lua-like C API** | Embed the VM with `avm_newstate` / `avm_register` / `avm_loadbuffer` / `avm_call` |
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

## Embedding in 30 seconds

```c
#include "avm.h"

static int host_strlen(avm_State *L) {
    avm_pushinteger(L, (int)strlen(avm_tostring(L, 1)));
    return 1;
}

const char *src = ".globl _main\n_main:\n  bl _strlen\n  bx lr\n";

avm_State *L = avm_newstate(64*1024, 64*1024);
avm_register(L, "strlen", host_strlen);
avm_loadbuffer(L, src, strlen(src));
avm_call(L, L->entry_point);
printf("result = %d\n", avm_tointeger(L, 1));
avm_close(L);
```

## Navigation

- [Getting Started](getting-started) — build, install, and run your first program
- [Assembly Reference](assembly-reference) — instructions, directives, and condition codes
- [API Reference](api-reference) — low-level C API (`vm_create`, `execute`, …)
- [Lua-like API](avm-api) — high-level embedding API (`avm_newstate`, `avm_register`, …)
- [Examples](examples) — complete worked examples including host function integration
- [Architecture](architecture) — VM internals, bytecode format, and memory model
