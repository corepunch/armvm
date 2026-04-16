/*
 * main.c - Host application for the complex-app example.
 *
 * Demonstrates embedding the ARM VM in a host application with multiple
 * host functions exposed to the ARM guest code.
 *
 * Host functions registered:
 *   print_int(int n)          — prints an integer followed by a newline
 *   print_string(const char*) — prints a NUL-terminated string
 *
 * Expected output:
 *   Running complex ARM32 program...
 *   compute_flags result: 3840
 *   sum_array result:     150
 *   process_record result: 1834
 *   Program returned: 150
 *
 * Usage:
 *   ./complex-app complex.s
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avm.h"

/* ---------------------------------------------------------------------- */
/* Host functions exposed to the ARM VM via avm_register()                */
/* ---------------------------------------------------------------------- */

/*
 * print_int(int n)
 *
 * Reads the integer from r0 (register index 1) and prints it with a
 * trailing newline.
 */
static int host_print_int(avm_State *S) {
    int n = avm_tointeger(S, 1);
    printf("%d\n", n);
    return 0; /* void — no return value */
}

/*
 * print_string(const char *s)
 *
 * ARM calling convention: s is passed as a VM-memory offset in r0.
 * We validate the offset and verify NUL-termination before printing.
 */
static int host_print_string(avm_State *S) {
    DWORD offset = avm_touinteger(S, 1);
    DWORD mem_size = S->progsize + S->stacksize + S->heapsize;
    if (offset >= mem_size) return 0;               /* offset out of range  */
    const char *base = (const char *)S->memory + offset;
    DWORD max_len = mem_size - offset;
    /* Verify the string is NUL-terminated within the addressable range */
    DWORD len = 0;
    while (len < max_len && base[len] != '\0') len++;
    if (len == max_len) return 0;                   /* not NUL-terminated   */
    if (fwrite(base, 1, len, stdout) != len) return 0;
    return 0; /* void — no return value */
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

    char *src = read_file(argv[1]);
    if (!src)
        return 1;

    if (!strstr(src, "_main:")) {
        fprintf(stderr, "error: assembly source does not define a '_main:' label\n");
        free(src);
        return 1;
    }

    /* 1. Create the VM state (64 KB stack, 64 KB heap). */
    avm_State *S = avm_newstate(VM_STACK_SIZE, VM_HEAP_SIZE);

    /* 2. Register host functions before loading code.
     *
     *    ARM assembly calls these with "bl _print_int" / "bl _print_string".
     *    The leading underscore is stripped; we register the bare name here.
     */
    avm_register(S, "print_int",    host_print_int);
    avm_register(S, "print_string", host_print_string);

    /* 3. Compile and load the assembly source. */
    if (avm_loadbuffer(S, src, strlen(src)) != 0) {
        fprintf(stderr, "Compilation failed\n");
        free(src);
        avm_close(S);
        return 1;
    }
    free(src);

    /* 4. Execute from the _main entry point. */
    printf("Running complex ARM32 program...\n");
    avm_call(S, S->entry_point);

    /* 5. Read the integer return value from r0 (register index 1). */
    int result = avm_tointeger(S, 1);
    printf("Program returned: %d\n", result);

    /* 6. Clean up. */
    avm_close(S);
    return 0;
}
