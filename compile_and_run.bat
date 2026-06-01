@echo off
title DSA Sorting Visualizer - C++ Backend
color 0A

echo ==========================================
echo   DSA Sorting Visualizer - C++ Backend
echo ==========================================
echo.

REM ── Check if g++ is installed ──
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    color 0C
    echo [ERROR] g++ not found!
    echo.
    echo Please install MinGW-w64:
    echo   1. Go to: https://winlibs.com
    echo   2. Download the latest GCC for Windows
    echo   3. Extract and add its "bin" folder to PATH
    echo      e.g. C:\mingw64\bin
    echo.
    echo Or install via MSYS2:
    echo   pacman -S mingw-w64-x86_64-gcc
    echo.
    pause
    exit /b 1
)

echo [1/3] g++ found:
g++ --version | findstr /C:"g++"
echo.

REM ── Compile ──
echo [2/3] Compiling server.cpp ...
g++ -o server.exe server.cpp -lws2_32 -std=c++17 -O2

if %errorlevel% neq 0 (
    color 0C
    echo.
    echo [ERROR] Compilation failed. Check the error above.
    pause
    exit /b 1
)

echo [OK]  Compiled successfully: server.exe
echo.

REM ── Run ──
echo [3/3] Starting server...
echo.
echo  Open frontend\index.html in your browser after this.
echo  Press Ctrl+C here to stop the server.
echo.
server.exe

pause
