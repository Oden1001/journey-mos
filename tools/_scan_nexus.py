import gzip, re, os

base = r"D:\Steam\steamapps\common\Journey\Data\Scripts"
want = [
    "BoundaryRailPush",
    "ModWindEmitter",
    "SetEnvFogCrossfade",
    "SetClearColor",
    "SetSkyboxMesh",
    "AttachObjectToJoint",
    "WindBattle",
    "SetTemperatureParams",
    "FlashEnvNode",
    "SetEnvNodeColorCorrect",
    "CreateLight",
    "SetHorizontalVelocity",
]

def dump(typename, limit=1):
    print(f"\n#### {typename}")
    n = 0
    for lv in os.listdir(base):
        p = os.path.join(base, lv, "__TriggerInstances.lua.gz")
        if not os.path.exists(p):
            continue
        s = gzip.open(p, "rt", encoding="utf-8", errors="ignore").read()
        matches = list(re.finditer(rf'Type\s*=\s*["\']{re.escape(typename)}["\']', s))
        if matches:
            print(f"  {lv}: {len(matches)}")
        for m in matches[:limit]:
            i = m.start()
            depth = 0
            start = None
            for j in range(i, max(0, i - 6000), -1):
                if s[j] == "}":
                    depth += 1
                elif s[j] == "{":
                    if depth == 0:
                        start = j
                        break
                    depth -= 1
            if start is None:
                continue
            depth = 0
            end = None
            for j in range(start, min(len(s), start + 3500)):
                if s[j] == "{":
                    depth += 1
                elif s[j] == "}":
                    depth -= 1
                    if depth == 0:
                        end = j + 1
                        break
            print(s[start:end][:900])
            print("---")
            n += 1
            if n >= limit:
                return


for t in want:
    total = 0
    for lv in os.listdir(base):
        p = os.path.join(base, lv, "__TriggerInstances.lua.gz")
        if not os.path.exists(p):
            continue
        s = gzip.open(p, "rt", encoding="utf-8", errors="ignore").read()
        total += len(re.findall(rf'Type\s*=\s*["\']{re.escape(t)}["\']', s))
    print(f"{t}: {total}")

dump("BoundaryRailPush", 1)
dump("ModWindEmitter", 1)
dump("AttachObjectToJoint", 1)
dump("SetClearColor", 1)
dump("SetEnvFogCrossfade", 1)
