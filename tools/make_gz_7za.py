#!/usr/bin/env python3
"""Create Tick.lua.gz using 7za (same tool family as community tutorials)."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SEVEN = ROOT / "tools" / "bin" / "7za.exe"


def make_gz_7za(tick_lua: Path, out_gz: Path | None = None) -> bytes:
    if not SEVEN.exists():
        raise SystemExit(f"missing {SEVEN}")
    tick_lua = tick_lua.resolve()
    with tempfile.TemporaryDirectory() as td:
        td_path = Path(td)
        # 7za names the gzip member after the source filename
        src = td_path / "Tick.lua"
        shutil.copy2(tick_lua, src)
        gz_path = td_path / "Tick.lua.gz"
        subprocess.check_call(
            [str(SEVEN), "a", "-tgzip", "-y", str(gz_path), str(src)],
            cwd=td,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        data = gz_path.read_bytes()
        if out_gz:
            out_gz.write_bytes(data)
        return data


if __name__ == "__main__":
    import sys

    src = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "mod" / "Tick_minimal.lua")
    blob = make_gz_7za(src, ROOT / "mod" / "Tick_probe.lua.gz")
    print(f"{src.name} -> gz {len(blob)} bytes (limit 1936) fits={len(blob)<=1936}")
