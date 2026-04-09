@echo off
echo ============================================================
echo    ASM Web Interface - Startup Script
echo ============================================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3.8+ from https://www.python.org/
    pause
    exit /b 1
)

echo [1/3] Checking dependencies...
pip show flask >nul 2>&1
if errorlevel 1 (
    echo Installing dependencies...
    pip install -r requirements.txt
    if errorlevel 1 (
        echo ERROR: Failed to install dependencies
        pause
        exit /b 1
    )
) else (
    echo Dependencies already installed
)

echo.
echo [2/3] Checking spaCy model...
python -m spacy download en_core_web_sm >nul 2>&1
echo spaCy model ready

echo.
echo [3/3] Starting ASM Web Server...
echo.
echo ============================================================
echo    Server will start at: http://localhost:5000
echo    Press Ctrl+C to stop the server
echo ============================================================
echo.

REM Start the Flask server
python app.py

pause
