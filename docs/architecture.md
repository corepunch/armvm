---
layout: page
title: Architecture
permalink: /architecture/
nav_order: 5
---

# Architecture

This page describes the internal design of armvm: how the assembler turns source text into bytecode, how the VM executes that bytecode, and how the memory model is organised.

## Components

```
┌──────────────────────────────────────────────┐
│                Host application               │
│                                               │
│  compile_buffer()        execute()            │
│         │                    │                │
│  ┌──────▼──────┐    ┌────────▼────────┐       │
│  │  Assembler  │    │   ARM32 VM      │       │
│  │  compiler.c │    │   armvm.c       │       │
│  │  armcomp.c  │    │                 │       │
│  │  expr.c     │    │  syscall_handler│←──────┤
│  └─────────────┘    └─────────────────┘       │
└──────────────────────────────────────────────┘
```

| Source file | Role |
|---|---|
| `armvm/compiler.c` | Assembler front-end: directive handling, label resolution, linker, `compile_buffer`, `vm_create`, `test_program` |
| `armvm/armcomp.c` | ARM instruction encoder: translates mnemonics to 32-bit machine words |
| `armvm/expr.c` | Expression evaluator used by the assembler for constant folding and label arithmetic |
| `armvm/armvm.c` | VM execution engine: instruction decode and execute loop |
| `armvm/memory.c` | Simple bump-pointer heap allocator inside the VM address space |
| `armvm/libpvm.c` | Standard library shims (`strlen`, `malloc`, `memset`, etc.) exposed to the VM via syscalls |

## Assembler

`compile_buffer` performs a **two-pass** assembly in a single function call:

**Pass 1 — emit**

Line by line it calls `assembleLine` → `assemble` (in `armcomp.c`), which encodes each instruction to a 4-byte word and writes it to the output file (`fp`).  When a forward reference is needed (e.g., a `bl` to a label that hasn't been seen yet), the encoder writes a placeholder (`0xffffffff`) and registers the location in a symbol table for later patching.

**Pass 2 — link**

After all lines are processed, `linkprogram` iterates the unresolved symbol table, looks up each label in the `labels[]` / `globals[]` arrays, and seeks back into `fp` to overwrite the placeholders with the correct relative offsets.

### Label resolution

Labels fall into three categories:

| Category | Registered by | Used by |
|---|---|---|
| Local labels (`foo:`) | `add_label` | `bl foo`, `b foo`, `.long foo-bar` |
| Global labels (`.globl foo`) | `add_global` | Exported to the symbol header |
| External symbols (`symbols[N]`) | Caller before `compile_buffer` | `bl _externalName` → `OP_BEXT \| N` |

The `_main` label additionally sets the global `main_label` variable to the byte offset of the entry point.

### External-call encoding

When the assembler encounters `bl _name`:
1. It strips the leading underscore.
2. It searches `symbols[]` for a matching entry.
3. If found at index *N*, it emits `OP_BEXT | N` instead of a normal branch.

`OP_BEXT` is defined as `0xff << 20`, which is not a valid ARM instruction, so the VM can distinguish it easily.

## Bytecode format

The bytes written by `compile_buffer` are **raw 32-bit ARM instructions** with no header.  The `armvm-compiler` CLI tool wraps them with a 12-byte ORCA header:

```
Offset  Size  Field
     0     4  magic        = 0x4143524F  ("ORCA" little-endian)
     4     4  programsize  = number of bytecode bytes
     8     4  numberofsymbols
    12   var  bytecode
    ...  var  symbol table entries (4-byte offset + NUL-terminated name each)
```

When you use `compile_buffer` directly (as in `simple-app`), there is no header — the raw bytecode starts at offset 0.

## VM execution engine

### Execution loop (`execute`)

```c
void execute(LPVM vm, DWORD pc) {
    vm->r[LR_REG] = vm->progsize;   /* sentinel: bx lr terminates */
    vm->location = pc;
    while (vm->location < vm->progsize) {
        exec_instruction(vm);
    }
}
```

The loop terminates when `vm->location` reaches or exceeds `vm->progsize`.  A top-level `bx lr` achieves this because `lr` was initialised to `vm->progsize`.

### Instruction dispatch (`exec_instruction`)

Each call to `exec_instruction`:

1. Reads the 4-byte instruction at `vm->memory + vm->location`.
2. Advances `vm->location += 4`.
3. Sets `vm->r[PC_REG] = vm->location + 4` (ARM PC-ahead convention).
4. Evaluates the condition code (bits 31–28); returns early if the condition is not met.
5. Dispatches on the instruction type (bits 27–25 and special masks):

| Pattern | Handler |
|---|---|
| `MASK_BX == OP_BX` | `exec_branchandexchange` |
| `MASK_MUL == OP_MUL` | `exec_mul` |
| `MASK_UMUL == OP_UMUL` | `exec_umul` |
| `MASK_LDRSB == OP_LDRSB` | `exec_ldrsb` (signed byte/halfword) |
| `MASK_BEXT == OP_BEXT` | `exec_branch_external` (syscall) |
| `MASK_TRAP == OP_TRAP` | skip (reserved for future use) |
| bits 26–25 = `00` or `01` | `exec_dataprocessing` |
| bits 26–25 = `10` or `11` | `exec_datatransfer` |
| bits 26–25 = `100` | `exec_blockdatatransfer` |
| bits 26–25 = `101` | `exec_branchwithlink` |

### Data processing (`exec_dataprocessing`)

Decodes the opcode (bits 24–21), fetches Rn, computes Op2 (immediate or shifted register), and writes the result into Rd.  When the `S` flag (bit 20) is set, CPSR flags N, Z, C, V are updated.

### Block data transfer (`exec_blockdatatransfer`)

Handles `push`, `pop`, `ldm`, `stm`.  The register list (bits 15–0) is iterated in ascending order for Up=1 or descending for Up=0.  If the load bit is set and `r15` is in the list, `vm->location` is updated from the loaded value — this is the mechanism that makes `pop {pc}` work as a function return.

### Branch with link (`exec_branchwithlink`)

For `bl`, saves `vm->location` (the address of the following instruction) into `lr` before adding the 24-bit signed offset to `vm->location`.

### Syscall (`exec_branch_external`)

```c
void exec_branch_external(LPVM vm, DWORD instr) {
    DWORD proc = instr & 0xffff;
    vm->r[0] = vm->syscall(vm, proc);
}
```

The lower 16 bits of the instruction carry the syscall ID.  The handler's return value is placed in `r0`.

## Memory model

```
vm->memory
    │
    ├── [0 .. progsize-1]                   bytecode (read-execute)
    ├── [progsize .. progsize+stacksize-1]   stack (grows down from top)
    └── [progsize+stacksize .. +heapsize-1]  heap (managed by memory.c)
```

- **Stack pointer** starts at `progsize + stacksize` (one past the top of the stack region) and decrements on `push`.
- **Heap** is managed by a simple doubly-linked free-list allocator in `memory.c`.  ARM code can call `malloc` / `free` via the syscall interface.
- **Total addressable bytes**: `progsize + stacksize + heapsize`.

## CPSR flags

The Current Program Status Register is a single `DWORD`:

| Bit | Constant | Meaning |
|---|---|---|
| 31 | `CPSR_N` | Last result was Negative |
| 30 | `CPSR_Z` | Last result was Zero |
| 29 | `CPSR_C` | Carry / borrow |
| 28 | `CPSR_V` | Signed oVerflow |

Flags are updated by instructions with the `S` suffix (`adds`, `subs`, `cmp`, `tst`, etc.) and are consumed by the conditional execution logic.
