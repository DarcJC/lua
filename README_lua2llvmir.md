# Lua to LLVM IR Compiler (lua2llvmir)

This implementation adds a Lua-to-LLVM IR compiler to the standard Lua 5.5 distribution. The compiler can convert Lua source code into LLVM intermediate representation (IR), enabling potential optimization and compilation to native code.

## Features

- **Direct Lua Source Compilation**: Compile Lua source code directly to LLVM IR
- **Bytecode Translation**: Convert Lua bytecode instructions to LLVM IR operations  
- **Value Type Support**: Handle Lua's dynamic typing system with tagged union representation
- **Memory Management**: Proper integration with Lua's memory management system
- **Extensible Architecture**: Easy to add support for additional Lua opcodes

## Supported Lua Operations

Currently implemented:
- **Constants**: `nil`, `true`, `false`, integer literals
- **Arithmetic**: `+`, `-`, `*`, `/`, immediate addition (`ADDI`)
- **Variables**: Local variable assignment and access (`MOVE`)
- **Control Flow**: Function returns (`RETURN`)
- **Tables**: Table creation (`NEWTABLE`)

## API

### Core Functions

```c
LuaLLVMResult lua2llvmir_compile_source(lua_State *L, const char *source, const char *chunkname);
LuaLLVMResult lua2llvmir_compile_proto(lua_State *L, Proto *p);
void lua2llvmir_free_result(LuaLLVMResult *result);
```

### Result Structure

```c
typedef struct LuaLLVMResult {
  char *ir_code;          /* Generated LLVM IR code */
  size_t ir_size;         /* Size of IR code */
  int status;             /* Compilation status (0 = success) */
  const char *error_msg;  /* Error message if compilation failed */
} LuaLLVMResult;
```

## Usage Example

```c
#include "lua2llvmir.h"

lua_State *L = luaL_newstate();
const char *lua_code = "return 42";

LuaLLVMResult result = lua2llvmir_compile_source(L, lua_code, "example");

if (result.status == LUA2LLVM_OK) {
    printf("Generated LLVM IR:\n%s\n", result.ir_code);
} else {
    printf("Error: %s\n", result.error_msg);
}

lua2llvmir_free_result(&result);
lua_close(L);
```

## LLVM IR Output Format

The compiler generates LLVM IR with:

- **Value Representation**: Lua values as `{i32, [8 x i8]}` structs (type tag + data)
- **Runtime Functions**: Declarations for Lua operations (`@lua_add`, `@lua_sub`, etc.)
- **Function Signature**: `%LuaValue @lua_main(i32 %argc, %LuaValue* %args)`

### Example Output

Input Lua:
```lua
local a = 10
local b = 20
return a + b
```

Generated LLVM IR:
```llvm
%LuaValue = type { i32, [8 x i8] }

declare %LuaValue @lua_add(%LuaValue, %LuaValue)

define %LuaValue @lua_main(i32 %argc, %LuaValue* %args) {
entry:
  ; Load constants
  %r0 = insertvalue %LuaValue zeroinitializer, i32 3, 0
  %r0_val = bitcast i64 10 to [8 x i8]
  %r0_final = insertvalue %LuaValue %r0, [8 x i8] %r0_val, 1
  
  %r1 = insertvalue %LuaValue zeroinitializer, i32 3, 0
  %r1_val = bitcast i64 20 to [8 x i8]
  %r1_final = insertvalue %LuaValue %r1, [8 x i8] %r1_val, 1
  
  ; Perform addition
  %r2 = call %LuaValue @lua_add(%LuaValue %r0, %LuaValue %r1)
  
  ; Return result
  ret %LuaValue %r2
}
```

## Building

The lua2llvmir module is automatically included when building Lua:

```bash
make clean
make
```

To test the implementation:

```bash
gcc -o test_lua2llvmir test_lua2llvmir.c liblua.a -lm -ldl
./test_lua2llvmir
```

## Architecture

### Components

1. **lua2llvmir.h**: Public API and type definitions
2. **lua2llvmir.c**: Core implementation
3. **LLVMContext**: Internal compilation context
4. **Buffer Management**: Dynamic string building for IR generation
5. **Opcode Translation**: Mapping from Lua bytecode to LLVM IR

### Integration Points

- Uses Lua's standard parser (`lua_load`) for source processing
- Integrates with Lua's memory management (`luaM_*` functions)
- Leverages existing opcode definitions (`lopcodes.h`, `lopnames.h`)

## Extension Points

To add support for additional Lua operations:

1. Add new case in `llvm_emit_instruction()` for the opcode
2. Implement the LLVM IR generation logic
3. Add any required runtime function declarations
4. Update tests to verify the new functionality

## Limitations

- Not all Lua opcodes are currently implemented
- Requires runtime support functions for execution
- Does not include LLVM optimization passes
- Memory management integration could be enhanced

## Future Enhancements

- Complete opcode coverage
- LLVM optimization integration  
- Direct native code generation
- Performance benchmarking
- Error handling improvements
- Support for Lua metatables and coroutines