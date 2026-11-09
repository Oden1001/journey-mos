import re
import struct
import zlib
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

data = open(r"D:\Steam\steamapps\common\Journey\Journey.exe", "rb").read()
ib = 0x140000000
md = Cs(CS_ARCH_X86, CS_MODE_64)


def fo(va):
    return 0x400 + (va - ib - 0x1000)


# Dump one barrencestor AttachObjectToJoint and nearby SetDudeAge
j = 0x3591300
blob = zlib.decompress(data[j:], 15 + 32)
for pat in [
    rb'GardenerName = "\|barrencestor:AttachObjectToJoint1\|[^"]+"[\s\S]{0,900}',
    rb'GardenerName = "\|dt2:ancestor:AttachObjectToJoint1\|[^"]+"[\s\S]{0,900}',
]:
    m = re.search(pat, blob)
    if m:
        print("=== match ===")
        print(m.group().decode("utf-8", "replace")[:900])
        print()

# ObjectNames containing spirit/ancestor/barren in this level script
for label, rx in [
    ("spirit", rb'ObjectName = "([^"]*[Ss]pirit[^"]*)"'),
    ("ancestor", rb'ObjectName = "([^"]*[Aa]ncestor[^"]*)"'),
    ("barren", rb'ObjectName = "([^"]*barrencestor[^"]*)"'),
]:
    hits = re.findall(rx, blob)
    print(label, "count", len(hits), "sample", hits[:8])

# GetLocalDude binding - find method that returns dude from PlayerBarn
# Search: mov rax,[rcx+imm]; ret patterns near PlayerBarn methods used by lua
# From lua: playerBarn():GetLocalDude() - typically offset into barn
text = data[0x400:0x552400]
text_va = ib + 0x1000

# Find string GetLocalDude and surrounding method table in .data by scanning for
# relative stores after lea of the string in init (already saw copy loop).
# Alternative: scan for functions that do: mov rax,[rcx+0x28] or similar short getters
# used after PlayerBarn.

# Disassemble 0x140117730 mentioned in _re_pb_off as GetLocalDude thunk
print("\n=== probe 0x140117730 ===")
for insn in md.disasm(data[fo(0x140117730) : fo(0x140117730) + 0x40], 0x140117730):
    print(f"0x{insn.address:x}:\t{insn.mnemonic}\t{insn.op_str}")
    if insn.mnemonic == "ret":
        break

# Find writes to cloth alpha / enabled on dude - search kInvisibleAlpha lea
j = data.find(b"kInvisibleAlpha\x00")
sva = ib + 0x553000 + (j - 0x552400)
print("\nkInvisibleAlpha", hex(sva))
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
print("xrefs", [hex(h) for h in hits[:10]])
