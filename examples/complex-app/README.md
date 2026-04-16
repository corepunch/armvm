# complex-app example

A more complex example showing how to embed **armvm** in a host application
with multiple exported functions, loops, pointer arithmetic, struct-like
field access, bit shifts, and bit masks.

## What it demonstrates

### `input.c` / `complex.s`

Three internal ARM functions plus `main()`, all demonstrating different
language features:

| Function | Features |
|---|---|
| `compute_flags(value, shift, mask)` | register-controlled bit shift (`lsl r1`), bit-AND mask |
| `sum_array(arr*, count)` | counted loop (`cmp`/`bge`/`b`), pointer walk via scaled register offset (`[r0, r4, lsl #2]`), callee-saved registers |
| `process_record(record*)` | struct-like field access via immediate offsets (`[r0, #4]`, `[r0, #8]`), immediate bit shift (`lsl #8`), bitwise OR |
| `main` | stack-allocated arrays, calling all helpers, PC-relative string addressing |

### `main.c`

The host application that:
1. Registers `print_int` and `print_string` as host functions accessible to ARM code
2. Loads and compiles `complex.s` via `avm_loadbuffer()`
3. Executes the program with `avm_call()`
4. Reads and prints the return value

## Expected output

```
Running complex ARM32 program...
compute_flags result: 3840
sum_array result:     150
process_record result: 1834
Program returned: 150
```

### Result breakdown

| Call | Computation | Result |
|---|---|---|
| `compute_flags(0xFF, 4, 0xF00)` | `(255 << 4) & 3840 = 4080 & 3840` | **3840** |
| `sum_array([10,20,30,40,50], 5)` | `10+20+30+40+50` | **150** |
| `process_record([1, 42, 7])` | `42 \| (7 << 8) = 42 \| 1792` | **1834** |

## Build and run

```bash
# from the repository root — build armvm first (if not already done)
make

# build the host runner and run with the pre-generated assembly
make -C examples/complex-app run
```

On macOS with Xcode command-line tools you can also regenerate `complex.s`
from `input.c`:

```bash
# regenerate assembly and run
make -C examples/complex-app all run
```

## Assembly highlights

### Bit shift and mask (`_compute_flags`)

```asm
_compute_flags:
    mov r0, r0, lsl r1      @ r0 = value << shift  (register-controlled)
    and r0, r0, r2          @ r0 = (value << shift) & mask
    bx lr
```

### Loop with pointer walk (`_sum_array`)

```asm
_sum_loop:
    cmp r4, r1              @ if i >= count, exit
    bge _sum_done
    ldr r2, [r0, r4, lsl #2]   @ r2 = arr[i]  (base + i*4)
    add r5, r5, r2
    add r4, r4, #1          @ i++
    b _sum_loop
```

### Struct-like field access (`_process_record`)

```asm
_process_record:
    ldr r1, [r0, #4]        @ value = record[1]
    ldr r2, [r0, #8]        @ flags = record[2]
    mov r2, r2, lsl #8      @ flags << 8
    orr r0, r1, r2          @ value | (flags << 8)
    bx lr
```
