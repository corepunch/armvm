#include <assert.h>
#include <memory.h>
#include "vm.h"

#define REG_SIZE sizeof(DWORD)
#define REG(vm, reg) vm->r[op.r##reg]
#define BIT_VALUE(value, number) ((value >> number) & 1)

typedef DWORD (*_ARMPROC)(LPVM vm, DWORD instr, DWORD Op1, DWORD Op2);

#define DECL_ARMPROC(NAME) \
DWORD f_##NAME(LPVM vm, DWORD instr, DWORD Op1, DWORD Op2)

#define CARRY(vm) (vm->cpsr & CPSR_C ? 1 : 0)

#define VM_ZERO(vm) vm->r[0]
#define REG_INSTR(vm, instr, OFFS) vm->r[(instr >> OFFS) & 0b1111]
#define REG_Rd(vm, instr) REG_INSTR(vm, instr, 12)
#define REG_Rn(vm, instr) REG_INSTR(vm, instr, 16)

DECL_ARMPROC(ADD) { return Op1 + Op2; }
DECL_ARMPROC(SUB) { return Op1 - Op2; }
DECL_ARMPROC(RSB) { return Op2 - Op1; }
DECL_ARMPROC(ADC) { return Op1 + Op2 + CARRY(vm); }
DECL_ARMPROC(SBC) { return Op1 - Op2 + CARRY(vm); }
DECL_ARMPROC(RSC) { return Op1 - Op2 + CARRY(vm); }
DECL_ARMPROC(MUL) { return Op1 * Op2; }
DECL_ARMPROC(AND) { return Op1 & Op2; }
DECL_ARMPROC(ORR) { return Op1 | Op2; }
DECL_ARMPROC(EOR) { return Op1 ^ Op2; }
DECL_ARMPROC(BIC) { return Op1 & ~Op2; }
DECL_ARMPROC(MOV) { return Op2; }
DECL_ARMPROC(MVN) { return ~Op2; }
DECL_ARMPROC(NOP) { return VM_ZERO(vm); }

DECL_ARMPROC(ADDS) {
    DWORD Rd = f_ADD(vm, instr, Op1, Op2);
    vm->cpsr &= ~(CPSR_N | CPSR_Z | CPSR_C | CPSR_V);
    vm->cpsr |= (Rd & MSB) ? CPSR_N : 0;
    vm->cpsr |= (Rd == 0) ? CPSR_Z : 0;
    vm->cpsr |= (Rd < Op1 || Rd < Op2) ? CPSR_C : 0;
    vm->cpsr |= ((Op1 ^ Op2) & MSB) && ((Rd ^ Op1) & MSB) ? CPSR_V : 0;
    return Rd;
}

DECL_ARMPROC(SUBS) {
    DWORD Rd = f_SUB(vm, instr, Op1, Op2);
    vm->cpsr &= ~(CPSR_N | CPSR_Z | CPSR_C | CPSR_V);
    vm->cpsr |= (Rd & MSB) ? CPSR_N : 0;
    vm->cpsr |= (Rd == 0) ? CPSR_Z : 0;
    vm->cpsr |= (Op1 >= Op2) ? CPSR_C : 0;
    vm->cpsr |= ((Op1 ^ Op2) & MSB) && ((Rd ^ Op1) & MSB) ? CPSR_V : 0;
    return Rd;
}

DECL_ARMPROC(RSBS) {
    DWORD Rd = f_RSB(vm, instr, Op1, Op2);
    vm->cpsr &= ~(CPSR_N | CPSR_Z | CPSR_C | CPSR_V);
    vm->cpsr |= (Rd & MSB) ? CPSR_N : 0;
    vm->cpsr |= (Rd == 0) ? CPSR_Z : 0;
    vm->cpsr |= (Op2 >= Op1) ? CPSR_C : 0;
    vm->cpsr |= ((Op2 ^ Op1) & MSB) && ((Rd ^ Op2) & MSB) ? CPSR_V : 0;
    return Rd;
}

DECL_ARMPROC(ANDS) {
    DWORD Rd = f_AND(vm, instr, Op1, Op2);
    vm->cpsr &= ~(CPSR_N | CPSR_Z | CPSR_C | CPSR_V);
    vm->cpsr |= (Rd & MSB) ? CPSR_N : 0;
    vm->cpsr |= (Rd == 0) ? CPSR_Z : 0;
    return Rd;
}

DECL_ARMPROC(EORS) {
    DWORD Rd = f_EOR(vm, instr, Op1, Op2);
    vm->cpsr &= ~(CPSR_N | CPSR_Z | CPSR_C | CPSR_V);
    vm->cpsr |= (Rd & MSB) ? CPSR_N : 0;
    vm->cpsr |= (Rd == 0) ? CPSR_Z : 0;
    return Rd;
}

DECL_ARMPROC(CMP) {
    f_SUBS(vm, instr, Op1, Op2);
    return VM_ZERO(vm);
}

DECL_ARMPROC(CMN) {
    f_ADDS(vm, instr, Op1, Op2);
    return VM_ZERO(vm);
}

DECL_ARMPROC(TST) {
    f_ANDS(vm, instr, Op1, Op2);
    return VM_ZERO(vm);
}

DECL_ARMPROC(TEQ) {
    f_EORS(vm, instr, Op1, Op2);
    return VM_ZERO(vm);
}

_ARMPROC _dp0[] = {
    f_AND,
    f_EOR,
    f_SUB,
    f_RSB,
    f_ADD,
    f_ADC,
    f_SBC,
    f_RSC,
    f_TST,
    f_TEQ,
    f_CMP,
    f_CMN,
    f_ORR,
    f_MOV,
    f_BIC,
    f_MVN,
    f_NOP,
};

_ARMPROC _dp1[] = {
    f_ANDS,
    f_EORS,
    f_SUBS,
    f_RSBS,
    f_ADDS,
    f_ADC,
    f_SBC,
    f_RSC,
    f_TST,
    f_TEQ,
    f_CMP,
    f_CMN,
    f_ORR,
    f_MOV,
    f_BIC,
    f_MVN,
    f_NOP,
};

BOOL _condition(LPVM vm, DWORD cond) {
    DWORD f = vm->cpsr;
    switch (cond) {
        case OPCOND_EQ: // Z set, equal
            return  (f&CPSR_Z);
        case OPCOND_NE: // Z clear, not equal
            return !(f&CPSR_Z);
        case OPCOND_CS: // C set, unsigned higher or same
            return  (f&CPSR_C);
        case OPCOND_CC: // C clear, unsigned lower
            return !(f&CPSR_C);
        case OPCOND_MI: // N set, negative
            return  (f&CPSR_N);
        case OPCOND_PL: // N clear, positive or zero
            return !(f&CPSR_N);
        case OPCOND_VS: // V set, overflow
            return  (f&CPSR_V);
        case OPCOND_VC: // V clear,no overflow
            return !(f&CPSR_V);
        case OPCOND_HI: // C set and Z clear, unsigned higher
            return  (f&CPSR_C) && !(f&CPSR_Z);
        case OPCOND_LS: // C clear or Z set, unsigned lower or same
            return !(f&CPSR_C) || (f&CPSR_Z);
        case OPCOND_GE: // N equals V, greater or equal
            return  !(f&CPSR_N) == !(f&CPSR_V);
        case OPCOND_LT: // N not equal to V, less than
            return  !(f&CPSR_N) != !(f&CPSR_V);
        case OPCOND_GT: // Z clear AND (N equals V), greater than
            return !(f&CPSR_Z) && (!(f&CPSR_N) == !(f&CPSR_V));
        case OPCOND_LE: // Z set OR (N not equal to V), less than or equal
            return  (f&CPSR_Z) || (!(f&CPSR_N) != !(f&CPSR_V));
        case OPCOND_AL: // (ignored), always
        default:
            return 1;
    }
}

DWORD _calcshift(LPVM vm, DWORD instr) {
    DWORD Rm = instr & 0b1111;
    DWORD ShiftType = (instr >> 5) & 0b11;
    DWORD ShiftAmount;
    if (BIT_VALUE(instr, 4)) {
        DWORD Rs = (instr >> 8) & 0b1111;
        ShiftAmount = vm->r[Rs];
    } else {
        ShiftAmount = (instr >> 7) & 0b11111;
    }
    switch (ShiftType) {
        case 0b00: return vm->r[Rm] << ShiftAmount;
        case 0b01: return vm->r[Rm] >> ShiftAmount;
        case 0b10: return ((int)vm->r[Rm]) >> ShiftAmount;
        case 0b11: return vm->r[Rm] >> ShiftAmount;
    }
    return 0;
}

DWORD _calcimmediate(LPVM vm, DWORD instr) {
    DWORD Imm = instr & 0xff;
    DWORD Rotate = ((instr >> 8) & 0xf) << 1;
    return Imm >> Rotate | (Imm << (32 - Rotate));
}

void exec_dataprocessing(LPVM vm, DWORD instr) {
    BOOL   Immediate = BIT_VALUE(instr, 25);
    BOOL   SetFlags = BIT_VALUE(instr, 20);
    OPCODE OpCode = (instr >> 21) & 0xf;
    DWORD  Op = Immediate ? _calcimmediate(vm, instr) : _calcshift(vm, instr);
    DWORD  Rn = REG_Rn(vm, instr);
    REG_Rd(vm, instr) = (SetFlags ? _dp1 : _dp0)[OpCode](vm, instr, Rn, Op);
}

static inline DWORD _loadptr(LPVM vm, int offset) {
//    printf("Load %08x from %08x\n", *(DWORD *)(vm->memory + offset), offset);
    return *((DWORD *)(vm->memory + offset));
}

static inline void _storeptr(LPVM vm, int offset, DWORD value) {
//    printf("Store %08x to %08x\n", value, offset);
    *((DWORD *)(vm->memory + offset)) = value;
}

static inline DWORD _offsetptr(DWORD Rn, DWORD Offset, BOOL Up) {
    return Up ? (Rn + Offset) : (Rn - Offset);
}

#define LDR_UP_BIT 23
#define LDR_PREOFFSET_BIT 24
#define LDR_BYTE_BIT 22
#define LDR_WRITEBACK_BIT 21
#define LDR_LOAD_BIT 20

void exec_datatransfer(LPVM vm, DWORD instr) {
    BOOL  Pre = BIT_VALUE(instr, LDR_PREOFFSET_BIT);
    BOOL  Immediate = !BIT_VALUE(instr, 25);
    BOOL  Byte = BIT_VALUE(instr, LDR_BYTE_BIT);
    DWORD Rn = REG_Rn(vm, instr);
    DWORD Offset = Immediate ? instr & 0xfff : _calcshift(vm, instr);
    DWORD Pointer = _offsetptr(Rn, Offset, BIT_VALUE(instr, LDR_UP_BIT));
    DWORD Mask = Byte ? 0xff : 0xffffffff;
    DWORD Existing = _loadptr(vm, Pre ? Pointer : Rn);
    
    if (BIT_VALUE(instr, LDR_LOAD_BIT)) {
        REG_Rd(vm, instr) = Existing & Mask;
    } else {
        DWORD Val = REG_Rd(vm, instr) & Mask;
        _storeptr(vm, Pre ? Pointer : Rn, Val | (Existing & ~Mask));
    }
    
    if (BIT_VALUE(instr, LDR_WRITEBACK_BIT) || !Pre) {
        REG_Rn(vm, instr) = Pointer;
    }
}

#define LDRSB_IMMEDIATE_BIT 22
#define LDRSB_HALFWORD_BIT 5
#define LDRSB_SIGNED_BIT 6

void exec_ldrsb(LPVM vm, DWORD instr, DWORD Rm) {
    BOOL  Pre = BIT_VALUE(instr, LDR_PREOFFSET_BIT);
    BOOL  Immediate = BIT_VALUE(instr, LDRSB_IMMEDIATE_BIT);
    BOOL  Halfword = BIT_VALUE(instr, LDRSB_HALFWORD_BIT);
    BOOL  Signed = BIT_VALUE(instr, LDRSB_SIGNED_BIT);
    DWORD Rn = REG_INSTR(vm, instr, 16);
    DWORD Offset = Immediate ? ((instr & 0xf00) >> 4) | (instr & 0xf) : Rm;
    DWORD Pointer = _offsetptr(Rn, Offset, BIT_VALUE(instr, LDR_UP_BIT));
    DWORD Mask = Halfword ? 0xffff : 0xff;
    DWORD Sign = Halfword ? (1 << 15) : (1 << 7);
    DWORD Existing = _loadptr(vm, Pre ? Pointer : Rn);
    
    if (BIT_VALUE(instr, LDR_LOAD_BIT)) {
        DWORD empty = (Existing & Sign) ? ~Mask : 0;
        REG_Rd(vm, instr) = (Signed ? empty : 0) | (Existing & Mask);
    } else {
        DWORD Val = REG_Rd(vm, instr) & Mask;
        _storeptr(vm, Pre ? Pointer : Rn, Val | (Existing & ~Mask));
    }
    
    if (BIT_VALUE(instr, LDR_WRITEBACK_BIT) || !Pre) {
        REG_Rn(vm, instr) = Pointer;
    }
}

void exec_blockdatatransfer(LPVM vm, DWORD instr) {
    BOOL  Load = BIT_VALUE(instr, 20);
    BOOL  WriteBack = BIT_VALUE(instr, 21);
//    BOOL  SetFlags = BIT_VALUE(instr, 22);
    BOOL  Up = BIT_VALUE(instr, 23);
    BOOL  Pre = BIT_VALUE(instr, 24);
    DWORD Rn = REG_Rn(vm, instr);
    for (DWORD i = 0; i <= 0xf; i++) {
        DWORD j = Up ? i : (0xf - i);
        if (BIT_VALUE(instr, j)) {
            DWORD Next = Up ? (Rn + REG_SIZE) : (Rn - REG_SIZE);
            if (Load) {
                vm->r[j] = _loadptr(vm, Pre ? Next : Rn);
            } else {
                _storeptr(vm, Pre ? Next : Rn, vm->r[j]);
            }
            Rn = Next;
        }
    }
    if (WriteBack) {
        REG_Rn(vm, instr) = Rn;
    }
}

#define MASK_24BIT 0x00ffffff

void exec_branchwithlink(LPVM vm, DWORD instr) {
    BOOL  Link = BIT_VALUE(instr, 24);
    BOOL  Negative = BIT_VALUE(instr, 23);
    DWORD Offset = instr & MASK_24BIT;
    if (Link) {
        vm->r[LR_REG] = vm->location;
    }
    if (Negative) {
        vm->location -= (~(Offset - 1)) & MASK_24BIT;
    } else {
        vm->location += Offset;
    }
}

void exec_branchandexchange(LPVM vm, DWORD instr) {
    DWORD Rn = instr & 0xf;
    vm->location = vm->r[Rn];
}

void exec_mul(LPVM vm, DWORD instr) {
    BOOL  Accumulate = BIT_VALUE(instr, 21);
    BOOL  SetFlags = BIT_VALUE(instr, 20);
    DWORD Rm = REG_INSTR(vm, instr, 0);
    DWORD Rs = REG_INSTR(vm, instr, 8);
    DWORD Rn = REG_INSTR(vm, instr, 12);
    if (Accumulate) {
        REG_INSTR(vm, instr, 16) = Rm * Rs + Rn;
    } else {
        REG_INSTR(vm, instr, 16) = Rm * Rs;
    }
    if (SetFlags) {
        DWORD Rd = REG_INSTR(vm, instr, 16);
        vm->cpsr &= ~(CPSR_N | CPSR_Z | CPSR_C | CPSR_V);
        vm->cpsr |= (Rd & MSB) ? CPSR_N : 0;
        vm->cpsr |= (Rd == 0) ? CPSR_Z : 0;
        vm->cpsr |= (Rd < Rm || Rd < Rs) ? CPSR_C : 0;
        vm->cpsr |= ((Rm ^ Rs) & MSB) && ((Rd ^ Rm) & MSB) ? CPSR_V : 0;
    }
}

void exec_umul(LPVM vm, DWORD instr) {
    typedef long long slong;
    typedef unsigned long long ulong;
    BOOL  Accumulate = BIT_VALUE(instr, 21);
    BOOL  SetFlags = BIT_VALUE(instr, 20);
    BOOL  Signed = BIT_VALUE(instr, 22);
    DWORD Rm = REG_INSTR(vm, instr, 0);
    DWORD Rs = REG_INSTR(vm, instr, 8);
    if (Signed) {
        slong result = 0;
        if (Accumulate) {
            DWORD RdLo = REG_INSTR(vm, instr, 12);
            DWORD RdHi = REG_INSTR(vm, instr, 16);
            result = (slong)Rm * (slong)Rs + ((slong)RdHi << 32) | RdLo;
        } else {
            result = (slong)Rm * (slong)Rs;
        }
        REG_INSTR(vm, instr, 16) = result >> 32;
        REG_INSTR(vm, instr, 12) = result & 0xffffffff;
    } else {
        ulong result = 0;
        if (Accumulate) {
            DWORD RdLo = REG_INSTR(vm, instr, 12);
            DWORD RdHi = REG_INSTR(vm, instr, 16);
            result = (ulong)Rm * (ulong)Rs + ((ulong)RdHi << 32) | RdLo;
        } else {
            result = (ulong)Rm * (ulong)Rs;
        }
        REG_INSTR(vm, instr, 16) = result >> 32;
        REG_INSTR(vm, instr, 12) = result & 0xffffffff;
    }
    assert(!SetFlags);
    if (SetFlags) {
        DWORD Rd = REG_INSTR(vm, instr, 16);
        vm->cpsr &= ~(CPSR_N | CPSR_Z | CPSR_C | CPSR_V);
        vm->cpsr |= (Rd & MSB) ? CPSR_N : 0;
        vm->cpsr |= (Rd == 0) ? CPSR_Z : 0;
        vm->cpsr |= (Rd < Rm || Rd < Rs) ? CPSR_C : 0;
        vm->cpsr |= ((Rm ^ Rs) & MSB) && ((Rd ^ Rm) & MSB) ? CPSR_V : 0;
    }
}

void exec_branch_external(LPVM vm, DWORD instr) {
    DWORD proc = instr & 0xffff;
    *vm->r = vm->syscall(vm, proc);
}

void exec_instruction(LPVM vm) {
    DWORD instr = *(DWORD *)(vm->memory + vm->location);
    vm->location += REG_SIZE;
    vm->r[PC_REG] = vm->location + REG_SIZE;
    if (!_condition(vm, instr >> 28))
        return;
    if ((instr & MASK_BX) == OP_BX) {
        exec_branchandexchange(vm, instr);
        return;
    }
    if ((instr & MASK_MUL) == OP_MUL) {
        exec_mul(vm, instr);
        return;
    }
    if ((instr & MASK_UMUL) == OP_UMUL) {
        exec_umul(vm, instr);
        return;
    }
    if ((instr & MASK_LDRSB) == OP_LDRSB) {
        exec_ldrsb(vm, instr, REG_INSTR(vm, instr, 0));
        return;
    }
    if ((instr & MASK_LDRSBI) == OP_LDRSBI) {
        DWORD value = (instr & 0xff) | ((instr >> 4) & 0xff00);
        exec_ldrsb(vm, instr, value);
        return;
    }
    if ((instr & MASK_BEXT) == OP_BEXT) {
        exec_branch_external(vm, instr);
        return;
    }
    if ((instr & MASK_TRAP) == OP_TRAP) {
        // skip
        return;
    }
    switch ((instr >> 25) & 0b111) {
        case 0b000:
        case 0b001:
            exec_dataprocessing(vm, instr);
            return;
        case 0b010:
        case 0b011:
            exec_datatransfer(vm, instr);
            return;
        case 0b100:
            exec_blockdatatransfer(vm, instr);
            return;
        case 0b101:
            exec_branchwithlink(vm, instr);
            return;
    }
    printf("Unknown instruction %08x\n", instr);
    return;
}

void execute(LPVM vm, DWORD pc) {
    memset(vm->r, 0xff, 4 * 13);
    vm->r[LR_REG] = vm->progsize;
    vm->location = pc;
    while (vm->location < vm->progsize) {
        DWORD line = vm->location / 4 + 1;
        exec_instruction(vm);
        assert(vm->location != 0xffffffff);
    }
}
