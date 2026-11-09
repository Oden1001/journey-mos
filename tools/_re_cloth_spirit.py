data = open(r"D:\Steam\steamapps\common\Journey\Journey.exe", "rb").read()

for label, needle in [
    ("ClothSpiritMesh", b'resource "Shader" "ClothSpiritMesh"'),
    ("MeshDudeLocal", b'resource "Shader" "MeshDudeLocal"'),
    ("MeshDudeRemote", b'resource "Shader" "MeshDudeRemote"'),
    ("ClothSpirit tex", b'resource "Texture" "ClothSpirit"'),
]:
    j = data.find(needle)
    print("====", label, hex(j) if j >= 0 else None)
    if j is None or j < 0:
        continue
    # print until next resource or 500 bytes
    chunk = data[j : j + 600]
    end = chunk.find(b"\r\n}\r\n")
    if end < 0:
        end = 500
    else:
        end += 6
    print(chunk[:end].decode("latin1", "replace"))
    print()
