---
layout: page
title: Examples
permalink: /examples/
nav_order: 4
---

# Examples

## 1. Hello World — inline assembly

The simplest possible program: write an ARM assembly string inline in C,
compile it with `compile_buffer`, and run it in the VM.

```c
#include <stdio.h>
#include <stdlib.h>
#include "vm.h"

static DWORD no_syscall(LPVM vm, DWORD id) { return 0; }

int main(void) {
    const char *src =
        ".globl _main\n"
        "_main:\n"
        "    mov r0, #42\n"
        "    bx lr\n";

    FILE *fp = tmpfile();
    compile_buffer(fp, NULL, "hello.s", src);

    fseek(fp, 0, SEEK_END);
    DWORD psize = (DWORD)ftell(fp);
    BYTE *prog = malloc(psize);
    fseek(fp, 0, SEEK_SET);
    fread(prog, psize, 1, fp);
    fclose(fp);

    LPVM vm = vm_create(no_syscall, VM_STACK_SIZE, VM_HEAP_SIZE, prog, psize);
    free(prog);

    execute(vm, (DWORD)main_label);
    printf("Result: %d\n", (int)vm->r[0]);  /* Result: 42 */

    vm_shutdown(vm);
    return 0;
}
```

## 2. Custom syscall — `strlen`

The ARM code calls `strlen`, which is mapped to syscall ID 1.

**Assembly (`strlen_test.s`):**

```asm
.globl _main
_main:
    ldr r0, LPC_str       @ load offset of the string
LPC0:
    add r0, pc, r0        @ r0 = absolute address of string in VM memory
    bl  _strlen           @ call host strlen (syscall 1)
    bx  lr                @ return with length in r0

LPC_str:
    .long L_str-(LPC0+8)
L_str:
    .asciz "hello world"
```

**Host (`host.c`):**

```c
#include <string.h>
#include "vm.h"

static DWORD my_syscall(LPVM vm, DWORD call_id) {
    switch (call_id) {
        case 1:  /* strlen(r0) */
            return (DWORD)strlen((char *)vm->memory + vm->r[0]);
        default:
            return 0;
    }
}

int main(void) {
    memset(symbols, 0, sizeof(symbols));
    strcpy(symbols[1], "strlen");
    /* ... compile, load, execute as shown in example 1 ... */
}
```

## 3. simple-app — C source cross-compiled to ARM32 (macOS)

The `examples/simple-app/` directory demonstrates the full workflow:

1. Write a C source file that calls extern host functions.
2. Cross-compile it to ARMv7 assembly with `clang -arch armv7`.
3. Register the host functions in `symbols[]`.
4. Compile the assembly at runtime and execute it.

### File layout

```
examples/simple-app/
├── input.c   — guest C program (compiled to input.s by the Makefile)
├── main.c    — host application runner
├── Makefile  — cross-compiles input.c and builds the runner
└── README.md — quick reference
```

### `input.c` — the guest program

```c
extern int print_string(const char *s);
extern int add_numbers(int a, int b);

int main(void) {
    print_string("Hello from ARM VM!\n");
    int result = add_numbers(21, 21);
    return result;  /* 42 */
}
```

### `main.c` — the host runner (key parts)

```c
/* Map function names to syscall IDs */
memset(symbols, 0, sizeof(symbols));
strcpy(symbols[1], "print_string");
strcpy(symbols[2], "add_numbers");

/* Syscall dispatcher */
static DWORD syscall_handler(LPVM vm, DWORD call_id) {
    switch (call_id) {
        case 1: return host_print_string(vm);
        case 2: return host_add_numbers(vm);
        default:
            fprintf(stderr, "Unknown syscall %u — halting\n", call_id);
            vm->location = vm->progsize;   /* stop the VM */
            return 0;
    }
}

/* host_print_string — safe bounded print */
static DWORD host_print_string(LPVM vm) {
    DWORD offset = vm->r[0];
    DWORD mem_size = vm->progsize + vm->stacksize + vm->heapsize;
    if (offset >= mem_size) return 0;           /* out-of-bounds */
    const char *str = (const char *)vm->memory + offset;
    DWORD max_len = mem_size - offset;
    DWORD len = 0;
    while (len < max_len && str[len] != '\0') len++;
    if (len == max_len) return 0;               /* not NUL-terminated */
    fwrite(str, 1, len, stdout);
    return 0;
}

/* host_add_numbers */
static DWORD host_add_numbers(LPVM vm) {
    return (DWORD)((int)vm->r[0] + (int)vm->r[1]);
}
```

### How `bl _print_string` reaches the host

```
input.c            bl _print_string
                        ↓
assembler:         strips underscore → looks up "print_string" in symbols[]
                   found at index 1  → emits  OP_BEXT | 1
                        ↓
VM at runtime:     exec_branch_external(vm, instr)
                   vm->r[0] = syscall_handler(vm, 1)
                        ↓
syscall_handler:   call_id == 1 → host_print_string(vm)
```

### Building and running

```bash
make                          # build armvm-compiler
make -C examples/simple-app run
```

Expected output:

```
Running ARM32 program...
Hello from ARM VM!
Program returned: 42
```

## 4. Function calls with `pop {pc}`

This test (from `test/armtest.c`) demonstrates that loading into `pc` via
`pop {pc}` correctly returns from a called function.

```c
const char *code =
    "push { lr }\n"   /* save outer lr (= progsize) on the stack */
    "bl _calc\n"      /* call _calc; lr = address of pop {pc}    */
    "pop { pc }\n"    /* restore outer lr → VM terminates        */
    "_calc:\n"
    "mov r0, #99\n"   /* set return value                         */
    "bx lr\n";        /* return to pop {pc}                       */

DWORD result = test_program(code, 0);
assert(result == 99);
```

## 5. Conditional arithmetic

```asm
.globl _main
_main:
    mov  r0, #10
    cmp  r0, #9
    addgt r0, #10    @ r0 = 20 (10 > 9)
    bx   lr
```

```c
DWORD result = test_program(code, 0);
/* result == 20 */
```

## 6. Using the VM as a scripting engine

You can integrate armvm into any C application to support user-defined scripts:

```c
DWORD app_syscall(LPVM vm, DWORD id) {
    switch (id) {
        case 1: draw_sprite(vm->r[0], vm->r[1], vm->r[2]); return 0;
        case 2: return get_time_ms();
        case 3: return load_texture((char *)vm->memory + vm->r[0]);
        default: vm->location = vm->progsize; return 0;  /* halt on unknown */
    }
}

/* Register and run a user script */
memset(symbols, 0, sizeof(symbols));
strcpy(symbols[1], "draw_sprite");
strcpy(symbols[2], "get_time_ms");
strcpy(symbols[3], "load_texture");

LPVM vm = vm_create(app_syscall, VM_STACK_SIZE, VM_HEAP_SIZE, script_bytes, script_len);
execute(vm, 0);
vm_shutdown(vm);
```
