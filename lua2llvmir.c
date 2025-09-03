/*
** $Id: lua2llvmir.c $
** Lua to LLVM IR Compiler
** See Copyright Notice in lua.h
*/

#define lua2llvmir_c
#define LUA_CORE

#include "lprefix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llimits.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lopnames.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lvm.h"
#include "lua2llvmir.h"

/*
** LLVM IR generation context
*/
typedef struct LLVMContext {
  lua_State *L;
  Proto *proto;
  char *buffer;
  size_t buffer_size;
  size_t buffer_pos;
  int reg_counter;
  int label_counter;
  int error_status;
  const char *error_msg;
} LLVMContext;

/*
** Buffer management
*/
static void llvm_init_context(LLVMContext *ctx, lua_State *L, Proto *p) {
  ctx->L = L;
  ctx->proto = p;
  ctx->buffer_size = 4096;
  ctx->buffer = cast_charp(luaM_malloc_(L, ctx->buffer_size, 0));
  ctx->buffer_pos = 0;
  ctx->reg_counter = 0;
  ctx->label_counter = 0;
  ctx->error_status = LUA2LLVM_OK;
  ctx->error_msg = NULL;
}

static void llvm_free_context(LLVMContext *ctx) {
  if (ctx->buffer) {
    luaM_freemem(ctx->L, ctx->buffer, ctx->buffer_size);
    ctx->buffer = NULL;
  }
}

static void llvm_ensure_buffer(LLVMContext *ctx, size_t needed) {
  if (ctx->buffer_pos + needed >= ctx->buffer_size) {
    size_t new_size = ctx->buffer_size * 2;
    while (new_size < ctx->buffer_pos + needed) new_size *= 2;
    ctx->buffer = cast_charp(luaM_saferealloc_(ctx->L, ctx->buffer, ctx->buffer_size, new_size));
    ctx->buffer_size = new_size;
  }
}

static void llvm_emit(LLVMContext *ctx, const char *format, ...) {
  va_list args;
  va_list args_copy;
  int needed;
  
  va_start(args, format);
  
  /* Calculate needed space */
  va_copy(args_copy, args);
  needed = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);
  
  if (needed < 0) {
    ctx->error_status = LUA2LLVM_LLVM_ERROR;
    ctx->error_msg = "Format error in LLVM emission";
    va_end(args);
    return;
  }
  
  llvm_ensure_buffer(ctx, (size_t)(needed + 1));
  vsnprintf(ctx->buffer + ctx->buffer_pos, (size_t)(needed + 1), format, args);
  ctx->buffer_pos += (size_t)needed;
  va_end(args);
}

/*
** Generate LLVM IR type for Lua values
*/
static void llvm_emit_lua_value_type(LLVMContext *ctx) {
  llvm_emit(ctx, "%%LuaValue = type { i32, [8 x i8] }\n");
}

/*
** Generate function prologue
*/
static void llvm_emit_function_start(LLVMContext *ctx, const char *func_name) {
  llvm_emit(ctx, "define %%LuaValue @%s(i32 %%argc, %%LuaValue* %%args) {\n", func_name);
  llvm_emit(ctx, "entry:\n");
}

/*
** Generate function epilogue
*/
static void llvm_emit_function_end(LLVMContext *ctx) {
  llvm_emit(ctx, "  ret %%LuaValue zeroinitializer\n");
  llvm_emit(ctx, "}\n");
}

/*
** Generate LLVM IR for a Lua constant
*/
static int llvm_emit_constant(LLVMContext *ctx, const TValue *k) {
  int reg = ctx->reg_counter++;
  
  switch (ttypetag(k)) {
    case LUA_TNIL:
      llvm_emit(ctx, "  %%r%d = insertvalue %%LuaValue zeroinitializer, i32 0, 0\n", reg);
      break;
    case LUA_VFALSE:
      llvm_emit(ctx, "  %%r%d = insertvalue %%LuaValue zeroinitializer, i32 1, 0\n", reg);
      break;
    case LUA_VTRUE:
      llvm_emit(ctx, "  %%r%d = insertvalue %%LuaValue zeroinitializer, i32 2, 0\n", reg);
      break;
    case LUA_VNUMINT:
      llvm_emit(ctx, "  %%r%d = insertvalue %%LuaValue zeroinitializer, i32 3, 0\n", reg);
      llvm_emit(ctx, "  %%temp%d = alloca i64\n", reg);
      llvm_emit(ctx, "  store i64 %lld, i64* %%temp%d\n", (long long)ivalue(k), reg);
      llvm_emit(ctx, "  %%temp%d_cast = bitcast i64* %%temp%d to [8 x i8]*\n", reg, reg);
      llvm_emit(ctx, "  %%r%d_val = load [8 x i8], [8 x i8]* %%temp%d_cast\n", reg, reg);
      llvm_emit(ctx, "  %%r%d_final = insertvalue %%LuaValue %%r%d, [8 x i8] %%r%d_val, 1\n", 
                reg, reg, reg);
      break;
    case LUA_VNUMFLT:
      llvm_emit(ctx, "  %%r%d = insertvalue %%LuaValue zeroinitializer, i32 4, 0\n", reg);
      llvm_emit(ctx, "  %%tempf%d = alloca double\n", reg);
      llvm_emit(ctx, "  store double %g, double* %%tempf%d\n", fltvalue(k), reg);
      llvm_emit(ctx, "  %%tempf%d_cast = bitcast double* %%tempf%d to [8 x i8]*\n", reg, reg);
      llvm_emit(ctx, "  %%r%d_val = load [8 x i8], [8 x i8]* %%tempf%d_cast\n", reg, reg);
      llvm_emit(ctx, "  %%r%d_final = insertvalue %%LuaValue %%r%d, [8 x i8] %%r%d_val, 1\n", 
                reg, reg, reg);
      break;
    default:
      /* For now, treat other types as nil */
      llvm_emit(ctx, "  %%r%d = insertvalue %%LuaValue zeroinitializer, i32 0, 0\n", reg);
      break;
  }
  
  return reg;
}

/*
** Generate LLVM IR for Lua bytecode instruction
*/
static void llvm_emit_instruction(LLVMContext *ctx, Instruction i, int pc) {
  OpCode op = GET_OPCODE(i);
  int a = GETARG_A(i);
  
  llvm_emit(ctx, "  ; Instruction %d: %s\n", pc, opnames[op]);
  
  switch (op) {
    case OP_LOADNIL: {
      int b = GETARG_B(i);
      for (int reg = a; reg <= a + b; reg++) {
        int result_reg = ctx->reg_counter++;
        llvm_emit(ctx, "  %%r%d = insertvalue %%LuaValue zeroinitializer, i32 0, 0\n", result_reg);
      }
      break;
    }
    case OP_LOADFALSE: {
      int result_reg = ctx->reg_counter++;
      llvm_emit(ctx, "  %%r%d = insertvalue %%LuaValue zeroinitializer, i32 1, 0\n", result_reg);
      break;
    }
    case OP_LOADTRUE: {
      int result_reg = ctx->reg_counter++;
      llvm_emit(ctx, "  %%r%d = insertvalue %%LuaValue zeroinitializer, i32 2, 0\n", result_reg);
      break;
    }
    case OP_LOADK: {
      int bx = GETARG_Bx(i);
      if (bx < ctx->proto->sizek) {
        llvm_emit_constant(ctx, &ctx->proto->k[bx]);
      }
      break;
    }
    case OP_LOADI: {
      int sbx = GETARG_sBx(i);
      int result_reg = ctx->reg_counter++;
      llvm_emit(ctx, "  %%r%d = insertvalue %%LuaValue zeroinitializer, i32 3, 0\n", result_reg);
      llvm_emit(ctx, "  %%temp%d = alloca i64\n", result_reg);
      llvm_emit(ctx, "  store i64 %d, i64* %%temp%d\n", sbx, result_reg);
      llvm_emit(ctx, "  %%temp%d_cast = bitcast i64* %%temp%d to [8 x i8]*\n", result_reg, result_reg);
      llvm_emit(ctx, "  %%r%d_val = load [8 x i8], [8 x i8]* %%temp%d_cast\n", result_reg, result_reg);
      llvm_emit(ctx, "  %%r%d_final = insertvalue %%LuaValue %%r%d, [8 x i8] %%r%d_val, 1\n", 
                result_reg, result_reg, result_reg);
      break;
    }
    case OP_MOVE: {
      int b = GETARG_B(i);
      int result_reg = ctx->reg_counter++;
      llvm_emit(ctx, "  %%r%d = load %%LuaValue, %%LuaValue* %%r%d\n", result_reg, b);
      break;
    }
    case OP_ADD: {
      int b = GETARG_B(i);
      int c = GETARG_C(i);
      int result_reg = ctx->reg_counter++;
      llvm_emit(ctx, "  %%r%d = call %%LuaValue @lua_add(%%LuaValue %%r%d, %%LuaValue %%r%d)\n", 
                result_reg, b, c);
      break;
    }
    case OP_SUB: {
      int b = GETARG_B(i);
      int c = GETARG_C(i);
      int result_reg = ctx->reg_counter++;
      llvm_emit(ctx, "  %%r%d = call %%LuaValue @lua_sub(%%LuaValue %%r%d, %%LuaValue %%r%d)\n", 
                result_reg, b, c);
      break;
    }
    case OP_MUL: {
      int b = GETARG_B(i);
      int c = GETARG_C(i);
      int result_reg = ctx->reg_counter++;
      llvm_emit(ctx, "  %%r%d = call %%LuaValue @lua_mul(%%LuaValue %%r%d, %%LuaValue %%r%d)\n", 
                result_reg, b, c);
      break;
    }
    case OP_DIV: {
      int b = GETARG_B(i);
      int c = GETARG_C(i);
      int result_reg = ctx->reg_counter++;
      llvm_emit(ctx, "  %%r%d = call %%LuaValue @lua_div(%%LuaValue %%r%d, %%LuaValue %%r%d)\n", 
                result_reg, b, c);
      break;
    }
    case OP_ADDI: {
      int b = GETARG_B(i);
      int sc = GETARG_sC(i);
      int result_reg = ctx->reg_counter++;
      llvm_emit(ctx, "  %%r%d_imm = insertvalue %%LuaValue zeroinitializer, i32 3, 0\n", result_reg);
      llvm_emit(ctx, "  %%temp%d = alloca i64\n", result_reg);
      llvm_emit(ctx, "  store i64 %d, i64* %%temp%d\n", sc, result_reg);
      llvm_emit(ctx, "  %%temp%d_cast = bitcast i64* %%temp%d to [8 x i8]*\n", result_reg, result_reg);
      llvm_emit(ctx, "  %%r%d_imm_val = load [8 x i8], [8 x i8]* %%temp%d_cast\n", result_reg, result_reg);
      llvm_emit(ctx, "  %%r%d_imm_final = insertvalue %%LuaValue %%r%d_imm, [8 x i8] %%r%d_imm_val, 1\n", 
                result_reg, result_reg, result_reg);
      llvm_emit(ctx, "  %%r%d = call %%LuaValue @lua_add(%%LuaValue %%r%d, %%LuaValue %%r%d_imm_final)\n", 
                result_reg, b, result_reg);
      break;
    }
    case OP_CALL: {
      int call_a = GETARG_A(i);
      int b = GETARG_B(i);
      int c = GETARG_C(i);
      int nargs = (b == 0) ? -1 : b - 1;  /* -1 means variable args */
      int nresults = (c == 0) ? -1 : c - 1;  /* -1 means variable results */
      int func_reg = ctx->reg_counter++;
      int args_reg, arg_ptr_reg, arg_val_reg, args_cast_reg, result_reg;
      int arg;
      
      /* Load function to call from register A */
      llvm_emit(ctx, "  %%r%d = load %%LuaValue, %%LuaValue* %%r%d\n", func_reg, call_a);
      
      if (nargs >= 0) {
        /* Fixed number of arguments - create argument array */
        if (nargs > 0) {
          /* Allocate array for arguments */
          args_reg = ctx->reg_counter++;
          llvm_emit(ctx, "  %%r%d = alloca [%d x %%LuaValue]\n", args_reg, nargs);
          
          /* Store arguments in array */
          for (arg = 0; arg < nargs; arg++) {
            arg_ptr_reg = ctx->reg_counter++;
            arg_val_reg = ctx->reg_counter++;
            llvm_emit(ctx, "  %%r%d = getelementptr [%d x %%LuaValue], [%d x %%LuaValue]* %%r%d, i32 0, i32 %d\n", 
                      arg_ptr_reg, nargs, nargs, args_reg, arg);
            llvm_emit(ctx, "  %%r%d = load %%LuaValue, %%LuaValue* %%r%d\n", arg_val_reg, call_a + 1 + arg);
            llvm_emit(ctx, "  store %%LuaValue %%r%d, %%LuaValue* %%r%d\n", arg_val_reg, arg_ptr_reg);
          }
          
          /* Cast to %LuaValue* for the call */
          args_cast_reg = ctx->reg_counter++;
          llvm_emit(ctx, "  %%r%d = bitcast [%d x %%LuaValue]* %%r%d to %%LuaValue*\n", 
                    args_cast_reg, nargs, args_reg);
          
          /* Make the function call */
          result_reg = ctx->reg_counter++;
          llvm_emit(ctx, "  %%r%d = call %%LuaValue @lua_call(%%LuaValue %%r%d, i32 %d, %%LuaValue* %%r%d)\n", 
                    result_reg, func_reg, nargs, args_cast_reg);
          
          /* Store result in register A if we expect results */
          if (nresults != 0) {
            llvm_emit(ctx, "  store %%LuaValue %%r%d, %%LuaValue* %%r%d\n", result_reg, call_a);
          }
        } else {
          /* No arguments */
          result_reg = ctx->reg_counter++;
          llvm_emit(ctx, "  %%r%d = call %%LuaValue @lua_call(%%LuaValue %%r%d, i32 0, %%LuaValue* null)\n", 
                    result_reg, func_reg);
          
          if (nresults != 0) {
            llvm_emit(ctx, "  store %%LuaValue %%r%d, %%LuaValue* %%r%d\n", result_reg, call_a);
          }
        }
      } else {
        /* Variable arguments - simplified version for now */
        llvm_emit(ctx, "  ; Variable argument function call (simplified)\n");
        result_reg = ctx->reg_counter++;
        llvm_emit(ctx, "  %%r%d = call %%LuaValue @lua_call(%%LuaValue %%r%d, i32 0, %%LuaValue* null)\n", 
                  result_reg, func_reg);
        
        if (nresults != 0) {
          llvm_emit(ctx, "  store %%LuaValue %%r%d, %%LuaValue* %%r%d\n", result_reg, call_a);
        }
      }
      break;
    }
    case OP_NEWTABLE: {
      int result_reg = ctx->reg_counter++;
      llvm_emit(ctx, "  %%r%d = call %%LuaValue @lua_newtable()\n", result_reg);
      break;
    }
    case OP_RETURN: {
      int b = GETARG_B(i);
      if (b == 1) {
        llvm_emit(ctx, "  ret %%LuaValue zeroinitializer\n");
      } else if (b == 2) {
        llvm_emit(ctx, "  ret %%LuaValue %%r%d\n", a);
      } else {
        llvm_emit(ctx, "  ret %%LuaValue %%r%d\n", a);
      }
      break;
    }
    default:
      /* Emit a comment for unhandled instructions */
      llvm_emit(ctx, "  ; TODO: Implement %s\n", opnames[op]);
      break;
  }
}

/*
** Generate runtime helper function declarations
*/
static void llvm_emit_runtime_decls(LLVMContext *ctx) {
  llvm_emit(ctx, "declare %%LuaValue @lua_add(%%LuaValue, %%LuaValue)\n");
  llvm_emit(ctx, "declare %%LuaValue @lua_sub(%%LuaValue, %%LuaValue)\n");
  llvm_emit(ctx, "declare %%LuaValue @lua_mul(%%LuaValue, %%LuaValue)\n");
  llvm_emit(ctx, "declare %%LuaValue @lua_div(%%LuaValue, %%LuaValue)\n");
  llvm_emit(ctx, "declare %%LuaValue @lua_newtable()\n");
  llvm_emit(ctx, "declare %%LuaValue @lua_call(%%LuaValue, i32, %%LuaValue*)\n");
  llvm_emit(ctx, "\n");
}

/*
** Main function to generate LLVM IR from a Lua Proto
*/
static LuaLLVMResult llvm_generate_from_proto(lua_State *L, Proto *p) {
  LLVMContext ctx;
  LuaLLVMResult result;
  
  llvm_init_context(&ctx, L, p);
  
  /* Emit module header and type definitions */
  llvm_emit(&ctx, "; Generated LLVM IR from Lua bytecode\n");
  llvm_emit(&ctx, "; Source: %s\n\n", getstr(p->source));
  
  llvm_emit_lua_value_type(&ctx);
  llvm_emit(&ctx, "\n");
  
  /* Emit runtime function declarations */
  llvm_emit_runtime_decls(&ctx);
  
  /* Generate main function */
  llvm_emit_function_start(&ctx, "lua_main");
  
  /* Process bytecode instructions */
  for (int pc = 0; pc < p->sizecode; pc++) {
    llvm_emit_instruction(&ctx, p->code[pc], pc);
  }
  
  llvm_emit_function_end(&ctx);
  
  /* Prepare result */
  if (ctx.error_status == LUA2LLVM_OK) {
    result.ir_code = cast_charp(luaM_malloc_(L, ctx.buffer_pos + 1, 0));
    memcpy(result.ir_code, ctx.buffer, ctx.buffer_pos);
    result.ir_code[ctx.buffer_pos] = '\0';
    result.ir_size = ctx.buffer_pos;
    result.status = LUA2LLVM_OK;
    result.error_msg = NULL;
  } else {
    result.ir_code = NULL;
    result.ir_size = 0;
    result.status = ctx.error_status;
    result.error_msg = ctx.error_msg;
  }
  
  llvm_free_context(&ctx);
  return result;
}

/*
** String reader structure (copied from lauxlib.c)
*/
typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;

static const char *getS (lua_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}

/*
** Compile Lua source code to LLVM IR
*/
LuaLLVMResult lua2llvmir_compile_source(lua_State *L, const char *source, 
                                        const char *chunkname) {
  LuaLLVMResult result;
  LoadS ls;
  int status;
  LClosure *cl;
  
  /* Try to load and compile the source */
  ls.s = source;
  ls.size = strlen(source);
  
  /* Use lua_load to parse the source */
  status = lua_load(L, getS, &ls, chunkname, "t");
  
  if (status != LUA_OK) {
    result.ir_code = NULL;
    result.ir_size = 0;
    result.status = LUA2LLVM_PARSE_ERROR;
    result.error_msg = "Failed to parse Lua source";
    return result;
  }
  
  /* Get the function from the stack */
  cl = clLvalue(s2v(L->top.p - 1));
  lua_pop(L, 1);  /* remove function from stack */
  
  /* Generate LLVM IR from the proto */
  result = llvm_generate_from_proto(L, cl->p);
  
  return result;
}

/*
** Compile a Lua Proto to LLVM IR
*/
LuaLLVMResult lua2llvmir_compile_proto(lua_State *L, Proto *p) {
  return llvm_generate_from_proto(L, p);
}

/*
** Free compilation result
*/
void lua2llvmir_free_result(LuaLLVMResult *result) {
  if (result->ir_code) {
    /* Note: In a real implementation, we should store the lua_State 
       to properly free with luaM_free, but for now we'll use free */
    free(result->ir_code);
    result->ir_code = NULL;
  }
  result->ir_size = 0;
  result->status = LUA2LLVM_OK;
  result->error_msg = NULL;
}