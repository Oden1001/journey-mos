#pragma once
#include "pch.h"

namespace mos {

// Click-through floating label — per-pixel alpha, no plate/box.
class NameTag {
 public:
  static NameTag& Instance();
  bool Init(HINSTANCE inst);
  void Shutdown();
  void Tick(HWND gameHwnd);

 private:
  NameTag() = default;
  bool EnsureTarget(int w, int h);
  void PaintToLayer(int screenX, int screenY);
  static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  HWND hwnd_ = nullptr;
  HINSTANCE inst_ = nullptr;
  ID2D1Factory* d2d_ = nullptr;
  IDWriteFactory* dwrite_ = nullptr;
  ID2D1DCRenderTarget* rt_ = nullptr;
  ID2D1SolidColorBrush* brushText_ = nullptr;
  ID2D1SolidColorBrush* brushShadow_ = nullptr;
  IDWriteTextFormat* font_ = nullptr;

  HDC memDc_ = nullptr;
  HBITMAP dib_ = nullptr;
  void* bits_ = nullptr;
  int dibW_ = 0;
  int dibH_ = 0;

  std::wstring name_ = L"同伴";
  std::wstring paintedName_;
  bool visible_ = false;
  bool bitmapDirty_ = true;
  int winW_ = 160;
  int winH_ = 40;
  int lastScreenX_ = INT_MIN;
  int lastScreenY_ = INT_MIN;
  DWORD lastNameRefresh_ = 0;
  bool hasLiveName_ = false;

  // Sticky W2S mode (0..3) + exponential position smoothing — stops mode
  // flip flicker and sub-pixel window thrash.
  int stickyMode_ = -1;
  bool smoothInit_ = false;
  float smoothSx_ = 0.f;
  float smoothSy_ = 0.f;
};

}  // namespace mos
