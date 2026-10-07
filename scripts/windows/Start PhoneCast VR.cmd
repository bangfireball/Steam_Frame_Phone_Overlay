@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\start-phonecast-vr.ps1"
if errorlevel 1 (
  echo.
  echo PhoneCast failed to start.
  pause
) else (
  timeout /t 4 /nobreak >nul
)
