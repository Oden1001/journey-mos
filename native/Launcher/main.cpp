#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>

#include "deploy.h"

#include <cstdio>
#include <string>

namespace {

std::wstring DirOf(const std::wstring& path) {
  auto slash = path.find_last_of(L"\\/");
  if (slash == std::wstring::npos) return L".";
  return path.substr(0, slash);
}

std::wstring Join(const std::wstring& a, const wchar_t* b) {
  if (a.empty()) return b;
  if (a.back() == L'\\' || a.back() == L'/') return a + b;
  return a + L"\\" + b;
}

std::wstring FullPath(const std::wstring& path) {
  wchar_t buf[MAX_PATH]{};
  DWORD n = GetFullPathNameW(path.c_str(), MAX_PATH, buf, nullptr);
  if (n == 0 || n >= MAX_PATH) return path;
  return buf;
}

DWORD FindJourneyPid() {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return 0;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  DWORD pid = 0;
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, L"Journey.exe") == 0) {
        pid = pe.th32ProcessID;
        break;
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return pid;
}

bool StartJourney(const std::wstring& gameDir) {
  auto exe = Join(gameDir, L"Journey.exe");
  if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

  // Prefer Steam appid if present (more stable launch on some PCs).
  auto appidPath = Join(gameDir, L"steam_appid.txt");
  HANDLE f = CreateFileW(appidPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f != INVALID_HANDLE_VALUE) {
    char buf[64]{};
    DWORD rd = 0;
    ReadFile(f, buf, sizeof(buf) - 1, &rd, nullptr);
    CloseHandle(f);
    std::string id;
    for (DWORD i = 0; i < rd; ++i) {
      if (buf[i] >= '0' && buf[i] <= '9') id.push_back(buf[i]);
      else if (!id.empty()) break;
    }
    if (!id.empty()) {
      std::wstring url = L"steam://rungameid/" + std::wstring(id.begin(), id.end());
      wprintf(L"[*] Starting via Steam %s\n", url.c_str());
      ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      return true;
    }
  }

  wprintf(L"[*] Starting %s\n", exe.c_str());
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::wstring cmd = L"\"" + exe + L"\"";
  if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      gameDir.c_str(), &si, &pi)) {
    wprintf(L"[!] CreateProcess failed (%lu)\n", GetLastError());
    return false;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

bool InjectDll(DWORD pid, const std::wstring& dllPath) {
  HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                                PROCESS_VM_READ,
                            FALSE, pid);
  if (!proc) {
    wprintf(L"[!] OpenProcess failed (%lu) — try running as admin\n", GetLastError());
    return false;
  }

  size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
  void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE,
                                PAGE_READWRITE);
  if (!remote) {
    wprintf(L"[!] VirtualAllocEx failed (%lu)\n", GetLastError());
    CloseHandle(proc);
    return false;
  }

  if (!WriteProcessMemory(proc, remote, dllPath.c_str(), bytes, nullptr)) {
    wprintf(L"[!] WriteProcessMemory failed (%lu)\n", GetLastError());
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    return false;
  }

  auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
  HANDLE th = CreateRemoteThread(proc, nullptr, 0, loadLib, remote, 0, nullptr);
  if (!th) {
    wprintf(L"[!] CreateRemoteThread failed (%lu)\n", GetLastError());
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    return false;
  }

  WaitForSingleObject(th, 15000);
  DWORD exitCode = 0;
  GetExitCodeThread(th, &exitCode);
  CloseHandle(th);
  VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
  CloseHandle(proc);
  if (!exitCode) {
    wprintf(L"[!] LoadLibraryW returned NULL inside target\n");
    return false;
  }
  return true;
}

bool AlreadyInjected(DWORD pid) {
  HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (!proc) return false;
  HMODULE mods[1024];
  DWORD needed = 0;
  bool found = false;
  if (EnumProcessModules(proc, mods, sizeof(mods), &needed)) {
    wchar_t path[MAX_PATH];
    for (DWORD i = 0; i < needed / sizeof(HMODULE); ++i) {
      if (GetModuleFileNameExW(proc, mods[i], path, MAX_PATH)) {
        std::wstring p(path);
        auto slash = p.find_last_of(L"\\/");
        std::wstring base = slash == std::wstring::npos ? p : p.substr(slash + 1);
        if (_wcsicmp(base.c_str(), L"JourneyMos.dll") == 0) {
          found = true;
          break;
        }
      }
    }
  }
  CloseHandle(proc);
  return found;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  SetConsoleOutputCP(CP_UTF8);

  wchar_t self[MAX_PATH]{};
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  std::wstring selfDir = DirOf(self);

  // Default: JourneyMos.dll sitting next to this EXE (put both in Journey folder).
  std::wstring dllPath = FullPath(Join(selfDir, L"JourneyMos.dll"));
  bool autoStart = true;
  bool skipSetup = false;
  bool forceHook = false;

  for (int i = 1; i < argc; ++i) {
    if ((_wcsicmp(argv[i], L"--dll") == 0) && i + 1 < argc) {
      dllPath = FullPath(argv[++i]);
    } else if (_wcsicmp(argv[i], L"--no-start") == 0) {
      autoStart = false;
    } else if (_wcsicmp(argv[i], L"--skip-setup") == 0) {
      skipSetup = true;
    } else if (_wcsicmp(argv[i], L"--repair") == 0) {
      forceHook = true;
    }
  }

  wprintf(L"======== Journey Mos Launcher ========\n");

  auto journeyExe = Join(selfDir, L"Journey.exe");
  const bool inGameDir =
      GetFileAttributesW(journeyExe.c_str()) != INVALID_FILE_ATTRIBUTES;

  if (!skipSetup && inGameDir) {
    // Release embedded Lua + patch Tick.lua into Journey.exe (once).
    if (!SetupGameInstall(selfDir, forceHook)) {
      wprintf(L"[!] setup incomplete — menu inject may still work if hook was OK before\n");
    }
  } else if (!inGameDir) {
    wprintf(L"[!] Put JourneyMosLauncher.exe + JourneyMos.dll next to Journey.exe\n");
    wprintf(L"    (scripts + Tick hook are embedded in the launcher)\n");
  }

  if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    wprintf(L"[!] DLL not found next to launcher:\n    %s\n", dllPath.c_str());
    wprintf(L"    Put JourneyMos.dll + JourneyMosLauncher.exe in the Journey folder.\n");
    return 1;
  }
  wprintf(L"[+] DLL  %s\n", dllPath.c_str());

  DWORD pid = FindJourneyPid();
  if (!pid && autoStart) {
    if (inGameDir) {
      if (!StartJourney(selfDir)) return 2;
    } else {
      wprintf(L"[*] Journey.exe not beside launcher — waiting for any Journey process...\n");
    }
  }

  wprintf(L"[*] Waiting for Journey.exe ...\n");
  for (int i = 0; i < 180 && !pid; ++i) {
    pid = FindJourneyPid();
    if (pid) break;
    Sleep(500);
  }
  if (!pid) {
    wprintf(L"[!] Journey.exe not running\n");
    return 2;
  }
  wprintf(L"[+] Journey pid %lu\n", pid);

  // Give the main window a moment when we just launched.
  Sleep(1500);

  // Prefer auto-load via dinput8.dll on next Steam launch. Still inject now so
  // F8 works in this session without restarting.
  if (AlreadyInjected(pid)) {
    wprintf(L"[+] Menu already loaded — press F8 in-game\n");
    wprintf(L"    Later you can start Journey from Steam; dinput8.dll loads the menu.\n");
    return 0;
  }

  wprintf(L"[*] Injecting for this session...\n");
  if (!InjectDll(pid, dllPath)) {
    wprintf(L"[!] Inject failed — if dinput8.dll is installed, restart Journey from Steam.\n");
    return 3;
  }
  wprintf(L"[+] OK — press F8 for Journey Mos menu\n");
  wprintf(L"    Next time: start Journey normally (Steam). Menu loads via dinput8.dll.\n");
  return 0;
}
