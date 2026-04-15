---
layout: page
title: API Reference
permalink: /api-reference/
nav_order: 3
---

# C API Reference

armvm exposes two levels of API:

- **High-level (recommended)**: `avm.h` — Lua-like interface. See [Lua-like API](avm-api) for the full reference.
- **Low-level**: `vm.h` — direct access to `vm_create`, `execute`, etc. Documented on this page.

```c
#include "vm.h"   /* low-level */
#include "avm.h"  /* high-level (includes vm.h) */
```

---

## Types

### `LPVM` / `avm_State` / `struct VM`

`avm_State` and `LPVM` are both aliases for `struct VM *`.

```c
typedef struct VM {
    DWORD  r[16];              /* registers r0–r15 (sp=r13, lr=r14, pc=r15)   */
    BYTE  *memory;             /* base pointer for VM-addressable memory        */
    DWORD  location;           /* current instruction pointer (byte offset)     */
    DWORD  progsize;           /* size of the loaded bytecode in bytes          */
    DWORD  stacksize;          /* stack region size in bytes                    */
    DWORD  heapsize;           /* heap region size in bytes                     */
    DWORD  cpsr;               /* current program status register (flags)       */
    VM_SysCall syscall;        /* registered syscall handler                    */
    avm_CFunction cfuncs[256]; /* per-function dispatch table (avm_register)    */
    DWORD  num_cfuncs;         /* number of registered C functions              */
    DWORD  entry_point;        /* offset of _main, set by avm_loadbuffer        */
} *LPVM;

typedef struct VM avm_State;
```

All fields are readable.  Write `vm->r[n]` to pass arguments before calling
`execute`, or read `vm->r[0]` to obtain the return value afterwards.

**Memory layout** (byte offsets from `vm->memory`):

```
0                  progsize           progsize+stacksize
|---- bytecode ----|---- stack --------|---- heap ---------|
```

---

### `VM_SysCall`

Prototype for the low-level syscall handler (used with `vm_create`):

```c
typedef DWORD (*VM_SysCall)(struct VM *vm, DWORD call_id);
```

The handler is invoked whenever ARM code executes an `OP_BEXT` instruction
(emitted by the assembler for every `bl _externalName` that matches a
registered symbol).  `call_id` is the index in the `symbols[]` array where
the name was registered.

When using `avm_newstate` instead of `vm_create`, the internal
`_avm_dispatch` function is installed automatically and routes calls through
`L->cfuncs[]`.  You do not need to write a `VM_SysCall` at all.

---

### `avm_CFunction`

```c
typedef int (*avm_CFunction)(avm_State *L);
```

Type for host functions registered with `avm_register()`.  See
[Lua-like API](avm-api) for details.

---

### Scalar types

| Type | C equivalent | Notes |
|---|---|---|
| `DWORD` | `unsigned int` | 32-bit unsigned |
| `BYTE` | `unsigned char` | 8-bit unsigned |
| `WORD` | `unsigned short` | 16-bit unsigned |
| `BOOL` | `unsigned int` | 0 = false, non-zero = true |
| `LPCSTR` | `const char *` | read-only string |

---

## Functions

### `vm_create`

```c
LPVM vm_create(VM_SysCall syscall,
               DWORD      stack_size,
               DWORD      heap_size,
               BYTE      *program,
               DWORD      progsize);
```

Allocates and initialises a new VM instance from a pre-compiled bytecode buffer.

| Parameter | Description |
|---|---|
| `syscall` | Syscall handler; called for every `OP_BEXT` instruction |
| `stack_size` | Stack region size in bytes (e.g. `VM_STACK_SIZE`) |
| `heap_size` | Heap region size in bytes (e.g. `VM_HEAP_SIZE`) |
| `program` | Pointer to compiled bytecode |
| `progsize` | Length of `program` in bytes |

The bytecode is **copied** into the VM's internal memory, so the caller may
free `program` immediately after the call returns.

The stack pointer (`r13`) is initialised to `stack_size + progsize`.  The
heap memory manager is initialised at offset `progsize + stack_size`.

**Returns** a non-NULL `LPVM` on success.  Call `vm_shutdown` when done.

> **Prefer `avm_newstate` + `avm_loadbuffer`** for new code — they handle
> compilation and memory allocation in one step without the boilerplate of a
> manual `VM_SysCall` switch statement.

---

### `execute`

```c
void execute(LPVM vm, DWORD pc);
```

Runs the VM starting at byte offset `pc` until `vm->location >= vm->progsize`.

| Parameter | Description |
|---|---|
| `vm` | VM instance created by `vm_create` or `avm_newstate` |
| `pc` | Entry point — byte offset of the first instruction to execute |

Before executing, `execute` sets `lr` to `vm->progsize` (so a `bx lr` at
the top level terminates execution) and sets `vm->location = pc`.

After `execute` returns, the ARM return value is in `vm->r[0]`.

---

### `vm_shutdown`

```c
void vm_shutdown(LPVM vm);
```

Frees all memory associated with `vm`.  Do not use the pointer afterwards.

For states created with `avm_newstate`, use `avm_close` instead.

---

### `compile_buffer`

```c
BOOL compile_buffer(FILE *fp, FILE *d_fp, LPCSTR filename, LPCSTR src,
                    const AsmSyntax *syntax);
```

Compiles an ARM assembly source string into bytecode and writes it to `fp`.

| Parameter | Description |
|---|---|
| `fp` | Output file opened for writing (may be a `tmpfile()`) |
| `d_fp` | Debug output file, or `NULL` to skip debug info |
| `filename` | Source filename used in diagnostic messages, or `NULL` |
| `src` | NUL-terminated assembly source text |
| `syntax` | Assembler syntax descriptor; use `&apple_asm_syntax` |

**Returns** non-zero (true) on success, zero on error.

After a successful call, `main_label` is set to the byte offset of the `_main`
label if one was present.  Seek `fp` to 0 and read its contents to obtain the
bytecode.

**Side effects**: modifies the global compiler state (`cs`) and `main_label`.

> **Note**: `avm_loadbuffer` wraps this function and additionally resets the
> compiler state, handles the `tmpfile` lifecycle, and copies bytecode into
> `L->memory`.  Prefer `avm_loadbuffer` unless you need direct control over
> the output file.

---

### `symbols` — external function table

```c
extern SYMBOL symbols[MAX_SYMBOLS];  /* SYMBOL is char[64] */
```

Before calling `compile_buffer`, populate this array to register external
function names.  When the assembler encounters `bl _name`, it strips the
leading underscore and searches `symbols[]` for `name`.  If found at index *N*
it emits `OP_BEXT | N`.  At runtime the VM calls `syscall_handler(vm, N)`.

```c
/* Register two host functions */
memset(symbols, 0, sizeof(symbols));
strcpy(symbols[1], "print_string");
strcpy(symbols[2], "add_numbers");
```

Index 0 is unused by convention (syscall ID 0 means "no function").

> **Note**: `avm_register` writes into `symbols[]` automatically and also
> stores the function pointer.  You only need to write `symbols[]` directly
> when using the low-level `compile_buffer` + `vm_create` pattern.

---

### `main_label`

```c
extern int main_label;
```

Set by `compile_buffer` to the byte offset of the `_main` label.  Use it as
the `pc` argument to `execute`:

```c
execute(vm, (DWORD)main_label);
```

When using `avm_loadbuffer`, the equivalent value is `L->entry_point`.

---

### `apple_asm_syntax`

```c
extern const AsmSyntax apple_asm_syntax;
```

Pre-built syntax descriptor for the Apple/Clang ARM assembler dialect.
Pass `&apple_asm_syntax` as the `syntax` argument to `compile_buffer`.

---

## Memory access macro

```c
#define VMA(reg)  ((void *)(vm->memory + vm->r[(reg)]))
```

A convenience macro for converting a VM register value (a memory offset) to a
host pointer.  Copy it into your host application as needed.

**Important**: always validate the offset before dereferencing.  The total
addressable memory is `vm->progsize + vm->stacksize + vm->heapsize` bytes.

When using the high-level API, `avm_tostring(L, idx)` and
`avm_topointer(L, idx)` do the same job.

---

## CPSR flag constants

| Constant | Bit | Meaning |
|---|---|---|
| `CPSR_N` | 31 | Negative |
| `CPSR_Z` | 30 | Zero |
| `CPSR_C` | 29 | Carry |
| `CPSR_V` | 28 | Overflow |

---

## Default sizes

```c
#define VM_STACK_SIZE  (64 * 1024)   /* 64 KB */
#define VM_HEAP_SIZE   (64 * 1024)   /* 64 KB */
```

---

## Low-level embedding example

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "asm_syntax.h"

/* Syscall handler */
static DWORD my_syscall(LPVM vm, DWORD call_id) {
    if (call_id == 1) {
        printf("%s\n", (char *)vm->memory + vm->r[0]);
        return 0;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    /* Register external functions */
    memset(symbols, 0, sizeof(symbols));
    strcpy(symbols[1], "puts");

    /* Read assembly source */
    FILE *f = fopen(argv[1], "r");
    fseek(f, 0, SEEK_END);  long sz = ftell(f);  fseek(f, 0, SEEK_SET);
    char *src = calloc(1, sz + 1);
    fread(src, sz, 1, f);  fclose(f);

    /* Compile */
    FILE *fp = tmpfile();
    compile_buffer(fp, NULL, argv[1], src, &apple_asm_syntax);
    free(src);

    /* Load bytecode */
    fseek(fp, 0, SEEK_END);
    DWORD psize = (DWORD)ftell(fp);
    BYTE *prog = malloc(psize);
    fseek(fp, 0, SEEK_SET);
    fread(prog, psize, 1, fp);
    fclose(fp);

    /* Run */
    LPVM vm = vm_create(my_syscall, VM_STACK_SIZE, VM_HEAP_SIZE, prog, psize);
    free(prog);
    execute(vm, (DWORD)main_label);
    printf("returned: %d\n", (int)vm->r[0]);
    vm_shutdown(vm);
    return 0;
}
```
