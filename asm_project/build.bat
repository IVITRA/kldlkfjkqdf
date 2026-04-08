@echo off
REM Build script for ASM Core Engine (Windows)

echo ========================================
echo Building ASM Core Engine
echo ========================================

cd /d "%~dp0"

if not exist build mkdir build
cd build

echo Running CMake...
echo Trying Visual Studio 2022 generator...
cmake .. -G "Visual Studio 17 2022" -A x64

if %errorlevel% neq 0 (
    echo Visual Studio 2022 not found, trying Visual Studio 2019...
    cmake .. -G "Visual Studio 16 2019" -A x64
)

if %errorlevel% neq 0 (
    echo Visual Studio not found, trying MinGW...
    cmake .. -G "MinGW Makefiles"
)

if %errorlevel% neq 0 (
    echo.
    echo ERROR: No suitable CMake generator found!
    echo.
    echo Please install one of the following:
    echo   1. Visual Studio 2022 (Community Edition is free)
    echo      https://visualstudio.microsoft.com/
    echo   2. Visual Studio 2019
    echo   3. MinGW-w64
    echo      https://www.mingw-w64.org/
    echo.
    echo After installation, run this script again.
    echo.
    pause
    exit /b 1
)

echo.
echo Building...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build successful!
echo ========================================
echo.
echo Run the following to test:
echo   cd build\Release
echo   asm_core.exe --test
echo   asm_core.exe --benchmark
echo   asm_core.exe
echo.
echo Or simply double-click the executable in: build\Release\asm_core.exe
echo.
pause
