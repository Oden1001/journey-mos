#include "toast.h"
#include "bridge.h"
#include <algorithm>
#include <cmath>

namespace mos {
namespace {

template <typename T>
void SafeRelease(T*& p) {
  if (p) {
    p->Release();
    p = nullptr;
  }
}

std::wstring Utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
  if (n <= 0) return L"";
  std::wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), w.data(), n);
  return w;
}

const wchar_t* kToastClass = L"JourneyMosToastClass";

}  // namespace

Toast& Toast::Instance() {
  static Toast t;
  return t;
}

int Toast::StyleToAnchor(const std::string& style) {
  if (style.find("top") != std::string::npos) return 0;
  if (style.find("center") != std::string::npos || style.find("big") != std::string::npos)
    return 1;
  if (style.find("left") != std::string::npos || style.find("right") != std::string::npos)
    return 1;
  return 2;  // bottom
}

LRESULT CALLBACK Toast::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      Toast::Instance().Render();
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_NCHITTEST:
      return HTTRANSPARENT;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

bool Toast::Init(HINSTANCE inst) {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = Proc;
  wc.hInstance = inst;
  wc.lpszClassName = kToastClass;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }

  hwnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED,
      kToastClass, L"JourneyMosToast", WS_POPUP, 0, 0, 480, 72, nullptr, nullptr, inst,
      nullptr);
  if (!hwnd_) return false;
  SetLayeredWindowAttributes(hwnd_, 0, 245, LWA_ALPHA);

  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_);
  if (FAILED(hr)) return false;
  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                           reinterpret_cast<IUnknown**>(&dwrite_));
  if (FAILED(hr)) return false;

  hr = d2d_->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(480, 72)), &rt_);
  if (FAILED(hr)) return false;

  rt_->CreateSolidColorBrush(D2D1::ColorF(0.98f, 0.95f, 0.90f, 1.f), &brushText_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.85f), &brushShadow_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.09f, 0.08f, 0.92f), &brushBg_);

  auto makeFont = [&](float size, IDWriteTextFormat** out) {
    HRESULT r = dwrite_->CreateTextFormat(L"Microsoft YaHei UI", nullptr,
                                          DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                                          DWRITE_FONT_STRETCH_NORMAL, size, L"zh-cn", out);
    if (FAILED(r) || !*out) {
      dwrite_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size,
                                L"zh-cn", out);
    }
    if (*out) {
      (*out)->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
      (*out)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
  };
  makeFont(22.f, &fontNormal_);
  makeFont(30.f, &fontBig_);

  ShowWindow(hwnd_, SW_HIDE);
  return true;
}

void Toast::Shutdown() {
  SafeRelease(fontBig_);
  SafeRelease(fontNormal_);
  SafeRelease(brushBg_);
  SafeRelease(brushShadow_);
  SafeRelease(brushText_);
  SafeRelease(rt_);
  SafeRelease(dwrite_);
  SafeRelease(d2d_);
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

void Toast::SetAnchor(int anchor) {
  if (anchor < 0) anchor = 0;
  if (anchor > 2) anchor = 2;
  anchor_ = anchor;
}

void Toast::Show(const std::wstring& text, float seconds, bool big, int anchor) {
  text_ = text;
  big_ = big;
  SetAnchor(anchor);
  until_ = GetTickCount() + (DWORD)(std::max(0.5f, seconds) * 1000.f);
  if (hwnd_) {
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void Toast::ShowUtf8(const std::string& utf8, float seconds, bool big, int anchor) {
  Show(Utf8ToWide(utf8), seconds, big, anchor);
}

void Toast::PollFile() {
  auto path = GameDir() + L"\\mos_toast.txt";
  WIN32_FILE_ATTRIBUTE_DATA fad{};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return;
  ULARGE_INTEGER li{};
  li.LowPart = fad.ftLastWriteTime.dwLowDateTime;
  li.HighPart = fad.ftLastWriteTime.dwHighDateTime;
  if (li.QuadPart == lastFileWrite_) return;
  lastFileWrite_ = li.QuadPart;

  std::ifstream in(path.c_str(), std::ios::binary);
  if (!in) return;
  std::string line1, rest;
  std::getline(in, line1);
  rest.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  while (!rest.empty() && (rest.back() == '\n' || rest.back() == '\r')) rest.pop_back();
  float dur = 3.f;
  bool big = false;
  int anchor = anchor_;
  if (!line1.empty()) {
    float d = 0.f;
    char styleBuf[64]{};
    if (sscanf(line1.c_str(), "%f %63s", &d, styleBuf) >= 1) {
      if (d > 0.1f) dur = d;
      if (styleBuf[0]) {
        std::string st(styleBuf);
        if (st.find("big") != std::string::npos) big = true;
        anchor = StyleToAnchor(st);
      }
    }
  }
  if (!rest.empty()) ShowUtf8(rest, dur, big, anchor);
}

void Toast::PlaceOverGame(HWND gameHwnd) {
  if (!hwnd_ || !gameHwnd || !IsWindow(gameHwnd)) return;
  RECT rc{};
  GetClientRect(gameHwnd, &rc);
  POINT tl{0, 0};
  ClientToScreen(gameHwnd, &tl);
  int gw = rc.right - rc.left;
  int gh = rc.bottom - rc.top;
  if (gw < 100 || gh < 100) return;

  int boxW = (std::min)(720, gw * 78 / 100);
  int boxH = big_ ? 72 : 56;
  int x = tl.x + (gw - boxW) / 2;
  float yFrac = 0.78f;
  if (anchor_ == 0) yFrac = 0.10f;
  else if (anchor_ == 1) yFrac = 0.42f;
  int y = tl.y + (int)(gh * yFrac);
  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, boxW, boxH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
  if (rt_) rt_->Resize(D2D1::SizeU((UINT)boxW, (UINT)boxH));
}

void Toast::Tick(HWND gameHwnd) {
  PollFile();
  DWORD now = GetTickCount();
  if (until_ == 0 || now >= until_) {
    if (until_ != 0) {
      until_ = 0;
      text_.clear();
      if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
    }
    return;
  }
  PlaceOverGame(gameHwnd);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void Toast::Render() {
  if (!rt_ || text_.empty() || until_ == 0) return;
  RECT rc{};
  GetClientRect(hwnd_, &rc);
  float w = (float)(rc.right - rc.left);
  float h = (float)(rc.bottom - rc.top);
  if (w < 10 || h < 10) return;

  rt_->BeginDraw();
  rt_->Clear(D2D1::ColorF(0.10f, 0.09f, 0.08f, 0.f));
  rt_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0, 0, w, h), 10, 10), brushBg_);

  auto* font = big_ ? fontBig_ : fontNormal_;
  rt_->DrawText(text_.c_str(), (UINT32)text_.size(), font,
                D2D1::RectF(3, 3, w + 3, h + 3), brushShadow_);
  rt_->DrawText(text_.c_str(), (UINT32)text_.size(), font, D2D1::RectF(0, 0, w, h),
                brushText_);
  rt_->EndDraw();
}

}  // namespace mos
