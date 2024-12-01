#include "vm.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

LPCSTR conditions[] = {
    "EQ",
    "NE",
    "CS",
    "CC",
    "MI",
    "PL",
    "VS",
    "VC",
    "HI",
    "LS",
    "GE",
    "LT",
    "GT",
    "LE",
    "AL",
    NULL
};

LPCSTR multiply[] = {
    "MUL",
    "MLA",
    NULL
};

LPCSTR long_multiply[] = {
    "UMULL",
    "UMLAL",
    "SMULL",
    "SMLAL",
    NULL
};

LPCSTR blockdatatransfer[] = {
    "STM",
    "LDM",
    NULL
};

LPCSTR dataprocessing[] = {
    "AND",
    "EOR",
    "SUB",
    "RSB",
    "ADD",
    "ADC",
    "SBC",
    "RSC",
    "TST",
    "TEQ",
    "CMP",
    "CMN",
    "ORR",
    "MOV",
    "BIC",
    "MVN",
    NULL
};

LPCSTR _shifts[] = {
    "LSL",
    "LSR",
    "ASR",
    "ROR",
    NULL
};

LPCSTR datatransfer[] = {
    "STR",
    "LDR",
    NULL
};

LPCSTR adrmodes[] = {
    "FA", "DA", // P 0, U 0
    "FD", "IA", // P 0, U 1
    "EA", "DB", // P 1, U 0
    "ED", "IB", // P 1, U 1
    NULL
};

enum {
    OPER_SHIFT = 1,
    OPER_IMMEDIATE = 2,
};

typedef struct _OPERAND {
    DWORD flags;
    DWORD immediate;
    BYTE regs[2];
    BOOL has_shift;
    BOOL negate;
    OPSHIFT shift;
} *LPOPERAND;

DWORD read_char(LPCSTR *line, int ch) {
    if (tolower(**line) == tolower(ch)) {
        (*line)++;
        return 1;
    } else {
        return 0;
    }
}

DWORD read_string(LPCSTR *line, LPCSTR str) {
    if (!strncasecmp(str, *line, strlen(str))) {
        *line += strlen(str);
        return 1;
    } else {
        return 0;
    }
}

BOOL skip_space(LPCSTR *line) {
    if (!isspace(**line)) return 0;
    while (isspace(**line)) (*line)++;
    return 1;
}

BOOL skip_comma(LPCSTR *line) {
    skip_space(line);
    if (read_char(line, ',')) {
        skip_space(line);
        return 1;
    } else {
        return 0;
    }
}

BOOL read_register(LPCSTR *line, BYTE *out) {
    skip_space(line);
    if (tolower(**line) == 'r' && isnumber(*((*line)+1))) {
        *out = strtol((*line)+1, (LPSTR *)line, 10);
        skip_comma(line);
        return 1;
    } else if (!strncasecmp(*line, "sp", 2)) {
        *out = SP_REG;
        *line += 2;
        skip_comma(line);
        return 1;
    } else if (!strncasecmp(*line, "lr", 2)) {
        *out = LR_REG;
        *line += 2;
        skip_comma(line);
        return 1;
    } else if (!strncasecmp(*line, "pc", 2)) {
        *out = PC_REG;
        *line += 2;
        skip_comma(line);
        return 1;
    } else {
        return 0;
    }
}

BOOL read_label(LPCSTR *line, BYTE *out) {
    skip_space(line);
    if (tolower(**line) == 'r' && isnumber(*((*line)+1))) {
        *out = strtol((*line)+1, (LPSTR *)line, 10);
        skip_comma(line);
        return 1;
    } else {
        return 0;
    }
}

BOOL read_number(LPCSTR *line, DWORD *out, BOOL *negative) {
    LPCSTR backup = *line;
    skip_space(line);
    if (!read_char(line, '#'))
        return 0;
    if (negative) {
        *negative = read_char(line, '-');
    }
    if (isnumber(**line)) {
        *out = (DWORD)strtol(*line, (LPSTR *)line, 10);
        skip_comma(line);
        return 1;
    } else {
        *line = backup;
        return 0;
    }
}

BOOL read_operand(LPCSTR *line, LPOPERAND op) {
    memset(op, 0, sizeof(struct _OPERAND));
    skip_space(line);
    if (**line == '-') {
        op->negate = 1;
    }
    if (read_number(line, &op->immediate, &op->negate)) {
        op->flags |= OPER_IMMEDIATE;
        return 1;
    }
    op->negate = read_char(line, '-');
    if (!read_register(line, &op->regs[0])) {
        return 0;
    }
    skip_space(line);
    skip_comma(line);
    for (LPCSTR *kw = _shifts; *kw; kw++) {
        if (!strncasecmp(*kw, *line, strlen(*kw))) {
            op->flags |= OPER_SHIFT;
            op->shift = (DWORD)(kw - _shifts);
            *line += strlen(*kw);
            BOOL dummy;
            if (read_number(line, &op->immediate, &dummy)) {
                op->flags |= OPER_IMMEDIATE;
                return 1;
            }
            if (!read_register(line, &op->regs[1])) {
                return 0;
            }
        }
    }
    return 1;
}

OPCOND read_condition(LPCSTR *line) {
    for (LPCSTR *kw = conditions; *kw; kw++) {
        if (!strncasecmp(*kw, *line, strlen(*kw))) {
            *line += strlen(*kw);
            return (DWORD)(kw - conditions);
        }
    }
    if (!strncasecmp("HS", *line, 2)) {
        *line += 2;
        return OPCOND_CS;
    }
    if (!strncasecmp("LO", *line, 2)) {
        *line += 2;
        return OPCOND_CC;
    }
    return OPCOND_AL;
}

OPCOND read_adrmode(LPCSTR *line) {
    for (LPCSTR *kw = adrmodes; *kw; kw++) {
        if (!strncasecmp(*kw, *line, strlen(*kw))) {
            *line += strlen(*kw);
            return (DWORD)(kw - adrmodes) >> 1;
        }
    }
    return 0b01;
}

struct _EXPORTSTRUCT *GetStdLib(void);
BOOL is_alnum2(int a);
void add_symbol2(LPLOCATION, LPCSTR symbol, SETPOSPROC bits);

extern DWORD curfilepos;

OPCOND read_address(LPCSTR *_b, DWORD instr, SETPOSPROC setpos) {
    skip_space(_b);
    char symbol[LABEL_SIZE] = { 0 };
    if (isalpha(**_b) || **_b == '_')  {
        for (char *s = symbol; is_alnum2(**_b); (*_b)++, s++) {
            *s = **_b;
        }
        add_symbol2(&(struct _LOCATION) {
            .Instruction = instr,
            .Position = curfilepos,
        }, symbol, setpos);
        return 1;
    } else {
        return 0;
    }
}

DWORD pack_immediate(LPOPERAND op) {
    DWORD value = op->negate ? -op->immediate : op->immediate;;
    DWORD shiftCount = 0;
    while (value > 0xff && shiftCount < 16) {
        value = value << 2 | (value >> 30);
        shiftCount++;
    }
    if (value > 0xFF) {
        printf("Error: Resulting value is out of range.\n");
    }
    return (shiftCount << 8) | (value & 0xFF);
}

DWORD write_12bit_operand(LPOPERAND Op, BOOL pack) {
    DWORD instr = 0;
    if (Op->flags & OPER_SHIFT) {
        instr |= Op->regs[0];
        instr |= Op->shift << 5;
        if (Op->flags & OPER_IMMEDIATE) {
            instr |= (Op->immediate & 0b11111) << 7;
        } else {
            instr |= 1 << 4;
            instr |= Op->regs[1] << 8;
        }
    } else if (Op->flags & OPER_IMMEDIATE) {
        instr |= 1 << 25;
        instr |= pack ? pack_immediate(Op) : Op->immediate;
    } else {
        instr |= Op->regs[0];
    }
    return instr;
}

DWORD write_sdt_operand(LPOPERAND Op, BYTE sh) {
    DWORD instr = (0b1001 << 4) | (sh << 5);
    if (Op->flags & OPER_IMMEDIATE) {
        instr |= 1 << 22;
        instr |= Op->immediate & 0x0f;
        instr |= (Op->immediate & 0xf0) << 4;
    } else {
        instr |= Op->regs[0];
    }
    return instr;
}

DWORD assemble_dataprocessing(LPCSTR line, DWORD opcode) {
    struct _OPERAND Rn = { 0 }, Op2 = { 0 };
    BYTE Rd;
    LPOPERAND Op = &Rn;
    DWORD instr = 0;

    instr |= opcode << 21;
    instr |= read_char(&line, 's') << 20;
    instr |= read_condition(&line) << 28;
    
    if (!skip_space(&line))
        return 0;

    if (!read_register(&line, &Rd))
        return 0;
    
    if (!read_operand(&line, &Rn))
        return 0;
    
    if (Rn.flags) {
        goto build_instruction;
    }
    
    if (read_operand(&line, &Op2)) {
        Op = &Op2;
    }
    
build_instruction:
    switch (opcode) {
        case OP_TST:
        case OP_TEQ:
        case OP_CMP:
        case OP_CMN:
            instr |= Rd << 16;
            break;
        default:
            instr |= Rd << 12;
            instr |= Rn.regs[0] << 16;
            break;
    }
    instr |= write_12bit_operand(Op, 1);
    return instr;
}

LPCSTR sdt_types[] = {
    "SH",
    "SB",
    "H",
    NULL
};

DWORD setpos_datatransfer(LPLOCATION loc, DWORD position) {
    DWORD instr = loc->Instruction;
    position -= SKIP_PC;
    if (position < loc->Position) {
        DWORD offset = loc->Position - position;
        assert(offset == (offset & 0xfff));
        instr |= offset & 0xfff;
        instr ^= 1 << 23;
    } else {
        DWORD offset = position - loc->Position;
        assert(offset == (offset & 0xfff));
        instr |= offset & 0xfff;
    }
    return instr;
}

enum {
    LDR_SH = 0b11,
    LDR_SB = 0b10,
    LDR_H  = 0b01,
};

DWORD assemble_datatransfer(LPCSTR line, BOOL load) {
    BYTE Rd, Rn;
    DWORD instr = 0;
    BYTE signeddt = 0;
    struct _OPERAND op = {0};
    instr |= TYPE_DATATRANSFER << 26;
    instr |= load << 20;
    instr |= 1 << 23; // down by default
    if (read_string(&line, "SH")) {
        signeddt = LDR_SH;
    } else if (read_string(&line, "SB")) {
        signeddt = LDR_SB;
    } else if (tolower(line[0]) == 'h' && tolower(line[1]) != 'i') {
        line++;
        signeddt = LDR_H;
    } else if (read_char(&line, 'B')) {
        instr |= 1 << 22;
    }
read_cond:
    instr |= read_condition(&line) << 28;
    if (!skip_space(&line))
        return 0;
    if (!read_register(&line, &Rd))
        return 0;
    instr |= Rd << 12;
    skip_space(&line);
    if (read_address(&line, instr | (PC_REG << 16) | (1 << 24), setpos_datatransfer)) {
        return instr | (PC_REG << 16) | (1 << 24);
    }
    if (!read_char(&line, '['))
        return 0;
    if (!read_register(&line, &Rn))
        return 0;
    instr |= Rn << 16;
    if (read_char(&line, ']')) {
        if (!skip_comma(&line)) {
            instr |= 1 << (signeddt ? 22 : 25); // mark as immediate value 0, not r0
            instr |= 1 << 24; // mark as preoffset
        } else if (!read_operand(&line, &op)) {
            return 0;
        }
        goto build_instruction;
    }
    if (!read_operand(&line, &op))
        return 0;
close_bracket:
    skip_space(&line);
    if (!read_char(&line, ']'))
        return 0;
    instr |= 1 << 24;
    instr |= read_char(&line, '!') << 21;
build_instruction:
    instr ^= op.negate << 23;
    if (signeddt) {
        instr &= ~(0b111 << 25);
        instr |= write_sdt_operand(&op, signeddt);
    } else {
        instr |= write_12bit_operand(&op, 0);
        instr ^= 1 << 25; // flip immediate flag
    }
    return instr;
}

DWORD assemble_blockdatatransfer(LPCSTR line, BOOL load) {
    BYTE Rn, Rlist;
    DWORD instr = 0;
    instr |= TYPE_BLOCKDATATRANSFER << 26;
    instr |= load << 20;
    instr |= read_condition(&line) << 28;
    instr |= read_adrmode(&line) << 23;
    if (!skip_space(&line))
        return 0;
    if (!read_register(&line, &Rn))
        return 0;
    instr |= read_char(&line, '!') << 21;
    instr |= Rn << 16;
    skip_comma(&line);
    if (!read_char(&line, '{'))
        return 0;
    while (read_register(&line, &Rlist)) {
        instr |= 1 << Rlist;
    }
    if (!read_char(&line, '}'))
        return 0;
    return instr;
}

DWORD assemble_multiply(LPCSTR line, DWORD opcode) {
    BYTE Rd, Rn=0, Rs, Rm;
    DWORD instr = 0;
    
    instr |= (opcode & 1) << 21; // accumulate
    instr |= read_condition(&line) << 28;
    instr |= read_char(&line, 's') << 20;
    
    if (!skip_space(&line))
        return 0;

    if (!read_register(&line, &Rd))
        return 0;

    if (!read_register(&line, &Rm))
        return 0;

    if (!read_register(&line, &Rs))
        return 0;
    
    read_register(&line, &Rn); // optional
    
build_instruction:
    instr |= Rd << 16;
    instr |= Rn << 12;
    instr |= Rs << 8;
    instr |= Rm;
    instr |= 0b1001 << 4;
    return instr;
}

DWORD assemble_long_multiply(LPCSTR line, DWORD opcode) {
    BYTE RdLo, RdHi, Rs, Rm;
    DWORD instr = 1 << 23;
    instr |= (opcode >= 2) << 22; // signed
    instr |= (opcode & 1) << 21; // accumulate
    instr |= read_condition(&line) << 28;
    instr |= read_char(&line, 's') << 20;
    
    if (!skip_space(&line))
        return 0;
    
    if (!read_register(&line, &RdLo))
        return 0;
    
    if (!read_register(&line, &RdHi))
        return 0;
    
    if (!read_register(&line, &Rm))
        return 0;
    
    if (!read_register(&line, &Rs))
        return 0;
    
build_instruction:
    instr |= RdHi << 16;
    instr |= RdLo << 12;
    instr |= Rs << 8;
    instr |= Rm;
    instr |= 0b1001 << 4;
    return instr;
}

DWORD assemble_bx(LPCSTR line) {
    DWORD instr = OP_BX;
    BYTE Rn;
    instr |= read_condition(&line) << 28;
    if (!skip_space(&line))
        return 0;
    if (!read_register(&line, &Rn))
        return 0;
    instr |= Rn;
    return instr;
}

DWORD assemble_trap(LPCSTR line) {
    return read_condition(&line) << 28 | 0xf << 24;
}

DWORD setpos_branch(LPLOCATION loc, DWORD position) {
    DWORD instr = loc->Instruction;
    position -= 4;
    if (position < loc->Position) {
        DWORD offset = ~(loc->Position - position) + 1;
        instr |= offset & 0xffffff;
    } else {
        DWORD offset = position - loc->Position;
        instr |= offset & 0xffffff;
    }
    return instr;
}

DWORD assemble_declare(LPCSTR line) {
    while (isspace(*line)) line++;
    SYMBOL sym = {0};
    DWORD num;
    if (sscanf(line, "%63[^,], %d", sym, &num) == 2) {
        if (num < MAX_SYMBOLS) {
            memcpy(symbols[num], sym, sizeof(SYMBOL));
            return 1;
        } else {
            printf("Index too high %s\n", line);
            return 0;
        }
    } else {
        printf("Can't parse %s\n", line);
        return 0;
    }
}

DWORD assemble_branch(LPCSTR line, BOOL withlink) {
    char symbol[LABEL_SIZE] = { 0 };
    DWORD instr = 0;
    
    instr |= (withlink ? OP_BL : OP_B) << 24;
    instr |= read_condition(&line) << 28;

    if (!skip_space(&line))
        return 0;

    if (isalpha(*line) || *line == '_')  {
        for (char *s = symbol; is_alnum2(*line); line++, s++) {
            *s = *line;
        }
    } else {
        return 0;
    }
    
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        LPCSTR src = symbols[i];
        if (!strcmp(src, symbol+1)) {
            return OP_BEXT | i | read_condition(&line) << 28 | (withlink << 19);
        }
    }
    
    add_symbol2(&(struct _LOCATION) {
        .Instruction = instr,
        .Position = curfilepos,
    }, symbol, setpos_branch);

    return instr;
}

// convert bitshifts into mov
DWORD assemble_shift(LPCSTR line, DWORD shift) {
    char conv[LABEL_SIZE] = { 0 };
    LPCSTR b = line;
    LPSTR a = conv;
    DWORD commas = 0;
    for (; commas < 2 && *b; *(a++) = *(b++)) {
        if (*b == ',') commas++;
        
    }
    strcpy(conv + strlen(conv), _shifts[shift]);
    strcpy(conv + strlen(conv), b);
    return assemble_dataprocessing(conv, OP_MOV);
}

DWORD setpos_adr(LPLOCATION loc, DWORD position) {
    DWORD instr = loc->Instruction;
    position -= SKIP_PC;
    if (position < loc->Position) {
        DWORD offset = loc->Position - position;
        instr |= offset & 0xff;
        instr &= ~(0xf << 21); // clear opcode
        instr |= OP_SUB << 21; // set opcode to SUBtract
    } else {
        DWORD offset = position - loc->Position;
        instr |= offset & 0xff;
    }
    return instr;
}

DWORD assemble(LPCSTR line) {
    char buffer[MAX_LINE_LENGTH] = { 0 };
    LPSTR a = buffer;
//    printf("%s\n", line);
    for (LPCSTR *kw = dataprocessing; *kw; kw++) {
        if (!strncasecmp(*kw, line, strlen(*kw))) {
            return assemble_dataprocessing(line + strlen(*kw), (DWORD)(kw - dataprocessing));
        }
    }
    for (LPCSTR *kw = multiply; *kw; kw++) {
        if (!strncasecmp(*kw, line, strlen(*kw))) {
            return assemble_multiply(line + strlen(*kw), (DWORD)(kw - multiply));
        }
    }
    for (LPCSTR *kw = long_multiply; *kw; kw++) {
        if (!strncasecmp(*kw, line, strlen(*kw))) {
            return assemble_long_multiply(line + strlen(*kw), (DWORD)(kw - long_multiply));
        }
    }
    for (LPCSTR *kw = _shifts; *kw; kw++) {
        if (!strncasecmp(*kw, line, strlen(*kw))) {
            return assemble_shift(line + strlen(*kw), (DWORD)(kw - _shifts));
        }
    }
    for (LPCSTR *kw = datatransfer; *kw; kw++) {
        if (!strncasecmp(*kw, line, strlen(*kw))) {
            return assemble_datatransfer(line + strlen(*kw), (DWORD)(kw - datatransfer));
        }
    }
    for (LPCSTR *kw = blockdatatransfer; *kw; kw++) {
        if (!strncasecmp(*kw, line, strlen(*kw))) {
            return assemble_blockdatatransfer(line + strlen(*kw), (DWORD)(kw - blockdatatransfer));
        }
    }
    if (!strncasecmp("RRX", line, 3)) {
        strcpy(buffer, line + 3);
        strcpy(buffer + strlen(buffer), ", #1");
        return assemble_shift(buffer, OPSHFT_ROR);
    }
    if (!strncasecmp("TRAP", line, 4)) {
        return assemble_trap(line + 4);
    }
    if (!strncasecmp("ADR", line, 3)) {
        LPCSTR s = line + 3;
        do { *a++ = *s; } while (*s++ != ',');
        skip_space(&s);
        strcpy(buffer + strlen(buffer), "PC, #0");
        DWORD instr = assemble_dataprocessing(buffer, OP_ADD);
        if (instr) {
            add_symbol2(&(struct _LOCATION) {
                .Instruction = instr,
                .Position = curfilepos,
            }, s, setpos_adr);
            return 1;
        } else {
            return 0;
        }
    }
    if (!strncasecmp("PUSH", line, 4)) {
        LPCSTR s = line + 4;
        while (isalpha(*s)) *a++ = *s++;
        strcpy(buffer + strlen(buffer), "DB SP!,");
        strcpy(buffer + strlen(buffer), s);
        return assemble_blockdatatransfer(buffer, 0);
    }
    if (!strncasecmp("POP", line, 3)) {
        LPCSTR s = line + 3;
        while (isalpha(*s)) *a++ = *s++;
        strcpy(buffer + strlen(buffer), "IA SP!,");
        strcpy(buffer + strlen(buffer), s);
        return assemble_blockdatatransfer(buffer, 1);
    }
    if (!strncasecmp("BX", line, 2)) {
        return assemble_bx(line + 2);
    }
    if (!strncasecmp("B", line, 1)) {
        DWORD instr = assemble_branch(line + 1, 0);
        if (instr) {
            return instr;
        }
        if (!strncasecmp("BL", line, 2)) {
            return assemble_branch(line + 2, 1);
        }
    }
    return 0;
}
