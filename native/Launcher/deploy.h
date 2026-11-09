#pragma once
#include <string>

// Extract embedded Lua scripts, install Tick.lua hook, and install dinput8.dll
// auto-load proxy (so Steam launch also gets the F8 menu).
// Safe to call every launch; skips work when already current.
// Returns false only on hard failure (missing Journey.exe / can't write hook while needed).
bool SetupGameInstall(const std::wstring& gameDir, bool forceHook);
