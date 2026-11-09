@echo off
cd /d "%~dp0"
title Journey Mos Repair
echo.
echo   Repair: restore bak + reinstall safe Tick hook
echo   Close Journey.exe first.
echo.
pause

set "PY="
if exist "%LocalAppData%\Python\bin\python.exe" set "PY=%LocalAppData%\Python\bin\python.exe"
if not defined PY (
  where py >nul 2>nul
  if not errorlevel 1 set "PY_CMD=py -3"
)
if not defined PY if not defined PY_CMD set "PY_CMD=python"

if defined PY (
  "%PY%" -u tools\launch.py --repair --no-game --no-menu
) else (
  %PY_CMD% -u tools\launch.py --repair --no-game --no-menu
)

echo.
echo Next: run 一键启动.bat
echo.
pause
