---
layout: page
title: Assembly Reference
permalink: /assembly-reference/
nav_order: 2
---

# Assembly Reference

armvm assembles **ARMv7 32-bit ARM** code written in **ARM UAL (Unified Assembly Language)** syntax with Apple Mach-O conventions.  This is the dialect produced by `clang -arch armv7 -target armv7-apple-darwin`.

## Registers

| Name | Number | Role |
|---|---|---|
| `r0`–`r12` | 0–12 | General-purpose |
| `sp` / `r13` | 13 | Stack pointer |
| `lr` / `r14` | 14 | Link register (return address) |
| `pc` / `r15` | 15 | Program counter |

- **r0–r3**: argument/result registers (caller-saved)
- **r4–r11**: callee-saved
- **r12**: intra-procedure scratch (`ip`)
- **sp** decrements on `push` and increments on `pop`
- **lr** receives the return address on `bl`; restored via `pop {pc}` or `bx lr`

## Condition codes

Every data-processing and branch instruction can carry an optional condition suffix.

| Suffix | Code | Flags | Meaning |
|---|---|---|---|
| `eq` | 0000 | Z=1 | Equal |
| `ne` | 0001 | Z=0 | Not equal |
| `cs` / `hs` | 0010 | C=1 | Carry set / unsigned higher or same |
| `cc` / `lo` | 0011 | C=0 | Carry clear / unsigned lower |
| `mi` | 0100 | N=1 | Minus / negative |
| `pl` | 0101 | N=0 | Plus / positive or zero |
| `vs` | 0110 | V=1 | Overflow |
| `vc` | 0111 | V=0 | No overflow |
| `hi` | 1000 | C=1, Z=0 | Unsigned higher |
| `ls` | 1001 | C=0 or Z=1 | Unsigned lower or same |
| `ge` | 1010 | N=V | Signed greater or equal |
| `lt` | 1011 | N≠V | Signed less than |
| `gt` | 1100 | Z=0, N=V | Signed greater than |
| `le` | 1101 | Z=1 or N≠V | Signed less than or equal |
| `al` | 1110 | — | Always (default) |

Example:

```asm
cmp  r0, #10
addgt r1, r1, #1   @ add only if r0 > 10
movle r0, #0       @ move only if r0 ≤ 10
```

## Data-processing instructions

Syntax: `<op>[cond][s]  Rd, Rn, <operand2>`

The optional `s` suffix sets the CPSR flags (N, Z, C, V) from the result.

| Instruction | Operation |
|---|---|
| `and` | Rd = Rn AND Op2 |
| `eor` | Rd = Rn EOR Op2 |
| `sub` | Rd = Rn − Op2 |
| `rsb` | Rd = Op2 − Rn |
| `add` | Rd = Rn + Op2 |
| `adc` | Rd = Rn + Op2 + C |
| `sbc` | Rd = Rn − Op2 + C − 1 |
| `rsc` | Rd = Op2 − Rn + C − 1 |
| `tst` | Sets flags for Rn AND Op2; no result written |
| `teq` | Sets flags for Rn EOR Op2; no result written |
| `cmp` | Sets flags for Rn − Op2; no result written |
| `cmn` | Sets flags for Rn + Op2; no result written |
| `orr` | Rd = Rn OR Op2 |
| `mov` | Rd = Op2 |
| `bic` | Rd = Rn AND NOT Op2 |
| `mvn` | Rd = NOT Op2 |

**Operand2** can be:
- An 8-bit immediate with optional even-bit rotation: `#value`
- A register: `Rm`
- A register with an immediate shift: `Rm, lsl #n` | `Rm, lsr #n` | `Rm, asr #n` | `Rm, ror #n`
- A register with a register-specified shift: `Rm, lsl Rs`

```asm
mov  r0, #10           @ r0 = 10
add  r1, r0, r2        @ r1 = r0 + r2
subs r0, r0, #1        @ r0 -= 1; set flags
mov  r3, r4, lsl #2    @ r3 = r4 << 2
add  r0, r0, r1, lsr r2
```

## Shift instructions (pseudo)

These are aliases for `mov Rd, Rm, <shift>`:

```asm
lsl  r0, r1, #3        @ r0 = r1 << 3
lsr  r0, r1, #1        @ r0 = r1 >> 1  (logical)
asr  r0, r1, #1        @ r0 = r1 >> 1  (arithmetic, sign-extended)
ror  r0, r1, #4        @ r0 = r1 rotated right by 4
```

## Multiply instructions

```asm
mul  Rd, Rm, Rs             @ Rd = Rm * Rs
mla  Rd, Rm, Rs, Rn         @ Rd = Rm * Rs + Rn
umull RdLo, RdHi, Rm, Rs    @ {RdHi,RdLo} = Rm * Rs  (unsigned 64-bit)
smull RdLo, RdHi, Rm, Rs    @ {RdHi,RdLo} = Rm * Rs  (signed 64-bit)
umlal RdLo, RdHi, Rm, Rs    @ {RdHi,RdLo} += Rm * Rs (unsigned accumulate)
smlal RdLo, RdHi, Rm, Rs    @ {RdHi,RdLo} += Rm * Rs (signed accumulate)
```

## Memory instructions

### Single register load/store

```asm
ldr  Rd, [Rn]             @ Rd = *(DWORD *)(Rn)
ldr  Rd, [Rn, #offset]    @ pre-indexed, no write-back
ldr  Rd, [Rn, #offset]!   @ pre-indexed with write-back
ldr  Rd, [Rn], #offset    @ post-indexed
str  Rd, [Rn, #offset]    @ store word
ldrb Rd, [Rn, #offset]    @ load byte (zero-extended)
strb Rd, [Rn, #offset]    @ store byte
ldrh Rd, [Rn, #offset]    @ load halfword (zero-extended)
strh Rd, [Rn, #offset]    @ store halfword
ldrsb Rd, [Rn, #offset]   @ load signed byte
ldrsh Rd, [Rn, #offset]   @ load signed halfword
```

### Block data transfer (push/pop)

```asm
push {r0, r1, lr}          @ STMDB sp!, {r0, r1, lr}
pop  {r0, r1, pc}          @ LDMIA sp!, {r0, r1, pc}
ldm  Rn, {reglist}         @ load multiple
stm  Rn, {reglist}         @ store multiple
```

The `{reglist}` may be a comma-separated list of registers and ranges: `{r0-r3, r7, lr}`.

`pop {pc}` is the standard way to return from a function that saved `lr` on the stack.

### PC-relative load

```asm
ldr  r0, LCPI0_0           @ load the 32-bit constant at label LCPI0_0
LCPI0_0:
.long 0xdeadbeef
```

## Branch instructions

```asm
b    label                 @ unconditional branch
bl   function              @ branch with link (saves return address in lr)
bx   lr                    @ branch and exchange (return via lr)
bgt  label                 @ branch if greater than
beq  loop_start            @ branch if equal
```

`bl` saves `pc` (the address of the _next_ instruction) into `lr` before branching.

## Directives

| Directive | Description |
|---|---|
| `.globl symbol` | Declare `symbol` as globally visible |
| `.asciz "text"` | Emit NUL-terminated string |
| `.ascii "text"` | Emit string without NUL terminator |
| `.byte value` | Emit 8-bit value |
| `.short value` | Emit 16-bit value |
| `.long value` | Emit 32-bit value |
| `.space n` | Emit `n` zero bytes |
| `.p2align n` | Align to 2ⁿ bytes |
| `.set name, expr` | Define assembler constant |
| `.section …` | Section marker (ignored by armvm) |
| `.build_version …` | Platform metadata (ignored) |
| `.subsections_via_symbols` | Apple linker hint (ignored) |
| `.indirect_symbol sym` | Reserve a slot for an external symbol |
| `.zerofill seg,sect,label,size,align` | Zero-initialised data |
| `.comm symbol,size,align` | Common symbol |

## Labels

Labels end with a colon and can start with a letter, digit, or underscore:

```asm
_main:           @ function entry (stripped underscore becomes "main")
loop_top:        @ local label
L_.str.1:        @ Clang-generated string label
LCPI0_0:         @ Clang-generated literal pool label
```

The special label `_main` sets the `main_label` variable used by the
`compile_buffer` caller as the execution entry point.

## Comment syntax

```asm
@ This is a comment (ARM UAL style)
; This is also a comment
```

## Escape sequences in strings

Inside `.ascii` / `.asciz`:

| Sequence | Character |
|---|---|
| `\n` | newline |
| `\t` | tab |
| `\r` | carriage return |
| `\\` | backslash |
| `\"` | double-quote |
| `\0` | NUL |
| `\nnn` | octal / decimal byte value |
