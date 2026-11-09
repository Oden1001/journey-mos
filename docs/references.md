# 参考资料摘要

## 本项目启动路线（当前）

1. **Tick.lua 嵌入** — 修改 `Journey.exe` 内嵌的 `Tick.lua.gz`，每帧 `dofile("TickHook.lua")`。  
2. **Lua 功能层** — `mod/features.lua` + `config.lua` / `commands.lua`（我们的脚本，非游戏原版）。  
3. **游戏内菜单** — `dist/native/JourneyMos.dll`（Direct2D/DirectWrite 浮层），由 `JourneyMosLauncher.exe` 注入。  
   - 按 **F8** 开关可拖动菜单  
   - 菜单写入 `config.lua` / `commands.lua`，由 Tick 钩子执行  
   - 不使用 ImGui / 浏览器 / WebView  

**发行形态**：启动器内嵌 Lua + Tick.gz；`dinput8.dll` 代理在 Steam 正常启动时自动加载菜单。用户说明见根目录 [`README.md`](../README.md)。

构建：`native\build.bat`（需 LLVM-MinGW UCRT + Python 3）

开发一键：`一键启动.bat` → 部署脚本 + 装钩子 + 开游戏 + 注入菜单。

## 注入路线（社区）

1. **Tick.lua 嵌入**  
   教程：[PC Lua Script Editing / Injection](https://thatgamecompanyfan.boards.net/thread/541/pc-lua-script-editing-injection)

2. **Journey_Lua_Modder**  
   Python GUI + `StartLua.lua` / `TickHook.lua`。  
   https://github.com/wolf109909/Journey_Lua_Modder

3. **journey_detour**（Present/ImGui 参考；本项目不采用其 ImGui UI）  
   DLL 注入 + 从 `Data/` 加载外部 Lua。  
   https://github.com/Mods-Journey/journey_detour

## 其它 mods

- [MEPAL](https://www.nexusmods.com/journey/mods/2) — Cheat Engine，全关卡/到处相遇  
- [Journey Randomizer](https://www.nexusmods.com/journey/mods/16) — CE 随机器  
- FearLess Journey +2 — 内存版无限飞 / 走路冲刺加速  
- [JourneyAmbiences](https://www.nexusmods.com/journey/mods/27) — 夜/黄昏/雪（需 ScriptLoader）  
- [No Windwalls](https://www.nexusmods.com/journey/mods/4) — 去掉风墙推回  

## wayfarer-groupe / journey_modding（强烈建议参考）

源码组：https://gitlab.com/wayfarer-groupe/  
本地摘录：`_ref_journey_modding/`（README + docs/memory + tools）

### 和 Journey Mos 的分工

| 路线 | 适合做什么 |
|------|------------|
| **Mos（Tick + F8 菜单）** | 运行时作弊：能量、加速、瞬移、跳关、表情、发光、NetGui… |
| **ScriptLoader + Mods\\** | 关卡数据包：天气/天空/沙色、关卡编辑、去风墙脚本包 |
| **其它 proxy DLL** | coo 轻鸣、同伴名、关挂机屏（勿与 Mos DLL 搞混） |

`Data\Scripts\` 磁盘文件**默认不被游戏读取**；真正脚本在 `Journey.exe` 嵌入表里。Mos 的 Tick 钩子、ScriptLoader 的 Mods 替换，都是在绕开这一点。

### 可直接借鉴的要点

1. **关卡氛围** = `__EnvNodeInstances.lua`（雾/天/光）+ 磁盘上的 `DecorationMeshInstances.lua.bin`（太阳/云/雾其实是装饰 mesh）。要夜/黄昏/雪 → 装 JourneyAmbiences，不要在 Mos 里硬 Spawn。  
2. **风墙** = `BoundaryRailPush`。Mos「无视风战」只减弱 Dude 风力 Vars（减轻推挤），**关不掉真正风墙**；要无风墙请装 [No Windwalls](https://www.nexusmods.com/journey/mods/4)。运行时 `KillTrigger` 实测无效。  
3. **表情** 也可用 `QueueAnimationSequence(animBarn, "JumpRoll")`（loader 文档验证过）；Mos 现用 `PlayDudeAnim` 事件。  
4. **零售版** `Vars.Game.cheatsEnabled()` 默认 false；触发器 Shortcut / DebugKeys 需先打开（Mos 已开）。  
5. **ScriptLoader 输入桥** `os.getenv("JOURNEY_INPUT")` 能读真键盘；Mos 用原生菜单 DLL，一般不需要。  
6. **共存**：Ambiences 自带 `dbghelp.dll`。与其它 Mods 并存时只留一份 loader；Mos 的 Tick 钩子与 loader 可同时用。

### 工具（若以后做数据向功能）

`tools/extract_scripts.pl`、`make_env.pl`、`filter_instances.pl`、`night_mod/make_night*.pl` 等——改关卡文件用，不是 Mos 运行时菜单。

## 关键游戏 API（已从本机脚本/字符串确认）

```lua
-- 补满围巾能量（DebugKeys.lua 按键 B）
game:playerBarn():GetLocalDude():FillScarf(99, game:soundBarn())

-- 白袍充能作弊开关
Vars.DudeColor.kWhiteOutfitCheat(true)

-- 关闭匹配
Vars.Lobby.kUseAutoLobby(false)

-- 启用调试作弊相关功能
Vars.Game.cheatsEnabled(true)
```

围巾最大容量：`Vars.Dude.kScarfMaxCapacity = 30`

## 内存钩子可继续挖（Mos native）

已落地：`Matchmaker_GetPartnerName`（GameUpdate +0x180 附近，名字缓冲约 `+0x11D64`）→ 同伴头顶实时名。

| 方向 | 思路 | 备注 |
|------|------|------|
| 大厅 / 房间列表 ESP | 扫 Matchmaker / lobby 结构里的房间指针与玩家名 | 需对标 CE / wayfarer memory 文档 |
| 同伴世界坐标直读 | 从 remote Dude 对象读 transform，绕过 Lua→磁盘 | 头顶名可完全不写 `mos_hud.bin` |
| 相机矩阵直读 | CameraNode world/view 内存 | 与上一项配对 → 真 120fps W2S |
| 无限飞 / 冲刺 | FearLess CE 表对应的重力/推力 float | 已有部分 Lua 加速；内存可更稳 |
| 去风墙 | `BoundaryRailPush` 相关 flag / rail | 现有「无视风战」只减风力 Vars |
| 压力 / 挂机防踢 | idle / AFK 计时器字段清零 | 社区 proxy DLL 也有类似 |

头顶名当前路径：Lua `writeHudSnap` → `mos_hud.bin`（~120Hz）+ overlay `SetTimer(8)`；名字仍走 Partner 钩子缓存。
