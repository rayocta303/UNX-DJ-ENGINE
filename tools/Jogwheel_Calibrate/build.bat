@echo off
if "%ZIG%"=="" set ZIG=t:\zig\zig.exe
set CC=%ZIG% cc

echo ==========================================
echo   Building XDJ-UNX Jogwheel Calibrator
echo ==========================================
echo.
%CC% -O3 main.c -o calibrate_jogwheel.exe -lwinmm
if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    pause
    exit /b
)
echo [SUCCESS] Built calibrate_jogwheel.exe
echo.
pause
