/*
** $Id: lua2llvmir.h $
** Lua to LLVM IR Compiler
** See Copyright Notice in lua.h
*/

#ifndef lua2llvmir_h
#define lua2llvmir_h

#include "lua.h"
#include "lstate.h"
#include "lobject.h"

/*
** Compilation result structure
*/
typedef struct LuaLLVMResult {
  char *ir_code;          /* Generated LLVM IR code */
  size_t ir_size;         /* Size of IR code */
  int status;             /* Compilation status (0 = success) */
  const char *error_msg;  /* Error message if compilation failed */
} LuaLLVMResult;

/*
** Main compilation functions
*/
LUAI_FUNC LuaLLVMResult lua2llvmir_compile_source (lua_State *L, const char *source, 
                                                   const char *chunkname);
LUAI_FUNC LuaLLVMResult lua2llvmir_compile_proto (lua_State *L, Proto *p);
LUAI_FUNC void lua2llvmir_free_result (LuaLLVMResult *result);

/*
** Error codes
*/
#define LUA2LLVM_OK           0
#define LUA2LLVM_PARSE_ERROR  1
#define LUA2LLVM_LLVM_ERROR   2
#define LUA2LLVM_MEMORY_ERROR 3

#endif