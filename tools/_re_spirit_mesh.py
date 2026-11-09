"""Find player cloth mesh resources and C_SpiritMesh load path."""
import re
import struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

data = open(r"D:\Steam\steamapps\common\Journey\Journey.exe", "rb").read()
ib = 0x140000000
md = Cs(CS_ARCH_X86, CS_MODE_64)
text = data[0x400:0x552400]
text_va = ib + 0x1000


def fo(va):
    return 0x400 + (va - ib - 0x1000)


# Mesh resource names of interest
names = re.findall(rb'wadresource "Mesh" "([^"]+)" \{ source = "([^"]+)"', data)
keys = (b"dude", b"cloth", b"robe", b"spirit", b"scarf", b"white", b"red", b"journey", b"player", b"c_")
print("=== interesting Mesh resources ===")
for name, src in names:
    low = name.lower()
    if any(k in low for k in keys) or b"Spirit" in name or b"Dude" in name:
        print(name.decode(), "<-", src.decode())

# clothMeshes AddMeshesToBarn context
j = data.find(b"function AddMeshesToBarn")
print("\n=== AddMeshesToBarn ===")
print(data[j : j + 500].decode("latin1", "replace"))

# Search CreateCreature / CreatureBarn Game offset
print("\n=== CreatureBarn ===")
j = data.find(b"CreatureBarn\x00")
print("str", hex(j))
sva = ib + 0x553000 + (j - 0x552400)
hits = []
for i in range(len(text) - 7):
    if text[i] in (0x48, 0x4C) and text[i + 1] == 0x8D and text[i + 2] in (
        0x05,
        0x0D,
        0x15,
        0x1D,
        0x35,
        0x3D,
    ):
        rel = struct.unpack_from("<i", text, i + 3)[0]
        if text_va + i + 7 + rel == sva:
            hits.append(text_va + i)
print("CreatureBarn lea", [hex(h) for h in hits])

# Game ctor stores: find size near creature
# From earlier Game+0x1c8 was 0x81880 - might be creature related (CreateCreatures used rdi+0x81848)
print("\nGame+0x1c8 used in CreateCreatures path as creature barn-ish")

# Disassemble 0x140147fc0 (mesh name lookup inside bind)
print("\n=== mesh name lookup 0x140147fc0 ===")
for insn in md.disasm(data[fo(0x140147fc0) : fo(0x140147fc0) + 0x80], 0x140147fc0):
    print(f"0x{insn.address:x}:\t{insn.mnemonic}\t{insn.op_str}")
    if insn.mnemonic == "ret":
        break

# Find SetScale implementation used on dude - earlier neighbor 0x1400ccd30 wrote [rdx+0x610]
print("\n=== scale write 0x1400ccd30 ===")
for insn in md.disasm(data[fo(0x1400ccd30) : fo(0x1400ccd30) + 0x50], 0x1400ccd30):
    print(f"0x{insn.address:x}:\t{insn.mnemonic}\t{insn.op_str}")
    if insn.mnemonic == "ret":
        break
