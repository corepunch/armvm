#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "vm.h"

#define MAX_LABELS (1024 * 32)
#define MAX_ASM_SYMBOLS (MAX_LABELS * 64)

struct _SYMBOL {
    char szName[LABEL_SIZE];
    struct _LOCATION loc;
    SETPOSPROC setpos;
    BOOL filled;
};

struct _LABEL {
    char szName[LABEL_SIZE];
    DWORD dwPosition;
};

struct _SET {
    char szName[LABEL_SIZE];
    char szCode[LABEL_SIZE];
};

int main_label = 0;

#define ARRAY(TYPE, NAME, SIZE) \
struct _##TYPE NAME[SIZE]; \
DWORD num_##NAME;

struct _COMPILER {
    ARRAY(LABEL, labels, MAX_LABELS);
    ARRAY(LABEL, globals, MAX_LABELS);
    ARRAY(SYMBOL, symbols, MAX_ASM_SYMBOLS);
    ARRAY(SET, sets, MAX_LABELS);
    ARRAY(LABEL, debug, MAX_ASM_SYMBOLS);
} cs = { 0 };

DWORD fnv1a32(LPCSTR str) {
    DWORD value = 0x811c9dc5;
    for (; *str; str++) {
        value ^= *str;
        value *= 0x1000193;
    }
    return value;
}

int curline = 0;
int curfileline = 0;

size_t curfilepos = 0;

SYMBOL symbols[MAX_SYMBOLS] = { 0 };

#define OP_SHIFT (('s' << 8) | '%')
#define OP_ARGS (('p' << 8) | '%')
#define OP_REGISTER (('r' << 8) | '%')
#define OP_ADDRESS (('a' << 8) | '%')
#define OP_NUMBER (('d' << 8) | '%')
#define OP_STACKPTR (('p' << 8) | 's')
#define OP_LINKRESULT (('r' << 8) | 'l')
#define OP_PROGRAMCTR (('c' << 8) | 'p')

BOOL ExtractLine(LPCSTR *text, LPSTR lineBuffer) {
    DWORD i, j;
    BOOL write = 1;
    BOOL quotes = 0;
    memset(lineBuffer, 0, MAX_LINE_LENGTH);
    if (**text == 0)
        return 0;
    for (i = 0, j = 0; i < MAX_LINE_LENGTH; i++) {
        if ((*text)[i] == ';' && !quotes) write = 0;
        if ((*text)[i] == '@' && !quotes) write = 0;
        if ((*text)[i] == 0 || (*text)[i] == '\n')
            break;
        if (isspace((*text)[i]) && j == 0) continue;
        if ((*text)[i] == '"') quotes = !quotes;
        if (write) {
            if ((*text)[i] == '\t') {
                lineBuffer[j++] = ' ';
            } else {
                lineBuffer[j++] = (*text)[i];
            }
        }
    }
    *text += i;
    if (**text == '\n') {
        (*text)++;
    }
    LPSTR s = lineBuffer + strlen(lineBuffer) - 1;
    while (s >= lineBuffer) {
        if (isspace(*s)) {
            *s = 0;
            s--;
        } else {
            break;
        }
    }
    return 1;
}

void add_symbol2(LPLOCATION loc, LPCSTR symbol, SETPOSPROC setpos) {
    assert(cs.num_symbols < MAX_ASM_SYMBOLS);
    struct _SYMBOL *sym = &cs.symbols[cs.num_symbols++];
    strncpy(sym->szName, symbol, sizeof(sym->szName));
    sym->setpos = setpos;
    sym->loc = *loc;
}

DWORD setpos_label(LPLOCATION loc, DWORD pos) {
    return pos;
}

void add_symbol(FILE *fp, LPCSTR symbol) {
    assert(cs.num_symbols < MAX_ASM_SYMBOLS);
    struct _SYMBOL *sym = &cs.symbols[cs.num_symbols++];
    strncpy(sym->szName, symbol, sizeof(sym->szName));
    sym->loc.Position = (DWORD)ftell(fp);
    sym->setpos = setpos_label;
    DWORD tmp = -1;
    fwrite(&tmp, 4, 1, fp);
}

BOOL is_alnum2(int a) {
    return isalnum(a) || a == '_' || a == '.' || a == '$';
}

struct _EXPORTSTRUCT *GetStdLib(void);

void add_global(FILE *fp, LPCSTR name) {
    assert(cs.num_globals < MAX_LABELS);
    strcpy(cs.globals[cs.num_globals].szName, name);
    cs.num_globals++;
}

void add_label(FILE *fp, LPCSTR name) {
    if (!strcmp(name, "_main")) {
        main_label = (DWORD)ftell(fp);
    }
    for (DWORD i = 0; i < cs.num_globals; i++) {
        if (!strcmp(cs.globals[i].szName, name)) {
            cs.globals[i].dwPosition = (DWORD)ftell(fp);
            return;
        }
    }
    assert(cs.num_labels < MAX_LABELS);
    size_t pos = ftell(fp);
    strcpy(cs.labels[cs.num_labels].szName, name);
    cs.labels[cs.num_labels].dwPosition = (DWORD)pos;
    cs.num_labels++;
}

DWORD assemble(LPCSTR line);
DWORD assemble_declare(LPCSTR line);

BOOL assembleLine(FILE *fp, LPSTR line) {
    assert(isalpha(*line) || *line == '_');
    if (strchr(line, ':')) {
        *strchr(line, ':') = '\0';
        add_label(fp, line);
        return 1;
    }/* else if (!findopcode2(fp, line)) {
        fprintf(stderr, "Can't find opcode %s on line %d\n", line, curfileline);
        return 0;
    }*/
    if (!strncasecmp("EDU", line, 3)) {
        return assemble_declare(line+3);
    }
    DWORD code = assemble(line);
    if (code != 0) {
        fwrite(&code, 4, 1, fp);
        return 1;
    } else {
        printf("Can't assemble line %s\n", line);
        return 0;
    }
}

int evaluate(LPSTR *expression);
BOOL getpos(LPCSTR str, DWORD *out);

BOOL findlabel(LPCSTR szName, DWORD *out) {
    for (int i = 0; i < cs.num_labels; i++) {
        if (!strcmp(cs.labels[i].szName, szName)) {
            *out = cs.labels[i].dwPosition;
            return 1;
        }
    }
    for (int i = 0; i < cs.num_globals; i++) {
        if (!strcmp(cs.globals[i].szName, szName)) {
            *out = cs.globals[i].dwPosition;
            return 1;
        }
    }
    for (int i = 0; i < cs.num_sets; i++) {
        struct _SET *set = &cs.sets[i];
        if (!strcmp(set->szName, szName)) {
            return getpos(set->szCode, out);
        }
    }
    return 0;
}

BOOL getpos(LPCSTR str, DWORD *out) {
    char buf[LABEL_SIZE] = { 0 };
    for (LPCSTR s = str, a = s;; s++) {
        if (is_alnum2(*s)) {
            continue;
        } else if (s != a) {
            char* endptr;
            strtod(a, &endptr);
            if (endptr == s) {
                memcpy(buf + strlen(buf), a, s - a);
            } else {
                char label[LABEL_SIZE] = { 0 };
                memcpy(label, a, s - a);
                DWORD line;
                if (!findlabel(label, &line)) {
                    return 0;
                }
                sprintf(buf + strlen(buf), "%d", line);
            }
        }
        if (isspace(*s)) {
            a = s+1;
        } else if (!*s) {
            break;
        } else {
            *(buf + strlen(buf)) = *s;
            a = s+1;
        }
    }
    char* expressionPtr = buf;
    *out = evaluate(&expressionPtr);
    return 1;
}

int compareSymbols(const void *a, const void *b) {
    return strcmp(((struct _SYMBOL *)a)->szName, ((struct _SYMBOL *)b)->szName);
}

void linkprogram(FILE *fp, DWORD startsym) {
    for (int i = startsym; i < cs.num_symbols; i++) {
        struct _SYMBOL *sym = &cs.symbols[i];
        DWORD position;
        if (sym->filled || !getpos(sym->szName, &position))
            continue;
        sym->filled = 1;
        fseek(fp, sym->loc.Position, SEEK_SET);
        DWORD instr = sym->setpos(&sym->loc, position);
        fwrite(&instr, 4, 1, fp);
    }
    fseek(fp, 0, SEEK_END);
}

void do_align(FILE *fp, int p2) {
    DWORD align = 1 << p2;
    size_t pos = ftell(fp);
    DWORD fill = pos % align;
    if (fill == 0)
        return;
    for (; fill < align; fill++) {
        fputc(0, fp);
    }
}

void f_p2align(FILE *fp, LPCSTR str) {
    do_align(fp, atoi(str));
}

void f_zerofill(FILE *fp, LPCSTR str) {
    char type[64] = { 0 };
    char init[64] = { 0 };
    char label[256] = { 0 };
    int size = 0;
    int alignment = 0;
//    int unknown = 0;
    sscanf(str, "%63[^,],%63[^,],%255[^,],%d,%d", type, init, label, &size, &alignment);
    do_align(fp, alignment);
    add_label(fp, label);
    while (size > 0) {
        fputc(0, fp);
        size--;
    }
}

void f_byte(FILE *fp, LPCSTR str) {
    unsigned short parsedValue;
    if (sscanf(str, "%hu", &parsedValue) == 1) {
        fwrite(&parsedValue, 1, 1, fp);
    } else {
//        add_symbol(fp, str);
    }
}

void f_short(FILE *fp, LPCSTR str) {
    unsigned short parsedValue;
    if (sscanf(str, "%hu", &parsedValue) == 1) {
        fwrite(&parsedValue, 2, 1, fp);
    } else {
//        add_symbol(fp, str);
    }
}
void f_long(FILE *fp, LPCSTR str) {
    unsigned int parsedValue;
    if (sscanf(str, "%u", &parsedValue) == 1) {
        fwrite(&parsedValue, 4, 1, fp);
    } else {
        add_symbol(fp, str);
    }
}
void f_set(FILE *fp, LPCSTR str) {
    assert(cs.num_sets < MAX_LABELS);
    struct _SET *set = &cs.sets[cs.num_sets++];
    LPCSTR a = str, b = a;
    for (; is_alnum2(*b); b++);
    memset(set, 0, sizeof(struct _SET));
    memcpy(set->szName, a, b - a);
    ++b;
    while (isspace(*b)) ++b;
    for (LPSTR s = set->szCode; *b; b++, s++) {
        *s = *b;
    }
}

void f_globl(FILE *fp, LPCSTR str) {
    add_global(fp, str);
}

void f_ascii(FILE *fp, LPCSTR str) {
    char parsedString[1024] = { 0 };
    if (sscanf(str, "\"%1023[^\"]\"", parsedString) != 1) {
        fprintf(stderr, "Failed to parse %s\n", str);
        return;
    }
    
    for (LPSTR s = parsedString; *s; s++) {
        if (*s != '\\') {
            fputc(*s, fp);
            continue;
        }
        s++;
        if (isdigit(*s)) {
            int byteValue = (int)strtol(s, &s, 0);
            fputc(byteValue, fp);
            s--; // to compensate for s++ above
        } else switch (*s) {
            case 'n': fputc('\n', fp); break;
            case 't': fputc('\t', fp); break;
            case 'b': fputc('\b', fp); break;
            case 'r': fputc('\r', fp); break;
            case 'f': fputc('\f', fp); break;
            case 'a': fputc('\a', fp); break;
            case 'v': fputc('\v', fp); break;
            case '?': fputc('\?', fp); break;
            case '\\': fputc('\\', fp); break;
            case '\'': fputc('\'', fp); break;
            case '\"': fputc('\"', fp); break;
            case '\0': fputc('\0', fp); break;
            default:
                printf("Unknown special character \\%c (%d)\n", *s, *s);
                break;
        }
    }
}

void f_asciz(FILE *fp, LPCSTR str) {
    f_ascii(fp, str);
    fputc(0, fp);
}

void f_data_region(FILE *fp, LPCSTR str) {
}

void f_end_data_region(FILE *fp, LPCSTR str) {
}

void f_section(FILE *fp, LPCSTR str) {
}

void f_space(FILE *fp, LPCSTR str) {
    int a = 0;
    for (int i = 0; i < atoi(str); i++) {
        fwrite(&a, 1, 1, fp);
    }
}

void f_code(FILE *fp, LPCSTR str) {
    assert(atoi(str) == 32);
}

void f_syntax(FILE *fp, LPCSTR str) {
}

void f_build_version(FILE *fp, LPCSTR str) {
}

void f_loc(FILE *fp, LPCSTR str) {
}

void f_file(FILE *fp, LPCSTR str) {
}

void f_subsections_via_symbols(FILE *fp, LPCSTR str) {
}

void f_comm(FILE *fp, LPCSTR str) {
    int size, align;
    char symbol[64];
    if (sscanf(str, "%63[^,],%d,%d", symbol, &size, &align) != 3) {
        printf("can't parse .COMM %s\n", str);
        return;
    }
    add_global(fp, symbol);
    add_label(fp, symbol);
    do_align(fp, align);
    for (int i = 0; i < size; i++) {
        fputc(0, fp);
    }
}

void f_indirect_symbol(FILE *fp, LPCSTR str) {
    add_symbol(fp, str);
}

struct _TAG {
    LPCSTR name;
    void (*proc)(FILE *fp, LPCSTR str);
};

struct _TAG tags[] = {
    { ".zerofill", f_zerofill },
    { ".byte", f_byte },
    { ".short", f_short },
    { ".long", f_long },
    { ".set", f_set },
    { ".globl", f_globl },
    { ".asciz", f_asciz },
    { ".ascii", f_ascii },
    { ".p2align", f_p2align },
    { ".code", f_code },
    { ".section", f_section },
    { ".space", f_space },
    { ".build_version", f_build_version },
    { ".syntax", f_syntax },
    { ".comm", f_comm },
    { ".data_region", f_data_region },
    { ".indirect_symbol", f_indirect_symbol },
    { ".end_data_region", f_end_data_region },
    { ".subsections_via_symbols", f_subsections_via_symbols },
    // debug info
    { ".loc", f_loc },
    { ".file", f_file },
    { NULL }
};

BOOL compile_buffer(FILE *fp, FILE *d_fp, LPCSTR filename, LPCSTR test) {
    main_label = 0;
    DWORD startsym = cs.num_symbols;
    
    static char lineBuffer[MAX_LINE_LENGTH];
    LPCSTR txt = test;
    while (ExtractLine(&txt, lineBuffer)) {
        curfilepos = (DWORD)ftell(fp);
        curline++;

        if (d_fp) {
            char d_file[28]={0};
            int pos = (DWORD)curfilepos;
            char const *ff = filename;
            for (const char *f2 = filename; *f2; f2++) {
                if (*f2 == '/' || *f2 == '\\') {
                    ff = f2+1;
                }
            }
            snprintf(d_file, sizeof(d_file), "%s:%d", ff, curfileline);
            fwrite(&pos, sizeof(pos), 1, d_fp);
            fwrite(d_file, sizeof(d_file), 1, d_fp);
        }

        curfileline++;
        
        
        if (*lineBuffer == '\0') continue;
        if (*lineBuffer == '.') {
            for (struct _TAG *tag = tags; tag->name; tag++) {
                if (strstr(lineBuffer, tag->name) == lineBuffer) {
                    LPCSTR s = lineBuffer + strlen(tag->name);
                    while (isspace(*s)) s++;
                    tag->proc(fp, s);
                    goto proceed;
                }
            }
            printf("Unknown tag %s\n", lineBuffer);
        proceed:
            continue;
        } else if (!assembleLine(fp, lineBuffer)) {
            return 0;
        }
    }
    
    linkprogram(fp, startsym);
        
    memcpy(cs.debug + cs.num_debug, cs.labels, sizeof(struct _LABEL) * cs.num_labels);
    cs.num_debug += cs.num_labels;
    
    cs.num_labels = 0;
    cs.num_sets = 0;
    
    return 1;
}

LPVM vm_create(VM_SysCall syscall, DWORD stack_size, DWORD heap_size, BYTE *program, DWORD progsize) {
    BYTE *memory = malloc(sizeof(struct VM) + stack_size + heap_size + progsize);
    memset(memory, 0, sizeof(struct VM));
    memcpy(memory + sizeof(struct VM), program, progsize);
    LPVM vm = (LPVM)memory;
    vm->memory = memory + sizeof(struct VM);
    vm->stacksize = stack_size;
    vm->heapsize = heap_size;
    vm->progsize = progsize;
    vm->syscall = syscall;
    vm->r[SP_REG] = VM_STACK_SIZE + progsize;
    initialize_memory_manager(vm, vm->memory + stack_size + progsize, heap_size);
    return vm;
}

void vm_shutdown(LPVM vm) {
    free(vm);
}

static void* extrect_input(FILE *input) {
    fseek(input, 0, SEEK_END);
    size_t size = ftell(input);
    char* test = malloc(size + 1);
    fseek(input, 0, SEEK_SET);
    fread(test, size, 1, input);
    test[size] = 0;
    fclose(input);
    return test;
}

int main(int argc, const char * argv[]) {
    LPSTR test;
    
    LPCSTR output = NULL;
    LPCSTR files[256];
    int num_files = 0;
    
    cs.num_debug = 0;
    cs.num_symbols = 0;
    cs.num_labels = 0;
    cs.num_globals = 0;
    
    for (int i = 1; i < argc; i++) {
        if (*argv[i] == '-') {
            switch (*(argv[i]+1)) {
                case 'o':
                    if (++i < argc) {
                        output = argv[i];
                    }
                    break;
            }
        } else {
            files[num_files++] = argv[i];
        }
    }
    
    if (!output) {
        printf("No output file specified\n");
        return 1;
    }

    if (!num_files) {
        printf("No input files specified\n");
        return 1;
    }
    
    char debug_file[256];
    sprintf(debug_file, "%s_d", output);
    
    FILE *fp = fopen(output, "wb");
    FILE *d_fp = fopen(debug_file, "wb");

    for (int i = 0; i < num_files; i++) {
        FILE *input = fopen(files[i], "r");
        
        printf("%s\n", files[i]);
//        fprintf(fp, "%s", files[i]);

        if (!input) {
            fprintf(stdout, "Input file not found\n");
            return 1;
        }
        curfileline=0;
        test = extrect_input(input);
        

        if (!compile_buffer(fp, d_fp, files[i], test)) {
            free(test);
            fclose(fp);
            remove(output);
            return 1;
        }

        free(test);
    }

    struct _VMHDR hdr = {
        .magic = ID_ORCA,
        .programsize = (DWORD)ftell(fp),
//        .numberofsymbols = cs.num_globals,
        .numberofsymbols = cs.num_globals,// + cs.num_debug,
    };

    for (int i = 0; i < cs.num_globals; i++) {
        struct _LABEL *l = &cs.globals[i];
        fwrite(&l->dwPosition, 4, 1, fp);
        fwrite(l->szName, strlen(l->szName)+1, 1, fp);
    }
//    for (int i = 0; i < cs.num_debug; i++) {
//        struct _LABEL *l = &cs.debug[i];
//        fwrite(&l->dwPosition, 4, 1, fp);
//        fwrite(l->szName, strlen(l->szName)+1, 1, fp);
//    }

    linkprogram(fp, 0);
    
    fclose(fp);
    fclose(d_fp);

    // rewrite file with header
    fp = fopen(output, "rb");
    fseek(fp, 0, SEEK_END);
    DWORD progsize = (DWORD)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    void *buffer = malloc(progsize);
    fread(buffer, progsize, 1, fp);
    fclose(fp);
    
    fp = fopen(output, "wb");
    fwrite(&hdr, sizeof(hdr), 1, fp);
    fwrite(buffer, progsize, 1, fp);
    fclose(fp);

    qsort(cs.symbols, cs.num_symbols, sizeof(struct _SYMBOL), compareSymbols);
    
    for (int i = 0; i < cs.num_symbols; i++) {
        struct _SYMBOL *sym = &cs.symbols[i];
        if (sym->filled)
            continue;
        for (int j = 0; j < i; j++) {
            if (!strcmp(cs.symbols[i].szName, cs.symbols[j].szName)) {
                goto next;
            }
        }
        printf("Undefined reference '%s'\n", sym->szName);
    next:
        ;
    }
    
//    struct VM vm = {
//        .memory = malloc(VM_STACK_SIZE + VM_HEAP_SIZE),
//        .stacksize = VM_STACK_SIZE,
//        .heapsize = VM_HEAP_SIZE,
//        .program = buffer,
//        .progsize = (DWORD)progsize,
//        .exports = exports
//    };
//    
//    vm.r[SP_REG] = VM_STACK_SIZE;
//    
//    int start_point = 0;
//    for (int i = 0; i < cs.num_labels; i++) {
//        if (!strcmp(labels[i].szName, "_UIView_UpdateGeometry")) {
//            start_point = labels[i].dwPosition;
//        }
//    }
//        
//    execute(&vm, start_point);
//    
//    free(vm.memory);
    
    return 0;
}

#define VMA(A) ((void *)(vm->memory + vm->r[A]))

DWORD _strlen(LPVM vm) {
    LPCSTR str = VMA(0);
    return (DWORD)strlen(str);
}

DWORD _snprintf(LPVM vm) {
    LPCSTR str = VMA(0);
    LPCSTR str3 = VMA(2);
    DWORD *ptr = vm->memory + vm->r[SP_REG];
    char *str2 = vm->memory + *(ptr++);
    DWORD value = *(ptr++);
    return 0;
}

DWORD VM_Malloc(LPVM vm);
DWORD VM_Memset(LPVM vm);

DWORD mysyscall(LPVM vm, DWORD call) {
    switch (call) {
        case 1:
            return _strlen(vm);
        case 2:
            return VM_Malloc(vm);
        case 3:
            return VM_Memset(vm);
        case 4:
            return puts(VMA(0));
        case 5:
            return _snprintf(vm);
        default:
            return 0;
    }
}

DWORD test_program(LPCSTR code, DWORD r) {
    FILE *fp = tmpfile();
    extern int main_label;
    main_label = 0;
    
    strcpy(symbols[1], "strlen");
    strcpy(symbols[2], "malloc");
    strcpy(symbols[3], "free");
    strcpy(symbols[4], "puts");
    strcpy(symbols[5], "snprintf");

    if (!compile_buffer(fp, NULL, NULL, code)) {
        printf("Failed to compile\n");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    DWORD psize = (DWORD)ftell(fp);
    void *buffer = malloc(psize);
    fseek(fp, 0, SEEK_SET);
    fread(buffer, psize, 1, fp);
    fclose(fp);
    LPVM vm = vm_create(mysyscall, VM_STACK_SIZE, VM_HEAP_SIZE, buffer, psize);
    execute(vm, main_label);
    DWORD result = vm->r[r];
    vm_shutdown(vm);
    free(buffer);
    return result;
}

DWORD run_program(LPCSTR filename) {
    char path[1024] = {0};
    sprintf(path, "/Users/igor/Developer/asm-test/%s", filename);
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("File not found %s\n", path);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    size_t psize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = malloc(psize+1);
    buf[psize]=0;
    fread(buf, psize, 1, fp);
    DWORD r = test_program(buf, 0);
    free(buf);
    return r;
}
