/*
 * main.c - Example application demonstrating the armvm API.
 *
 * This program shows how to embed the ARM32 VM in a host application and
 * expose custom host functions to ARM code using the Lua-like avm_* API.
 *
 * Steps performed:
 *   1. Create a new VM state with avm_newstate().
 *   2. Register custom host functions with avm_register() — this must happen
 *      before loading code so the assembler can resolve the symbols.
 *   3. Compile and load an ARMv7 assembly source file with avm_loadbuffer().
 *   4. Execute the compiled program with avm_call().
 *   5. Read the return value with avm_tointeger().
 *   6. Clean up with avm_close().
 *
 * Usage:
 *   ./simple-app <assembly_file.s>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avm.h"

/* ---------------------------------------------------------------------- */
/* Custom host functions exposed to the ARM VM via avm_register()         */
/* ---------------------------------------------------------------------- */

/*
 * print_string(const char *s)
 *
 * ARM calling convention: s is passed as an offset into VM memory in r0.
 * avm_tostring(L, 1) converts that offset to a host pointer automatically.
 */
static int host_print_string(avm_State *L) {
    const char *str = avm_tostring(L, 1);
    /* Basic bounds check: avm_tostring returns L->memory + r0, which is
     * always within the VM's address space as long as r0 < total memory.
     * For a production implementation you would add an explicit length check. */
    fputs(str, stdout);
    return 0; /* void — no return value */
}

/*
 * add_numbers(int a, int b) -> int
 *
 * a is in r0 (idx 1), b is in r1 (idx 2).
 */
static int host_add_numbers(avm_State *L) {
    avm_pushinteger(L, avm_tointeger(L, 1) + avm_tointeger(L, 2));
    return 1; /* one integer result in r0 */
}

/* ---------------------------------------------------------------------- */
/* Utility                                                                 */
/* ---------------------------------------------------------------------- */

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

/* ---------------------------------------------------------------------- */
/* Main                                                                    */
/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <assembly_file.s>\n", argv[0]);
        return 1;
    }

    /* Validate that the assembly defines a _main entry point */
    char *src = read_file(argv[1]);
    if (!src)
        return 1;

    if (!strstr(src, "_main:")) {
        fprintf(stderr, "error: assembly source does not define a '_main:' label\n");
        free(src);
        return 1;
    }

    /* ------------------------------------------------------------------
     * 1. Create the VM state (64 KB stack, 64 KB heap).
     * ------------------------------------------------------------------ */
    avm_State *L = avm_newstate(VM_STACK_SIZE, VM_HEAP_SIZE);

    /* ------------------------------------------------------------------
     * 2. Register host functions before loading code.
     *
     *    ARM assembly calls these with "bl _print_string" / "bl _add_numbers".
     *    The leading underscore is stripped by the assembler; we register the
     *    bare name here.
     * ------------------------------------------------------------------ */
    avm_register(L, "print_string", host_print_string);
    avm_register(L, "add_numbers",  host_add_numbers);

    /* ------------------------------------------------------------------
     * 3. Compile and load the assembly source.
     * ------------------------------------------------------------------ */
    if (avm_loadbuffer(L, src, strlen(src)) != 0) {
        fprintf(stderr, "Compilation failed\n");
        free(src);
        avm_close(L);
        return 1;
    }
    free(src);

    /* ------------------------------------------------------------------
     * 4. Execute from the _main entry point.
     * ------------------------------------------------------------------ */
    printf("Running ARM32 program...\n");
    avm_call(L, L->entry_point);

    /* ------------------------------------------------------------------
     * 5. Read the integer return value from r0 (register index 1).
     * ------------------------------------------------------------------ */
    int result = avm_tointeger(L, 1);
    printf("Program returned: %d\n", result);

    /* ------------------------------------------------------------------
     * 6. Clean up.
     * ------------------------------------------------------------------ */
    avm_close(L);
    return 0;
}

