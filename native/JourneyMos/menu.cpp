#include "menu.h"
#include "partner.h"
#include "anims_i18n.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef IACE_CHILDREN
#define IACE_CHILDREN 0x0001
#endif
#ifndef IACE_DEFAULT
#define IACE_DEFAULT 0x0010
#endif

namespace mos {
namespace {

template <typename T>
void SafeRelease(T*& p) {
  if (p) {
    p->Release();
    p = nullptr;
  }
}

bool InRect(float x, float y, float rx, float ry, float rw, float rh) {
  return x >= rx && y >= ry && x <= rx + rw && y <= ry + rh;
}

float Clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

float Lerpf(float a, float b, float t) { return a + (b - a) * t; }

void KillImeOnHwnd(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) return;
  ImmAssociateContext(hwnd, nullptr);
  ImmAssociateContextEx(hwnd, nullptr, IACE_CHILDREN);

  HIMC himc = ImmGetContext(hwnd);
  if (himc) {
    ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmSetOpenStatus(himc, FALSE);
    ImmReleaseContext(hwnd, himc);
  }
}

}  // namespace

Menu& Menu::Instance() {
  static Menu m;
  return m;
}

bool Menu::Init(HWND host) {
  hwnd_ = host;
  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory_);
  if (FAILED(hr)) return false;
  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                           reinterpret_cast<IUnknown**>(&dwrite_));
  if (FAILED(hr)) return false;

  RECT rc{};
  GetClientRect(hwnd_, &rc);
  UINT w = rc.right > 0 ? (UINT)rc.right : (UINT)state_.w;
  UINT h = rc.bottom > 0 ? (UINT)rc.bottom : (UINT)state_.h;
  hr = d2dFactory_->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(w, h)), &rt_);
  if (FAILED(hr)) return false;

  // Warm desert sand — matches Journey's scarf/dune palette.
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.07f, 0.06f, 1.f), &brushBg_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.13f, 0.11f, 0.10f, 1.f), &brushSurface_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.93f, 0.64f, 0.34f, 1.f), &brushAccent_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.93f, 0.64f, 0.34f, 0.18f), &brushAccentSoft_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.93f, 0.88f, 1.f), &brushText_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.51f, 0.46f, 1.f), &brushMuted_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.19f, 0.17f, 0.15f, 1.f), &brushBtn_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.22f, 0.19f, 1.f), &brushBtnHi_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.74f, 0.34f, 0.28f, 1.f), &brushDanger_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.10f, 0.09f, 1.f), &brushTrack_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 0.07f), &brushLine_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.45f, 0.72f, 0.48f, 1.f), &brushOk_);

  auto makeFont = [&](const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight,
                      IDWriteTextFormat** out) {
    HRESULT r = dwrite_->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                          DWRITE_FONT_STRETCH_NORMAL, size, L"zh-cn", out);
    if (FAILED(r) || !*out) {
      dwrite_->CreateTextFormat(L"Segoe UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                DWRITE_FONT_STRETCH_NORMAL, size, L"zh-cn", out);
    }
  };
  makeFont(L"Microsoft YaHei UI", 15.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, &fontTitle_);
  makeFont(L"Microsoft YaHei UI", 12.5f, DWRITE_FONT_WEIGHT_NORMAL, &fontBody_);
  makeFont(L"Microsoft YaHei UI", 11.f, DWRITE_FONT_WEIGHT_NORMAL, &fontSmall_);
  makeFont(L"Microsoft YaHei UI", 12.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, &fontTab_);
  makeFont(L"Microsoft YaHei UI", 12.5f, DWRITE_FONT_WEIGHT_MEDIUM, &fontBtn_);
  if (fontTab_) {
    fontTab_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fontTab_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  }
  if (fontBtn_) {
    fontBtn_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fontBtn_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  }
  return true;
}

void Menu::Shutdown() {
  SafeRelease(fontBtn_);
  SafeRelease(fontTab_);
  SafeRelease(fontSmall_);
  SafeRelease(fontBody_);
  SafeRelease(fontTitle_);
  SafeRelease(brushOk_);
  SafeRelease(brushLine_);
  SafeRelease(brushTrack_);
  SafeRelease(brushDanger_);
  SafeRelease(brushBtnHi_);
  SafeRelease(brushBtn_);
  SafeRelease(brushMuted_);
  SafeRelease(brushText_);
  SafeRelease(brushAccentSoft_);
  SafeRelease(brushAccent_);
  SafeRelease(brushSurface_);
  SafeRelease(brushBg_);
  SafeRelease(rt_);
  SafeRelease(dwrite_);
  SafeRelease(d2dFactory_);
}

void Menu::SuppressIme(HWND gameHwnd) {
  if (state_.editingSubtitle) return;
  KillImeOnHwnd(hwnd_);
  KillImeOnHwnd(gameHwnd);
}

void Menu::EnableImeForEdit() {
  if (!hwnd_) return;
  ImmAssociateContextEx(hwnd_, nullptr, IACE_DEFAULT);
  HIMC himc = ImmGetContext(hwnd_);
  if (himc) {
    ImmSetOpenStatus(himc, TRUE);
    ImmReleaseContext(hwnd_, himc);
  } else {
    if (!savedImc_) savedImc_ = ImmCreateContext();
    if (savedImc_) {
      ImmAssociateContext(hwnd_, savedImc_);
      ImmSetOpenStatus(savedImc_, TRUE);
    }
  }
  SetFocus(hwnd_);
}

void Menu::Toggle() { SetVisible(!state_.visible); }

void Menu::SetStatus(const wchar_t* text, DWORD holdMs) {
  state_.statusLine = text;
  statusHoldUntil_ = GetTickCount() + holdMs;
}

void Menu::CycleLang() {
  state_.lang = NextLang(state_.lang);
  state_.cfg.uiLang = (int)state_.lang;
  configDirty_ = true;
  FlushConfig(true);
  SetStatus(T().langCycled, 1600);
}

void Menu::HintIfHover(float x, float y, float w, float h, const wchar_t* tip) {
  if (!tip || !*tip) return;
  if (GetTickCount() < statusHoldUntil_) return;
  if (InRect((float)mouse_.x, (float)mouse_.y, x, y, w, h)) {
    state_.statusLine = tip;
  }
}

void Menu::Focus() {
  if (!hwnd_ || !state_.visible) return;
  ShowWindow(hwnd_, SW_SHOW);
  SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  SetForegroundWindow(hwnd_);
  SetFocus(hwnd_);
  if (!cursorShown_) {
    ShowCursor(TRUE);
    cursorShown_ = true;
  }
  if (state_.editingSubtitle) EnableImeForEdit();
  else SuppressIme(nullptr);
  SetStatus(state_.editingSubtitle ? T().subtitleEdit : T().focused, 1200);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void Menu::SetVisible(bool v) {
  state_.visible = v;
  if (!hwnd_) return;
  if (v) {
    if (!configLoaded_) {
      LoadConfig(state_.cfg);
      Partner_SetAttractGlyphs(state_.cfg.attractGlyphs);
      configLoaded_ = true;
    }
    state_.lang = (UiLang)state_.cfg.uiLang;
    if ((int)state_.lang < 0 || (int)state_.lang > 2) state_.lang = UiLang::Zh;
    state_.tabAnim = (float)state_.tab;
    ShowWindow(hwnd_, SW_SHOW);
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, (int)state_.w, (int)state_.h,
                 SWP_NOMOVE | SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
    if (!cursorShown_) {
      ShowCursor(TRUE);
      cursorShown_ = true;
    }
    ImmAssociateContext(hwnd_, nullptr);
    SuppressIme(nullptr);
    SetStatus(T().openHint, 2500);
  } else {
    state_.editingSubtitle = false;
    FlushConfig(true);
    if (clothSliderDirty_) {
      CmdClothSet((int)std::lround(state_.clothSteps));
      clothSliderDirty_ = false;
    }
    ShowWindow(hwnd_, SW_HIDE);
    if (cursorShown_) {
      ShowCursor(FALSE);
      cursorShown_ = false;
    }
    ImmAssociateContext(hwnd_, nullptr);
  }
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void Menu::OnMouseMove(int x, int y) {
  mouse_ = {x, y};
  if (state_.dragging) {
    POINT scr{};
    GetCursorPos(&scr);
    SetWindowPos(hwnd_, HWND_TOPMOST, scr.x - state_.dragOffset.x,
                 scr.y - state_.dragOffset.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
  }
  if (scrollDragging_ && state_.maxScroll > 0) {
    float trackTop = kHeaderH + kTabH + 8.f;
    float trackBot = state_.h - kFooterH - 4.f;
    float trackH = trackBot - trackTop;
    float viewH = state_.h - kHeaderH - kTabH - kFooterH - 20.f;
    float contentH = viewH + (float)state_.maxScroll;
    float thumbH = Clampf(trackH * (viewH / contentH), 28.f, trackH);
    float usable = trackH - thumbH;
    if (usable > 1.f) {
      float rel = Clampf((float)y - trackTop - scrollDragGrabY_, 0.f, usable);
      state_.scroll = (int)std::lround((rel / usable) * (float)state_.maxScroll);
      InvalidateRect(hwnd_, nullptr, FALSE);
    }
  }
}

void Menu::OnLButtonDown(int x, int y) {
  mouse_ = {x, y};

  // F8 close chip (top-right)
  float f8X = state_.w - 44.f;
  if (InRect((float)x, (float)y, f8X, 8.f, 36.f, 24.f)) {
    SetVisible(false);
    return;
  }
  // Language chip
  float langX = state_.w - 108.f;
  if (InRect((float)x, (float)y, langX, 8.f, 58.f, 24.f)) {
    CycleLang();
    InvalidateRect(hwnd_, nullptr, FALSE);
    return;
  }

  // Scrollbar grab
  if (state_.maxScroll > 0) {
    float trackTop = kHeaderH + kTabH + 8.f;
    float trackBot = state_.h - kFooterH - 4.f;
    float trackH = trackBot - trackTop;
    float viewH = state_.h - kHeaderH - kTabH - kFooterH - 20.f;
    float contentH = viewH + (float)state_.maxScroll;
    float thumbH = Clampf(trackH * (viewH / contentH), 28.f, trackH);
    float t = (float)state_.scroll / (float)state_.maxScroll;
    float thumbY = trackTop + t * (trackH - thumbH);
    float sx = state_.w - 12.f;
    if (InRect((float)x, (float)y, sx - 4.f, thumbY, 14.f, thumbH)) {
      scrollDragging_ = true;
      scrollDragGrabY_ = (float)y - thumbY;
      return;
    }
    if (InRect((float)x, (float)y, sx - 4.f, trackTop, 14.f, trackH)) {
      float ratio = Clampf(((float)y - trackTop - thumbH * 0.5f) / (trackH - thumbH), 0.f, 1.f);
      state_.scroll = (int)std::lround(ratio * (float)state_.maxScroll);
      InvalidateRect(hwnd_, nullptr, FALSE);
      return;
    }
  }

  // Title-bar drag (leave room for lang + F8 chips)
  if (InRect((float)x, (float)y, 0.f, 0.f, state_.w - 120.f, kHeaderH)) {
    state_.dragging = true;
    POINT scr{};
    GetCursorPos(&scr);
    RECT wr{};
    GetWindowRect(hwnd_, &wr);
    state_.dragOffset.x = scr.x - wr.left;
    state_.dragOffset.y = scr.y - wr.top;
  }
}

void Menu::OnLButtonUp(int x, int y) {
  bool wasDrag = state_.dragging || scrollDragging_;
  state_.dragging = false;
  scrollDragging_ = false;
  if (sliderDragging_) {
    sliderDragging_ = false;
    MarkDirty();
    if (clothSliderDirty_) {
      CmdClothSet((int)std::lround(state_.clothSteps));
      clothSliderDirty_ = false;
      SetStatus(T().clothApplied);
    }
  }
  mouse_ = {x, y};
  // Include tab strip so page buttons receive clicks (was excluding tabs → only ←→ worked).
  if (!wasDrag && y >= (int)kHeaderH) {
    hasClick_ = true;
    clickPt_ = {x, y};
  } else {
    hasClick_ = false;
  }
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void Menu::ScrollBy(int delta) {
  state_.scroll = (int)Clampf((float)(state_.scroll + delta), 0.f, (float)state_.maxScroll);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void Menu::SetTab(int tab) {
  if (tab < 0 || tab > 3 || tab == state_.tab) return;
  state_.tab = tab;
  state_.scroll = 0;
  state_.editingSubtitle = false;
  SuppressIme(nullptr);
  const wchar_t* names[] = {T().tabConfig, T().tabActions, T().tabAnims, T().tabLevels};
  SetStatus(names[tab], 1200);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void Menu::OnMouseWheel(int delta) {
  if (state_.editingSubtitle) return;
  ScrollBy(delta > 0 ? -48 : 48);
}

void Menu::OnChar(wchar_t ch) {
  if (!state_.editingSubtitle) return;
  if (ch == L'\r' || ch == L'\n') {
    SendSubtitle();
    return;
  }
  if (ch == L'\b') {
    if (!state_.subtitleDraft.empty()) state_.subtitleDraft.pop_back();
    InvalidateRect(hwnd_, nullptr, FALSE);
    return;
  }
  if (ch < 32 && ch != L'\t') return;
  if (state_.subtitleDraft.size() >= 80) return;
  state_.subtitleDraft.push_back(ch);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void Menu::OnKeyDown(WPARAM vk) {
  if (state_.editingSubtitle) {
    if (vk == VK_BACK) {
      if (!state_.subtitleDraft.empty()) state_.subtitleDraft.pop_back();
      InvalidateRect(hwnd_, nullptr, FALSE);
    } else if (vk == VK_ESCAPE) {
      state_.editingSubtitle = false;
      SuppressIme(nullptr);
      SetStatus(T().subtitleCancel);
      InvalidateRect(hwnd_, nullptr, FALSE);
    } else if (vk == VK_RETURN) {
      SendSubtitle();
    }
    return;
  }
  if (vk == VK_ESCAPE) {
    SetVisible(false);
    return;
  }
  if (vk == '1' || vk == VK_NUMPAD1) SetTab(0);
  else if (vk == '2' || vk == VK_NUMPAD2) SetTab(1);
  else if (vk == '3' || vk == VK_NUMPAD3) SetTab(2);
  else if (vk == '4' || vk == VK_NUMPAD4) SetTab(3);
  else if (vk == VK_LEFT) SetTab(state_.tab - 1);
  else if (vk == VK_RIGHT) SetTab(state_.tab + 1);
  else if (vk == VK_PRIOR) ScrollBy(-220);
  else if (vk == VK_NEXT) ScrollBy(220);
  else if (vk == VK_HOME) {
    state_.scroll = 0;
    InvalidateRect(hwnd_, nullptr, FALSE);
  } else if (vk == VK_END) {
    state_.scroll = state_.maxScroll;
    InvalidateRect(hwnd_, nullptr, FALSE);
  } else if (vk == VK_UP) ScrollBy(-36);
  else if (vk == VK_DOWN) ScrollBy(36);
}

void Menu::SendSubtitle() {
  if (state_.subtitleDraft.empty()) state_.subtitleDraft = L"Journey Mos";
  const char* style = "center";
  if (state_.subtitlePos == 0) style = "top";
  else if (state_.subtitlePos == 2) style = "bottom";
  // Game built-in DisplayText only (no overlay). Font is Latin-oriented.
  CmdShowTextW(state_.subtitleDraft.c_str(), style, 4.0);
  SetStatus(T().subtitleSent, 2200);
  state_.editingSubtitle = false;
  SuppressIme(nullptr);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

bool Menu::ConsumeClick(float rx, float ry, float rw, float rh) {
  if (!hasClick_) return false;
  if (!InRect((float)clickPt_.x, (float)clickPt_.y, rx, ry, rw, rh)) return false;
  hasClick_ = false;
  return true;
}

void Menu::MarkDirty() {
  configDirty_ = true;
  dirtyAt_ = GetTickCount();
  if (GetTickCount() >= statusHoldUntil_) state_.statusLine = T().pendingSync;
}

void Menu::FlushConfig(bool force) {
  if (!configDirty_) return;
  if (!force && (GetTickCount() - dirtyAt_ < kDebounceMs)) return;
  state_.cfg.uiLang = (int)state_.lang;
  if (!WriteConfig(state_.cfg)) {
    SetStatus(T().syncFail, 2500);
    return;
  }
  Partner_SetAttractGlyphs(state_.cfg.attractGlyphs);
  configDirty_ = false;
  SetStatus(T().synced, 1600);
}

void Menu::ApplyNgPlusKit() {
  auto& c = state_.cfg;
  c.infiniteEnergy = true;
  c.speedBoost = true;
  c.speedMult = std::max(c.speedMult, 4.f);
  c.flyBoost = true;
  c.flyBoostMult = std::max(c.flyBoostMult, 2.f);
  c.skipCinematic = true;
  c.disableAutoLobby = true;
  c.gameSpeed = std::max(c.gameSpeed, 3.f);
  c.autoFarm = true;
  if (c.farmInterval < 4.f) c.farmInterval = 12.f;
  configDirty_ = true;
  FlushConfig(true);
  CmdOp("ng_plus_kit");
  SetStatus(T().farmOn);
}

void Menu::StopFarm() {
  state_.cfg.autoFarm = false;
  configDirty_ = true;
  FlushConfig(true);
  CmdOp("stop_farm");
  SetStatus(T().farmOff);
}

void Menu::UpdateMaxScroll(float contentBottom) {
  float viewBot = state_.h - kFooterH - 8.f;
  state_.maxScroll = std::max(0, (int)contentBottom + state_.scroll - (int)viewBot);
}

bool Menu::Button(float x, float y, float w, float h, const wchar_t* label, bool primary) {
  bool hover = InRect((float)mouse_.x, (float)mouse_.y, x, y, w, h);
  auto* br = primary ? brushAccent_ : (hover ? brushBtnHi_ : brushBtn_);
  rt_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), 5, 5), br);
  if (hover && !primary) {
    rt_->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), 5, 5),
                              brushLine_, 1.f);
  }
  auto* fmt = fontBtn_ ? fontBtn_ : fontBody_;
  rt_->DrawText(label, (UINT32)wcslen(label), fmt, D2D1::RectF(x, y, x + w, y + h),
                primary ? brushBg_ : brushText_);
  return ConsumeClick(x, y, w, h);
}

bool Menu::ToggleRow(float x, float y, float w, const wchar_t* label, bool* value,
                     const wchar_t* tip) {
  float h = 32.f;
  bool hover = InRect((float)mouse_.x, (float)mouse_.y, x, y, w, h);
  if (hover) {
    rt_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), 5, 5),
                              brushAccentSoft_);
  }
  HintIfHover(x, y, w, h, tip);

  rt_->DrawText(label, (UINT32)wcslen(label), fontBody_,
                D2D1::RectF(x + 12, y + 6, x + w - 72, y + h - 4), brushText_);

  float sw = 42.f, sh = 22.f;
  float sx = x + w - sw - 12.f;
  float sy = y + (h - sh) * 0.5f;
  rt_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(sx, sy, sx + sw, sy + sh), 11, 11),
                            *value ? brushAccent_ : brushTrack_);
  float kx = *value ? (sx + sw - 11.f) : (sx + 11.f);
  rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx, sy + sh * 0.5f), 7.5f, 7.5f), brushText_);
  return ConsumeClick(x, y, w, h);
}

void Menu::SliderRow(float x, float y, float w, const wchar_t* label, float* value,
                     float minV, float maxV, bool enabled) {
  wchar_t buf[96];
  // Prefer compact integers when range is coarse.
  bool asInt = (maxV - minV) >= 8.f || (value == &state_.clothSteps);
  if (asInt)
    swprintf(buf, 96, L"%s   %d", label, (int)std::lround(*value));
  else
    swprintf(buf, 96, L"%s   %.2f", label, *value);

  auto* labBrush = enabled ? brushMuted_ : brushLine_;
  rt_->DrawText(buf, (UINT32)wcslen(buf), fontSmall_,
                D2D1::RectF(x + 12, y, x + w - 12, y + 16), labBrush);

  float trackY = y + 22;
  float trackH = 4;
  rt_->FillRoundedRectangle(
      D2D1::RoundedRect(D2D1::RectF(x + 12, trackY, x + w - 12, trackY + trackH), 2, 2),
      brushTrack_);
  float t = Clampf((*value - minV) / (maxV - minV), 0.f, 1.f);
  float fillR = x + 12 + t * (w - 24);
  if (fillR > x + 14) {
    rt_->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(x + 12, trackY, fillR, trackY + trackH), 2, 2),
        enabled ? brushAccent_ : brushMuted_);
  }
  float knobX = x + 12 + t * (w - 24);
  rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knobX, trackY + trackH * 0.5f), 7, 7),
                   enabled ? brushAccent_ : brushMuted_);
  rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knobX, trackY + trackH * 0.5f), 3, 3),
                   brushText_);

  if (!enabled) return;

  bool over = InRect((float)mouse_.x, (float)mouse_.y, x + 12, trackY - 10, w - 24, trackH + 20);
  bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  if (over && down) {
    float nt = Clampf((mouse_.x - (x + 12)) / (w - 24), 0.f, 1.f);
    float nv = minV + nt * (maxV - minV);
    if (asInt) nv = (float)std::lround(nv);
    if (fabsf(nv - *value) > 0.001f) {
      *value = nv;
      sliderDragging_ = true;
      if (value == &state_.clothSteps) clothSliderDirty_ = true;
      else MarkDirty();
    }
  }
  if (ConsumeClick(x + 12, trackY - 10, w - 24, trackH + 20)) {
    float nt = Clampf((clickPt_.x - (x + 12)) / (float)(w - 24), 0.f, 1.f);
    *value = minV + nt * (maxV - minV);
    if (asInt) *value = (float)std::lround(*value);
    if (value == &state_.clothSteps) {
      clothSliderDirty_ = true;
      CmdClothSet((int)std::lround(state_.clothSteps));
      clothSliderDirty_ = false;
      SetStatus(T().clothApplied);
    } else {
      MarkDirty();
    }
  }
}

void Menu::SectionTitle(float x, float y, const wchar_t* text) {
  rt_->FillRectangle(D2D1::RectF(x, y + 4, x + 2.5f, y + 15), brushAccent_);
  rt_->DrawText(text, (UINT32)wcslen(text), fontSmall_,
                D2D1::RectF(x + 10, y, x + 400, y + 18), brushMuted_);
}

void Menu::DrawScrollbar() {
  if (state_.maxScroll <= 0) return;
  float trackTop = kHeaderH + kTabH + 8.f;
  float trackBot = state_.h - kFooterH - 4.f;
  float trackH = trackBot - trackTop;
  if (trackH < 40.f) return;
  float viewH = state_.h - kHeaderH - kTabH - kFooterH - 20.f;
  float contentH = viewH + (float)state_.maxScroll;
  float thumbH = Clampf(trackH * (viewH / contentH), 28.f, trackH);
  float t = (float)state_.scroll / (float)state_.maxScroll;
  float thumbY = trackTop + t * (trackH - thumbH);
  float x = state_.w - 9.f;
  rt_->FillRoundedRectangle(
      D2D1::RoundedRect(D2D1::RectF(x, trackTop, x + 4.f, trackBot), 2.f, 2.f), brushTrack_);
  rt_->FillRoundedRectangle(
      D2D1::RoundedRect(D2D1::RectF(x, thumbY, x + 4.f, thumbY + thumbH), 2.f, 2.f),
      brushAccent_);
}

void Menu::DrawTextField(float x, float y, float w, float h) {
  bool focus = state_.editingSubtitle;
  rt_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), 5, 5),
                            focus ? brushAccentSoft_ : brushTrack_);
  rt_->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), 5, 5),
                            focus ? brushAccent_ : brushLine_, 1.2f);

  std::wstring shown = state_.subtitleDraft;
  if (focus && ((GetTickCount() / 500) % 2 == 0)) shown.push_back(L'|');
  const wchar_t* placeholder = T().subtitleHint;
  if (shown.empty() && !focus) shown = placeholder;
  rt_->DrawText(shown.c_str(), (UINT32)shown.size(), fontBody_,
                D2D1::RectF(x + 10, y + 8, x + w - 10, y + h - 6),
                (!focus && shown == placeholder) ? brushMuted_ : brushText_);

  if (ConsumeClick(x, y, w, h)) {
    state_.editingSubtitle = true;
    EnableImeForEdit();
    SetFocus(hwnd_);
    SetStatus(T().subtitleEdit);
  }
}

void Menu::Tick(HWND gameHwnd) {
  DWORD now = GetTickCount();
  FlushConfig(false);

  if (state_.visible && now - lastImePulse_ > 400) {
    lastImePulse_ = now;
    if (state_.editingSubtitle) EnableImeForEdit();
    else SuppressIme(gameHwnd);
  }

  if (state_.visible) {
    float dt = lastAnimTick_ ? Clampf((now - lastAnimTick_) / 1000.f, 0.f, 0.05f) : 0.016f;
    lastAnimTick_ = now;
    float target = (float)state_.tab;
    float alpha = 1.f - expf(-18.f * dt);
    state_.tabAnim = Lerpf(state_.tabAnim, target, alpha);
  }

  if (now - lastStatusPoll_ > 900) {
    lastStatusPoll_ = now;
    if (!configDirty_ && !state_.editingSubtitle && now >= statusHoldUntil_) {
      auto st = ReadStatus();
      if (st.ok) {
        wchar_t buf[240];
        std::wstring level(st.level.begin(), st.level.end());
        auto& t = T();
        if (st.farm)
          swprintf(buf, 240, L"%s · %s%s · ×%.1f · t×%.1f · %s", level.c_str(), t.statusEnergy,
                   st.energy ? t.on : t.off, st.speedMult, st.gameSpeed, t.statusFarming);
        else
          swprintf(buf, 240, L"%s · %s%s · ×%.1f · t×%.1f", level.c_str(), t.statusEnergy,
                   st.energy ? t.on : t.off, st.speedMult, st.gameSpeed);
        state_.statusLine = buf;
      }
    }
  }
}

void Menu::Render() {
  if (!rt_ || !state_.visible) return;
  RECT rc{};
  GetClientRect(hwnd_, &rc);
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
  if (size.width > 0 && size.height > 0) rt_->Resize(size);

  rt_->BeginDraw();
  rt_->Clear(D2D1::ColorF(0.08f, 0.07f, 0.06f, 1.f));
  Draw();
  rt_->EndDraw();
  hasClick_ = false;
}

void Menu::DrawHeader() {
  float w = state_.w;
  rt_->DrawText(L"Journey Mos", 11, fontTitle_, D2D1::RectF(16, 11, w - 160, 34), brushText_);

  if (configDirty_) {
    rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(w - 118.f, 22.f), 3.5f, 3.5f), brushAccent_);
  }

  // Language chip
  float langX = w - 108.f;
  bool langHover = InRect((float)mouse_.x, (float)mouse_.y, langX, 8.f, 58.f, 24.f);
  if (langHover) {
    rt_->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(langX, 8.f, langX + 58.f, 32.f), 4, 4), brushBtnHi_);
  }
  const wchar_t* langLabel = LangCode(state_.lang);
  auto* fmt = fontBtn_ ? fontBtn_ : fontSmall_;
  rt_->DrawText(langLabel, (UINT32)wcslen(langLabel), fmt,
                D2D1::RectF(langX, 8.f, langX + 58.f, 32.f),
                langHover ? brushText_ : brushMuted_);

  // F8 close chip (replaces ×)
  float f8X = w - 44.f;
  bool f8Hover = InRect((float)mouse_.x, (float)mouse_.y, f8X, 8.f, 36.f, 24.f);
  if (f8Hover) {
    rt_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(f8X, 8.f, f8X + 36.f, 32.f), 4, 4),
                              brushBtnHi_);
  }
  rt_->DrawText(L"F8", 2, fmt, D2D1::RectF(f8X, 8.f, f8X + 36.f, 32.f),
                f8Hover ? brushText_ : brushMuted_);
  HintIfHover(f8X, 8.f, 36.f, 24.f, T().closeHint);
}

void Menu::DrawTabs(float y) {
  const wchar_t* tabs[] = {T().tabConfig, T().tabActions, T().tabAnims, T().tabLevels};
  const float pad = 12.f;
  float tw = (state_.w - pad * 2) / 4.f;
  for (int i = 0; i < 4; ++i) {
    float x = pad + i * tw;
    bool on = state_.tab == i;
    bool hover = InRect((float)mouse_.x, (float)mouse_.y, x + 2, y, tw - 4, 30);
    if (on) {
      rt_->FillRoundedRectangle(
          D2D1::RoundedRect(D2D1::RectF(x + 3, y + 1, x + tw - 3, y + 29), 6, 6),
          brushAccentSoft_);
    } else if (hover) {
      rt_->FillRoundedRectangle(
          D2D1::RoundedRect(D2D1::RectF(x + 3, y + 1, x + tw - 3, y + 29), 6, 6), brushBtn_);
    }
    auto* fmt = fontTab_ ? fontTab_ : fontBody_;
    rt_->DrawText(tabs[i], (UINT32)wcslen(tabs[i]), fmt,
                  D2D1::RectF(x, y, x + tw, y + 28),
                  on || hover ? brushText_ : brushMuted_);
    if (ConsumeClick(x, y, tw, 30)) SetTab(i);
  }
  float ux = pad + state_.tabAnim * tw + tw * 0.5f;
  float dist = fabsf(state_.tabAnim - (float)state_.tab);
  float barW = Lerpf(28.f, 16.f, Clampf(dist * 2.f, 0.f, 1.f));
  rt_->FillRoundedRectangle(
      D2D1::RoundedRect(D2D1::RectF(ux - barW * 0.5f, y + 30, ux + barW * 0.5f, y + 33), 1.5f,
                        1.5f),
      brushAccent_);
}

void Menu::DrawConfig(float y0) {
  float pad = 14.f;
  float innerW = state_.w - pad * 2 - 6.f;
  float cy = y0;

  auto emitTogs = [&](auto& list) {
    for (auto& t : list) {
      if (ToggleRow(pad, cy, innerW, t.label, t.ptr, t.tip)) {
        *t.ptr = !*t.ptr;
        MarkDirty();
        wchar_t buf[96];
        swprintf(buf, 96, L"%s · %s", t.label, *t.ptr ? T().on : T().off);
        SetStatus(buf);
      }
      cy += 34.f;
    }
  };

  struct Tog {
    const wchar_t* label;
    bool* ptr;
    const wchar_t* tip;
  };

  auto& t = T();
  SectionTitle(pad, cy, t.secMove);
  cy += 22.f;
  Tog moveTogs[] = {
      {t.infiniteEnergy, &state_.cfg.infiniteEnergy, t.tipEnergy},
      {t.speedBoost, &state_.cfg.speedBoost, t.tipSpeed},
      {t.flyBoost, &state_.cfg.flyBoost, t.tipFly},
      {t.noWind, &state_.cfg.noWind, t.tipWind},
  };
  emitTogs(moveTogs);

  cy += 6.f;
  SectionTitle(pad, cy, t.secOnline);
  cy += 22.f;
  Tog mpTogs[] = {
      {t.meetEverywhere, &state_.cfg.meetEverywhere, t.tipMeet},
      {t.disableLobby, &state_.cfg.disableAutoLobby, t.tipLobby},
      {t.companionName, &state_.cfg.showCompanionName, t.tipName},
  };
  emitTogs(mpTogs);

  cy += 6.f;
  SectionTitle(pad, cy, t.secOther);
  cy += 22.f;
  Tog miscTogs[] = {
      {t.skipCinematic, &state_.cfg.skipCinematic, t.tipSkip},
      {t.autoFarm, &state_.cfg.autoFarm, t.tipFarm},
      {t.forceGlow, &state_.cfg.forceGlow, t.tipGlow},
      {t.attractGlyphs, &state_.cfg.attractGlyphs, t.tipAttract},
  };
  emitTogs(miscTogs);

  cy += 10.f;
  SectionTitle(pad, cy, t.secValues);
  cy += 22.f;

  struct Sld {
    const wchar_t* label;
    float* ptr;
    float minV;
    float maxV;
    bool enabled;
  } slds[] = {
      {t.speedMult, &state_.cfg.speedMult, 1.f, 12.f, state_.cfg.speedBoost},
      {t.flyMult, &state_.cfg.flyBoostMult, 1.f, 8.f, state_.cfg.flyBoost},
      {t.timeMult, &state_.cfg.gameSpeed, 0.25f, 8.f, true},
      {t.farmInterval, &state_.cfg.farmInterval, 4.f, 60.f, state_.cfg.autoFarm},
  };
  for (auto& s : slds) {
    SliderRow(pad, cy, innerW, s.label, s.ptr, s.minV, s.maxV, s.enabled);
    if (s.ptr == &state_.cfg.speedMult && *s.ptr > 1.05f) state_.cfg.speedBoost = true;
    if (s.ptr == &state_.cfg.flyBoostMult && *s.ptr > 1.05f) state_.cfg.flyBoost = true;
    cy += 44.f;
  }

  if (Button(pad, cy, innerW, 30.f, t.syncNow, true)) {
    configDirty_ = true;
    dirtyAt_ = 0;
    FlushConfig(true);
  }
  cy += 42.f;
  UpdateMaxScroll(cy);
}

void Menu::DrawActions(float y0) {
  float pad = 14.f;
  float innerW = state_.w - pad * 2 - 6.f;
  float bw = (innerW - 8) * 0.5f;
  float cy = y0;
  int col = 0;
  auto& ta = T();

  auto endRow = [&] {
    if (col == 1) {
      col = 0;
      cy += 34.f;
    }
  };

  auto emit = [&](const wchar_t* label, bool primary, const wchar_t* okMsg, auto&& fn) {
    float bx = pad + col * (bw + 8);
    float by = cy;
    bool danger = label == ta.actBoom || label == ta.actDisconnect || label == ta.actDm ||
                  label == ta.actNgStop;
    bool hover = InRect((float)mouse_.x, (float)mouse_.y, bx, by, bw, 30.f);
    auto* br = danger ? brushDanger_ : (primary ? brushAccent_ : (hover ? brushBtnHi_ : brushBtn_));
    rt_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(bx, by, bx + bw, by + 30), 5, 5),
                              br);
    auto* fmt = fontBtn_ ? fontBtn_ : fontBody_;
    rt_->DrawText(label, (UINT32)wcslen(label), fmt, D2D1::RectF(bx, by, bx + bw, by + 30),
                  (primary || danger) ? brushBg_ : brushText_);
    if (ConsumeClick(bx, by, bw, 30.f)) {
      fn();
      if (okMsg && *okMsg) SetStatus(okMsg);
    }
    col = 1 - col;
    if (col == 0) cy += 34.f;
  };

  SectionTitle(pad, cy, ta.secCommon);
  cy += 22.f;
  col = 0;
  emit(ta.actChirp, true, ta.actChirp, [&] { CmdOp("fire_chirp"); });
  emit(ta.actFill, false, ta.actFill, [&] { CmdOp("fill_scarf"); });
  emit(ta.actMaxCloth, true, ta.actMaxCloth, [&] {
    state_.clothSteps = 20.f;
    CmdOp("max_cloth");
  });
  emit(ta.actSave, false, ta.actSave, [&] { CmdOp("save_now"); });
  emit(ta.actTpCompanion, true, ta.actTpCompanion, [&] { CmdOp("tp_companion"); });
  emit(ta.actNextSave, false, ta.actNextSave, [&] { CmdOp("next_level_save"); });
  endRow();

  cy += 8.f;
  SectionTitle(pad, cy, ta.secNgPlus);
  cy += 22.f;
  col = 0;
  emit(ta.actNgStart, true, nullptr, [&] { ApplyNgPlusKit(); });
  emit(ta.actNgStop, false, nullptr, [&] { StopFarm(); });
  emit(ta.actNextFast, false, ta.actNextFast, [&] { CmdOp("next_level"); });
  emit(ta.actAchievements, false, ta.actAchievements, [&] { CmdOp("unlock_achievements"); });
  endRow();

  cy += 8.f;
  SectionTitle(pad, cy, ta.secCloth);
  cy += 20.f;
  SliderRow(pad, cy, innerW, ta.clothSteps, &state_.clothSteps, 1.f, 20.f);
  cy += 46.f;

  cy += 2.f;
  SectionTitle(pad, cy, ta.secOutfit);
  cy += 22.f;
  col = 0;
  emit(ta.actWhite, false, ta.actWhite, [&] { CmdOp("outfit_white"); });
  emit(ta.actRed, false, ta.actRed, [&] { CmdOp("outfit_red"); });
  emit(ta.actTier, false, ta.actTier, [&] { CmdOp("cycle_outfit_tier"); });
  emit(ta.actNoCold, false, ta.actNoCold, [&] { CmdOp("no_cold"); });
  emit(ta.actFlower, false, ta.actFlower, [&] { CmdOp("tp_flower"); });
  emit(ta.actTrigFlower, false, ta.actTrigFlower, [&] { CmdOp("trigger_flower"); });
  endRow();

  cy += 8.f;
  SectionTitle(pad, cy, ta.secPartnerSub);
  cy += 22.f;
  col = 0;
  emit(ta.actDisconnect, false, ta.actDisconnect, [&] { CmdOp("disconnect"); });
  emit(ta.actChapters, true, ta.actChapters, [&] { CmdOp("chapter_select"); });
  endRow();

  cy += 6.f;
  // Subtitle position chips
  {
    const wchar_t* labels[] = {ta.posTop, ta.posCenter, ta.posBottom};
    float chipW = (innerW - 16.f) / 3.f;
    for (int i = 0; i < 3; ++i) {
      float bx = pad + i * (chipW + 8.f);
      bool on = state_.subtitlePos == i;
      bool hover = InRect((float)mouse_.x, (float)mouse_.y, bx, cy, chipW, 28.f);
      auto* br = on ? brushAccent_ : (hover ? brushBtnHi_ : brushBtn_);
      rt_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(bx, cy, bx + chipW, cy + 28), 5, 5),
                                br);
      auto* fmt = fontBtn_ ? fontBtn_ : fontBody_;
      rt_->DrawText(labels[i], (UINT32)wcslen(labels[i]), fmt,
                    D2D1::RectF(bx, cy, bx + chipW, cy + 28), on ? brushBg_ : brushText_);
      if (ConsumeClick(bx, cy, chipW, 28.f)) {
        state_.subtitlePos = i;
        wchar_t buf[80];
        swprintf(buf, 80, L"%s · %s", ta.subtitlePos, labels[i]);
        SetStatus(buf, 1200);
      }
    }
    cy += 34.f;
  }
  DrawTextField(pad, cy, innerW, 34.f);
  cy += 40.f;
  col = 0;
  emit(ta.sendSubtitle, true, nullptr, [&] { SendSubtitle(); });
  emit(L"Hello", false, nullptr, [&] {
    state_.subtitleDraft = L"Hello Journey";
    SendSubtitle();
  });
  emit(L"Nice", false, nullptr, [&] {
    state_.subtitleDraft = L"Nice to meet you";
    SendSubtitle();
  });
  emit(L"Mos", false, nullptr, [&] {
    state_.subtitleDraft = L"Journey Mos";
    SendSubtitle();
  });
  endRow();
  HintIfHover(pad, cy - 40.f, innerW, 74.f, ta.subtitleLocalOnly);

  cy += 8.f;
  SectionTitle(pad, cy, ta.secFx);
  cy += 22.f;
  col = 0;
  emit(ta.actGlow, true, ta.actGlow,
       [&] { WriteCommand("op = \"toggle_glow\", long = false"); });
  emit(ta.actLongGlow, true, ta.actLongGlow,
       [&] { WriteCommand("op = \"toggle_glow\", long = true"); });
  emit(ta.actSlow, false, ta.actSlow, [&] { CmdScaleTime(0.3, 4); });
  emit(ta.actFast, false, ta.actFast, [&] { CmdScaleTime(2.0, 3); });
  emit(ta.actBoom, false, ta.actBoom, [&] { CmdOp("explode_dude"); });
  emit(ta.actDm, false, ta.actDm, [&] { CmdOp("toggle_dm"); });
  endRow();

  cy += 8.f;
  SectionTitle(pad, cy, ta.secNetgui);
  cy += 22.f;
  col = 0;
  emit(ta.actNetToggle, true, ta.actNetToggle, [&] { CmdOp("netgui"); });
  emit(ta.actNetRun, true, ta.actNetRun, [&] { CmdOp("netgui_select"); });
  emit(ta.actNetPrev, false, ta.actNetPrev, [&] { CmdOp("netgui_prev"); });
  emit(ta.actNetNext, false, ta.actNetNext, [&] { CmdOp("netgui_next"); });
  endRow();

  cy += 8.f;
  SectionTitle(pad, cy, ta.secDebug);
  cy += 22.f;
  col = 0;
  emit(ta.actMute, false, ta.actMute, [&] { CmdOp("mute_music"); });
  emit(ta.actHud, false, ta.actHud, [&] { CmdOp("debug_hud"); });
  endRow();

  cy += 16.f;
  UpdateMaxScroll(cy);
}

void Menu::DrawAnims(float y0) {
  float pad = 14.f;
  float innerW = state_.w - pad * 2 - 6.f;
  float bw = (innerW - 8) * 0.5f;
  float cy = y0;
  int col = 0;

  auto endRow = [&] {
    if (col == 1) {
      col = 0;
      cy += 36.f;
    }
  };

  auto emitSection = [&](const wchar_t* title, const char** ids, int n, bool primaryFirst) {
    SectionTitle(pad, cy, title);
    cy += 22.f;
    col = 0;
    for (int i = 0; i < n; ++i) {
      float bx = pad + col * (bw + 8);
      const wchar_t* label = AnimLabel(state_.lang, ids[i]);
      if (Button(bx, cy, bw, 30.f, label, primaryFirst && i < 2)) {
        CmdAnim(ids[i]);
        wchar_t buf[96];
        swprintf(buf, 96, L"%s · %s", title, label);
        SetStatus(buf);
      }
      col = 1 - col;
      if (col == 0) cy += 36.f;
    }
    endRow();
    cy += 6.f;
  };

  const char* fav[] = {
      "NodHead", "ShakeHead", "JumpRoll", "Act01Surprise", "LookUp", "ShakeOff",
      "ShoutL",  "Flap3",     "Med",      "Crouch",        "LieF",   "SpinF",
  };
  emitSection(T().animFav, fav, (int)(sizeof(fav) / sizeof(fav[0])), true);

  const char* idle[] = {
      "IdleStretch",     "IdleCheckFootL", "IdleCheckFootR", "LookBackL",
      "LookBackR",       "TurnLookBackL",  "TurnLookBackR",  "Act01LookAround",
      "Act01LookDown",   "Act01Hold",      "Act06LookAround","Act06LookLR",
      "Cough",           "FixCloth",       "StandTurnL",     "StandTurnR",
  };
  emitSection(T().animIdle, idle, (int)(sizeof(idle) / sizeof(idle[0])), false);

  const char* shout[] = {
      "ShoutM", "ShoutR", "ShoutBL", "ShoutBR", "Charge", "ChargeAir", "ChargeWalk", "ChargeRun",
  };
  emitSection(T().animShout, shout, (int)(sizeof(shout) / sizeof(shout[0])), false);

  const char* air[] = {
      "Flap1",         "BoostFlap",    "BoostSpiralL", "BoostSpiralR", "AirFlipL",
      "AirFlipR",      "RollF",        "RollB",        "RollL",        "RollR",
      "HopL",          "MPAirSwing1L", "MPAirSwing1R", "MPRunFlip1L",  "MPRunFlip1R",
      "MPFSpinL",      "MPFSpinR",     "PrepareToLaunch",
  };
  emitSection(T().animAir, air, (int)(sizeof(air) / sizeof(air[0])), false);

  const char* pose[] = {
      "LieB", "GetUpF", "GetUpB", "Crouching", "Kneel", "Kneeling", "ReadyToMed", "StopMed",
      "Collapse", "Revive", "BlownBack", "FallForward", "HitBodyL", "HitBodyR", "LiftA", "LiftEL",
  };
  emitSection(T().animPose, pose, (int)(sizeof(pose) / sizeof(pose[0])), false);

  cy += 8.f;
  UpdateMaxScroll(cy);
}

void Menu::DrawLevels(float y0) {
  float pad = 14.f;
  float innerW = state_.w - pad * 2 - 6.f;
  float cy = y0;

  auto& tl = T();
  SectionTitle(pad, cy, tl.secQuick);
  cy += 22.f;
  if (Button(pad, cy, innerW, 32.f, tl.chapterSelect, true)) {
    CmdOp("chapter_select");
    SetStatus(tl.chapterSelect);
  }
  cy += 38.f;
  if (Button(pad, cy, innerW, 32.f, tl.respawnStart, false)) {
    CmdOp("respawn_start");
    SetStatus(tl.respawnStart);
  }
  cy += 42.f;

  SectionTitle(pad, cy, tl.secWarp);
  cy += 22.f;

  struct Lv {
    const wchar_t* name;
    const char* id;
  } levels[] = {
      {tl.lvGrave, "Level_Graveyard"}, {tl.lvBarrens, "Level_Barrens"},
      {tl.lvDesert, "Level_Desert"},   {tl.lvCanyon, "Level_Canyon"},
      {tl.lvCave, "Level_Cave"},       {tl.lvRuins, "Level_Ruins"},
      {tl.lvMountain, "Level_Mountain"}, {tl.lvSummit, "Level_Summit"},
      {tl.lvCredits, "Level_Credits"},
  };

  for (auto& lv : levels) {
    bool hub = strcmp(lv.id, "Level_Graveyard") == 0;
    if (Button(pad, cy, innerW, 32.f, lv.name, hub)) {
      if (hub) CmdOp("chapter_select");
      else CmdLevel("change_level_save", lv.id);
      SetStatus(lv.name);
    }
    cy += 36.f;
  }

  cy += 6.f;
  if (Button(pad, cy, innerW, 30.f, tl.nextFast, false)) {
    CmdOp("next_level");
    SetStatus(tl.nextFast);
  }
  cy += 40.f;
  UpdateMaxScroll(cy);
}

void Menu::Draw() {
  float w = state_.w;
  float h = state_.h;

  rt_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0, 0, w, h), 10, 10), brushSurface_);
  // Top accent hairline
  rt_->FillRectangle(D2D1::RectF(0, 0, w, 2.f), brushAccent_);

  DrawHeader();
  rt_->FillRectangle(D2D1::RectF(14, kHeaderH - 2.f, w - 14, kHeaderH - 1.f), brushLine_);

  DrawTabs(kHeaderH + 2.f);
  rt_->FillRectangle(D2D1::RectF(14, kHeaderH + kTabH, w - 14, kHeaderH + kTabH + 1.f),
                     brushLine_);

  float clipTop = kHeaderH + kTabH + 4.f;
  float clipBot = h - kFooterH;
  rt_->PushAxisAlignedClip(D2D1::RectF(0, clipTop, w - 12, clipBot),
                           D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  float y0 = clipTop + 6.f - (float)state_.scroll;
  if (state_.tab == 0) DrawConfig(y0);
  else if (state_.tab == 1) DrawActions(y0);
  else if (state_.tab == 2) DrawAnims(y0);
  else DrawLevels(y0);
  rt_->PopAxisAlignedClip();
  DrawScrollbar();

  // Footer
  rt_->FillRectangle(D2D1::RectF(0, h - kFooterH, w, h), brushBg_);
  rt_->FillRectangle(D2D1::RectF(14, h - kFooterH, w - 14, h - kFooterH + 1.f), brushLine_);
  auto* footBrush = configDirty_ ? brushAccent_ : brushMuted_;
  rt_->DrawText(state_.statusLine.c_str(), (UINT32)state_.statusLine.size(), fontSmall_,
                D2D1::RectF(16, h - 24, w - 16, h - 6), footBrush);
}

}  // namespace mos
