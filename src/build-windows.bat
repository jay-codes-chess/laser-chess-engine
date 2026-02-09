@echo off
REM Human Chess Engine - Windows Build Script
REM Requires: MinGW-w64 or Visual Studio with C++ compiler

echo Building Human Chess Engine for Windows...

cd /d "%~dp0"

IF EXIST "src\laser.exe" DEL "src\laser.exe"

cd src

REM Option 1: Using MinGW (if installed)
IF EXIST "C:\MinGW\bin\mingw32-make.exe" (
    echo Using MinGW...
    mingw32-make clean
    mingw32-make
    GOTO BUILD_DONE
)

REM Option 2: Using g++ from command line
where /q g++
IF %ERRORLEVEL% EQU 0 (
    echo Using g++...
    g++ -c -O3 -std=c++11 bbinit.cpp -o bbinit.o
    g++ -c -O3 -std=c++11 board.cpp -o board.o
    g++ -c -O3 -std=c++11 common.cpp -o common.o
    g++ -c -O3 -std=c++11 eval.cpp -o eval.o
    g++ -c -O3 -std=c++11 hash.cpp -o hash.o
    g++ -c -O3 -std=c++11 search.cpp -o search.o
    g++ -c -O3 -std=c++11 moveorder.cpp -o moveorder.o
    g++ -c -O3 -std=c++11 uci.cpp -o uci.o
    g++ -c -O3 -std=c++11 -x c++ -I. syzygy/stub.cpp -o syzygy/tbprobe.o
    g++ -O3 -o laser.exe bbinit.o board.o common.o eval.o hash.o search.o moveorder.o syzygy/tbprobe.o human_eval.o uci.o -lpthread
    GOTO BUILD_DONE
)

REM Option 3: Visual Studio
IF EXIST "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.32\bin\Hostx64\x64\cl.exe" (
    echo Using Visual Studio 2022...
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.bat"
    cl /O2 /EHsc /std:c++17 /MD /I. bbinit.cpp board.cpp common.cpp eval.cpp hash.cpp search.cpp moveorder.cpp uci.cpp syzygy/tbcore.c /Fe:laser.exe /link pthread.lib
    GOTO BUILD_DONE
)

echo.
echo ERROR: No C++ compiler found!
echo Please install MinGW-w64 or Visual Studio with C++ workload.
echo Download MinGW: https://www.mingw-w64.org/
echo Download VS: https://visualstudio.microsoft.com/downloads/
exit /b 1

:BUILD_DONE
IF EXIST "laser.exe" (
    echo.
    echo Build SUCCESS!
    echo Binary: src\laser.exe
    echo.
    echo To run:
    echo   src\laser.exe
    echo.
    echo Or use with chess GUI (Arena, ChessBase, etc.)
) ELSE (
    echo Build FAILED!
    exit /b 1
)
