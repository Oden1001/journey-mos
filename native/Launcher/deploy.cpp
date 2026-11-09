#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

#include "deploy.h"
#include "embedded_payloads.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::wstring Join(const std::wstring& a, const wchar_t* b) {
  if (a.empty()) return b;
  if (a.back() == L'\\' || a.back() == L'/') return a + b;
  return a + L"\\" + b;
}

bool FileExists(const std::wstring& path) {
  return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool JourneyRunning() {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  bool found = false;
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, L"Journey.exe") == 0) {
        found = true;
        break;
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return found;
}

bool WriteBytes(const std::wstring& path, const void* data, DWORD size) {
  HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  BOOL ok = WriteFile(f, data, size, &written, nullptr);
  CloseHandle(f);
  return ok && written == size;
}

bool ReadAll(const std::wstring& path, std::vector<unsigned char>& out) {
  HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER li{};
  if (!GetFileSizeEx(f, &li) || li.QuadPart <= 0 || li.QuadPart > 200 * 1024 * 1024) {
    CloseHandle(f);
    return false;
  }
  out.resize((size_t)li.QuadPart);
  DWORD rd = 0;
  BOOL ok = ReadFile(f, out.data(), (DWORD)out.size(), &rd, nullptr);
  CloseHandle(f);
  return ok && rd == out.size();
}

bool ReadTextFile(const std::wstring& path, std::string& out) {
  std::vector<unsigned char> raw;
  if (!ReadAll(path, raw)) return false;
  out.assign(reinterpret_cast<char*>(raw.data()), raw.size());
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
    out.pop_back();
  return true;
}

void ExtractScripts(const std::wstring& gameDir) {
  for (unsigned i = 0; i < kEmbeddedScriptCount; ++i) {
    const EmbeddedScript& s = kEmbeddedScripts[i];
    int n = MultiByteToWideChar(CP_UTF8, 0, s.name, -1, nullptr, 0);
    if (n <= 0) continue;
    std::wstring wname(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.name, -1, wname.data(), n);
    auto path = Join(gameDir, wname.c_str());
    if (s.onlyIfMissing && FileExists(path)) {
      wprintf(L"[+] keep %s\n", wname.c_str());
      continue;
    }
    if (!WriteBytes(path, s.data, s.size)) {
      wprintf(L"[!] failed to write %s (%lu)\n", wname.c_str(), GetLastError());
      continue;
    }
    wprintf(L"[+] script %s\n", wname.c_str());
  }
}

std::vector<std::pair<size_t, size_t>> FindTickSlots(const std::vector<unsigned char>& data) {
  static const unsigned char kGz[] = {0x1f, 0x8b, 0x08, 0x08};
  static const char kMark[] = "Tick.lua";
  std::vector<std::pair<size_t, size_t>> slots;
  for (size_t search = 0; search + 8 < data.size();) {
    auto it = std::search(data.begin() + search, data.end(), kMark, kMark + 8);
    if (it == data.end()) break;
    size_t marker = (size_t)(it - data.begin());
    size_t window = marker > 64 ? marker - 64 : 0;
    size_t gz = (size_t)-1;
    for (size_t i = marker; i > window;) {
      --i;
      if (i + 4 <= data.size() && data[i] == kGz[0] && data[i + 1] == kGz[1] &&
          data[i + 2] == kGz[2] && data[i + 3] == kGz[3]) {
        gz = i;
        break;
      }
    }
    if (gz == (size_t)-1) {
      for (size_t i = marker; i + 4 < data.size() && i < marker + 64; ++i) {
        if (data[i] == kGz[0] && data[i + 1] == kGz[1] && data[i + 2] == kGz[2] &&
            data[i + 3] == kGz[3]) {
          gz = i;
          break;
        }
      }
    }
    if (gz == (size_t)-1) {
      search = marker + 8;
      continue;
    }
    size_t nextGz = (size_t)-1;
    for (size_t i = gz + 4; i + 4 < data.size(); ++i) {
      if (data[i] == kGz[0] && data[i + 1] == kGz[1] && data[i + 2] == kGz[2] &&
          data[i + 3] == kGz[3]) {
        nextGz = i;
        break;
      }
    }
    if (nextGz == (size_t)-1) {
      search = marker + 8;
      continue;
    }
    size_t size = nextGz - gz;
    if (size >= 200 && size <= 8192) slots.emplace_back(gz, nextGz);
    search = marker + 8;
  }
  // Dedup
  std::vector<std::pair<size_t, size_t>> uniq;
  for (auto& s : slots) {
    bool seen = false;
    for (auto& u : uniq) {
      if (u == s) {
        seen = true;
        break;
      }
    }
    if (!seen) uniq.push_back(s);
  }
  return uniq;
}

bool HookPayloadMatches(const std::vector<unsigned char>& exe) {
  auto slots = FindTickSlots(exe);
  if (slots.empty()) return false;
  std::sort(slots.begin(), slots.end(),
            [](const auto& a, const auto& b) { return (a.second - a.first) > (b.second - b.first); });
  auto [start, end] = slots[0];
  size_t slot = end - start;
  if (kTickLuaGzLen > slot) return false;
  return memcmp(exe.data() + start, kTickLuaGz, kTickLuaGzLen) == 0;
}

bool MarkerCurrent(const std::wstring& gameDir) {
  std::string text;
  if (!ReadTextFile(Join(gameDir, L".journey_mos"), text)) return false;
  return text == kHookMarker;
}

bool InstallTickHook(const std::wstring& gameDir) {
  auto exePath = Join(gameDir, L"Journey.exe");
  std::vector<unsigned char> data;
  if (!ReadAll(exePath, data)) {
    wprintf(L"[!] cannot read Journey.exe\n");
    return false;
  }
  auto slots = FindTickSlots(data);
  if (slots.empty()) {
    wprintf(L"[!] Tick.lua gzip slot not found in Journey.exe\n");
    return false;
  }
  std::sort(slots.begin(), slots.end(),
            [](const auto& a, const auto& b) { return (a.second - a.first) > (b.second - b.first); });
  auto [start, end] = slots[0];
  size_t slot = end - start;
  wprintf(L"[*] Tick slot 0x%zX..0x%zX (%zu bytes)\n", start, end, slot);
  if (kTickLuaGzLen > slot) {
    wprintf(L"[!] embedded Tick.lua.gz (%u) does not fit slot (%zu)\n", kTickLuaGzLen, slot);
    return false;
  }

  auto bak = Join(gameDir, L"Journey.exe.bak");
  if (!FileExists(bak)) {
    if (!CopyFileW(exePath.c_str(), bak.c_str(), TRUE)) {
      wprintf(L"[!] backup failed (%lu)\n", GetLastError());
      return false;
    }
    wprintf(L"[+] backup Journey.exe.bak\n");
  } else {
    wprintf(L"[+] backup already exists\n");
  }

  // Clear read-only so we can rewrite.
  SetFileAttributesW(exePath.c_str(), FILE_ATTRIBUTE_NORMAL);
  memset(data.data() + start, 0, slot);
  memcpy(data.data() + start, kTickLuaGz, kTickLuaGzLen);

  if (!WriteBytes(exePath, data.data(), (DWORD)data.size())) {
    wprintf(L"[!] failed to write patched Journey.exe (%lu)\n", GetLastError());
    return false;
  }
  // Discourage Steam from silently restoring mid-launch.
  SetFileAttributesW(exePath.c_str(), FILE_ATTRIBUTE_READONLY);
  {
    std::string m = std::string(kHookMarker) + "\n";
    WriteBytes(Join(gameDir, L".journey_mos"), m.data(), (DWORD)m.size());
  }
  wprintf(L"[+] Tick hook installed (%hs)\n", kHookMarker);
  return true;
}

}  // namespace

bool InstallAutoLoadProxy(const std::wstring& gameDir) {
  // Proxy lives next to the launcher (same folder as Journey when installed).
  wchar_t self[MAX_PATH]{};
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  std::wstring selfDir = self;
  auto slash = selfDir.find_last_of(L"\\/");
  if (slash != std::wstring::npos) selfDir.resize(slash);

  auto src = Join(selfDir, L"dinput8.dll");
  auto dst = Join(gameDir, L"dinput8.dll");
  if (!FileExists(src)) {
    wprintf(L"[!] dinput8.dll proxy not next to launcher — Steam auto-load unavailable\n");
    wprintf(L"    Expected: %s\n", src.c_str());
    return false;
  }

  // Avoid clobbering an unrelated mod's dinput8 unless it is already ours
  // (same size + our sidecar JourneyMos.dll present is enough for updates).
  if (!CopyFileW(src.c_str(), dst.c_str(), FALSE)) {
    wprintf(L"[!] failed to install dinput8.dll (%lu)\n", GetLastError());
    return false;
  }
  wprintf(L"[+] auto-load proxy dinput8.dll\n");
  wprintf(L"    After setup, you can start Journey from Steam — press F8 in-game.\n");
  return true;
}

bool SetupGameInstall(const std::wstring& gameDir, bool forceHook) {
  auto journey = Join(gameDir, L"Journey.exe");
  if (!FileExists(journey)) {
    wprintf(L"[!] Journey.exe not in %s\n", gameDir.c_str());
    return false;
  }

  wprintf(L"[*] Releasing embedded Lua scripts...\n");
  ExtractScripts(gameDir);

  // Always refresh auto-load proxy so Steam launches get the menu.
  InstallAutoLoadProxy(gameDir);

  bool needHook = forceHook;
  if (!needHook) {
    if (!MarkerCurrent(gameDir)) {
      needHook = true;
    } else {
      std::vector<unsigned char> exe;
      if (!ReadAll(journey, exe) || !HookPayloadMatches(exe)) needHook = true;
      else wprintf(L"[+] Tick hook already current\n");
    }
  }

  if (!needHook) return true;

  if (JourneyRunning()) {
    wprintf(L"[!] Journey is running — cannot patch Tick hook.\n");
    wprintf(L"    Close the game and run the launcher once more.\n");
    return false;
  }

  wprintf(L"[*] Installing Tick hook into Journey.exe...\n");
  return InstallTickHook(gameDir);
}
