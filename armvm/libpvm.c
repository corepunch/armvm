#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/termios.h>
#include <time.h>
#include <unistd.h>

#include "vm.h"

#define VMU(N) (*((unsigned int *)vm->r + N))
#define VMI(N) (*((int *)vm->r + N))
#define VMF(N) (*((float *)vm->r + N))
#define VMD(N) (*((double *)vm->r + N))
#define VMA(A) ((void *)(vm->memory + vm->r[A]))
#define SYSCALL(name) DWORD _##name(LPVM vm)

#define VM_ConvPointer(vm, ptr) (DWORD)((void *)(ptr) - (void *)vm->memory)

#define VM_ALLOC(vm, size) VM_ConvPointer(vm, my_malloc(vm, size))
#define VM_FREE(vm, reg) my_free(vm, VMA(reg))

inline static DWORD FloatAsInt(float f) {
  DWORD temp;
  *(float *)&temp = f;
  return temp;
}

DWORD VM_Malloc(LPVM vm) {
    VMU(0) = VM_ALLOC(vm, VMU(0));
    return *vm->r;
}

DWORD VM_Memset(LPVM vm) {
    memset(VMA(0), VMU(1), VMU(2));
    return *vm->r;
}

float VM_GetFloat(LPVM vm, DWORD reg) { return *(float *)&vm->r[reg]; }

void VM_SetFloat(LPVM vm, DWORD reg, float value) {
  *(float *)&vm->r[reg] = value;
}

void VM_SetInteger(LPVM vm, DWORD reg, int value) { vm->r[reg] = value; }

typedef struct _matrix4 {
  float v[16];
} MAT4, LPMAT4;

SYSCALL(MAT4_Identity) { return 0; }
SYSCALL(MAT4_LookAt) { return 0; }
SYSCALL(MAT4_Ortho) { return 0; }
SYSCALL(MAT4_Inverse) { return 0; }
SYSCALL(MAT4_MultiplyVector3) { return 0; }
SYSCALL(MAT4_Multiply) { return 0; }
SYSCALL(MAT4_Translate) { return 0; }

SYSCALL(TRANSFORM2_Identity) { return 0; }
SYSCALL(TRANSFORM2_ToMatrix4) { return 0; }
SYSCALL(TRANSFORM3_Identity) { return 0; }
SYSCALL(TRANSFORM3_ToMatrix4) { return 0; }


// Time

#define EXPORT(NAME)                                                           \
  { #NAME, _##NAME }

//struct _EXPORTSTRUCT exports[] = {
//    EXPORT(__addsf3),
//    EXPORT(__subsf3),
//    EXPORT(__mulsf3),
//    EXPORT(__divsf3),
//    EXPORT(__gtsf2),
//    EXPORT(__gesf2),
//    EXPORT(__ltsf2),
//    EXPORT(__lesf2),
//    EXPORT(__eqsf2),
//    EXPORT(__nesf2),
//    EXPORT(__fixsfsi),
//    EXPORT(__fixunssfsi),
//    EXPORT(__floatsisf),
//    EXPORT(__floatunsisf),
//    EXPORT(__unordsf2),
//    EXPORT(__modsi3),
//    EXPORT(__divsi3),
//    EXPORT(__udivsi3),
//    EXPORT(__umodsi3),
//    EXPORT(__adddf3),
//    EXPORT(__muldf3),
//    EXPORT(__extendsfdf2),
//    EXPORT(__fixsfdi),
//    EXPORT(__floatdisf),
//    EXPORT(__floatundisf),
//    EXPORT(__floatunsidf),
//    EXPORT(__eqdf2),
//    EXPORT(__gedf2),
//    EXPORT(__truncdfsf2),
//    
//    EXPORT(atof),
//    EXPORT(snprintf),
//    EXPORT(sprintf),
//    EXPORT(strchr),
//    EXPORT(strcmp),
//    EXPORT(strcoll),
//    EXPORT(strcpy),
//    EXPORT(strncpy),
//    EXPORT(strdup),
//    EXPORT(strerror),
//    EXPORT(strftime),
//    EXPORT(strlen),
//    EXPORT(strncmp),
//    EXPORT(strpbrk),
//    EXPORT(strspn),
//    EXPORT(strstr),
//    EXPORT(strtof),
//    EXPORT(strtok),
//    
//    EXPORT(fopen),
//    EXPORT(fwrite),
//    EXPORT(fread),
//    EXPORT(fclose),
//    EXPORT(feof),
//    EXPORT(ferror),
//    EXPORT(fflush),
//    EXPORT(fgets),
//    EXPORT(fprintf),
//    EXPORT(fputc),
//    EXPORT(fputs),
//    EXPORT(freopen),
//    EXPORT(fseek),
//    EXPORT(ftell),
//    EXPORT(setvbuf),
//    
//    EXPORT(remove),
//    EXPORT(rename),
//    EXPORT(tmpfile),
//    EXPORT(tmpnam),
//    
//    EXPORT(system),
//    
//    EXPORT(ungetc),
//    EXPORT(getc),
//    EXPORT(puts),
//    EXPORT(putchar),
//    
//    EXPORT(read),
//    EXPORT(write),
//    
//    EXPORT(getenv),
//    EXPORT(clearerr),
//    
//    EXPORT(memchr),
//    EXPORT(memcmp),
//    EXPORT(memcpy),
//    
//    EXPORT(frexpf),
//    EXPORT(floorf),
//    EXPORT(fmaxf),
//    EXPORT(fminf),
//    EXPORT(acosf),
//    EXPORT(asinf),
//    EXPORT(atan2f),
//    EXPORT(ceilf),
//    EXPORT(cosf),
//    EXPORT(expf),
//    EXPORT(fmax),
//    EXPORT(fmin),
//    EXPORT(fmodf),
//    EXPORT(log10f),
//    EXPORT(log2f),
//    EXPORT(logf),
//    EXPORT(tanf),
//    EXPORT(sqrtf),
//    EXPORT(sinf),
//    EXPORT(powf),
//    EXPORT(roundf),
//    
//    EXPORT(__stack_chk_fail),
//    EXPORT(__assert_rtn),
//    EXPORT(__maskrune),
//    
//    EXPORT(MAT4_Identity),
//    EXPORT(MAT4_LookAt),
//    EXPORT(MAT4_Ortho),
//    EXPORT(MAT4_Inverse),
//    EXPORT(MAT4_MultiplyVector3),
//    EXPORT(MAT4_Multiply),
//    EXPORT(MAT4_Translate),
//    EXPORT(TRANSFORM2_Identity),
//    EXPORT(TRANSFORM2_ToMatrix4),
//    EXPORT(TRANSFORM3_Identity),
//    EXPORT(TRANSFORM3_ToMatrix4),
//    
//    EXPORT(malloc),
//    EXPORT(realloc),
//    EXPORT(free),
//    EXPORT(memset),
//    
//    EXPORT(clock),
//    EXPORT(difftime),
//    EXPORT(time),
//    EXPORT(gmtime),
//    EXPORT(localtime),
//    EXPORT(mktime),
//    
//    EXPORT(setlocale),
//    EXPORT(localeconv),
//    
//    {"__error", __exit},
//    {"exit", __exit},
//    {"abort", __exit},
//    
//    {"__tolower", __tolower2},
//    {"__toupper", __toupper2},
//    {"__memcpy_chk", _memcpy},
//    {"__strcpy_chk", _strcpy},
//    
//    {"setjmp", __setjmp},
//    {"longjmp", __longjmp},
//    
//    {NULL}
//};
