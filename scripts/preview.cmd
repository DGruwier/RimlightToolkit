@echo off
setlocal

set "ROOT=%~dp0.."
set "OUT=%ROOT%\out\preview.png"

if "%~1"=="" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\rtk.ps1" run
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\rtk.ps1" run --input "%~1" --out "%OUT%"
)

if errorlevel 1 (
  pause
  exit /b %errorlevel%
)

echo Wrote "%OUT%"
pause
