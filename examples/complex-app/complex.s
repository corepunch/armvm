@ complex.s — Hand-written ARMv7 assembly for the complex-app example.
@
@ Corresponds to input.c; generated manually because cross-compilation to
@ ARMv7 is only available on macOS.  The syntax follows Apple/ARM UAL as
@ consumed by armvm's assembler (armcomp.c / compiler.c).
@
@ Functions exported to the host via avm_register():
@   print_int(int n)          — prints an integer followed by a newline
@   print_string(const char*) — prints a NUL-terminated string
@
@ Internal functions (resolved by the assembler's linker pass):
@   _compute_flags(value, shift, mask) -> (value << shift) & mask
@   _sum_array(arr*, count)            -> sum of arr[0..count-1]
@   _process_record(record*)           -> record[1] | (record[2] << 8)
@
@ ARM calling convention (AAPCS):
@   Arguments  : r0-r3 (first four integer args)
@   Return value: r0
@   Callee-saved: r4-r11, sp, lr  (push/pop in prologue/epilogue)
@   Caller-saved: r0-r3, r12

    .section __TEXT,__text,regular,pure_instructions
    .globl _main
    .globl _compute_flags
    .globl _sum_array
    .globl _process_record

@ -----------------------------------------------------------------------
@ compute_flags(int value, int shift, int mask) -> int
@
@   r0 = value, r1 = shift, r2 = mask
@   Returns (value << shift) & mask.
@
@   Demonstrates: register-controlled bit shift (lsl r1), bit-AND mask.
@ -----------------------------------------------------------------------
_compute_flags:
    mov r0, r0, lsl r1      @ r0 = value << shift
    and r0, r0, r2          @ r0 = (value << shift) & mask
    bx lr

@ -----------------------------------------------------------------------
@ sum_array(int *arr, int count) -> int
@
@   r0 = pointer to int array (in VM memory)
@   r1 = number of elements
@   Returns sum of arr[0] + arr[1] + ... + arr[count-1].
@
@   Demonstrates: loop (cmp/bge/b), pointer arithmetic via scaled register
@   offset ([r0, r4, lsl #2] = arr + i*4), callee-saved registers.
@ -----------------------------------------------------------------------
_sum_array:
    push { r4, r5, lr }
    mov r4, #0              @ i = 0
    mov r5, #0              @ total = 0
_sum_loop:
    cmp r4, r1              @ if i >= count, exit
    bge _sum_done
    ldr r2, [r0, r4, lsl #2]   @ r2 = arr[i]  (arr + i*4)
    add r5, r5, r2          @ total += arr[i]
    add r4, r4, #1          @ i++
    b _sum_loop
_sum_done:
    mov r0, r5              @ return total
    pop { r4, r5, pc }

@ -----------------------------------------------------------------------
@ process_record(int *record) -> int
@
@   r0 = pointer to int[3]: { id, value, flags }
@   Returns record[1] | (record[2] << 8).
@
@   Demonstrates: struct-like field access via immediate offsets,
@   immediate bit shift (lsl #8), bitwise OR.
@ -----------------------------------------------------------------------
_process_record:
    ldr r1, [r0, #4]        @ r1 = record[1]  (value)
    ldr r2, [r0, #8]        @ r2 = record[2]  (flags)
    mov r2, r2, lsl #8      @ r2 = flags << 8
    orr r0, r1, r2          @ r0 = value | (flags << 8)
    bx lr

@ -----------------------------------------------------------------------
@ main() — entry point
@
@   Stack frame layout (after push + sub):
@     [sp +  0 .. +16]  arr[5]    (5 × int = 20 bytes)
@     [sp + 20 .. +28]  record[3] (3 × int = 12 bytes)
@     — 32 bytes total —
@
@   Registers used:
@     r4 = compute_flags result
@     r5 = sum_array result  (also the return value)
@     r6 = process_record result
@
@   Returns: sum_array result (150).
@ -----------------------------------------------------------------------
_main:
    push { r4, r5, r6, lr }
    sub sp, sp, #32         @ allocate 32 bytes for locals

    @ ---- 1. compute_flags(0xFF, 4, 0xF00) ----
    @ (0xFF << 4) & 0xF00 = 0xFF0 & 0xF00 = 0xF00 = 3840
    mov r0, #255            @ value = 0xFF
    mov r1, #4              @ shift = 4
    mov r2, #3840           @ mask  = 0xF00
    bl _compute_flags
    mov r4, r0              @ save flags result (3840)

    ldr r0, LCPI0_0
LPC0_0:
    add r0, pc, r0          @ r0 = address of string "compute_flags result: "
    bl _print_string

    mov r0, r4
    bl _print_int

    @ ---- 2. sum_array({10,20,30,40,50}, 5) ----
    @ Build arr[5] on the stack at [sp, #0..#16]
    mov r0, #10
    str r0, [sp, #0]
    mov r0, #20
    str r0, [sp, #4]
    mov r0, #30
    str r0, [sp, #8]
    mov r0, #40
    str r0, [sp, #12]
    mov r0, #50
    str r0, [sp, #16]

    mov r0, sp              @ r0 = &arr[0]
    mov r1, #5              @ count = 5
    bl _sum_array
    mov r5, r0              @ save total (150)

    ldr r0, LCPI0_1
LPC0_1:
    add r0, pc, r0          @ r0 = address of string "sum_array result:     "
    bl _print_string

    mov r0, r5
    bl _print_int

    @ ---- 3. process_record({1, 42, 7}) ----
    @ Build record[3] on the stack at [sp, #20..#28]
    @ record = { id=1, value=42, flags=7 }
    @ result  = 42 | (7 << 8) = 42 | 1792 = 1834
    mov r0, #1
    str r0, [sp, #20]
    mov r0, #42
    str r0, [sp, #24]
    mov r0, #7
    str r0, [sp, #28]

    add r0, sp, #20         @ r0 = &record[0]
    bl _process_record
    mov r6, r0              @ save processed (1834)

    ldr r0, LCPI0_2
LPC0_2:
    add r0, pc, r0          @ r0 = address of string "process_record result: "
    bl _print_string

    mov r0, r6
    bl _print_int

    @ ---- return total (150) ----
    mov r0, r5
    add sp, sp, #32
    pop { r4, r5, r6, pc }

@ -----------------------------------------------------------------------
@ PC-relative string address constants
@
@ Each LCPI0_x holds (label_address - (LPC0_x + 8)), so that at runtime:
@   ldr r0, LCPI0_x       -> r0 = offset
@   add r0, pc, r0        -> r0 = absolute address in VM memory
@
@ The PC value used in the add is the address of LPC0_x + 8 because the
@ ARM PC is always 2 instructions (8 bytes) ahead.
@ -----------------------------------------------------------------------
    .p2align 2
LCPI0_0:
    .long L_str0-(LPC0_0+8)
LCPI0_1:
    .long L_str1-(LPC0_1+8)
LCPI0_2:
    .long L_str2-(LPC0_2+8)

L_str0:
    .asciz "compute_flags result: "
L_str1:
    .asciz "sum_array result:     "
L_str2:
    .asciz "process_record result: "
