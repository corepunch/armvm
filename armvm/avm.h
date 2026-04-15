#ifndef avm_h
#define avm_h

/*
 * avm.h — Public API for the ARM Virtual Machine.
 *
 * The interface is modelled after the Lua C API so that embedding the VM
 * feels familiar to anyone who has used Lua:
 *
 *   avm_State  ↔  lua_State
 *   avm_CFunction ↔  lua_CFunction
 *   avm_newstate  ↔  luaL_newstate
 *   avm_close     ↔  lua_close
 *   avm_register  ↔  lua_register
 *   avm_loadbuffer↔  luaL_loadbuffer
 *   avm_call      ↔  lua_call
 *   avm_tointeger ↔  lua_tointeger   (1-indexed register access)
 *   avm_pushinteger↔ lua_pushinteger  (writes to r0 as return value)
 *
 * Quick-start:
 *
 *   static int my_strlen(avm_State *S) {
 *       avm_pushinteger(S, (int)strlen(avm_tostring(S, 1)));
 *       return 1;  // one return value in r0
 *   }
 *
 *   avm_State *S = avm_newstate(64*1024, 64*1024);
 *   avm_register(S, "strlen", my_strlen);
 *   avm_loadbuffer(S, asm_source, strlen(asm_source));
 *   avm_call(S, S->entry_point);
 *   printf("result = %d\n", avm_tointeger(S, 1));
 *   avm_close(S);
 */

#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* State management                                                        */
/* ---------------------------------------------------------------------- */

/*
 * avm_newstate — create a new VM state (like luaL_newstate).
 *
 * stack_size and heap_size are in bytes.  The state starts with no program
 * loaded; call avm_loadbuffer() before avm_call().
 */
avm_State *avm_newstate(DWORD stack_size, DWORD heap_size);

/*
 * avm_close — destroy a VM state and free all associated memory
 * (like lua_close).
 */
void avm_close(avm_State *S);

/* ---------------------------------------------------------------------- */
/* Loading code                                                            */
/* ---------------------------------------------------------------------- */

/*
 * avm_loadbuffer — compile ARM assembly source and load it into the VM
 * (like luaL_loadbuffer).
 *
 * Must be called after all avm_register() calls so that external function
 * names are resolved during compilation.
 *
 * Returns 0 on success, non-zero on compilation error.
 * On success, S->entry_point is set to the position of the _main label.
 */
int avm_loadbuffer(avm_State *S, const char *code, size_t len);

/* ---------------------------------------------------------------------- */
/* Execution                                                               */
/* ---------------------------------------------------------------------- */

/*
 * avm_call — execute the VM starting at the given program-counter offset
 * (like lua_call).
 *
 * After avm_loadbuffer() you typically pass S->entry_point as pc.
 */
void avm_call(avm_State *S, DWORD pc);

/* ---------------------------------------------------------------------- */
/* C function registration                                                 */
/* ---------------------------------------------------------------------- */

/*
 * avm_register — register a C function under a name so that ARM assembly
 * can call it with "bl _<name>" (like lua_register).
 *
 * Must be called before avm_loadbuffer() so the assembler can resolve the
 * symbol.  name must not include a leading underscore.
 */
void avm_register(avm_State *S, const char *name, avm_CFunction fn);

/* ---------------------------------------------------------------------- */
/* Reading ARM registers (1-indexed, like lua_to*)                        */
/*                                                                         */
/* idx == 1 maps to r0, idx == 2 maps to r1, etc.                        */
/* ---------------------------------------------------------------------- */

int          avm_tointeger (avm_State *S, int idx);
unsigned int avm_touinteger(avm_State *S, int idx);
float        avm_tonumber  (avm_State *S, int idx);
const char  *avm_tostring  (avm_State *S, int idx); /* r[idx-1] as VM memory pointer */
void        *avm_topointer (avm_State *S, int idx); /* r[idx-1] as VM memory pointer */
int          avm_toboolean (avm_State *S, int idx);

/* ---------------------------------------------------------------------- */
/* Writing return values (like lua_push*)                                  */
/*                                                                         */
/* In an avm_CFunction these set the value(s) returned to the ARM caller. */
/* The first push goes to r0, matching the ARM calling convention.        */
/* ---------------------------------------------------------------------- */

void avm_pushinteger(avm_State *S, int n);
void avm_pushnumber (avm_State *S, float n);
void avm_pushboolean(avm_State *S, int b);

#ifdef __cplusplus
}
#endif

#endif /* avm_h */
