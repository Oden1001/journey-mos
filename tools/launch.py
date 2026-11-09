#!/usr/bin/env python3
"""One-click Journey Mos launcher.

- Find Journey install
- Copy Lua scripts
- Install Tick hook if missing
- Start the game
- Inject in-game Direct2D menu (F8)
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MOD = ROOT / "mod"
NATIVE_DIST = ROOT / "dist" / "native"
MARKER = ".journey_mos"
HOOK_VERSION = "v5-once"


def _import_install():
    sys.path.insert(0, str(ROOT / "tools"))
    import install_tick_hook as ith  # noqa: WPS433
    return ith


def find_game_dir(explicit: Path | None) -> Path:
    ith = _import_install()
    if explicit:
        p = explicit
        if not (p / "Journey.exe").exists():
            raise SystemExit(f"Journey.exe not found in {p}")
        return p
    found = ith.default_game_dirs()
    if not found:
        raise SystemExit(
            "找不到 Journey 安装目录。请用：\n"
            '  一键启动.bat --game-dir "你的\\Journey路径"'
        )
    return found[0]


def deploy_scripts(game_dir: Path) -> None:
    names = [
        "StartLua.lua",
        "TickHook.lua",
        "GameTick.lua",
        "features.lua",
        "config.lua",
        "commands.lua",
        "RunOnce.lua",
    ]
    for name in names:
        src = MOD / name
        if not src.exists():
            print(f"[!] missing {src}")
            continue
        shutil.copy2(src, game_dir / name)
    print(f"[+] scripts -> {game_dir}")


def _decompress_gzip_member(blob: bytes) -> bytes:
    """Decompress first gzip member even if the buffer has trailing padding."""
    # 16 + MAX_WBITS = gzip wrapper; don't rstrip zeros (breaks CRC/isize)
    return zlib.decompress(blob, 16 + zlib.MAX_WBITS)


def marker_version(game_dir: Path) -> str:
    path = game_dir / MARKER
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8", errors="ignore").strip()


def tick_hook_installed(game_dir: Path) -> bool:
    # Require current safe hook version
    if marker_version(game_dir) == HOOK_VERSION:
        return True

    exe = game_dir / "Journey.exe"
    if not exe.exists():
        return False

    data = exe.read_bytes()
    ith = _import_install()
    slots = ith.find_tick_slots(data)
    if not slots:
        return False

    slots.sort(key=lambda s: s[1] - s[0], reverse=True)
    start, end = slots[0]
    blob = bytes(data[start:end])
    try:
        text = _decompress_gzip_member(blob).decode("utf-8", errors="ignore")
    except Exception:
        return False
    # v2 marker string inside Tick.lua
    return ("TickHook.lua" in text) and ("mos_boot" in text)


def ensure_hook(game_dir: Path, force: bool = False) -> None:
    try:
        installed = (not force) and tick_hook_installed(game_dir)
    except Exception as exc:  # noqa: BLE001
        print(f"[!] hook check failed ({exc}); will reinstall")
        installed = False

    if installed:
        print("[+] Tick hook already installed")
        return

    if journey_running():
        raise SystemExit(
            "[!] Journey.exe 正在运行，无法写入钩子。\n"
            "    请先关掉游戏，再双击「一键启动.bat」。"
        )

    ith = _import_install()
    bak = game_dir / "Journey.exe.bak"
    # If an older broken patch exists, restore vanilla first then patch cleanly
    if bak.exists() and marker_version(game_dir) != HOOK_VERSION:
        print("[*] Restoring Journey.exe.bak then installing safe hook...")
        ith.restore(game_dir)
    else:
        print("[*] Installing Tick hook into Journey.exe...")

    ith.install(game_dir, MOD / "Tick.lua", backup=True)
    (game_dir / MARKER).write_text(HOOK_VERSION + "\n", encoding="utf-8")
    print("[+] Tick hook installed (", HOOK_VERSION, ")")


def journey_running() -> bool:
    try:
        out = subprocess.check_output(
            ["tasklist", "/FI", "IMAGENAME eq Journey.exe"],
            text=True,
            errors="ignore",
        )
        return "Journey.exe" in out
    except Exception:
        return False


def start_game(game_dir: Path, direct: bool = False) -> None:
    if journey_running():
        print("[+] Journey already running")
        return

    # Journey PC often crashes after TGC splash if started as bare Journey.exe
    # without Steam's launch context. Prefer steam:// and keep exe read-only so
    # Steam cannot silently restore an unpatched binary over our hook.
    appid_file = game_dir / "steam_appid.txt"
    appid = ""
    if appid_file.exists():
        appid = appid_file.read_text(encoding="utf-8", errors="ignore").strip().splitlines()[0].strip()

    if (not direct) and appid.isdigit():
        exe = game_dir / "Journey.exe"
        try:
            os.chmod(exe, 0o444)
        except OSError:
            pass
        print(f"[*] Launching via Steam (appid {appid})")
        print("    Journey.exe is read-only to protect the Tick hook")
        os.startfile(f"steam://rungameid/{appid}")  # type: ignore[attr-defined]
        return

    exe = game_dir / "Journey.exe"
    print(f"[*] Launching {exe} DIRECT (may crash after TGC on some setups)")
    subprocess.Popen([str(exe)], cwd=str(game_dir), close_fds=True)


def native_paths() -> tuple[Path, Path]:
    dll = NATIVE_DIST / "JourneyMos.dll"
    launcher = NATIVE_DIST / "JourneyMosLauncher.exe"
    return dll, launcher


def ensure_native_built() -> tuple[Path, Path]:
    dll, launcher = native_paths()
    if dll.exists() and launcher.exists():
        return dll, launcher
    build_bat = ROOT / "native" / "build.bat"
    if not build_bat.exists():
        raise SystemExit(f"missing {build_bat}")
    print("[*] Building native menu (first run)...")
    rc = subprocess.call(["cmd", "/c", str(build_bat)], cwd=str(ROOT / "native"))
    if rc != 0 or not dll.exists() or not launcher.exists():
        raise SystemExit(
            "native build failed. Install LLVM-MinGW then run native\\build.bat:\n"
            "  winget install MartinStorsjo.LLVM-MinGW.UCRT"
        )
    return dll, launcher


def deploy_native(game_dir: Path) -> tuple[Path, Path]:
    """Copy EXE+DLL into the Journey install folder (sidecar layout)."""
    src_dll, src_launcher = ensure_native_built()
    dst_dll = game_dir / "JourneyMos.dll"
    dst_launcher = game_dir / "JourneyMosLauncher.exe"
    shutil.copy2(src_dll, dst_dll)
    shutil.copy2(src_launcher, dst_launcher)
    print(f"[+] native -> {game_dir}")
    print(f"    {dst_dll.name}")
    print(f"    {dst_launcher.name}")
    return dst_dll, dst_launcher


def inject_menu(game_dir: Path, start_game_if_needed: bool = True) -> None:
    _, launcher = deploy_native(game_dir)
    # Launcher auto-loads JourneyMos.dll from the same folder.
    args = [str(launcher)]
    if not start_game_if_needed:
        args.append("--no-start")
    rc = subprocess.call(args, cwd=str(game_dir))
    if rc != 0:
        raise SystemExit(f"menu inject failed (exit {rc})")
    print("[+] In-game menu ready — press F8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Journey Mos one-click launcher")
    parser.add_argument("--game-dir", type=Path, default=None)
    parser.add_argument("--no-game", action="store_true", help="Don't launch Journey")
    parser.add_argument("--no-menu", action="store_true", help="Don't inject in-game menu")
    parser.add_argument("--skip-hook", action="store_true", help="Only sync scripts")
    parser.add_argument("--repair", action="store_true", help="Force restore.bak + reinstall safe hook")
    parser.add_argument("--steam", action="store_true", help="Launch via steam:// instead of direct exe")
    parser.add_argument("--direct", action="store_true", help="Launch Journey.exe directly (default)")
    args = parser.parse_args()

    os.chdir(ROOT)
    print("======== Journey Mos ========")
    game_dir = find_game_dir(args.game_dir)
    print(f"[+] game  {game_dir}")

    deploy_scripts(game_dir)
    if not args.skip_hook:
        ensure_hook(game_dir, force=args.repair)
        deploy_scripts(game_dir)

    # Always keep EXE+DLL next to Journey.exe
    deploy_native(game_dir)

    if args.no_menu:
        if not args.no_game:
            start_game(game_dir, direct=not args.steam)
        print("[+] done (no menu inject)")
        return 0

    # Injector lives in the game folder: auto-finds JourneyMos.dll beside itself,
    # starts Journey if needed, then injects.
    inject_menu(game_dir, start_game_if_needed=not args.no_game)
    print("[+] done — press F8 in-game; you can close this window")
    return 0


if __name__ == "__main__":
    err_path = ROOT / "launch_error.txt"
    try:
        if err_path.exists():
            err_path.unlink()
    except Exception:
        pass
    try:
        raise SystemExit(main())
    except SystemExit as exc:
        code = exc.code if isinstance(exc.code, int) else (1 if exc.code else 0)
        if code:
            msg = str(exc.code if not isinstance(exc.code, int) else exc)
            print(f"[!] SystemExit: {exc.code}")
            try:
                err_path.write_text(f"SystemExit: {exc.code}\n", encoding="utf-8")
            except Exception:
                pass
        raise
    except Exception as exc:  # noqa: BLE001
        import traceback

        tb = traceback.format_exc()
        print(tb)
        try:
            err_path.write_text(tb, encoding="utf-8")
        except Exception:
            pass
        print("[!] 已写入 launch_error.txt")
        try:
            input("按回车键退出...")
        except Exception:
            time.sleep(8)
        raise SystemExit(1) from exc
