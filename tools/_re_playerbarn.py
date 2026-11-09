import struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

data = open(r"D:\Steam\steamapps\common\Journey\Journey.exe", "rb").read()
image_base = 0x140000000
md = Cs(CS_ARCH_X86, CS_MODE_64)
text = data[0x400:0x552400]

# Pattern: mov r64, [r64+imm32] then within 24 bytes lea r64, [r64+0xb0]
hits = []
for i in range(len(text) - 20):
    if text[i] != 0x48 or text[i + 1] != 0x8B:
        continue
    modrm = text[i + 2]
    # 81/83/8B/89 etc with imm32: modrm & 0xC7 patterns for [reg+disp32]
    if (modrm & 0xC0) != 0x80:  # mod=10 disp32
        continue
    imm = struct.unpack_from("<I", text, i + 3)[0]
    if not (0x100 <= imm <= 0x300 and imm % 8 == 0):
        continue
    window = text[i : i + 32]
    for j in range(7, 28):
        if (
            window[j] == 0x48
            and window[j + 1] == 0x8D
            and window[j + 4 : j + 8] == bytes([0xB0, 0, 0, 0])
        ):
            va = image_base + 0x1000 + i
            hits.append((va, imm, window[j + 2]))
            break

print("game+off then +0xb0 hits", len(hits))
for va, imm, mod in hits[:30]:
    print(hex(va), "Game+" + hex(imm), "lea modrm", hex(mod))

# Identify PlayerBarn ctor: search string PlayerBarn.cpp lea xrefs
j = data.find(b"PlayerBarn.cpp\x00")
rva = 0x553000 + (j - 0x552400)
va = image_base + rva
print("PlayerBarn.cpp", hex(va))
# rip-relative lea to this string
for i in range(len(text) - 7):
    if text[i] in (0x48, 0x4C) and text[i + 1] == 0x8D and text[i + 2] in (
        0x05,
        0x0D,
        0x15,
        0x1D,
        0x25,
        0x2D,
        0x35,
        0x3D,
    ):
        disp = struct.unpack_from("<i", text, i + 3)[0]
        instr = image_base + 0x1000 + i
        if instr + 7 + disp == va:
            print("xref", hex(instr))
            # disasm backwards for function
            o = 0x400 + i
            print(data[o - 40 : o + 40].hex())
