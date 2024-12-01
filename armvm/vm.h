#ifndef vm_h
#define vm_h

#include <stdio.h>

#ifndef __OBJC__
typedef unsigned int BOOL;
#endif
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef unsigned int DWORD;
typedef unsigned char BYTE;
typedef unsigned short WORD;

#define OF_IMM 0x0100
#define OF_PTR 0x0200
#define OF_LSL 0x0400
#define OF_LSR 0x0800
#define OF_UPD 0x1600
#define OF_ADD 0x3200
#define OF_NEG 0x6400

#define OF_IMP OF_PTR

#define SKIP_PC 8
#define NUM_REGISTERS 16
#define VM_STACK_SIZE 64 * 1024
#define VM_HEAP_SIZE 64 * 1024
#define MAX_LINE_LENGTH 1024 * 64
#define LABEL_SIZE 256


#define SP_REG 13
#define LR_REG 14
#define PC_REG 15

#define CPSR_N (1U << 31)  // Negative
#define CPSR_Z (1U << 30)  // Zero
#define CPSR_C (1U << 29)  // Carry
#define CPSR_V (1U << 28)  // Overflow
#define CPSR_I (1U << 7)   // Interrupt Disable
#define CPSR_F (1U << 6)   // Fast Interrupt Disable
#define CPSR_T (1U << 5)   // Thumb State
#define MSB (1U << 31)  // Most Significant Bit

typedef enum {
    OPSHFT_LSL = 0b00, // logical left
    OPSHFT_LSR = 0b01, // logical right
    OPSHFT_ASR = 0b10, // arithmetic right
    OPSHFT_ROR = 0b11, // rotate right
} OPSHIFT;

typedef enum {
    OPCOND_EQ = 0b0000, // Z set, equal
    OPCOND_NE = 0b0001, // Z clear, not equal
    OPCOND_CS = 0b0010, // C set, unsigned higher or same
    OPCOND_CC = 0b0011, // C clear, unsigned lower
    OPCOND_MI = 0b0100, // N set, negative
    OPCOND_PL = 0b0101, // N clear, positive or zero
    OPCOND_VS = 0b0110, // V set, overflow
    OPCOND_VC = 0b0111, // V clear,no overflow
    OPCOND_HI = 0b1000, // C set and Z clear, unsigned higher
    OPCOND_LS = 0b1001, // C clear or Z set, unsigned lower or same
    OPCOND_GE = 0b1010, // N equals V, greater or equal
    OPCOND_LT = 0b1011, // N not equal to V, less than
    OPCOND_GT = 0b1100, // Z clear AND (N equals V), greater than
    OPCOND_LE = 0b1101, // Z set OR (N not equal to V), less than or equal
    OPCOND_AL = 0b1110, // (ignored), always
} OPCOND;

// Data Processing
typedef enum {
    OP_AND = 0b0000, // operand1 AND operand2
    OP_EOR = 0b0001, // operand1 EOR operand2
    OP_SUB = 0b0010, // operand1 - operand
    OP_RSB = 0b0011, // operand2 - operand1
    OP_ADD = 0b0100, // operand1 + operand2
    OP_ADC = 0b0101, // operand1 + operand2 + carry
    OP_SBC = 0b0110, // operand1 - operand2 + carry - 1
    OP_RSC = 0b0111, // operand2 - operand1 + carry - 1
    OP_TST = 0b1000, // as AND, but result is not written
    OP_TEQ = 0b1001, // as EOR, but result is not written
    OP_CMP = 0b1010, // as SUB, but result is not written
    OP_CMN = 0b1011, // as ADD, but result is not written
    OP_ORR = 0b1100, // operand1 OR operand2
    OP_MOV = 0b1101, // operand2(operand1 is ignored)
    OP_BIC = 0b1110, // operand1 AND NOT operand2(Bit clear)
    OP_MVN = 0b1111, // NOT operand2 (operand1 is ignored)
} OPCODE;

enum {
    OP_MUL = 0b0000,
    OP_MLA = 0b0001,
};

enum {
    OP_B  = 0b1010,
    OP_BL = 0b1011,
};

enum {
    OP_STR,
    OP_LDR,
};

#define TYPE_DATATRANSFER 0b01
#define TYPE_BLOCKDATATRANSFER 0b10

#define OP_BX   0x012FFF10
#define MASK_BX 0x0ffffff0

#define OP_MUL  (0b1001 << 4)
#define MASK_MUL (0b1111 << 4 | 0b111111 << 22)

#define OP_UMUL  (0b1001 << 4 | 1 << 23)
#define MASK_UMUL (0b1111 << 4 | 0b11111 << 23)

#define OP_LDRSB (0b1001 << 4)
#define MASK_LDRSB (0b111 << 25 | 0b1001 << 4)

#define OP_LDRSBI (0b1001 << 4 | 0b1 << 22)
#define MASK_LDRSBI (0b111 << 25 | 0b1 << 22 | 0b1001 << 4)

#define OP_BEXT (0xff << 20)
#define MASK_BEXT (0xff << 20)

#define OP_TRAP (0xf << 24)
#define MASK_TRAP (0xf << 24)

struct VM;

typedef DWORD (*EXPORTPROC)(struct VM *);

typedef struct _LOCATION {
    DWORD Instruction;
    DWORD Position;
} * LPLOCATION;

typedef DWORD (*SETPOSPROC)(LPLOCATION, DWORD);

// Head of the linked list
struct Node;

struct VM;

typedef DWORD (*VM_SysCall)(struct VM *, DWORD);

typedef struct VM {
    DWORD r[NUM_REGISTERS];
    BYTE *memory;
    VM_SysCall syscall;
    DWORD location;
    DWORD stacksize;
    DWORD heapsize;
    DWORD progsize;
    DWORD cpsr;
    struct Node *head;
} *LPVM;

#define ID_ORCA 0x4143524F

struct _VMHDR {
    DWORD magic;
    DWORD programsize;
    DWORD numberofsymbols;
};

typedef char SYMBOL[64];

void execute(LPVM vm, DWORD pc);

// Function to initialize the memory manager
void initialize_memory_manager(LPVM vm, void* buffer, size_t buffer_size);

// Function to allocate memory from the buffer
void* my_malloc(LPVM vm, size_t size);

// Function to free previously allocated memory
void my_free(LPVM vm, void* ptr);

LPVM vm_create(VM_SysCall, DWORD stack_size, DWORD heap_size, BYTE *program, DWORD progsize);
void vm_shutdown(LPVM);

#define MAX_SYMBOLS 1024 * 64

extern SYMBOL symbols[MAX_SYMBOLS];

#endif /* vm_h */
