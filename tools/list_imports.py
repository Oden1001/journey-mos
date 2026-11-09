#!/usr/bin/env python3
import struct
import sys
from pathlib import Path


def main() -> int:
    p = Path(sys.argv[1] if len(sys.argv) > 1 else r"D:\Steam\steamapps\common\Journey\Journey.exe")
    data = p.read_bytes()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    magic = struct.unpack_from("<H", data, e_lfanew + 24)[0]
    opt = e_lfanew + 24
    dd = opt + (112 if magic == 0x20B else 96)
    imp_rva = struct.unpack_from("<I", data, dd + 8)[0]
    nsec = struct.unpack_from("<H", data, e_lfanew + 6)[0]
    size_opt = struct.unpack_from("<H", data, e_lfanew + 20)[0]
    sec = e_lfanew + 24 + size_opt

    def rva2off(rva: int):
        for i in range(nsec):
            o = sec + i * 40
            va, vsz, raw, rsz = struct.unpack_from("<IIII", data, o + 8)
            span = max(vsz, rsz)
            if va <= rva < va + span:
                return rva - va + raw
        return None

    off = rva2off(imp_rva)
    if off is None:
        print("no import dir")
        return 1
    dlls = []
    while True:
        name_rva = struct.unpack_from("<I", data, off + 12)[0]
        if name_rva == 0:
            break
        no = rva2off(name_rva)
        if no is None:
            break
        n = data[no : no + 64].split(b"\x00", 1)[0].decode("ascii", "ignore")
        dlls.append(n)
        off += 20
    for n in sorted(set(dlls), key=str.lower):
        print(n)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
