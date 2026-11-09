from capstone import Cs, CS_ARCH_X86, CS_MODE_64
import struct

data = open(r"D:\Steam\steamapps\common\Journey\Journey.exe", "rb").read()
image_base = 0x140000000
md = Cs(CS_ARCH_X86, CS_MODE_64)


def dis(va, size=0x120):
    o = 0x400 + (va - image_base - 0x1000)
    for insn in md.disasm(data[o : o + size], va):
        print(f"0x{insn.address:x}:\t{insn.mnemonic}\t{insn.op_str}")
        if insn.mnemonic == "ret" and insn.address > va + 8:
            break


# Find function containing 0x1400d9761
# scan back for CC padding / prologue
target = 0x1400d9761
o = 0x400 + (target - image_base - 0x1000)
for back in range(0, 0x400):
    if data[o - back - 1] == 0xCC and data[o - back] in (0x40, 0x48, 0x55, 0x41, 0x53, 0x56, 0x57):
        print("fn start?", hex(target - back))
        dis(target - back, 0x40)
        break

# Callers of GetLocalDude thunk 0x140117730
# Or search mov reg,[reg+imm] where imm is playerBarn - look at 0xd9761 function for r13 origin
print("\n=== around mesh setup fn ===")
# go back further
for back in [0x100, 0x200, 0x300, 0x50, 0x80]:
    va = target - back
    o2 = 0x400 + (va - image_base - 0x1000)
    if data[o2 : o2 + 3] in (bytes([0x48, 0x89, 0x5C]), bytes([0x40, 0x53, 0x48]), bytes([0x48, 0x8B, 0xC4])):
        print("cand", hex(va))

# Search all mov r13, [reg+imm32] or mov r13, rax after load from Game
text = data[0x400:0x552400]
# In Game::Update / load: 4C 8B A8 xx xx xx xx = mov r13,[rax+imm]
for i in range(len(text) - 7):
    if text[i : i + 3] == bytes([0x4C, 0x8B, 0xA8]):  # mov r13,[rax+disp32]
        imm = struct.unpack_from("<I", text, i + 3)[0]
        if 0x100 <= imm <= 0x280:
            print("mov r13,[rax+" + hex(imm) + "]", hex(image_base + 0x1000 + i))
    if text[i : i + 3] == bytes([0x4C, 0x8B, 0xAE]):  # mov r13,[rsi+disp32]
        imm = struct.unpack_from("<I", text, i + 3)[0]
        if 0x100 <= imm <= 0x280:
            print("mov r13,[rsi+" + hex(imm) + "]", hex(image_base + 0x1000 + i))

# Identify playerBarn: lua string playerBarn at VA, find getter that returns [game+X]
# From earlier Game stores, try matching PlayerBarn.cpp ctor call site
# PlayerBarn.cpp xrefs were empty earlier - try .?AV
print("\nRTTI", data.find(b".?AVPlayerBarn"))
print("PlayerBarn str", data.find(b"PlayerBarn\x00"))
