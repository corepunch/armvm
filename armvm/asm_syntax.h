#ifndef asm_syntax_h
#define asm_syntax_h

#include <stdio.h>

/*
 * AsmDirective - a named assembler directive and its handler function.
 *
 * Directive tables are NULL-terminated arrays of these structs, one per
 * syntax dialect.  Each entry maps a textual directive (e.g. ".byte") to
 * the function that processes it.
 */
typedef struct {
    const char *name;
    void (*handler)(FILE *fp, const char *args);
} AsmDirective;

/*
 * AsmSyntax - vtable that describes an assembly language syntax dialect.
 *
 * Currently the only built-in implementation is Apple/Clang ARM assembly
 * (apple_asm_syntax, defined in compiler.c).  Adding a new dialect requires:
 *   1. Writing a new is_comment_char() function for the dialect.
 *   2. Building a NULL-terminated AsmDirective[] table for its directives.
 *   3. Constructing a const AsmSyntax with those two items.
 *   4. Passing a pointer to that AsmSyntax to compile_buffer().
 *
 * Syntax comparison across common dialects:
 *
 *   Feature           | Apple/Clang      | GNU GAS (ARM)    | NASM
 *   ------------------+------------------+------------------+-----------
 *   Line comment      | ; or @           | @                | ;
 *   Immediate prefix  | #value           | #value           | value
 *   Directive prefix  | .                | .                | % or none
 *   Export symbol     | .globl           | .global          | global
 *   Alignment         | .p2align N       | .balign N        | ALIGN N
 *   Zero-fill section | .zerofill seg,.. | .comm sym,size   | N/A
 *   Build metadata    | .build_version   | (none)           | (none)
 */
typedef struct {
    /* Human-readable name, e.g. "apple", "gnu", "nasm" */
    const char *name;

    /*
     * Returns non-zero when ch is a character that starts a rest-of-line
     * comment in this dialect.
     *
     *   Apple/Clang : ';' and '@'
     *   GNU GAS ARM : '@'
     *   NASM        : ';'
     */
    int (*is_comment_char)(char ch);

    /*
     * NULL-terminated table of directives recognised by this dialect.
     * Directives not listed here are silently skipped with a warning.
     */
    const AsmDirective *directives;
} AsmSyntax;

#endif /* asm_syntax_h */
