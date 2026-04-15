---
layout: page
title: Lua-like API (avm.h)
permalink: /avm-api/
nav_order: 6
---

# Lua-like API (`avm.h`)

`avm.h` provides a high-level embedding API modelled after the **Lua C API**.
If you have ever embedded Lua in a C application, the pattern will feel
immediately familiar — create a state, register host functions, load code,
call it, read results, close the state.

```c
#include "avm.h"
```

## Design philosophy

| Concept | Lua C API | armvm avm API |
|---|---|---|
| VM state handle | `lua_State *L` | `avm_State *L` |
| Host function type | `lua_CFunction` | `avm_CFunction` |
| Create state | `luaL_newstate()` | `avm_newstate()` |
| Destroy state | `lua_close()` | `avm_close()` |
| Register host function | `lua_register()` | `avm_register()` |
| Load source code | `luaL_loadbuffer()` | `avm_loadbuffer()` |
| Execute code | `lua_call()` | `avm_call()` |
| Read integer result | `lua_tointeger()` | `avm_tointeger()` |
| Push integer return | `lua_pushinteger()` | `avm_pushinteger()` |

In Lua, values pass through a virtual **stack**.  In armvm, arguments and
return values pass through **ARM registers** (r0–r15) following the standard
ARM calling convention.  The accessor functions use **1-based indices**
(index 1 = r0, index 2 = r1, …) to match Lua's 1-based stack convention.

---

## Types

### `avm_State`

```c
typedef struct VM avm_State;
```

The opaque VM state.  All `avm_*` functions take a pointer to this type as
their first argument.  Obtain one from `avm_newstate()` and release it with
`avm_close()`.

Notable public fields (read-only after `avm_loadbuffer`):

| Field | Type | Description |
|---|---|---|
| `r[0..15]` | `DWORD[16]` | ARM registers; r0=result after `avm_call` |
| `memory` | `BYTE *` | Base of VM-addressable memory |
| `progsize` | `DWORD` | Size of loaded bytecode in bytes |
| `stacksize` | `DWORD` | Stack region size |
| `heapsize` | `DWORD` | Heap region size |
| `entry_point` | `DWORD` | Byte offset of `_main`; set by `avm_loadbuffer` |

---

### `avm_CFunction`

```c
typedef int (*avm_CFunction)(avm_State *L);
```

Prototype for every host function registered with `avm_register()`.

- **Arguments**: read from registers using `avm_tointeger(L, 1)` (= r0),
  `avm_tointeger(L, 2)` (= r1), etc.
- **Return values**: write with `avm_pushinteger(L, result)` to set r0.
- **Return the count**: return 0 if void (no result), 1 after pushing one result. Only r0 is used for return values; returning a count higher than 1 has no additional effect.

```c
#include <string.h>
#include "avm.h"

/* Example: strlen wrapper */
static int host_strlen(avm_State *L) {
    const char *s = avm_tostring(L, 1);   /* r0 = VM-relative pointer */
    avm_pushinteger(L, (int)strlen(s));   /* result → r0              */
    return 1;                             /* one return value         */
}
```

---

## State management

### `avm_newstate`

```c
avm_State *avm_newstate(DWORD stack_size, DWORD heap_size);
```

Allocates and initialises a new VM state.  No bytecode is loaded yet — call
`avm_register()` for every host function you need, then call
`avm_loadbuffer()`.

| Parameter | Description |
|---|---|
| `stack_size` | Stack region size in bytes (`VM_STACK_SIZE` = 64 KB by default) |
| `heap_size` | Heap region size in bytes (`VM_HEAP_SIZE` = 64 KB by default) |

Returns a non-NULL `avm_State *` on success.

---

### `avm_close`

```c
void avm_close(avm_State *L);
```

Frees all memory associated with the state.  Do not use `L` afterwards.

---

## Registering host functions

### `avm_register`

```c
void avm_register(avm_State *L, const char *name, avm_CFunction fn);
```

Registers a C function so that ARM assembly can call it with
`bl _<name>`.

| Parameter | Description |
|---|---|
| `L` | VM state |
| `name` | Symbol name **without** a leading underscore |
| `fn` | C function to call at runtime |

**Must be called before `avm_loadbuffer()`** because the assembler resolves
`bl _name` references at compile time.  Calling `avm_register` after
`avm_loadbuffer` has no effect on already-compiled code.

```c
avm_register(L, "strlen",     host_strlen);
avm_register(L, "malloc",     host_malloc);
avm_register(L, "puts",       host_puts);
```

ARM assembly side:

```asm
bl _strlen     @ calls host_strlen(L); r0 = strlen(r0)
bl _malloc     @ calls host_malloc(L); r0 = address of allocated block
```

**Internals**: `avm_register` writes `name` into the global `symbols[]` array
at the next available index and stores `fn` in `L->cfuncs[index]`.  The
internal `_avm_dispatch` syscall handler looks up `L->cfuncs[call_id]` and
calls it.

> **Note**: `symbols[]` is a global table shared across all states in the same
> process.  Creating two `avm_State` instances and registering different
> functions will overwrite the same indices.  Only one state (or one shared
> global registry) should be used for compilation at a time.

---

## Loading and compiling code

### `avm_loadbuffer`

```c
int avm_loadbuffer(avm_State *L, const char *code, size_t len);
```

Compiles ARM assembly source text and loads the resulting bytecode into `L`.

| Parameter | Description |
|---|---|
| `L` | VM state |
| `code` | NUL-terminated ARM assembly source |
| `len` | Length of `code` in bytes (for API parity; `compile_buffer` reads to NUL) |

**Returns** 0 on success, −1 on compilation failure.

On success:
- `L->memory` points to a freshly-allocated block containing the bytecode,
  stack space, and heap.
- `L->progsize` is set to the number of bytecode bytes.
- `L->entry_point` is set to the byte offset of the `_main` label, or 0 if
  no `_main` was found.
- The stack pointer (`L->r[SP_REG]`) is reset to `progsize + stacksize`.

`avm_loadbuffer` can be called multiple times on the same state — each call
replaces the previous bytecode.  Host functions registered with
`avm_register` persist across calls.

**Compilation internals**:

1. Resets the global compiler state (`cs`): all label, symbol, and set tables
   are cleared.
2. Calls `compile_buffer`, which performs a two-pass assembly:
   - Pass 1 encodes instructions and records forward-reference placeholders.
   - Pass 2 (`linkprogram`) patches the placeholders with resolved offsets.
3. Reads the assembled bytes from the temporary file into a fresh memory
   region of size `progsize + stacksize + heapsize`.
4. Initialises the heap allocator at offset `progsize + stacksize`.

---

## Executing code

### `avm_call`

```c
void avm_call(avm_State *L, DWORD pc);
```

Executes the loaded bytecode starting at byte offset `pc`.

| Parameter | Description |
|---|---|
| `L` | VM state (must have had `avm_loadbuffer` called first) |
| `pc` | Entry point; use `L->entry_point` to start from `_main` |

`avm_call` is a thin wrapper around `execute()`.  Execution terminates when
`vm->location >= vm->progsize` — normally when the top-level function
executes `bx lr` (because `lr` was initialised to `vm->progsize`).

After `avm_call` returns, read the result with:

```c
int result = avm_tointeger(L, 1);   /* register r0 */
```

---

## Reading register values (`avm_to*`)

These functions read ARM register values from the state.  Index 1 = r0,
index 2 = r1, …, index 16 = r15.  In an `avm_CFunction`, arguments are in
the registers according to the ARM calling convention:

| Register | Argument / role |
|---|---|
| r0 (idx 1) | 1st argument; integer return value |
| r1 (idx 2) | 2nd argument |
| r2 (idx 3) | 3rd argument |
| r3 (idx 4) | 4th argument |

### `avm_tointeger`

```c
int avm_tointeger(avm_State *L, int idx);
```

Returns `r[idx-1]` reinterpreted as a signed 32-bit integer.

### `avm_touinteger`

```c
unsigned int avm_touinteger(avm_State *L, int idx);
```

Returns `r[idx-1]` as an unsigned 32-bit integer.

### `avm_tonumber`

```c
float avm_tonumber(avm_State *L, int idx);
```

Returns `r[idx-1]` reinterpreted as a 32-bit IEEE 754 float.

### `avm_tostring`

```c
const char *avm_tostring(avm_State *L, int idx);
```

Treats `r[idx-1]` as a byte offset into `L->memory` and returns a pointer to
that location.  Use this when the ARM code passes a VM-relative string pointer
in a register.

```c
static int host_puts(avm_State *L) {
    puts(avm_tostring(L, 1));   /* r0 = offset of string in VM memory */
    return 0;
}
```

> **Safety**: `avm_tostring` does not validate the offset.  Add a bounds
> check if the offset comes from untrusted ARM code.

### `avm_topointer`

```c
void *avm_topointer(avm_State *L, int idx);
```

Like `avm_tostring`, but returns `void *` — useful when passing buffers.

### `avm_toboolean`

```c
int avm_toboolean(avm_State *L, int idx);
```

Returns 1 if `r[idx-1]` is non-zero, 0 otherwise.

---

## Writing return values (`avm_push*`)

Inside an `avm_CFunction`, call these to set the return value that the ARM
caller will see in r0 after the `bl _name` returns.

### `avm_pushinteger`

```c
void avm_pushinteger(avm_State *L, int n);
```

Sets `r0 = (DWORD)n`.

### `avm_pushnumber`

```c
void avm_pushnumber(avm_State *L, float n);
```

Sets `r0` to the bit pattern of the IEEE 754 float `n`.

### `avm_pushboolean`

```c
void avm_pushboolean(avm_State *L, int b);
```

Sets `r0 = 1` if `b` is non-zero, `r0 = 0` otherwise.

---

## Complete example

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avm.h"

/* ---- Host functions ---- */

static int host_strlen(avm_State *L) {
    avm_pushinteger(L, (int)strlen(avm_tostring(L, 1)));
    return 1;
}

static int host_puts(avm_State *L) {
    puts(avm_tostring(L, 1));
    return 0;
}

/* ---- ARM assembly source ---- */

static const char *src =
    ".globl _main\n"
    "_main:\n"
    "    bl _puts\n"   /* r0 must already point at a string */
    "    mov r0, #42\n"
    "    bx lr\n";

int main(void) {
    /* 1. Create state */
    avm_State *L = avm_newstate(VM_STACK_SIZE, VM_HEAP_SIZE);

    /* 2. Register host functions BEFORE avm_loadbuffer */
    avm_register(L, "strlen", host_strlen);
    avm_register(L, "puts",   host_puts);

    /* 3. Compile and load */
    if (avm_loadbuffer(L, src, strlen(src)) != 0) {
        fprintf(stderr, "compilation failed\n");
        avm_close(L);
        return 1;
    }

    /* 4. Execute from _main */
    avm_call(L, L->entry_point);

    /* 5. Read result from r0 */
    printf("returned %d\n", avm_tointeger(L, 1));

    /* 6. Clean up */
    avm_close(L);
    return 0;
}
```

---

## Reloading code

You can call `avm_loadbuffer` multiple times to replace the loaded program
without recreating the state or re-registering host functions:

```c
avm_State *L = avm_newstate(VM_STACK_SIZE, VM_HEAP_SIZE);
avm_register(L, "puts", host_puts);

avm_loadbuffer(L, program_a, strlen(program_a));
avm_call(L, L->entry_point);

avm_loadbuffer(L, program_b, strlen(program_b));   /* replaces program_a */
avm_call(L, L->entry_point);

avm_close(L);
```

Each call to `avm_loadbuffer` resets the compiler state (labels, forward
references) and allocates a fresh memory region for the new bytecode.

---

## Relationship to the low-level API

`avm_*` is built on top of the lower-level `vm_create` / `execute` interface.
You can mix the two in the same binary — for instance, use `vm_create` for
the execution side while still using `avm_*` accessors for reading/writing
registers.  The state types are the same (`avm_State *` ≡ `LPVM`).

See [API Reference](api-reference) for the low-level interface documentation.

---

## Porting from the low-level API

| Old pattern | New equivalent |
|---|---|
| `memset(symbols, 0, …); strcpy(symbols[N], "name");` | `avm_register(L, "name", fn)` |
| Manually writing a `VM_SysCall` switch statement | Not needed — `avm_register` installs a per-function dispatcher |
| `FILE *fp = tmpfile(); compile_buffer(…); fread(prog,…); vm_create(…)` | `avm_loadbuffer(L, src, len)` |
| `execute(vm, (DWORD)main_label)` | `avm_call(L, L->entry_point)` |
| `vm->r[0]` | `avm_tointeger(L, 1)` |
| `(char *)vm->memory + vm->r[0]` | `avm_tostring(L, 1)` |
| `vm_shutdown(vm)` | `avm_close(L)` |
