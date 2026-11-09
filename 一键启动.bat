@echo off
cd /d "%~dp0"
title Journey Mos
echo.
echo   Journey Mos Launcher
echo   F8 = in-game menu
echo   --------------------
echo.

set "PY="
if exist "%LocalAppData%\Python\bin\python.exe" set "PY=%LocalAppData%\Python\bin\python.exe"
if not defined PY if exist "%LocalAppData%\Programs\Python\Python314\python.exe" set "PY=%LocalAppData%\Programs\Python\Python314\python.exe"
if not defined PY if exist "%LocalAppData%\Programs\Python\Python313\python.exe" set "PY=%LocalAppData%\Programs\Python\Python313\python.exe"
if not defined PY if exist "%LocalAppData%\Programs\Python\Python312\python.exe" set "PY=%LocalAppData%\Programs\Python\Python312\python.exe"

if not defined PY (
  where py >nul 2>nul
  if not errorlevel 1 set "PY_CMD=py -3"
)
if not defined PY if not defined PY_CMD (
  where python >nul 2>nul
  if not errorlevel 1 set "PY_CMD=python"
)

if not defined PY if not defined PY_CMD (
  echo [!] Python not found. Install Python 3 and check Add to PATH.
  echo     https://www.python.org/downloads/
  goto :hold
)

echo [+] starting...
echo.

if defined PY (
  "%PY%" -u tools\launch.py %*
) else (
  %PY_CMD% -u tools\launch.py %*
)
set "ERR=%ERRORLEVEL%"

echo.
if not "%ERR%"=="0" (
  echo [!] exit code %ERR%
  if exist launch_error.txt (
    echo ---- launch_error.txt ----
    type launch_error.txt
    echo --------------------------
  )
)

:hold
echo.
echo Press any key to close...
pause >nul
