@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start-phonecast-vr.ps1" -Diagnostic Visible
if errorlevel 1 pause
