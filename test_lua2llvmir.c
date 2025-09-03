/*
** Simple test program for lua2llvmir
*/

#include <stdio.h>
#include <stdlib.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lua2llvmir.h"

static void test_simple_lua_to_llvm(void) {
  lua_State *L;
  LuaLLVMResult result;
  const char *simple_lua = "return 42";
  
  printf("=== Testing Lua to LLVM IR Compilation ===\n");
  printf("Input Lua code: %s\n\n", simple_lua);
  
  /* Create Lua state */
  L = luaL_newstate();
  if (L == NULL) {
    printf("Error: Cannot create Lua state\n");
    return;
  }
  
  /* Compile Lua source to LLVM IR */
  result = lua2llvmir_compile_source(L, simple_lua, "test");
  
  if (result.status == LUA2LLVM_OK) {
    printf("Compilation successful!\n");
    printf("Generated LLVM IR:\n");
    printf("==================\n");
    printf("%s\n", result.ir_code);
    printf("==================\n");
    printf("IR size: %zu bytes\n", result.ir_size);
  } else {
    printf("Compilation failed!\n");
    printf("Error: %s\n", result.error_msg ? result.error_msg : "Unknown error");
  }
  
  /* Cleanup */
  lua2llvmir_free_result(&result);
  lua_close(L);
}

static void test_complex_lua_to_llvm(void) {
  lua_State *L;
  LuaLLVMResult result;
  const char *complex_lua = 
    "local a = 10\n"
    "local b = 20\n"
    "local c = a + b\n"
    "local d = c * 2\n"
    "return d\n";
  
  printf("\n=== Testing Complex Lua to LLVM IR Compilation ===\n");
  printf("Input Lua code:\n%s\n", complex_lua);
  
  /* Create Lua state */
  L = luaL_newstate();
  if (L == NULL) {
    printf("Error: Cannot create Lua state\n");
    return;
  }
  
  /* Compile Lua source to LLVM IR */
  result = lua2llvmir_compile_source(L, complex_lua, "complex_test");
  
  if (result.status == LUA2LLVM_OK) {
    printf("Compilation successful!\n");
    printf("Generated LLVM IR:\n");
    printf("==================\n");
    printf("%s\n", result.ir_code);
    printf("==================\n");
    printf("IR size: %zu bytes\n", result.ir_size);
  } else {
    printf("Compilation failed!\n");
    printf("Error: %s\n", result.error_msg ? result.error_msg : "Unknown error");
  }
  
  /* Cleanup */
  lua2llvmir_free_result(&result);
  lua_close(L);
}

static void test_table_lua_to_llvm(void) {
  lua_State *L;
  LuaLLVMResult result;
  const char *table_lua = 
    "local t = {}\n"
    "t[1] = 42\n"
    "return t[1]\n";
  
  printf("\n=== Testing Table Lua to LLVM IR Compilation ===\n");
  printf("Input Lua code:\n%s\n", table_lua);
  
  /* Create Lua state */
  L = luaL_newstate();
  if (L == NULL) {
    printf("Error: Cannot create Lua state\n");
    return;
  }
  
  /* Compile Lua source to LLVM IR */
  result = lua2llvmir_compile_source(L, table_lua, "table_test");
  
  if (result.status == LUA2LLVM_OK) {
    printf("Compilation successful!\n");
    printf("Generated LLVM IR:\n");
    printf("==================\n");
    printf("%s\n", result.ir_code);
    printf("==================\n");
    printf("IR size: %zu bytes\n", result.ir_size);
  } else {
    printf("Compilation failed!\n");
    printf("Error: %s\n", result.error_msg ? result.error_msg : "Unknown error");
  }
  
  /* Cleanup */
  lua2llvmir_free_result(&result);
  lua_close(L);
}

int main(void) {
  printf("Lua to LLVM IR Compiler Test\n");
  printf("=============================\n\n");
  
  test_simple_lua_to_llvm();
  test_complex_lua_to_llvm();
  test_table_lua_to_llvm();
  
  printf("\nTest completed.\n");
  return 0;
}