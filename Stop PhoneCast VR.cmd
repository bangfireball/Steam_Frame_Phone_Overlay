@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\stop-phonecast-vr.ps1"
timeout /t 3 /nobreak >nul
