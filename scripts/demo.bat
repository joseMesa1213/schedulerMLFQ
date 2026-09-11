@echo off
REM demo.bat - Lanza la demostracion de PowerShell sin cambiar la politica de
REM ejecucion del sistema. Se puede ejecutar con doble clic o desde cmd.exe:
REM
REM   scripts\demo.bat
REM   scripts\demo.bat -Pausa
REM
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo.ps1" %*
if errorlevel 1 pause
