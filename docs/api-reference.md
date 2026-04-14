---
layout: page
title: API Reference
permalink: /api-reference/
nav_order: 3
---

# C API Reference

Include `vm.h` to access the entire public API.

```c
#include "vm.h"
```

## Types

### `LPVM` / `struct VM`

A pointer to the virtual machine state.

```c
typedef struct VM {
    DWORD  r[16];       /* registers r0–r15 (sp=r13, lr=r14, pc=r15) */
    BYTE  *memory;      /* base pointer for VM-addressable memory     */
    DWORD  location;    /* current instruction pointer (byte offset)  */
    DWORD  progsize;    /* size of the loaded bytecode in bytes        */
    DWORD  stacksize;   /* stack region size in bytes                  */
    DWORD  heapsize;    /* heap region size in bytes                   */
    DWORD  cpsr;        /* current program status register (flags)     */
    VM_SysCall syscall; /* registered syscall handler                  */
} *LPVM;
```

All fields are readable; write `vm->r[n]` to pass arguments before calling
`execute`, or read `vm->r[0]` to obtain the return value afterwards.

**Memory layout** (byte offsets from `vm->memory`):

```
0                  progsize           progsize+stacksize
|---- bytecode ----|---- stack --------|---- heap ---------|
```

### `VM_SysCall`

Prototype for the user-supplied syscall handler:

```c
typedef DWORD (*VM_SysCall)(struct VM *vm, DWORD call_id);
```

The handler is invoked whenever ARM code executes an `OP_BEXT` instruction
(emitted by the assembler for every `bl _externalName` that matches a registered
symbol).  `call_id` is the index in the `symbols[]` array where the name was
registered.

### Scalar types

| Type | C equivalent | Notes |
|---|---|---|
| `DWORD` | `unsigned int` | 32-bit unsigned |
| `BYTE` | `unsigned char` | 8-bit unsigned |
| `WORD` | `unsigned short` | 16-bit unsigned |
| `BOOL` | `unsigned int` | 0 = false, non-zero = true |
| `LPCSTR` | `const char *` | read-only string |

## Functions

### `vm_create`

```c
LPVM vm_create(VM_SysCall syscall,
               DWORD      stack_size,
               DWORD      heap_size,
               BYTE      *program,
               DWORD      progsize);
```

Allocates and initialises a new VM instance.

| Parameter | Description |
|---|---|
| `syscall` | Syscall handler; called for every `OP_BEXT` instruction |
| `stack_size` | Stack region size in bytes (e.g. `64 * 1024`) |
| `heap_size` | Heap region size in bytes (e.g. `64 * 1024`) |
| `program` | Pointer to compiled bytecode |
| `progsize` | Length of `program` in bytes |

The bytecode is **copied** into the VM's internal memory, so the caller may
free `program` immediately after the call returns.

The stack pointer (`r13`) is initialised to `stack_size + progsize`.  The heap
memory manager is initialised at offset `stack_size + progsize`.

**Returns** a non-NULL `LPVM` on success.  The caller is responsible for
calling `vm_shutdown` when done.

---

### `execute`

```c
void execute(LPVM vm, DWORD pc);
```

Runs the VM starting at byte offset `pc` until `vm->location >= vm->progsize`.

| Parameter | Description |
|---|---|
| `vm` | VM instance created by `vm_create` |
| `pc` | Entry point — byte offset of the first instruction to execute |

Before executing, `execute` clears r0–r12, sets `lr` to `vm->progsize` (so a
`bx lr` at the top level terminates execution), and sets `vm->location = pc`.

After `execute` returns, the ARM return value is in `vm->r[0]`.

---

### `vm_shutdown`

```c
void vm_shutdown(LPVM vm);
```

Frees all memory associated with `vm`.  Do not use the pointer afterwards.

---

### `compile_buffer`

```c
BOOL compile_buffer(FILE *fp, FILE *d_fp, LPCSTR filename, LPCSTR src);
```

Compiles an ARM assembly source string into bytecode and writes it to `fp`.

| Parameter | Description |
|---|---|
| `fp` | Output file opened for writing (may be a `tmpfile()`) |
| `d_fp` | Debug output file, or `NULL` to skip debug info |
| `filename` | Source filename used in diagnostic messages |
| `src` | NUL-terminated assembly source text |

**Returns** non-zero (true) on success, zero on error.

After a successful call, `main_label` is set to the byte offset of the `_main`
label if one was present.  Seek `fp` to 0 and read its contents to obtain the
bytecode.

**Side effects**: modifies the global compiler state (`cs`) and `main_label`.

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

Index 0 is unused by convention (syscall ID 0 typically means "no function").

---

### `main_label`

```c
extern int main_label;
```

Set by `compile_buffer` to the byte offset of the `_main` label in the
generated bytecode.  Use it as the `pc` argument to `execute`:

```c
execute(vm, (DWORD)main_label);
```

---

## Memory access macro

```c
#define VMA(reg)  ((void *)(vm->memory + vm->r[(reg)]))
```

A convenience macro for converting a VM register value (a memory offset) to a
host pointer.  Defined in the example code (not in `vm.h`); copy it into your
host application as needed.

**Important**: always validate the offset before dereferencing.  The total
addressable memory is `vm->progsize + vm->stacksize + vm->heapsize` bytes.

## CPSR flag constants

| Constant | Bit | Meaning |
|---|---|---|
| `CPSR_N` | 31 | Negative |
| `CPSR_Z` | 30 | Zero |
| `CPSR_C` | 29 | Carry |
| `CPSR_V` | 28 | Overflow |

## Default sizes

```c
#define VM_STACK_SIZE  (64 * 1024)   /* 64 KB */
#define VM_HEAP_SIZE   (64 * 1024)   /* 64 KB */
```

## Minimal embedding example

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

/* Syscall handler */
static DWORD my_syscall(LPVM vm, DWORD call_id) {
    if (call_id == 1) {
        /* puts(r0) */
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
    compile_buffer(fp, NULL, argv[1], src);
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
