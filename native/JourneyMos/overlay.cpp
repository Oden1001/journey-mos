#include "overlay.h"
#include "menu.h"
#include "bridge.h"
#include "nametag.h"
#include "partner.h"
#include <timeapi.h>

namespace mos {
namespace {

const wchar_t* kClass = L"JourneyMosOverlayClass";
HWND gOverlay = nullptr;
HWND gGame = nullptr;
std::atomic<bool> gRunning{false};
std::atomic<bool> gStarted{false};

HWND FindGameWindow() {
  struct Ctx {
    DWORD pid;
    HWND best;
    LONG bestArea;
  } ctx{GetCurrentProcessId(), nullptr, 0};

  EnumWindows(
      [](HWND hwnd, LPARAM lp) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(lp);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != c->pid) return TRUE;
        if (!IsWindowVisible(hwnd)) return TRUE;
        if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
        RECT rc{};
        GetClientRect(hwnd, &rc);
        LONG area = (rc.right - rc.left) * (rc.bottom - rc.top);
        if (area < 200 * 150) return TRUE;
        if (area > c->bestArea) {
          c->bestArea = area;
          c->best = hwnd;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&ctx));
  return ctx.best;
}

bool GameClientOrigin(POINT* out) {
  if (!gGame || !IsWindow(gGame) || !out) return false;
  out->x = 0;
  out->y = 0;
  return ClientToScreen(gGame, out) != FALSE;
}

// Panel-sized window only — never cover the whole game.
void PlacePanelWindow() {
  if (!gOverlay) return;
  auto& st = Menu::Instance().State();
  if (!st.visible || st.dragging) return;

  POINT origin{0, 0};
  if (!GameClientOrigin(&origin)) return;

  int x = origin.x + (int)st.ox;
  int y = origin.y + (int)st.oy;
  int w = (int)st.w;
  int h = (int)st.h;
  if (w < 200) w = 200;
  if (h < 200) h = 200;

  SetWindowPos(gOverlay, HWND_TOPMOST, x, y, w, h,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void CapturePanelOffsetFromWindow() {
  if (!gOverlay || !gGame) return;
  POINT origin{0, 0};
  if (!GameClientOrigin(&origin)) return;
  RECT wr{};
  GetWindowRect(gOverlay, &wr);
  auto& st = Menu::Instance().State();
  st.ox = (float)(wr.left - origin.x);
  st.oy = (float)(wr.top - origin.y);
}

void ApplyMenuVisibilitySideEffects(bool visible) {
  if (visible) {
    Menu::Instance().SuppressIme(gGame);
  } else if (gGame && IsWindow(gGame)) {
    SetForegroundWindow(gGame);
  }
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  auto& menu = Menu::Instance();
  switch (msg) {
    case WM_CREATE:
      Menu::Instance().Init(hwnd);
      return 0;
    case WM_DESTROY:
      Menu::Instance().Shutdown();
      PostQuitMessage(0);
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      menu.Render();
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_MOUSEACTIVATE:
      return MA_ACTIVATE;
    case WM_MOUSEMOVE:
      menu.OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
      return 0;
    case WM_LBUTTONDOWN:
      SetCapture(hwnd);
      SetFocus(hwnd);
      menu.OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
      return 0;
    case WM_LBUTTONUP:
      ReleaseCapture();
      menu.OnLButtonUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
      CapturePanelOffsetFromWindow();
      menu.Render();
      return 0;
    case WM_MOUSEWHEEL:
      menu.OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_CHAR:
      menu.OnChar((wchar_t)wp);
      return 0;
    case WM_IME_CHAR:
      menu.OnChar((wchar_t)wp);
      return 0;
    case WM_KEYDOWN: {
      bool wasVis = menu.Visible();
      menu.OnKeyDown(wp);
      if (wasVis && !menu.Visible()) ApplyMenuVisibilitySideEffects(false);
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_NOTIFY:
    case WM_IME_SETCONTEXT:
    case WM_IME_REQUEST:
      // Must reach DefWindowProc so Chinese IME can commit characters.
      break;
    case WM_TIMER:
      if (wp == 1) {
        if (!gGame || !IsWindow(gGame)) gGame = FindGameWindow();

        static bool wasF8 = false;
        static bool wasAlt = false;
        bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;  // either Alt
        if (f8 && !wasF8) {
          menu.Toggle();
          ApplyMenuVisibilitySideEffects(menu.Visible());
          if (menu.Visible()) PlacePanelWindow();
        }
        wasF8 = f8;

        // Menu already open → Alt re-focuses it (game steals focus often).
        if (menu.Visible() && alt && !wasAlt) {
          menu.Focus();
          if (!menu.EditingText()) menu.SuppressIme(gGame);
          PlacePanelWindow();
        }
        wasAlt = alt;

        if (menu.Visible()) {
          PlacePanelWindow();
          menu.Tick(gGame);
          InvalidateRect(hwnd, nullptr, FALSE);
        }
        NameTag::Instance().Tick(gGame);
      }
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

bool CreateOverlayWindow() {
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = OverlayProc;
  wc.hInstance = inst;
  wc.lpszClassName = kClass;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }

  auto& st = Menu::Instance().State();
  gOverlay = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      kClass, L"Journey Mos", WS_POPUP, 40, 40, (int)st.w, (int)st.h, nullptr,
      nullptr, inst, nullptr);
  if (!gOverlay) return false;

  // ~120fps for companion name-tag; menu underline/caret also rides this timer.
  timeBeginPeriod(1);
  SetTimer(gOverlay, 1, 8, nullptr);
  ShowWindow(gOverlay, SW_HIDE);
  return true;
}

void MessageLoop() {
  MSG msg;
  while (gRunning && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}

void OverlayThreadMain() {
  for (int i = 0; i < 400 && gRunning; ++i) {
    gGame = FindGameWindow();
    if (gGame) break;
    Sleep(100);
  }

  if (!CreateOverlayWindow()) {
    OutputDebugStringW(L"[JourneyMos] CreateOverlayWindow failed\n");
    return;
  }
  Partner_Init();
  NameTag::Instance().Init(GetModuleHandleW(nullptr));
  Menu::Instance().SetVisible(false);
  MessageLoop();

  NameTag::Instance().Shutdown();
  Partner_Shutdown();
  timeEndPeriod(1);
  if (gOverlay) {
    DestroyWindow(gOverlay);
    gOverlay = nullptr;
  }
}

}  // namespace

void StartOverlayHost() {
  if (gStarted.exchange(true)) return;
  gRunning = true;
  std::thread([] {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    OverlayThreadMain();
    CoUninitialize();
    gRunning = false;
    gStarted = false;
  }).detach();
}

void StopOverlayHost() {
  gRunning = false;
  if (gOverlay) PostMessageW(gOverlay, WM_CLOSE, 0, 0);
}

}  // namespace mos

extern "C" __declspec(dllexport) void JourneyMos_Start(const wchar_t* gameDir) {
  if (gameDir && *gameDir) {
    mos::SetGameDir(gameDir);
  } else {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path);
    auto slash = p.find_last_of(L"\\/");
    if (slash != std::wstring::npos) p.resize(slash);
    mos::SetGameDir(p);
  }
  mos::StartOverlayHost();
}

extern "C" __declspec(dllexport) void JourneyMos_Stop() { mos::StopOverlayHost(); }
