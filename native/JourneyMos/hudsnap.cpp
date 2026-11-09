#include "hudsnap.h"
#include "bridge.h"

namespace mos {

bool ReadHudSnap(HudSnap* out) {
  if (!out) return false;
  // Always ReadFile — do NOT keep a file mapping. Lua opens with "wb" which
  // recreates the file; a stale MapViewOfFile freezes the nametag on frame 1.
  auto path = GameDir() + L"\\mos_hud.bin";
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                         nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD got = 0;
  BOOL ok = ReadFile(h, out, sizeof(HudSnap), &got, nullptr);
  CloseHandle(h);
  if (!ok || got != sizeof(HudSnap)) return false;
  if (out->magic != kHudMagic) return false;
  return true;
}

}  // namespace mos
