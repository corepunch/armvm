/*
 * input.c - Example C program compiled to ARMv7 assembly for use with armvm.
 *
 * This file is compiled to ARM32 assembly with:
 *   clang -S -arch armv7 -target armv7-apple-darwin -O1 input.c -o input.s
 *
 * The resulting assembly is then run by the simple-app example, which
 * provides print_string and add_numbers via the VM's syscall interface.
 */

/* Custom functions provided by the host application via the syscall interface */
extern int print_string(const char *s);
extern int add_numbers(int a, int b);

int main(void) {
    print_string("Hello from ARM VM!\n");
    int result = add_numbers(21, 21);
    return result; /* returns 42 */
}
