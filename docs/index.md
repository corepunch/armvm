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
#include <stdio.h>
#include <string.h>
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

## Use Cases

### Sandboxed plugin / mod execution
Run untrusted user-supplied code — game mods, content scripts, user plugins — in a hard-isolated environment, inspired by the **Quake III Arena VM**. The guest has no direct access to host memory or system calls beyond the narrow set of host functions you register, so a buggy or malicious plugin cannot corrupt or escape the host process.

### Cross-architecture legacy code
Execute 32-bit ARMv7 logic — old iOS app libraries, embedded firmware, legacy mobile SDKs — on any modern host (x86-64 servers, Apple Silicon Macs, RISC-V boards) without a full OS emulator. Think of it like a lightweight container for ARM32 binaries: bring the code, not the hardware.

### Embedded scripting engine
Embed armvm as a scripting layer inside a C/C++ application. Authors write behaviour in ARM assembly (or compile C to ARM32 with Clang), and you register only the host functions you want to expose — a fully deterministic, zero-dependency alternative to Lua or Wren for latency-sensitive applications.

### Education & ARM assembly learning
A safe, self-contained environment for learning ARMv7 assembly. Students can write, compile, and step through real ARM instructions on any laptop without needing physical hardware or a full QEMU image.

### Embedded / IoT firmware testing
Compile embedded firmware to ARM32, load it into armvm, stub peripherals as host functions, and run automated tests on a CI server — no evaluation board required.

### Security research & fuzzing
Fuzz ARM binary code paths in a fully-isolated, in-process sandbox. The VM's configurable memory sizes and register-level introspection make it easy to inject inputs, catch crashes, and inspect state without spinning up a separate OS process.

### UI-managed VM environments
Pair armvm with **[orion-ui](https://github.com/corepunch/orion-ui)** for a graphical interface to create, run, inspect, and debug VMs — step-by-step execution, live register/memory views, and multi-VM management, suited for teaching environments and modding toolchains.

---

## Roadmap

### Near-term
- **Thumb / Thumb-2 instruction set** — support the 16/32-bit encoding emitted by modern GCC and Clang by default
- **VFP floating-point** — single/double-precision arithmetic for numerical guest code
- **Snapshot & restore** — serialize full VM state to pause, migrate, or replay execution
- **Step / breakpoint API** — `avm_step()` and `avm_breakpoint()` for debugger and orion-ui integration

### Medium-term
- **Multiple concurrent VM instances** — thread-safe state for parallel VMs in one host process
- **Inter-VM messaging** — lightweight channels so cooperating VMs exchange data without host involvement
- **Filesystem & network sandboxing helpers** — optional host-function packs for controlled file/socket access (similar to WASI)
- **orion-ui integration** — first-party adapter exposing armvm's API for graphical VM management and live disassembly

### Long-term
- **AArch64 (ARM64) guest support** — execute 64-bit ARM code in the same embeddable model
- **JIT compilation back-end** — translate hot bytecode paths to native instructions for performance-critical workloads
- **WebAssembly target** — compile the VM itself to WASM so ARM32 code runs in a browser
- **Profiling & tracing API** — instruction-level counters and call-graph tracing for guest code analysis

---

## Navigation

- [Getting Started](getting-started) — build, install, and run your first program
- [Assembly Reference](assembly-reference) — instructions, directives, and condition codes
- [API Reference](api-reference) — low-level C API (`vm_create`, `execute`, …)
- [Lua-like API](avm-api) — high-level embedding API (`avm_newstate`, `avm_register`, …)
- [Examples](examples) — complete worked examples including host function integration
- [Architecture](architecture) — VM internals, bytecode format, and memory model
