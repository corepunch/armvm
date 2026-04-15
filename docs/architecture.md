---
layout: page
title: Architecture
permalink: /architecture/
nav_order: 7
---

# Architecture

This page describes the internal design of armvm: how the assembler turns source text into bytecode, how the VM executes that bytecode, and how the memory model is organised.

## Components

```
┌──────────────────────────────────────────────────────┐
│                   Host application                    │
│                                                       │
│  avm_register()   avm_loadbuffer()   avm_call()       │
│       │                 │                │            │
│  ┌────▼─────────────────▼──────┐  ┌─────▼──────────┐ │
│  │   Assembler / Compiler      │  │  ARM32 VM      │ │
│  │   compiler.c  (front-end)   │  │  armvm.c       │ │
│  │   armcomp.c   (encoder)     │  │                │ │
│  │   expr.c      (expressions) │  │  cfuncs[]  ←───┤ │
│  └─────────────────────────────┘  └────────────────┘ │
└──────────────────────────────────────────────────────┘
```

| Source file | Role |
|---|---|
| `armvm/avm.h` | Public Lua-like API header: `avm_newstate`, `avm_register`, `avm_loadbuffer`, `avm_call`, `avm_to*`, `avm_push*` |
| `armvm/vm.h` | Low-level types and API: `struct VM`, `vm_create`, `execute`, `vm_shutdown` |
| `armvm/armvm.c` | VM execution engine + all `avm_*` function implementations |
| `armvm/compiler.c` | Assembler front-end: directive handling, label resolution, linker, `compile_buffer`, `avm_loadbuffer` |
| `armvm/armcomp.c` | ARM instruction encoder: translates mnemonics to 32-bit machine words |
| `armvm/expr.c` | Expression evaluator for constant folding and label arithmetic |
| `armvm/memory.c` | Doubly-linked free-list heap allocator inside the VM address space |
| `armvm/libpvm.c` | Standard library shims (`strlen`, `malloc`, `memset`, …) used by the compiler's built-in test harness |
| `armvm/asm_syntax.h` | `AsmSyntax` / `AsmDirective` types; `apple_asm_syntax` declaration |

---

## Assembler

`compile_buffer` performs a **two-pass** assembly in a single function call:

**Pass 1 — emit**

Line by line it calls `assembleLine` → `assemble` (in `armcomp.c`), which
encodes each instruction to a 4-byte word and writes it to the output file
(`fp`).  When a forward reference is needed (e.g., a `bl` to a label that
hasn't been seen yet), the encoder writes a placeholder (`0xffffffff`) and
registers the location in a symbol table (`cs.symbols[]`) for later patching.

**Pass 2 — link**

After all lines are processed, `linkprogram` iterates the unresolved symbol
table, looks up each label in the `labels[]` / `globals[]` arrays, and seeks
back into `fp` to overwrite the placeholders with the correct relative offsets.

### Label resolution

Labels fall into three categories:

| Category | Registered by | Used by |
|---|---|---|
| Local labels (`foo:`) | `add_label` | `bl foo`, `b foo`, `.long foo-bar` |
| Global labels (`.globl foo`) | `add_global` | Exported to the symbol header |
| External symbols (`symbols[N]`) | `avm_register` or direct write | `bl _externalName` → `OP_BEXT \| N` |

The `_main` label additionally sets the global `main_label` variable (and, via
`avm_loadbuffer`, `L->entry_point`) to the byte offset of the entry point.

### Forward-reference invariant

Each pending forward reference is stored as a `struct _SYMBOL` with a `filled`
flag.  **Both** `add_symbol` and `add_symbol2` initialise `filled = 0` when
creating a new entry.  This is important when `avm_loadbuffer` is called
multiple times: the compiler state is reset (`cs.num_symbols = 0`), which
reuses the same slots.  If `filled` were not reset, `linkprogram` would see
the stale `filled = 1` from the previous compilation and skip patching —
silently producing wrong PC-relative offsets.

### External-call encoding

When the assembler encounters `bl _name`:

1. It strips the leading underscore.
2. It searches `symbols[]` for a matching entry.
3. If found at index *N*, it emits `OP_BEXT | N` instead of a normal branch.

`OP_BEXT` (`0xff << 20`) is not a valid ARM instruction, so the VM can
distinguish it easily in `exec_instruction`.

---

## `avm_loadbuffer` — compile + load

`avm_loadbuffer` (defined in `compiler.c`, declared in `avm.h`) replaces the
manual `tmpfile` / `compile_buffer` / `fread` / `vm_create` boilerplate:

```
avm_loadbuffer(L, src, len)
    │
    ├─ Reset compiler state: cs.num_{symbols,globals,labels,sets,debug} = 0
    ├─ compile_buffer(tmpfile, NULL, NULL, src, &apple_asm_syntax)
    ├─ free(L->memory)
    ├─ L->memory = malloc(progsize + stacksize + heapsize)
    ├─ fread(L->memory, progsize, 1, fp)
    ├─ L->progsize    = progsize
    ├─ L->r[SP_REG]   = stacksize + progsize
    ├─ L->entry_point = main_label
    └─ initialize_memory_manager(L, memory + progsize + stacksize, heapsize)
```

It can be called multiple times on the same state — each call replaces
`L->memory` with a fresh allocation.  Host functions registered via
`avm_register` persist because they live in `L->cfuncs[]`, not in `L->memory`.

---

## `avm_register` and `_avm_dispatch`

`avm_register(L, "name", fn)` does two things:

1. Writes `"name"` into `symbols[++L->num_cfuncs]` so the assembler can map
   `bl _name` to `OP_BEXT | N`.
2. Stores `fn` in `L->cfuncs[N]`.

When `avm_newstate` creates the state it installs the internal
`_avm_dispatch` function as the `VM_SysCall`:

```c
static DWORD _avm_dispatch(LPVM vm, DWORD call_id) {
    if (call_id < AVM_MAX_CFUNCTIONS && vm->cfuncs[call_id])
        vm->cfuncs[call_id](vm);
    return vm->r[0];
}
```

At runtime, `exec_branch_external` calls `vm->syscall(vm, call_id)` →
`_avm_dispatch` → `L->cfuncs[call_id](L)`.  The `avm_CFunction` reads
arguments with `avm_to*`, writes a return value with `avm_push*`, and returns
the result count.  `_avm_dispatch` then returns `vm->r[0]`, which
`exec_branch_external` writes back to `r[0]` — a no-op if the function
already wrote it there.

---

## Bytecode format

The bytes written by `compile_buffer` are **raw 32-bit ARM instructions** with
no header.  The `armvm-compiler` CLI tool wraps them with a 12-byte ORCA
header:

```
Offset  Size  Field
     0     4  magic        = 0x4143524F  ("ORCA" little-endian)
     4     4  programsize  = number of bytecode bytes
     8     4  numberofsymbols
    12   var  bytecode
    ...  var  symbol table entries (4-byte offset + NUL-terminated name each)
```

When you use `avm_loadbuffer` or `compile_buffer` directly, there is no
header — the raw bytecode starts at offset 0.

---

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

The loop terminates when `vm->location` reaches or exceeds `vm->progsize`.  A
top-level `bx lr` achieves this because `lr` was initialised to `vm->progsize`.

### Instruction dispatch (`exec_instruction`)

Each call to `exec_instruction`:

1. Reads the 4-byte instruction at `vm->memory + vm->location`.
2. Advances `vm->location += 4`.
3. Sets `vm->r[PC_REG] = vm->location + 4` (ARM PC-ahead convention).
4. Evaluates the condition code (bits 31–28); returns early if not met.
5. Dispatches on the instruction type:

| Pattern | Handler |
|---|---|
| `MASK_BX == OP_BX` | `exec_branchandexchange` |
| `MASK_MUL == OP_MUL` | `exec_mul` |
| `MASK_UMUL == OP_UMUL` | `exec_umul` |
| `MASK_LDRSB == OP_LDRSB` | `exec_ldrsb` (signed byte/halfword) |
| `MASK_LDRSBI == OP_LDRSBI` | `exec_ldrsb` (immediate form) |
| `MASK_BEXT == OP_BEXT` | `exec_branch_external` (syscall / host call) |
| `MASK_TRAP == OP_TRAP` | skip (reserved) |
| bits 26–25 = `00`/`01` | `exec_dataprocessing` |
| bits 26–25 = `10`/`11` | `exec_datatransfer` |
| bits 26–25 = `100` | `exec_blockdatatransfer` |
| bits 26–25 = `101` | `exec_branchwithlink` |

### Data processing (`exec_dataprocessing`)

Decodes the opcode (bits 24–21), fetches Rn, computes Op2 (immediate or
shifted register), and writes the result into Rd.  When the `S` flag (bit 20)
is set, CPSR flags N, Z, C, V are updated via the `_dp1` dispatch table.

### Block data transfer (`exec_blockdatatransfer`)

Handles `push`, `pop`, `ldm`, `stm`.  The register list (bits 15–0) is
iterated in ascending order for Up=1 or descending for Up=0.  If the load bit
is set and `r15` is in the list, `vm->location` is updated from the loaded
value — this is the mechanism that makes `pop {pc}` work as a function return.

### Branch with link (`exec_branchwithlink`)

For `bl`, saves `vm->location` (the address of the following instruction) into
`lr` before adding the 24-bit signed offset to `vm->location`.

### External function call (`exec_branch_external`)

```c
void exec_branch_external(LPVM vm, DWORD instr) {
    DWORD proc = instr & 0xffff;
    vm->r[0] = vm->syscall(vm, proc);
}
```

The lower 16 bits of the instruction carry the syscall/function ID.  The
handler's return value is placed in `r0`, matching the ARM calling convention
for integer return values.

When the state was created with `avm_newstate`, `vm->syscall` is
`_avm_dispatch`, which looks up `vm->cfuncs[proc]` and calls it.

---

## Memory model

```
vm->memory (single malloc'd block)
    │
    ├── [0 .. progsize-1]                     bytecode (read-execute)
    ├── [progsize .. progsize+stacksize-1]     stack (grows down from top)
    └── [progsize+stacksize .. +heapsize-1]    heap (managed by memory.c)
```

The `struct VM` itself is a **separate** allocation (`calloc`) independent from
`vm->memory`.  This design (introduced alongside `avm_loadbuffer`) lets
`avm_loadbuffer` free and reallocate `vm->memory` for a new program without
touching the VM control structure or the registered `cfuncs[]`.

- **Stack pointer** starts at `progsize + stacksize` (one past the top of the
  stack region) and decrements on `push`.
- **Heap** is managed by a simple doubly-linked free-list allocator in
  `memory.c`.  ARM code can call `malloc` / `free` via the syscall interface.
- **Total addressable bytes**: `progsize + stacksize + heapsize`.

---

## CPSR flags

The Current Program Status Register is a single `DWORD`:

| Bit | Constant | Meaning |
|---|---|---|
| 31 | `CPSR_N` | Last result was Negative |
| 30 | `CPSR_Z` | Last result was Zero |
| 29 | `CPSR_C` | Carry / borrow |
| 28 | `CPSR_V` | Signed oVerflow |

Flags are updated by instructions with the `S` suffix (`adds`, `subs`, `cmp`,
`tst`, etc.) and are consumed by the conditional execution logic.

---

## Adding a new host function

1. Write an `avm_CFunction`:

   ```c
   static int host_abs(avm_State *L) {
       int n = avm_tointeger(L, 1);   /* r0 */
       avm_pushinteger(L, n < 0 ? -n : n);
       return 1;
   }
   ```

2. Register it **before** `avm_loadbuffer`:

   ```c
   avm_register(L, "abs", host_abs);
   ```

3. Call it from ARM assembly:

   ```asm
   bl _abs       @ r0 = abs(r0)
   ```

That is all.  No switch statement, no manual index tracking.

---

## Adding a new assembler directive

Directives are declared in `compiler.c` in the `apple_directives[]` table:

```c
static const AsmDirective apple_directives[] = {
    { ".zerofill",  f_zerofill },
    { ".byte",      f_byte     },
    /* ... */
    { NULL, NULL }             /* sentinel */
};
```

To add a new directive, implement a handler:

```c
void f_mydir(FILE *fp, LPCSTR str) {
    /* str is the rest of the line after ".mydir" with leading spaces stripped */
    /* write bytes to fp with fwrite / fputc */
}
```

Then add an entry to `apple_directives[]`:

```c
{ ".mydir", f_mydir },
```

---

## Adding a new ARM instruction

Instructions are encoded in `armcomp.c`.  The entry point is `assemble(line)`,
which parses the mnemonic and delegates to a specific `assemble_*` helper.

For a data-processing instruction, add a case to `assemble_dataprocessing` or
the appropriate section of `assemble()`.  For a completely new instruction
format, add a new `assemble_*` function and wire it up in `assemble()`.

The VM must also be able to execute the new instruction.  Add a new handler
in `armvm.c` and call it from `exec_instruction` under the appropriate mask
check.
