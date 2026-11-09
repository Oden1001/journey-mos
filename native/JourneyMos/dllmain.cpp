#include "overlay.h"
#include "bridge.h"

namespace {

std::wstring ExeDir() {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring p(path);
  auto slash = p.find_last_of(L"\\/");
  if (slash != std::wstring::npos) p.resize(slash);
  return p;
}

void BootLog(const wchar_t* msg) {
  auto path = ExeDir() + L"\\journeymos.log";
  HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) return;
  SYSTEMTIME st{};
  GetLocalTime(&st);
  char line[512];
  int n = snprintf(line, sizeof(line), "[%02u:%02u:%02u] ", st.wHour, st.wMinute,
                   st.wSecond);
  DWORD w = 0;
  if (n > 0) WriteFile(f, line, (DWORD)n, &w, nullptr);
  int need = WideCharToMultiByte(CP_UTF8, 0, msg, -1, nullptr, 0, nullptr, nullptr);
  if (need > 1) {
    std::string utf8(static_cast<size_t>(need - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, msg, -1, utf8.data(), need, nullptr, nullptr);
    WriteFile(f, utf8.data(), (DWORD)utf8.size(), &w, nullptr);
  }
  WriteFile(f, "\r\n", 2, &w, nullptr);
  CloseHandle(f);
}

DWORD WINAPI BootThread(LPVOID) {
  mos::SetGameDir(ExeDir());
  // Do NOT call ImmDisableIME — it permanently blocks Chinese IME for the
  // whole Journey process (subtitle typing needs it).
  BootLog(L"DLL attached, waiting for window...");
  Sleep(1200);
  mos::StartOverlayHost();
  BootLog(L"Overlay host started — press F8 in-game");
  return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    HANDLE th = CreateThread(nullptr, 0, BootThread, nullptr, 0, nullptr);
    if (th) CloseHandle(th);
  } else if (reason == DLL_PROCESS_DETACH) {
    mos::StopOverlayHost();
  }
  return TRUE;
}
