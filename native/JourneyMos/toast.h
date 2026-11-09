#pragma once
#include "pch.h"

namespace mos {

// Click-through toast using system fonts (supports Chinese via DirectWrite).
class Toast {
 public:
  static Toast& Instance();
  bool Init(HINSTANCE inst);
  void Shutdown();
  void Tick(HWND gameHwnd);
  // anchor: 0=top 1=center 2=bottom
  void Show(const std::wstring& text, float seconds, bool big, int anchor = 2);
  void ShowUtf8(const std::string& utf8, float seconds, bool big, int anchor = 2);
  void SetAnchor(int anchor);
  int Anchor() const { return anchor_; }
  // Poll mos_toast.txt: "duration [style]\ntext"
  void PollFile();

 private:
  Toast() = default;
  void PlaceOverGame(HWND gameHwnd);
  void Render();
  static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static int StyleToAnchor(const std::string& style);

  HWND hwnd_ = nullptr;
  ID2D1Factory* d2d_ = nullptr;
  IDWriteFactory* dwrite_ = nullptr;
  ID2D1HwndRenderTarget* rt_ = nullptr;
  ID2D1SolidColorBrush* brushText_ = nullptr;
  ID2D1SolidColorBrush* brushShadow_ = nullptr;
  ID2D1SolidColorBrush* brushBg_ = nullptr;
  IDWriteTextFormat* fontNormal_ = nullptr;
  IDWriteTextFormat* fontBig_ = nullptr;

  std::wstring text_;
  DWORD until_ = 0;
  bool big_ = false;
  int anchor_ = 2;  // bottom default
  ULONGLONG lastFileWrite_ = 0;
};

}  // namespace mos
