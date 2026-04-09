@echo off
setlocal

echo ========================================
echo Setting up Visual Studio 2022 Environment...
echo ========================================

:: محاولة إيجاد vcvarsall.bat (مسار Visual Studio)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
) else (
    echo ERROR: Could not find Visual Studio 2022!
    echo Please install Visual Studio 2022 with C++ tools
    pause
    exit /b 1
)

echo Environment set up successfully!

echo.
echo ========================================
echo Cleaning old build...
echo ========================================

if exist build rmdir /s /q build
if exist CMakeCache.txt del CMakeCache.txt
if exist CMakeFiles rmdir /s /q CMakeFiles 2>nul

mkdir build
cd build

echo.
echo ========================================
echo Configuring with CMake...
echo ========================================

cmake .. -G "Visual Studio 17 2022" -A x64

if errorlevel 1 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Building Release...
echo ========================================

cmake --build . --config Release --parallel 4

if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo SUCCESS! Build completed.
echo ========================================

cd Release
if exist asm_core.exe (
    echo Running asm_core...
    asm_core.exe
) else (
    echo Executable: build\Release\asm_core.exe
)

pause