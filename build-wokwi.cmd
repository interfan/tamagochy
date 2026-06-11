@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-wokwi.ps1"
exit /b %ERRORLEVEL%
