// Thin DirectInput8 proxy: forwards DirectInput8Create to System32\dinput8.dll
// and loads JourneyMos.dll so the F8 menu starts with the game (Steam OK).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>

namespace {

HMODULE gReal = nullptr;
HMODULE gMos = nullptr;

using DirectInput8Create_t = HRESULT(WINAPI*)(HINSTANCE, DWORD, const GUID*, LPVOID*, void*);

DirectInput8Create_t gCreate = nullptr;

std::wstring DirOfModule(HMODULE mod) {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(mod, path, MAX_PATH);
  std::wstring p(path);
  auto slash = p.find_last_of(L"\\/");
  if (slash != std::wstring::npos) p.resize(slash);
  return p;
}

void LoadMos(HMODULE self) {
  if (gMos) return;
  // Prefer sideloaded JourneyMos.dll next to this proxy / Journey.exe.
  auto path = DirOfModule(self) + L"\\JourneyMos.dll";
  gMos = LoadLibraryW(path.c_str());
}

bool LoadReal() {
  if (gReal) return gCreate != nullptr;
  wchar_t sys[MAX_PATH]{};
  UINT n = GetSystemDirectoryW(sys, MAX_PATH);
  if (!n || n >= MAX_PATH) return false;
  std::wstring path = std::wstring(sys) + L"\\dinput8.dll";
  gReal = LoadLibraryW(path.c_str());
  if (!gReal) return false;
  gCreate = reinterpret_cast<DirectInput8Create_t>(GetProcAddress(gReal, "DirectInput8Create"));
  return gCreate != nullptr;
}

}  // namespace

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, const GUID* riidltf,
                                             LPVOID* ppvOut, void* punkOuter) {
  if (!LoadReal() || !gCreate) return E_FAIL;
  return gCreate(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    LoadReal();
    LoadMos(hModule);
  }
  // Do not FreeLibrary(JourneyMos) on detach — overlay threads may still be winding down.
  return TRUE;
}
