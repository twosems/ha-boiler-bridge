@echo off
setlocal
chcp 65001 >nul
set "DIR=%~dp0"
set "PY=%DIR%.venv\Scripts\python.exe"
if not exist "%PY%" (
  echo ESPHome Python environment not found. Contact installer administrator.
  pause
  exit /b 3
)
"%PY%" "%DIR%installer.py"
if errorlevel 1 pause
