@echo off
cd /d "%~dp0"
title Journey Mos — inject menu
echo.
echo   Deploy EXE+DLL into Journey folder, then inject (F8)
echo   (starts game if not running)
echo.

set "PY="
if exist "%LocalAppData%\Python\bin\python.exe" set "PY=%LocalAppData%\Python\bin\python.exe"
if not defined PY if exist "%LocalAppData%\Programs\Python\Python314\python.exe" set "PY=%LocalAppData%\Programs\Python\Python314\python.exe"
if not defined PY (
  where py >nul 2>nul
  if not errorlevel 1 set "PY_CMD=py -3"
)
if not defined PY if not defined PY_CMD set "PY_CMD=python"

if defined PY (
  "%PY%" -u tools\launch.py --skip-hook %*
) else (
  %PY_CMD% -u tools\launch.py --skip-hook %*
)

echo.
pause
