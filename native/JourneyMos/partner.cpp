#include "partner.h"
#include "bridge.h"
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>

namespace mos {
namespace {

using GetPartnerNameFn = const char*(__fastcall*)(uintptr_t matchmaker);
using GameUpdateFn = __int64(__fastcall*)(__int64 game, float dt);

constexpr size_t kStolen = 17;
// From GetPartnerName body: lea rax,[rcx+0x11D64]
constexpr uintptr_t kNameOffDefault = 0x11D64;

std::atomic<bool> gReady{false};
std::atomic<uintptr_t> gMatchmaker{0};
std::atomic<uintptr_t> gGame{0};
GetPartnerNameFn gGetPartnerName = nullptr;
GameUpdateFn gGameUpdateOrig = nullptr;
uintptr_t gNameOff = kNameOffDefault;

uint8_t gGameUpdateStolen[32]{};
void* gGameUpdateAddr = nullptr;
void* gTrampoline = nullptr;
std::mutex gNameMu;
std::string gCachedName;
DWORD gLastFetch = 0;
DWORD gLastAttractDbg = 0;
DWORD gLastAttractCfg = 0;
std::atomic<bool> gAttractGlyphs{false};
int gAttractMoved = 0;

// Game+0x1C0 → SecretSymbolBarn* (size 0x1040).
// Slots: barn + i*0x80 (i < count @ +0x1010). Collect tick (0x14013cf50):
//   +0x18 transform*  → pos xmm @ +0x10 (distance check)
//   +0x20 aux*
//   +0x30 scene node* → world translation @ +0x570
//   +0x85 pending-collect (set by 0x14013cd90 after in-range)
//   +0x86 active (spawn=1, cleared on collect)
// Player pos used by collect: *(Game+0x1E0) + 0xB0 + 0x2260.
constexpr uintptr_t kSecretSymbolBarnOff = 0x1C0;
constexpr uintptr_t kPlayerCtxOff = 0x1E0;
constexpr uintptr_t kPlayerCtxBody = 0xB0;
constexpr uintptr_t kPlayerPosOff = 0x2260;
constexpr uintptr_t kSymbolCountOff = 0x1010;
constexpr uintptr_t kSlotStride = 0x80;
constexpr uintptr_t kSlotXformOff = 0x18;
constexpr uintptr_t kSlotNodeOff = 0x30;
constexpr uintptr_t kSlotPendingOff = 0x85;
constexpr uintptr_t kSlotActiveOff = 0x86;
constexpr uintptr_t kXformPosOff = 0x10;
constexpr uintptr_t kNodeTransOff = 0x570;

bool LikelyHeapPtr(uintptr_t p) {
  return p >= 0x10000ull && p <= 0x00007FFFFFFFFFFFull && (p & 7ull) == 0;
}

bool ReadPlayerPos(uintptr_t game, float* x, float* y, float* z) {
  if (!LikelyHeapPtr(game) || !x || !y || !z) return false;
  uintptr_t ctx = *(uintptr_t*)(game + kPlayerCtxOff);
  if (!LikelyHeapPtr(ctx)) return false;
  float* pos = (float*)(ctx + kPlayerCtxBody + kPlayerPosOff);
  float px = pos[0], py = pos[1], pz = pos[2];
  if (!std::isfinite(px) || !std::isfinite(py) || !std::isfinite(pz)) return false;
  if (px == 0.f && py == 0.f && pz == 0.f) return false;
  *x = px;
  *y = py;
  *z = pz;
  return true;
}

void WritePos3(uintptr_t addr, float x, float y, float z) {
  float* p = (float*)addr;
  p[0] = x;
  p[1] = y;
  p[2] = z;
}

void RefreshAttractFromConfig() {
  DWORD now = GetTickCount();
  if (now - gLastAttractCfg < 400) return;
  gLastAttractCfg = now;
  Config c{};
  if (LoadConfig(c)) gAttractGlyphs.store(c.attractGlyphs, std::memory_order_relaxed);
}

void PullSecretSymbols(uintptr_t game) {
  if (!LikelyHeapPtr(game)) return;
  float x = 0, y = 0, z = 0;
  bool havePos = ReadPlayerPos(game, &x, &y, &z);
  const float destY = y + 0.35f;

  uintptr_t barn = *(uintptr_t*)(game + kSecretSymbolBarnOff);
  if (!LikelyHeapPtr(barn)) return;

  int32_t count = *(int32_t*)(barn + kSymbolCountOff);
  if (count < 0) count = 0;
  if (count > 32) count = 32;

  int moved = 0, active = 0, forced = 0, scanned = 0;
  uintptr_t sampleXform = 0, sampleNode = 0;

  for (int i = 0; i < 32; ++i) {
    if (count > 0 && i >= count) break;
    uintptr_t slot = barn + (uintptr_t)i * kSlotStride;
    uint8_t isActive = *(uint8_t*)(slot + kSlotActiveOff);
    uintptr_t xform = *(uintptr_t*)(slot + kSlotXformOff);
    uintptr_t node = *(uintptr_t*)(slot + kSlotNodeOff);
    if (!isActive) continue;
    active++;
    scanned++;

    if (!sampleXform && LikelyHeapPtr(xform)) sampleXform = xform;
    if (!sampleNode && LikelyHeapPtr(node)) sampleNode = node;

    // Force collect path: barn tick collects whenever +0x85!=0 (even if far).
    *(uint8_t*)(slot + kSlotPendingOff) = 1;
    forced++;

    if (!havePos) continue;
    bool did = false;
    if (LikelyHeapPtr(xform)) {
      WritePos3(xform + kXformPosOff, x, destY, z);
      did = true;
    }
    if (LikelyHeapPtr(node)) {
      WritePos3(node + kNodeTransOff, x, destY, z);
      did = true;
    }
    if (did) moved++;
  }

  gAttractMoved = moved;
  DWORD now = GetTickCount();
  if (now - gLastAttractDbg > 1500) {
    gLastAttractDbg = now;
    auto path = GameDir() + L"\\mos_attract_debug.txt";
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
      char line[320];
      int n = snprintf(
          line, sizeof(line),
          "mem vacuum moved=%d forced=%d active=%d scanned=%d count=%d havePos=%d "
          "barn=%p xform=%p node=%p pos=%.2f,%.2f,%.2f\r\n",
          moved, forced, active, scanned, (int)count, (int)havePos, (void*)barn,
          (void*)sampleXform, (void*)sampleNode, x, y, z);
      DWORD w = 0;
      if (n > 0) WriteFile(f, line, (DWORD)n, &w, nullptr);
      CloseHandle(f);
    }
  }
}

void Log(const wchar_t* msg) {
  auto dir = GameDir();
  if (dir.empty()) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    dir = path;
    auto slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir.resize(slash);
  }
  auto path = dir + L"\\journeymos.log";
  HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) return;
  SYSTEMTIME st{};
  GetLocalTime(&st);
  char line[256];
  int n = snprintf(line, sizeof(line), "[%02u:%02u:%02u] ", st.wHour, st.wMinute, st.wSecond);
  DWORD w = 0;
  if (n > 0) WriteFile(f, line, (DWORD)n, &w, nullptr);
  int need = WideCharToMultiByte(CP_UTF8, 0, msg, -1, nullptr, 0, nullptr, nullptr);
  if (need > 1) {
    std::string utf8((size_t)need - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, msg, -1, utf8.data(), need, nullptr, nullptr);
    WriteFile(f, utf8.data(), (DWORD)utf8.size(), &w, nullptr);
  }
  WriteFile(f, "\r\n", 2, &w, nullptr);
  CloseHandle(f);
}

uint8_t* ModuleBase(size_t* outSize) {
  HMODULE mod = GetModuleHandleW(L"Journey.exe");
  if (!mod) mod = GetModuleHandleW(nullptr);
  if (!mod) return nullptr;
  auto* dos = (IMAGE_DOS_HEADER*)mod;
  auto* nt = (IMAGE_NT_HEADERS*)((uint8_t*)mod + dos->e_lfanew);
  if (outSize) *outSize = nt->OptionalHeader.SizeOfImage;
  return (uint8_t*)mod;
}

uint8_t* FindPattern(uint8_t* base, size_t size, const char* pat) {
  std::vector<int> bytes;
  for (const char* p = pat; *p;) {
    while (*p == ' ') ++p;
    if (!*p) break;
    if (p[0] == '?' && (p[1] == '?' || p[1] == '\0' || p[1] == ' ')) {
      bytes.push_back(-1);
      p += (p[1] == '?') ? 2 : 1;
    } else {
      char buf[3] = {p[0], p[1], 0};
      bytes.push_back((int)strtoul(buf, nullptr, 16));
      p += 2;
    }
  }
  if (bytes.empty() || size < bytes.size()) return nullptr;
  for (size_t i = 0; i + bytes.size() <= size; ++i) {
    bool ok = true;
    for (size_t j = 0; j < bytes.size(); ++j) {
      if (bytes[j] >= 0 && base[i + j] != (uint8_t)bytes[j]) {
        ok = false;
        break;
      }
    }
    if (ok) return base + i;
  }
  return nullptr;
}

__int64 __fastcall HookedGameUpdate(__int64 game, float dt) {
  __int64 r = gGameUpdateOrig(game, dt);
  if (game) {
    gGame.store((uintptr_t)game, std::memory_order_relaxed);
    uintptr_t mm = *(uintptr_t*)(game + 0x180);
    if (mm) gMatchmaker.store(mm, std::memory_order_relaxed);

    RefreshAttractFromConfig();
    if (gAttractGlyphs.load(std::memory_order_relaxed)) {
      PullSecretSymbols((uintptr_t)game);
    }
  }
  return r;
}

bool InstallGameUpdateHook(uint8_t* target) {
  gTrampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (!gTrampoline) return false;

  memcpy(gGameUpdateStolen, target, kStolen);
  auto* tr = (uint8_t*)gTrampoline;
  memcpy(tr, gGameUpdateStolen, kStolen);
  tr[kStolen + 0] = 0xFF;
  tr[kStolen + 1] = 0x25;
  *(uint32_t*)(tr + kStolen + 2) = 0;
  *(uint64_t*)(tr + kStolen + 6) = (uint64_t)(target + kStolen);
  gGameUpdateOrig = (GameUpdateFn)gTrampoline;

  DWORD old = 0;
  if (!VirtualProtect(target, kStolen, PAGE_EXECUTE_READWRITE, &old)) return false;
  target[0] = 0xFF;
  target[1] = 0x25;
  *(uint32_t*)(target + 2) = 0;
  *(uint64_t*)(target + 6) = (uint64_t)&HookedGameUpdate;
  for (size_t i = 14; i < kStolen; ++i) target[i] = 0x90;
  VirtualProtect(target, kStolen, old, &old);
  FlushInstructionCache(GetCurrentProcess(), target, kStolen);
  gGameUpdateAddr = target;
  return true;
}

void RemoveGameUpdateHook() {
  if (!gGameUpdateAddr) return;
  DWORD old = 0;
  VirtualProtect(gGameUpdateAddr, kStolen, PAGE_EXECUTE_READWRITE, &old);
  memcpy(gGameUpdateAddr, gGameUpdateStolen, kStolen);
  VirtualProtect(gGameUpdateAddr, kStolen, old, &old);
  FlushInstructionCache(GetCurrentProcess(), gGameUpdateAddr, kStolen);
  gGameUpdateAddr = nullptr;
  if (gTrampoline) {
    VirtualFree(gTrampoline, 0, MEM_RELEASE);
    gTrampoline = nullptr;
  }
  gGameUpdateOrig = nullptr;
}

std::string ReadNameAt(uintptr_t mm) {
  if (!mm) return {};
  const char* raw = nullptr;
  // Prefer direct string in Matchmaker (same as GetPartnerName lea target).
  raw = (const char*)(mm + gNameOff);
  std::string name;
  if (raw) {
    for (int i = 0; i < 64 && raw[i]; ++i) {
      unsigned char c = (unsigned char)raw[i];
      if (c < 32 && c != 9) break;
      name.push_back((char)c);
    }
  }
  // Fallback to calling the game function.
  if (name.empty() && gGetPartnerName) {
    const char* p = gGetPartnerName(mm);
    if (p) {
      for (int i = 0; i < 64 && p[i]; ++i) name.push_back(p[i]);
    }
  }
  return name;
}

}  // namespace

bool Partner_Init() {
  size_t sz = 0;
  uint8_t* base = ModuleBase(&sz);
  if (!base) {
    Log(L"Partner: no Journey.exe module");
    return false;
  }

  auto* getName = FindPattern(
      base, sz, "80 B9 ?? ?? ?? ?? ?? 75 ?? 80 B9 ?? ?? ?? ?? ?? 48 8D 81");
  auto* gameUpdate =
      FindPattern(base, sz, "40 55 53 56 57 41 55 41 56 48 8D AC 24");

  wchar_t buf[128];
  swprintf(buf, 128, L"Partner: GetPartnerName=%p GameUpdate=%p", getName, gameUpdate);
  Log(buf);

  if (!getName) {
    Log(L"Partner: GetPartnerName pattern miss");
    return false;
  }

  // Decode lea rax,[rcx+imm32] right after the two cmp/jne bytes (offset +16).
  if (getName[16] == 0x48 && getName[17] == 0x8D && getName[18] == 0x81) {
    gNameOff = (uintptr_t)*(int32_t*)(getName + 19);
    swprintf(buf, 128, L"Partner: name offset 0x%X", (unsigned)gNameOff);
    Log(buf);
  }

  gGetPartnerName = (GetPartnerNameFn)getName;
  if (!gameUpdate || !InstallGameUpdateHook(gameUpdate)) {
    Log(L"Partner: GameUpdate hook failed (name read may still work later)");
    // Still mark ready so we can use GetPartnerName if matchmaker appears another way.
    gReady.store(true);
    return true;
  }
  gReady.store(true);
  Log(L"Partner: ready (live name)");
  return true;
}

void Partner_Shutdown() {
  gReady.store(false);
  RemoveGameUpdateHook();
  gGetPartnerName = nullptr;
  gMatchmaker.store(0);
  gGame.store(0);
  std::lock_guard lock(gNameMu);
  gCachedName.clear();
}

bool Partner_IsReady() { return gReady.load(); }

void Partner_SetAttractGlyphs(bool on) {
  gAttractGlyphs.store(on, std::memory_order_relaxed);
}

std::string Partner_GetLiveName() {
  if (!gReady.load()) return {};
  uintptr_t mm = gMatchmaker.load(std::memory_order_relaxed);
  if (!mm) return {};

  DWORD now = GetTickCount();
  if (now - gLastFetch < 300) {
    std::lock_guard lock(gNameMu);
    return gCachedName;
  }
  gLastFetch = now;

  std::string name = ReadNameAt(mm);
  {
    std::lock_guard lock(gNameMu);
    gCachedName = name;
  }
  return name;
}

}  // namespace mos
