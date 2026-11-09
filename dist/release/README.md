# Journey Mos

PC mod for [*Journey*](https://thatgamecompany.com/journey/) — in-game **F8** menu, Lua Tick hook, companion tools.  
为 PC 版 *Journey* 提供游戏内 **F8** 菜单、Lua Tick 钩子与同伴相关工具。

**License:** MIT（见 [`LICENSE`](LICENSE)）

> **Note / 特别说明:** Some features **edit game memory at runtime** (trainer-style). Use at your own risk; be considerate online.  
> 部分功能会**在运行时修改游戏内存**（类似修改器）。请自负风险，联机请克制。

---

## Install / 安装

**Files / 文件**

| File | Role |
|------|------|
| `JourneyMosLauncher.exe` | One-time setup: scripts + Tick hook + auto-load proxy |
| `JourneyMos.dll` | F8 overlay menu |
| `dinput8.dll` | Auto-loads the menu when you start Journey (e.g. from Steam) |

If the host renames binaries to `.txt`, rename back:

- `JourneyMosLauncher.txt` → `JourneyMosLauncher.exe`  
- `JourneyMos.txt` → `JourneyMos.dll`  
- `dinput8.txt` → `dinput8.dll`  

**Steps / 步骤**

1. Close Journey.  
2. Copy the three files next to `Journey.exe`.  
3. Run **`JourneyMosLauncher.exe` once** (installs hook + scripts + `dinput8.dll`).  
4. Press **F8** in-game.  

Afterwards you can start Journey from **Steam as usual** — `dinput8.dll` loads the menu automatically. Re-run the launcher after a game update, or if the menu stops appearing.

1. 关闭游戏。  
2. 将上述三个文件放到 `Journey.exe` 同目录。  
3. **先运行一次** `JourneyMosLauncher.exe`（打钩、释放脚本、安装自动加载）。  
4. 游戏内按 **F8**。  

之后可直接用 **Steam 正常启动**；菜单由 `dinput8.dll` 自动加载。游戏更新后或菜单失效时，再运行一次启动器。

**Uninstall / 卸载:** Restore `Journey.exe` from `Journey.exe.bak`, then delete `JourneyMos*`, `dinput8.dll`, mod `*.lua`, and `.journey_mos`.  
用 `Journey.exe.bak` 还原 exe，再删除上述模组文件。

> **Conflict:** Other mods that also ship `dinput8.dll` (e.g. some companion-name mods) cannot sit in the same folder. Journey Mos already includes a companion nametag.  
> **冲突：** 勿与其它占用 `dinput8.dll` 的模组同目录使用（本模组已含同伴名牌）。

---

## Features / 功能

- F8 menu — Chinese / English / 日本語  
- Infinite scarf energy, speed & fly boosts, softer wind, meet-everywhere  
- Chapter select / skip, NG+ helpers, quick actions  
- Emotes, companion nametag, local DisplayText subtitles  
- Glow / scarf / outfit shortcuts  

- F8 菜单（中 / 英 / 日）  
- 无限能量、推波助澜、飞行抬升、减弱风推、到处相遇  
- 选关 / 跳关、刷周目、快捷操作  
- 表情、同伴名牌、本地字幕  
- 发光 / 围巾 / 袍花等  

---

## Build (open source) / 编译

```bat
native\build.bat
```

Requires **LLVM-MinGW UCRT** and **Python 3** (embeds `mod\*.lua` into the launcher).

Outputs in `dist\native\`:

- `JourneyMos.dll`  
- `JourneyMosLauncher.exe`  
- `dinput8.dll`  

Layout:

- `mod\` — Lua scripts + Tick hook source  
- `native\JourneyMos\` — overlay menu DLL  
- `native\Launcher\` — setup / inject tool  
- `native\Proxy\` — `dinput8.dll` auto-load proxy  

---

## Shout outs / 致谢

[Alazar88 Tick.lua guide](https://thatgamecompanyfan.boards.net/thread/541/pc-lua-script-editing-injection), [Journey_Lua_Modder](https://github.com/wolf109909/Journey_Lua_Modder), [journey_detour](https://github.com/Mods-Journey/journey_detour), wayfarer-groupe / Journey modding community, and thatgamecompany for *Journey*.
