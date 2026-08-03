@echo off
setlocal
echo ========================================================
echo   Building UNX-DJ Controller Debug Terminal...
echo ========================================================

if "%ZIG%"=="" set ZIG=t:\zig\zig.exe

if exist %ZIG% (
    echo Using Zig Compiler: %ZIG%
    %ZIG% cc -O2 -target x86_64-windows controller_debug.c -lwinmm -o controller_debug.exe
) else (
    echo Zig compiler not found at %ZIG%. Trying system gcc/clang...
    gcc -O2 controller_debug.c -lwinmm -o controller_debug.exe 2>nul || clang -O2 controller_debug.c -lwinmm -o controller_debug.exe
)

if exist controller_debug.exe (
    echo [SUCCESS] Built controller_debug.exe
    if "%1"=="run" (
        echo Running Controller Debug Terminal... 
        controller_debug.exe
    )
) else (
    echo [ERROR] Build failed.
    exit /b 1
)

endlocal
