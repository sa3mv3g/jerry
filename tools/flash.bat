@echo off
:: Copyright (c) 2026
:: All rights reserved.
::
:: Jerry Firmware Production Flash Launcher
::
:: One-click script for production-line programming of NUCLEO-H563ZI boards.
::
:: Prerequisites:
::   - Python 3.10+ installed and on PATH
::   - STM32CubeCLT or STM32CubeProgrammer installed
::     (STM32_Programmer_CLI must be on PATH or in a standard install location)
::   - Board connected via USB (ST-LINK)
::   - Board option bytes already configured (TrustZone pre-configured)
::
:: Usage: Double-click this file in Windows Explorer.

:: Change to the directory containing this script so that flash_nucleo.py
:: can find the ELF files (jerry_app.elf, jerry_secure_app.elf) next to it.
cd /d "%~dp0"

echo.
echo ============================================================
echo   Jerry Firmware Production Flash
echo ============================================================
echo.
echo   Secure app  : jerry_secure_app.elf
echo   Non-secure  : jerry_app.elf
echo.
echo   Connect the board via USB and press any key to start...
echo   (Press Ctrl+C to cancel)
echo.
pause >nul

echo.
echo Starting flash process...
echo.

python flash_nucleo.py --skip-option-bytes --force

if %ERRORLEVEL% neq 0 (
    echo.
    echo ============================================================
    echo   ERROR: Flashing FAILED!
    echo   Review the output above for details.
    echo ============================================================
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo   SUCCESS: Board programmed successfully!
echo ============================================================
echo.
pause
exit /b 0
