# Copy runtime Lua to Journey folder without patching Journey.exe
param(
  [string]$GameDir = "D:\Steam\steamapps\common\Journey"
)

$Root = Split-Path -Parent $PSScriptRoot
$files = @(
  "StartLua.lua",
  "TickHook.lua",
  "GameTick.lua",
  "features.lua",
  "config.lua",
  "commands.lua",
  "RunOnce.lua"
)
foreach ($f in $files) {
  Copy-Item (Join-Path $Root "mod\$f") (Join-Path $GameDir $f) -Force
  Write-Host "Copied $f"
}
Write-Host "Done. Requires Tick.lua hook already installed in Journey.exe."
