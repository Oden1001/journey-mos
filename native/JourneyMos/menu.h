#pragma once
#include "pch.h"
#include "bridge.h"
#include "lang.h"

namespace mos {

struct MenuState {
  bool visible = false;
  bool dragging = false;
  POINT dragOffset{};
  float ox = 48.f;
  float oy = 48.f;
  float w = 480.f;
  float h = 700.f;

  Config cfg{};
  int tab = 0;
  float tabAnim = 0.f;  // lerped underline slot 0..3
  int scroll = 0;
  int maxScroll = 800;
  float clothSteps = 8.f;
  std::wstring statusLine = L"F8 · Alt · click tabs";
  std::wstring subtitleDraft = L"Hello Journey";
  bool editingSubtitle = false;
  UiLang lang = UiLang::Zh;
  int subtitlePos = 1;  // 0=top 1=center 2=bottom
};

class Menu {
 public:
  static Menu& Instance();

  bool Init(HWND host);
  void Shutdown();
  void Toggle();
  bool Visible() const { return state_.visible; }
  void SetVisible(bool v);
  void Focus();
  void SuppressIme(HWND gameHwnd);
  void EnableImeForEdit();

  void Render();
  void OnMouseMove(int x, int y);
  void OnLButtonDown(int x, int y);
  void OnLButtonUp(int x, int y);
  void OnMouseWheel(int delta);
  void OnChar(wchar_t ch);
  void OnKeyDown(WPARAM vk);
  void Tick(HWND gameHwnd);

  MenuState& State() { return state_; }
  HWND Hwnd() const { return hwnd_; }
  bool EditingText() const { return state_.editingSubtitle; }

 private:
  Menu() = default;
  void Draw();
  void DrawHeader();
  void DrawTabs(float y);
  void DrawConfig(float y0);
  void DrawActions(float y0);
  void DrawAnims(float y0);
  void DrawLevels(float y0);

  void MarkDirty();
  void FlushConfig(bool force);
  void ApplyNgPlusKit();
  void StopFarm();
  void SendSubtitle();
  void SetStatus(const wchar_t* text, DWORD holdMs = 1800);
  void HintIfHover(float x, float y, float w, float h, const wchar_t* tip);

  bool Button(float x, float y, float w, float h, const wchar_t* label, bool primary);
  bool ToggleRow(float x, float y, float w, const wchar_t* label, bool* value,
                 const wchar_t* tip = nullptr);
  void SliderRow(float x, float y, float w, const wchar_t* label, float* value,
                 float minV, float maxV, bool enabled = true);
  bool ConsumeClick(float rx, float ry, float rw, float rh);
  void SectionTitle(float x, float y, const wchar_t* text);
  void DrawTextField(float x, float y, float w, float h);
  void DrawScrollbar();
  void SetTab(int tab);
  void ScrollBy(int delta);
  void UpdateMaxScroll(float contentBottom);
  const UiStrings& T() const { return S(state_.lang); }
  void CycleLang();

  HWND hwnd_ = nullptr;
  ID2D1Factory* d2dFactory_ = nullptr;
  IDWriteFactory* dwrite_ = nullptr;
  ID2D1HwndRenderTarget* rt_ = nullptr;
  ID2D1SolidColorBrush* brushBg_ = nullptr;
  ID2D1SolidColorBrush* brushSurface_ = nullptr;
  ID2D1SolidColorBrush* brushAccent_ = nullptr;
  ID2D1SolidColorBrush* brushAccentSoft_ = nullptr;
  ID2D1SolidColorBrush* brushText_ = nullptr;
  ID2D1SolidColorBrush* brushMuted_ = nullptr;
  ID2D1SolidColorBrush* brushBtn_ = nullptr;
  ID2D1SolidColorBrush* brushBtnHi_ = nullptr;
  ID2D1SolidColorBrush* brushDanger_ = nullptr;
  ID2D1SolidColorBrush* brushTrack_ = nullptr;
  ID2D1SolidColorBrush* brushLine_ = nullptr;
  ID2D1SolidColorBrush* brushOk_ = nullptr;
  IDWriteTextFormat* fontTitle_ = nullptr;
  IDWriteTextFormat* fontBody_ = nullptr;
  IDWriteTextFormat* fontSmall_ = nullptr;
  IDWriteTextFormat* fontTab_ = nullptr;
  IDWriteTextFormat* fontBtn_ = nullptr;
  MenuState state_{};
  POINT mouse_{};
  bool hasClick_ = false;
  POINT clickPt_{};
  DWORD lastStatusPoll_ = 0;
  DWORD lastImePulse_ = 0;
  DWORD lastAnimTick_ = 0;
  bool cursorShown_ = false;
  bool configLoaded_ = false;
  bool imeDisabledProcess_ = false;
  HIMC savedImc_ = nullptr;

  bool configDirty_ = false;
  DWORD dirtyAt_ = 0;
  bool sliderDragging_ = false;
  bool clothSliderDirty_ = false;
  bool scrollDragging_ = false;
  float scrollDragGrabY_ = 0.f;
  DWORD statusHoldUntil_ = 0;
  float subtitleFieldY_ = 0.f;
  static constexpr DWORD kDebounceMs = 350;
  static constexpr float kHeaderH = 44.f;
  static constexpr float kTabH = 36.f;
  static constexpr float kFooterH = 34.f;
};

}  // namespace mos
