from capstone import Cs, CS_ARCH_X86, CS_MODE_64
import struct

data = open(r"D:\Steam\steamapps\common\Journey\Journey.exe", "rb").read()
ib = 0x140000000
md = Cs(CS_ARCH_X86, CS_MODE_64)
text = data[0x400:0x552400]
text_va = ib + 0x1000


def fo(va):
    return 0x400 + (va - ib - 0x1000)


# Callers of scale-apply that uses [rdx+0xe0]
target = 0x14017ea40
chs = []
for i in range(len(text) - 5):
    if text[i] == 0xE8:
        rel = struct.unpack_from("<i", text, i + 1)[0]
        if text_va + i + 5 + rel == target:
            chs.append(text_va + i)
print("callers of mesh-scale-from-e0", [hex(c) for c in chs])
for c in chs[:4]:
    print("---", hex(c))
    for insn in md.disasm(data[fo(c - 0x40) : fo(c - 0x40) + 0x60], c - 0x40):
        print(f"0x{insn.address:x}:\t{insn.mnemonic}\t{insn.op_str}")
        if insn.address > c + 4:
            break

# Find Game+imm stores near CreateCreature / CreatureBarn allocation
# Search for string ptr table referencing CreateCreature
sva = ib + 0x553000 + (0x5cdc00 - 0x552400)
print("CreateCreature va", hex(sva))

# Who loads [game+?] as creatureBarn: mov rax,[rcx+imm]; ret style near 0x100-0x280
# Match known: matchmaker 0x180, secret 0x1c0, player 0x210
for imm in range(0x100, 0x280, 8):
    # pattern 48 8B 81 xx xx 00 00 C3
    pat = bytes([0x48, 0x8B, 0x81]) + struct.pack("<I", imm) + bytes([0xC3])
    idx = text.find(pat)
    if idx >= 0:
        print("getter Game+" + hex(imm), hex(text_va + idx))

# Disassemble 0x14017ea40 caller context for rdx identity
