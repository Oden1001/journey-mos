"""Scan Journey.exe for Dude mesh / visibility / AttachObjectToJoint native paths."""
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
import struct

data = open(r"D:\Steam\steamapps\common\Journey\Journey.exe", "rb").read()
image_base = 0x140000000
md = Cs(CS_ARCH_X86, CS_MODE_64)


def va_to_off(va):
    rva = va - image_base
    return 0x400 + (rva - 0x1000)


def dis(start, size):
    o = va_to_off(start)
    for insn in md.disasm(data[o : o + size], start):
        print(f"0x{insn.address:x}:\t{insn.mnemonic}\t{insn.op_str}")
        if insn.mnemonic == "ret" and insn.address > start + 8:
            break


# Find AttachObjectToJoint event MetaAdd / Start implementation via string xrefs
target = None
# string at file 0x659d40 area - AttachObjectToJoint
j = data.find(b"AttachObjectToJoint\x00")
print("AttachObjectToJoint str", hex(j))
# rdata: raw 0x552400 va 0x553000
rva = 0x553000 + (j - 0x552400)
va = image_base + rva
print("VA", hex(va))
pat = struct.pack("<Q", va)
refs = []
start = 0
while True:
    k = data.find(pat, start)
    if k < 0:
        break
    refs.append(k)
    start = k + 1
print("ptr refs", [hex(x) for x in refs[:12]], "n", len(refs))

# Dude.cpp nearby strings for scale/alpha/mesh
for s in [
    b"Dude.cpp",
    b"kSpeedMinBoost",
    b"MeshDudeLocal",
    b"SetEnabled",
    b"attachRotScale",
    b"clothMeshes",
]:
    print(s, hex(data.find(s)))

# Find simple float writes related to dude scale - search "Scale" near Dude
j = data.find(b"Dude.cpp")
print("Dude.cpp ctx", data[j : j + 200])
