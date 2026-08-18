@echo off
echo ==========================================
echo   XDJ-UNX Jogwheel Calibrator Launcher
echo ==========================================
echo.
echo Checking for Python dependencies (mido, python-rtmidi)...
pip install mido python-rtmidi --quiet
if %errorlevel% neq 0 (
    echo [ERROR] Failed to install required Python libraries. Make sure you have python and pip installed.
    pause
    exit /b
)

echo.
py "%~dp0calibrate_jogwheel.py"
pause
