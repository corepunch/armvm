/*
 * main.c - Example application demonstrating the armvm API.
 *
 * This program shows how to embed the ARM32 VM in a host application and
 * expose custom host functions to ARM code via the syscall/interrupt interface.
 *
 * Steps performed:
 *   1. Register custom host functions (print_string, add_numbers) in the
 *      assembler's symbol table so that calls to them in ARM assembly are
 *      translated to the appropriate external-call instructions.
 *   2. Read and compile an ARMv7 assembly source file.
 *   3. Create a VM instance with a custom syscall handler.
 *   4. Execute the compiled bytecode.
 *
 * Usage:
 *   ./simple-app <assembly_file.s>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

/* Helper: convert a VM register value (VM-relative address) to a host pointer */
#define VMA(reg) ((void *)(vm->memory + vm->r[(reg)]))

/* ------------------------------------------------------------------ */
/* Custom host functions exposed to the ARM VM via the syscall interface */
/* ------------------------------------------------------------------ */

/* Syscall 1 — print_string(const char *s) */
static DWORD host_print_string(LPVM vm) {
    printf("%s", (const char *)VMA(0));
    return 0;
}

/* Syscall 2 — add_numbers(int a, int b) -> int */
static DWORD host_add_numbers(LPVM vm) {
    return (DWORD)((int)vm->r[0] + (int)vm->r[1]);
}

/* ------------------------------------------------------------------ */
/* Syscall dispatcher — called by the VM for every external function call */
/* ------------------------------------------------------------------ */

static DWORD syscall_handler(LPVM vm, DWORD call_id) {
    switch (call_id) {
        case 1: return host_print_string(vm);
        case 2: return host_add_numbers(vm);
        default:
            fprintf(stderr, "Unknown syscall: %u\n", call_id);
            return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Symbols from compiler.c accessed by this translation unit           */
/* ------------------------------------------------------------------ */

/*
 * Global symbol table used by the assembler to map external function names
 * to syscall IDs.  Defined in compiler.c, declared in vm.h.
 */
extern SYMBOL symbols[MAX_SYMBOLS];

/*
 * Set by the assembler whenever it encounters a "_main" label.
 * We use it as the execution entry point.
 */
extern int main_label;

/*
 * Compile an ARM assembly source string into bytecode written to fp.
 * Defined in compiler.c.
 */
BOOL compile_buffer(FILE *fp, FILE *d_fp, LPCSTR filename, LPCSTR src);

/* ------------------------------------------------------------------ */
/* Utility                                                              */
/* ------------------------------------------------------------------ */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open '%s'\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, (size_t)size, 1, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <assembly_file.s>\n", argv[0]);
        return 1;
    }

    /*
     * Register custom functions in the assembler's symbol table.
     *
     * When the assembler encounters "bl _print_string" it strips the
     * leading underscore and looks up "print_string" in symbols[].
     * If found at index N it emits an external-call instruction that
     * passes N to the syscall handler at runtime.
     */
    memset(symbols, 0, sizeof(symbols));
    strcpy(symbols[1], "print_string");
    strcpy(symbols[2], "add_numbers");

    /* Read the assembly source file */
    char *src = read_file(argv[1]);
    if (!src)
        return 1;

    /* Compile the assembly source into bytecode */
    FILE *fp = tmpfile();
    if (!fp) {
        fprintf(stderr, "tmpfile() failed\n");
        free(src);
        return 1;
    }

    if (!compile_buffer(fp, NULL, argv[1], src)) {
        fprintf(stderr, "Compilation failed\n");
        free(src);
        fclose(fp);
        return 1;
    }
    free(src);

    /* Load the compiled bytecode into a buffer */
    fseek(fp, 0, SEEK_END);
    DWORD psize = (DWORD)ftell(fp);
    BYTE *program = malloc(psize);
    if (!program) { fclose(fp); return 1; }
    fseek(fp, 0, SEEK_SET);
    fread(program, psize, 1, fp);
    fclose(fp);

    /* Create the VM with 64 KB stack and 64 KB heap */
    LPVM vm = vm_create(syscall_handler, VM_STACK_SIZE, VM_HEAP_SIZE,
                        program, psize);
    free(program);

    /* Execute from the _main entry point */
    printf("Running ARM32 program...\n");
    execute(vm, (DWORD)main_label);

    /* r0 holds the integer return value */
    int result = (int)vm->r[0];
    printf("Program returned: %d\n", result);

    vm_shutdown(vm);
    return 0;
}
