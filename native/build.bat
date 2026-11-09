@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "OUT=%~dp0..\dist\native"
if not exist "%OUT%" mkdir "%OUT%"

set "CXX="
where clang++ >nul 2>nul && set "CXX=clang++"
if not defined CXX where x86_64-w64-mingw32-g++ >nul 2>nul && set "CXX=x86_64-w64-mingw32-g++"
if not defined CXX where g++ >nul 2>nul && set "CXX=g++"

if not defined CXX (
  rem Bundled / common LLVM-MinGW locations
  for %%D in (
    "%~dp0..\tools\llvm-mingw"
    "%LocalAppData%\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe"
    "%ProgramFiles%\LLVM-MINGW"
    "%LocalAppData%\Programs\LLVM-MINGW"
  ) do (
    if exist "%%~D" (
      for /f "delims=" %%F in ('dir /s /b "%%~D\clang++.exe" 2^>nul') do (
        if not defined CXX set "CXX=%%F"
      )
    )
  )
)

if not defined CXX (
  echo [!] No clang++ / g++ found. Install LLVM-MinGW UCRT first:
  echo     winget install MartinStorsjo.LLVM-MinGW.UCRT
  exit /b 1
)

echo [+] CXX = %CXX%
echo [+] OUT = %OUT%

rem Embed mod/*.lua + Tick.lua.gz into the launcher.
set "PY="
where py >nul 2>nul && set "PY=py -3"
if not defined PY where python >nul 2>nul && set "PY=python"
if not defined PY if exist "%LocalAppData%\Programs\Python\Python314\python.exe" set "PY=%LocalAppData%\Programs\Python\Python314\python.exe"
if not defined PY if exist "%LocalAppData%\Python\bin\python.exe" set "PY=%LocalAppData%\Python\bin\python.exe"
if not defined PY (
  echo [!] Python required to embed Lua payloads into launcher
  exit /b 1
)
echo [*] Generating launcher payloads ...
%PY% "%~dp0..\tools\gen_launcher_payloads.py"
if errorlevel 1 (
  echo [!] payload generation failed
  exit /b 1
)

rem Statically link libc++ / libunwind / winpthread so no runtime DLLs are needed.
set "COMMON=-std=c++17 -O2 -DUNICODE -D_UNICODE -municode -fno-exceptions -fno-rtti"
set "STATIC=-static -Wl,-Bstatic -lc++ -lc++abi -lunwind -lwinpthread -Wl,-Bdynamic"
set "LIBS=-ld2d1 -ldwrite -limm32 -lole32 -luuid -lgdi32 -luser32 -lkernel32 -lshell32 -lwinmm"

echo [*] Building JourneyMos.dll ...
"%CXX%" %COMMON% -shared -o "%OUT%\JourneyMos.dll" ^
  JourneyMos\dllmain.cpp ^
  JourneyMos\bridge.cpp ^
  JourneyMos\menu.cpp ^
  JourneyMos\lang.cpp ^
  JourneyMos\anims_i18n.cpp ^
  JourneyMos\overlay.cpp ^
  JourneyMos\nametag.cpp ^
  JourneyMos\hudsnap.cpp ^
  JourneyMos\partner.cpp ^
  %LIBS% %STATIC% -Wl,--out-implib,"%OUT%\JourneyMos.dll.a"
if errorlevel 1 (
  echo [!] DLL build failed
  exit /b 1
)

echo [*] Building JourneyMosLauncher.exe ...
"%CXX%" %COMMON% -o "%OUT%\JourneyMosLauncher.exe" ^
  Launcher\main.cpp ^
  Launcher\deploy.cpp ^
  Launcher\embedded_payloads.cpp ^
  -luser32 -lkernel32 -lpsapi -lshell32 %STATIC%
if errorlevel 1 (
  echo [!] Launcher build failed
  exit /b 1
)

echo [*] Building dinput8.dll (Steam auto-load proxy) ...
"%CXX%" %COMMON% -shared -o "%OUT%\dinput8.dll" ^
  Proxy\dinput8_proxy.cpp ^
  Proxy\dinput8.def ^
  -luser32 -lkernel32 %STATIC%
if errorlevel 1 (
  echo [!] dinput8 proxy build failed
  exit /b 1
)

echo [+] OK
echo     %OUT%\JourneyMos.dll
echo     %OUT%\JourneyMosLauncher.exe
echo     %OUT%\dinput8.dll
exit /b 0
