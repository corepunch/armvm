	.section	__TEXT,__text,regular,pure_instructions
	.ios_version_min 5, 0
	.syntax unified
	.globl	_main                           @ -- Begin function main
	.p2align	2
	.code	32                              @ @main
_main:
@ %bb.0:
	push	{r7, lr}
	mov	r7, sp
	movw	r0, :lower16:(L_.str-(LPC0_0+8))
	movt	r0, :upper16:(L_.str-(LPC0_0+8))
LPC0_0:
	add	r0, pc, r0
	bl	_print_string
	mov	r0, #21
	mov	r1, #21
	pop	{r7, lr}
	b	_add_numbers
                                        @ -- End function
	.section	__TEXT,__cstring,cstring_literals
L_.str:                                 @ @.str
	.asciz	"Hello from ARM VM!\n"

.subsections_via_symbols
