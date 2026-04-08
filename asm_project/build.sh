#!/bin/bash
# Build script for ASM Core Engine (Linux/Mac)

echo "========================================"
echo "Building ASM Core Engine"
echo "========================================"

cd "$(dirname "$0")"

mkdir -p build
cd build

echo "Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

echo ""
echo "Building..."
cmake --build . --config Release

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "========================================"
echo "Build successful!"
echo "========================================"
echo ""
echo "Run the following to test:"
echo "  ./build/asm_core --test"
echo "  ./build/asm_core --benchmark"
echo "  ./build/asm_core"
echo ""
