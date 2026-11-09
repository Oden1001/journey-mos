from capstone import Cs, CS_ARCH_X86, CS_MODE_64

data = open(r"D:\Steam\steamapps\common\Journey\Journey.exe", "rb").read()
image_base = 0x140000000
md = Cs(CS_ARCH_X86, CS_MODE_64)


def dis(va, size=0x100):
    o = 0x400 + (va - image_base - 0x1000)
    n = 0
    for insn in md.disasm(data[o : o + size], va):
        print(f"0x{insn.address:x}:\t{insn.mnemonic}\t{insn.op_str}")
        n += 1
        if insn.mnemonic == "ret" and n > 3:
            break


# GetPos at 0x14007e3e0: mov rax,[rcx+0x28]; movaps xmm0,[rax+0x1a0]
print("=== GetPos ===")
dis(0x14007e3e0, 0x40)
print("=== SetPos ===")
dis(0x14007f310, 0x40)

# Search for scale-like: movss [rax+imm], xmm with imm near transform
# Or "SetScale" at 0x5d7168 - find xrefs
import struct

j = data.find(b"SetScale\x00")
rva = 0x553000 + (j - 0x552400)
va = image_base + rva
print("SetScale va", hex(va), "file", hex(j))
text = data[0x400:0x552400]
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
            print("SetScale xref", hex(instr))

# Find binding table for SetScale like GetPos
pat = struct.pack("<Q", va)
idxs = []
s = 0
while True:
    k = data.find(pat, s)
    if k < 0:
        break
    idxs.append(k)
    s = k + 1
print("SetScale ptr refs", [hex(x) for x in idxs[:8]])
for base in idxs[:3]:
    # dump nearby name/fn
    a, b = struct.unpack_from("<QQ", data, base)
    print(hex(base), hex(a), hex(b))
    # fn often at base+8 or base-8
    for off in (-8, 8, 16, -16):
        x = struct.unpack_from("<Q", data, base + off)[0]
        if 0x140001000 < x < 0x140600000:
            print("  neighbor", off, hex(x))
            dis(x, 0x50)
