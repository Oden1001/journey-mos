@echo off
chcp 65001 >nul
cd /d "%~dp0"
title Journey Mos Restore
echo.
echo   还原 Journey.exe.bak（纯原版，不装模组）
echo   用来确认游戏本身能否正常进主菜单
echo.
echo   请先完全关闭 Journey
echo.
where py >nul 2>nul
if %errorlevel%==0 (
  py -3 tools\install_tick_hook.py --restore
  goto :end
)
python tools\install_tick_hook.py --restore
:end
echo.
echo 还原完成。请手动从 Steam 启动 Journey 试一次。
echo 若原版也闪退，问题不在模组钩子。
pause
