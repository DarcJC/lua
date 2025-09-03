#!/bin/bash
# Test script for lua2llvmir tool

set -e  # Exit on any error

echo "=== Lua to LLVM IR Compiler Test ==="
echo

# Test 1: Simple print statement
echo "Test 1: Simple hello world"
cat > test1.lua << 'EOF'
print("Hello from lua2llvmir!")
EOF

echo "Lua source:"
cat test1.lua
echo

echo "Lua bytecode:"
./luac -l test1.lua
echo

echo "Generating LLVM IR..."
./lua2llvmir -v test1.lua
echo

echo "Generated LLVM IR (first 30 lines):"
head -30 test1.ll
echo "..."
echo

# Test 2: Variables and arithmetic
echo "Test 2: Variables and arithmetic"
cat > test2.lua << 'EOF'
local x = 100
local y = 23
local message = "The answer is"
local result = x + y - 23
print(message, result)
return result
EOF

echo "Lua source:"
cat test2.lua
echo

echo "Lua bytecode:"
./luac -l test2.lua
echo

echo "Generating LLVM IR..."
./lua2llvmir test2.lua
echo

echo "Verifying LLVM IR compiles..."
if llvm-as-14 test1.ll -o test1.bc && llvm-as-14 test2.ll -o test2.bc; then
    echo "✅ All LLVM IR files compile successfully!"
else
    echo "❌ LLVM IR compilation failed"
    exit 1
fi

echo
echo "=== Test Summary ==="
echo "✅ Lua parsing and bytecode generation working"
echo "✅ LLVM IR generation working"
echo "✅ Generated IR compiles with LLVM"
echo "🚧 Runtime execution requires Lua runtime library (future work)"
echo
echo "Files generated:"
ls -la *.lua *.ll *.bc 2>/dev/null | grep -E '\.(lua|ll|bc)$' || true

# Clean up test files
rm -f test1.lua test1.ll test1.bc test2.lua test2.ll test2.bc

echo
echo "Test completed successfully! 🎉"