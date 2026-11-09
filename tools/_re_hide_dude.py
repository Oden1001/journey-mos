import struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

data = open(r"D:\Steam\steamapps\common\Journey\Journey.exe", "rb").read()
image_base = 0x140000000
md = Cs(CS_ARCH_X86, CS_MODE_64)
text = data[0x400:0x552400]


def dis(va, size=0x80):
    o = 0x400 + (va - image_base - 0x1000)
    for insn in md.disasm(data[o : o + size], va):
        print(f"0x{insn.address:x}:\t{insn.mnemonic}\t{insn.op_str}")
        if insn.mnemonic == "ret" and insn.address > va + 4:
            break


# MeshDudeLocal string xrefs - shader name, maybe used when selecting mesh
j = data.find(b"MeshDudeLocal\x00")
rva = 0x553000 + (j - 0x552400)
va = image_base + rva
print("MeshDudeLocal", hex(va))
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
            dis(instr - 0x20 if instr > 0x20 else instr, 0x60)

# Look for kInvisibleAlpha / GlowAlpha on companion - and Dude render skip
for s in [b"kInvisibleAlpha", b"kSymbolFloatDistance", b"Frozen", b"MeshDudeLocalFrozen"]:
    print(s, hex(data.find(s)))

# Find Dude vtable via RTTI or ctor that sets vtable near Dude.cpp usage
# Search lea to Dude.cpp then nearby mov [rcx], rax
j = data.find(b"Dude.cpp\x00")
rva = 0x553000 + (j - 0x552400)
va = image_base + rva
print("Dude.cpp va", hex(va))
count = 0
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
            print("Dude.cpp xref", hex(instr))
            count += 1
            if count >= 8:
                break
