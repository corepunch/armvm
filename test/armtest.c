/**
 * ARM VM Test Suite
 * Converted from XCTest to plain C test runner
 */

#include <stdio.h>
#include <string.h>
#include "vm.h"

// Test function declarations (from compiler.c)
DWORD test_program(LPCSTR code, DWORD r);
DWORD run_program(LPCSTR filename);

// Test statistics
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Test assertion macro
#define ASSERT_EQUAL(actual, expected, test_name) \
    do { \
        tests_run++; \
        DWORD _actual = (actual); \
        DWORD _expected = (expected); \
        if (_actual == _expected) { \
            tests_passed++; \
            printf("✓ %s\n", test_name); \
        } else { \
            tests_failed++; \
            printf("✗ %s: expected %u, got %u\n", test_name, _expected, _actual); \
        } \
    } while(0)

void testMOV() {
    const char *code = "mov r0, #10\n";
    ASSERT_EQUAL(test_program(code, 0), 10, "testMOV");
}

void testLSL() {
    const char *code =
    "mov r0, #10\n"
    "mov r1, #20\n"
    "mov r1, r1, lsl #1\n"
    "add r0, r0, r1\n";
    ASSERT_EQUAL(test_program(code, 0), 50, "testLSL");
}

void testLSR() {
    const char *code =
    "mov r0, #30\n"
    "mov r1, #50\n"
    "mov r2, #1\n"
    "add r0, r0, r1, lsr r2\n";
    ASSERT_EQUAL(test_program(code, 0), 55, "testLSR");
}

void testPUSH() {
    const char *code =
    "mov r0, #10\n"
    "mov r1, #30\n"
    "push { r0, r1 }\n"
    "mov r0, #0\n"
    "mov r1, #0\n"
    "pop { r0, r1 }\n"
    "sub r0, r1, r0\n";
    ASSERT_EQUAL(test_program(code, 0), 20, "testPUSH");
}

void testCMP() {
    const char *code =
    "mov r0, #10\n"
    "cmp r0, #9\n"
    "addgt r0, #10\n";
    ASSERT_EQUAL(test_program(code, 0), 20, "testCMP");
}

void testCMP2() {
    const char *code =
    "mov r0, #20\n"
    "mov r1, #20\n"
    "cmp r0, #20\n"
    "movls r0, #10\n";
    ASSERT_EQUAL(test_program(code, 0), 10, "testCMP2");
}

void testBL() {
    const char *code =
    "mov r0, #10\n"
    "subs r2, r0, #9\n"
    "bgt _overjump\n"
    "add r0, #10\n"
    "_overjump:\n"
    "add r0, #25\n";
    ASSERT_EQUAL(test_program(code, 0), 35, "testBL");
}

void testLDR() {
    const char *code =
    "mov r0, #123\n"
    "sub sp, sp, 100\n"
    "str r0, [sp, #-8]\n"
    "ldr r1, [sp, #-8]\n";
    ASSERT_EQUAL(test_program(code, 1), 123, "testLDR");
}

void testLDR2() {
    const char *code =
    "ldr r0, LCPI1_0\n"
    "LPC1_0:\n"
    "add r0, pc, r0\n"
    "b _strlen\n"
    "bx lr\n"
    "LCPI1_0:\n"
    ".long L_.str.1-(LPC1_0+8)\n"
    "L_.str.1:\n"
    ".asciz \"hello world\"\n";
    ASSERT_EQUAL(test_program(code, 0), 11, "testLDR2");
}

void testADD() {
    const char *code =
    "mov r0, #5\n"
    "mov r1, #3\n"
    "add r2, r0, r1\n";
    ASSERT_EQUAL(test_program(code, 2), 8, "testADD");
}

// Note: testLinkedList and other file-based tests are commented out
// because they require external test files that don't exist in the repo
/*
void testLinkedList() {
    ASSERT_EQUAL(run_program("linked-list.s"), 30, "testLinkedList");
}

void testLinkedList2() {
    ASSERT_EQUAL(run_program("linked-list2.s"), 90, "testLinkedList2");
}

void testShifts() {
    ASSERT_EQUAL(run_program("shifts.s"), 12, "testShifts");
}

void testLDRSB() {
    ASSERT_EQUAL(run_program("ldrsb.s"), 5, "testLDRSB");
}

void testShifts2() {
    ASSERT_EQUAL(run_program("shifts2.s"), 15, "testShifts2");
}

void testFormat() {
    ASSERT_EQUAL(run_program("format.s"), 11, "testFormat");
}

void testModulo() {
    ASSERT_EQUAL(run_program("modulo.s"), 2, "testModulo");
}

void testMemcpy() {
    ASSERT_EQUAL(run_program("memcpy.s"), 12345, "testMemcpy");
}

void testSnprintf() {
    ASSERT_EQUAL(run_program("snprintf.s"), 12345, "testSnprintf");
}
*/

int main() {
    printf("ARM VM Test Suite\n");
    printf("=================\n\n");

    // Run all tests
    testMOV();
    testLSL();
    testLSR();
    testPUSH();
    testCMP();
    testCMP2();
    testBL();
    testLDR();
    testLDR2();
    testADD();

    // Print summary
    printf("\n=================\n");
    printf("Test Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED", tests_failed);
    }
    printf("\n");

    return tests_failed > 0 ? 1 : 0;
}
