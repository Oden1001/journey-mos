#!/usr/bin/env python3
"""Embed mod/Tick.lua (gzipped) into Journey.exe Tick.lua slot.

Based on Alazar's HxD workflow:
https://thatgamecompanyfan.boards.net/thread/541/pc-lua-script-editing-injection

Usage:
  python tools/install_tick_hook.py
  python tools/install_tick_hook.py --game-dir "D:\\Steam\\steamapps\\common\\Journey"
  python tools/install_tick_hook.py --restore
"""

from __future__ import annotations

import argparse
import os
import shutil
import stat
import struct
import sys
import zlib
from pathlib import Path


GZIP_MAGIC = b"\x1f\x8b\x08\x08"
TICK_MARKER = b"Tick.lua"


def project_root() -> Path:
    return Path(__file__).resolve().parents[1]


def default_game_dirs() -> list[Path]:
    roots = [
        Path(r"D:\Steam\steamapps\common\Journey"),
        Path(r"C:\Program Files (x86)\Steam\steamapps\common\Journey"),
        Path(r"C:\Program Files\Steam\steamapps\common\Journey"),
        Path(r"C:\Program Files\Epic Games\Journey"),
    ]
    return [p for p in roots if (p / "Journey.exe").exists()]


def find_tick_slots(data: bytes) -> list[tuple[int, int]]:
    """Return list of (gzip_start, gzip_end_exclusive) for Tick.lua.gz blobs."""
    slots: list[tuple[int, int]] = []
    search_from = 0
    while True:
        marker = data.find(TICK_MARKER, search_from)
        if marker < 0:
            break
        # Prefer gzip header shortly before the filename marker
        window_start = max(0, marker - 64)
        gz = data.rfind(GZIP_MAGIC, window_start, marker + 1)
        if gz < 0:
            gz = data.find(GZIP_MAGIC, marker)
        if gz < 0:
            search_from = marker + len(TICK_MARKER)
            continue
        # Next gzip member ends this slot (Journey packs many .lua.gz back-to-back)
        next_gz = data.find(GZIP_MAGIC, gz + 4)
        if next_gz < 0:
            search_from = marker + len(TICK_MARKER)
            continue
        # Heuristic: Tick slot is typically a few KB
        size = next_gz - gz
        if 200 <= size <= 8192:
            slots.append((gz, next_gz))
        search_from = marker + len(TICK_MARKER)
    # Deduplicate
    uniq = []
    seen = set()
    for s in slots:
        if s not in seen:
            uniq.append(s)
            seen.add(s)
    return uniq


def make_tick_gz(tick_lua: Path, original_name: bytes = b"Tick.lua") -> bytes:
    """Build Tick.lua.gz.

    Prefer 7za (community/HxD tutorial style). Fall back to zlib gzip.
    """
    seven = project_root() / "tools" / "bin" / "7za.exe"
    if seven.exists():
        import subprocess
        import tempfile

        raw = tick_lua.read_bytes()
        if raw.startswith(b"\xef\xbb\xbf"):
            raw = raw[3:]
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            src = td_path / "Tick.lua"
            src.write_bytes(raw)
            gz_path = td_path / "Tick.lua.gz"
            subprocess.check_call(
                [str(seven), "a", "-tgzip", "-y", str(gz_path), str(src)],
                cwd=td,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            return gz_path.read_bytes()

    # Fallback zlib gzip
    raw = tick_lua.read_bytes()
    if raw.startswith(b"\xef\xbb\xbf"):
        raw = raw[3:]
    filename = original_name if original_name.endswith(b"\x00") else original_name + b"\x00"
    header = b"\x1f\x8b\x08\x08" + struct.pack("<I", 0) + b"\x00\x00" + filename
    compressor = zlib.compressobj(9, zlib.DEFLATED, -zlib.MAX_WBITS)
    body = compressor.compress(raw) + compressor.flush()
    trailer = struct.pack("<II", zlib.crc32(raw) & 0xFFFFFFFF, len(raw) & 0xFFFFFFFF)
    return header + body + trailer


def pad_or_fail(blob: bytes, slot_size: int) -> bytes:
    if len(blob) > slot_size:
        raise SystemExit(
            f"Gzipped Tick.lua is {len(blob)} bytes but slot is only {slot_size}. "
            "Trim Tick.lua comments / code and retry."
        )
    return blob + (b"\x00" * (slot_size - len(blob)))


def install(game_dir: Path, tick_lua: Path, backup: bool = True) -> None:
    exe = game_dir / "Journey.exe"
    if not exe.exists():
        raise SystemExit(f"Journey.exe not found in {game_dir}")

    data = bytearray(exe.read_bytes())
    slots = find_tick_slots(bytes(data))
    if not slots:
        raise SystemExit("Could not locate Tick.lua gzip slot inside Journey.exe")

    # Prefer the largest candidate (real embedded script, not a tiny false positive)
    slots.sort(key=lambda s: s[1] - s[0], reverse=True)
    start, end = slots[0]
    slot_size = end - start
    print(f"Found Tick.lua slot at 0x{start:X}..0x{end:X} ({slot_size} bytes)")

    if backup:
        bak = game_dir / "Journey.exe.bak"
        if not bak.exists():
            shutil.copy2(exe, bak)
            print(f"Backup written: {bak}")
        else:
            print(f"Backup already exists: {bak}")

    blob = make_tick_gz(tick_lua)
    print(f"New Tick.lua.gz size: {len(blob)} bytes")
    payload = pad_or_fail(blob, slot_size)
    data[start:end] = payload
    # Ensure writable before overwrite (may be read-only from prior install)
    try:
        os.chmod(exe, stat.S_IWRITE | stat.S_IREAD)
    except OSError:
        pass
    exe.write_bytes(data)
    # Discourage Steam from silently restoring the exe mid-launch
    try:
        os.chmod(exe, stat.S_IREAD | stat.S_IRUSR)
    except OSError:
        pass
    print(f"Patched {exe} (set read-only)")

    # Deploy runtime lua files next to Journey.exe
    deploy = [
        "StartLua.lua",
        "TickHook.lua",
        "GameTick.lua",
        "features.lua",
        "hud_menu.lua",
        "config.lua",
        "commands.lua",
        "RunOnce.lua",
    ]
    mod_dir = project_root() / "mod"
    for name in deploy:
        src = mod_dir / name
        dst = game_dir / name
        shutil.copy2(src, dst)
        print(f"Copied {src.name} -> {dst}")

    (game_dir / ".journey_mos").write_text("v5-once\n", encoding="utf-8")
    print("\nDone. Launch Journey.exe and look for 'Journey Mod loaded'.")
    print("Open menu in-game: hold Fly + Chirp + Pause together.")


def restore(game_dir: Path) -> None:
    exe = game_dir / "Journey.exe"
    bak = game_dir / "Journey.exe.bak"
    if not bak.exists():
        raise SystemExit(f"No backup found at {bak}")
    try:
        os.chmod(exe, stat.S_IWRITE | stat.S_IREAD)
    except OSError:
        pass
    shutil.copy2(bak, exe)
    marker = game_dir / ".journey_mos"
    if marker.exists():
        marker.unlink()
    print(f"Restored {exe} from {bak}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Install Journey Lua Tick hook")
    parser.add_argument("--game-dir", type=Path, help="Folder containing Journey.exe")
    parser.add_argument("--tick", type=Path, default=project_root() / "mod" / "Tick.lua")
    parser.add_argument("--restore", action="store_true", help="Restore Journey.exe.bak")
    parser.add_argument("--no-backup", action="store_true")
    args = parser.parse_args()

    game_dir = args.game_dir
    if game_dir is None:
        found = default_game_dirs()
        if not found:
            raise SystemExit("Pass --game-dir pointing at your Journey install")
        game_dir = found[0]
        print(f"Using game dir: {game_dir}")

    if args.restore:
        restore(game_dir)
        return 0

    if not args.tick.exists():
        raise SystemExit(f"Missing {args.tick}")

    install(game_dir, args.tick, backup=not args.no_backup)
    return 0


if __name__ == "__main__":
    sys.exit(main())
