@echo off
setlocal

set "ROOT=%~dp0.."
set "OUT=%ROOT%\out\preview.png"

if "%~1"=="" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\rtk.ps1" gui
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\rtk.ps1" gui "%~1"
)

if errorlevel 1 (
  pause
  exit /b %errorlevel%
)

echo Wrote "%OUT%"
pause
