/*
 * input.c - Complex example C program compiled to ARMv7 assembly for use
 * with armvm.
 *
 * This file demonstrates several language features:
 *   - Multiple exported functions
 *   - Loops
 *   - Function calls
 *   - Pointer operations
 *   - Struct-like field access (int array acting as a struct)
 *   - Bit shifts
 *   - Bit masks
 *
 * To regenerate the assembly on macOS:
 *   clang -S -arch armv7 -target armv7-apple-darwin -O1 input.c -o complex.s
 *
 * Expected output when run via complex-app:
 *   compute_flags result: 3840
 *   sum_array result:     150
 *   process_record result: 1834
 *   Program returned: 150
 */

/* Host functions provided by the host application via the syscall interface */
extern int print_int(int n);
extern int print_string(const char *s);

/*
 * compute_flags — bit-shift and bit-mask demonstration.
 *
 * Shifts 'value' left by 'shift' bits, then masks with 'mask'.
 * Example: compute_flags(0xFF, 4, 0xF00) = (0xFF << 4) & 0xF00 = 0xF00 = 3840
 */
static int compute_flags(int value, int shift, int mask) {
    return (value << shift) & mask;
}

/*
 * sum_array — loop and pointer demonstration.
 *
 * Iterates over an integer array and returns the sum of all elements.
 */
static int sum_array(int *arr, int count) {
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += arr[i];
    }
    return total;
}

/*
 * process_record — struct-like pointer access demonstration.
 *
 * Treats a three-element int array as a record:
 *   record[0] = id    (not used in result)
 *   record[1] = value
 *   record[2] = flags
 *
 * Returns value | (flags << 8), combining value and flags via bit ops.
 * Example: process_record({1, 42, 7}) = 42 | (7 << 8) = 42 | 1792 = 1834
 */
static int process_record(int *record) {
    int value = record[1];  /* +4 bytes from base */
    int flags = record[2];  /* +8 bytes from base */
    return value | (flags << 8);
}

int main(void) {
    /* 1. Bit shifts and masks */
    int flags = compute_flags(0xFF, 4, 0xF00);   /* = 3840 */
    print_string("compute_flags result: ");
    print_int(flags);

    /* 2. Loop over a pointer/array */
    int arr[5] = {10, 20, 30, 40, 50};
    int total = sum_array(arr, 5);               /* = 150 */
    print_string("sum_array result:     ");
    print_int(total);

    /* 3. Struct-like pointer access + bit ops */
    int record[3] = {1, 42, 7};
    int processed = process_record(record);      /* = 1834 */
    print_string("process_record result: ");
    print_int(processed);

    return total;   /* 150 */
}
