@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\start-phonecast-vr.ps1" -Diagnostic Baseline
if errorlevel 1 pause
