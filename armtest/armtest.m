#import <XCTest/XCTest.h>
#include "vm.h"

DWORD test_program(LPCSTR code, DWORD r);
DWORD run_program(LPCSTR filename);

@interface armtest : XCTestCase

@end

@implementation armtest

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)testMOV {
    LPCSTR code =
    "mov r0, #10\n";
    XCTAssertEqual(test_program(code, 0), 10);
}

- (void)testLSL {
    LPCSTR code =
    "mov r0, #10\n"
    "mov r1, #20\n"
    "mov r1, r1, lsl #1\n"
    "add r0, r0, r1\n";
    XCTAssertEqual(test_program(code, 0), 50);
}

- (void)testLSR {
    LPCSTR code =
    "mov r0, #30\n"
    "mov r1, #50\n"
    "mov r2, #1\n"
    "add r0, r0, r1, lsr r2\n";
    XCTAssertEqual(test_program(code, 0), 55);
}

- (void)testPUSH {
    LPCSTR code =
    "mov r0, #10\n"
    "mov r1, #30\n"
    "push { r0, r1 }\n"
    "mov r0, #0\n"
    "mov r1, #0\n"
    "pop { r0, r1 }\n"
    "sub r0, r1, r0\n";
    XCTAssertEqual(test_program(code, 0), 20);
}

- (void)testCMP {
    LPCSTR code =
    "mov r0, #10\n"
    "cmp r0, #9\n"
    "addgt r0, #10\n";
    XCTAssertEqual(test_program(code, 0), 20);
}

- (void)testCMP2 {
    LPCSTR code =
    "mov r0, #20\n"
    "mov r1, #20\n"
    "cmp r0, #20\n"
    "movls r0, #10\n";
    XCTAssertEqual(test_program(code, 0), 10);
}

- (void)testBL {
    LPCSTR code =
    "mov r0, #10\n"
    "subs r2, r0, #9\n"
    "bgt _overjump\n"
    "add r0, #10\n"
    "_overjump:\n"
    "add r0, #25\n";
    XCTAssertEqual(test_program(code, 0), 35);
}

- (void)testLDR {
    LPCSTR code =
    "mov r0, #123\n"
    "sub sp, sp, 100\n"
//    "ldr r1, [sp, #-4]\n"
//    "ldr r1, [sp, -r2, lsl #3]\n"
    "str r0, [sp, #-8]\n"
    "ldr r1, [sp, #-8]\n";
    XCTAssertEqual(test_program(code, 1), 123);
}

- (void)testLDR2 {
    LPCSTR code =
    "ldr r0, LCPI1_0\n"
    "LPC1_0:\n"
    "add r0, pc, r0\n"
    "b _strlen\n"
    "bx lr\n"
    "LCPI1_0:\n"
    ".long L_.str.1-(LPC1_0+8)\n"
    "L_.str.1:\n"
    ".asciz \"hello world\"\n";
    XCTAssertEqual(test_program(code, 0), 11);
}

- (void)testADD {
    LPCSTR code =
    "mov r0, #5\n"
    "mov r1, #3\n"
    "add r2, r0, r1\n";
    XCTAssertEqual(test_program(code, 2), 8); // Check if r2 contains the expected value (5 + 3 = 8)
}

//- (void)testSUBGT {
//    LPCSTR code =
//    "mov r0, #8\n"
//    "mov r1, #3\n"
//    "sub r2, r0, r1\n"
//    "cmp r2, #5\n"
//    "movgt r2, #1\n"
//    "movle r2, #0\n";
//    XCTAssertEqual(test_program(code, 2), 1); // Check if r2 contains 1 (because 8 - 3 > 5)
//}
//
//- (void)testMUL {
//    LPCSTR code =
//    "mov r0, #6\n"
//    "mov r1, #7\n"
//    "mul r2, r0, r1\n";
//    XCTAssertEqual(test_program(code, 2), 42); // Check if r2 contains the expected value (6 * 7 = 42)
//}

- (void)testLinkedList {
    XCTAssertEqual(run_program("linked-list.s"), 30);
}

- (void)testLinkedList2 {
    XCTAssertEqual(run_program("linked-list2.s"), 90);
}

- (void)testShifts {
    XCTAssertEqual(run_program("shifts.s"), 12);
}

- (void)testLDRSB {
    XCTAssertEqual(run_program("ldrsb.s"), 5);
}

- (void)testShifts2 {
    XCTAssertEqual(run_program("shifts2.s"), 15);
}

- (void)testFormat {
    XCTAssertEqual(run_program("format.s"), 11);
}

- (void)testModulo {
    XCTAssertEqual(run_program("modulo.s"), 2);
}

- (void)testMemcpy {
    XCTAssertEqual(run_program("memcpy.s"), 12345);
}

- (void)testSnprintf {
    XCTAssertEqual(run_program("snprintf.s"), 12345);
}

@end
