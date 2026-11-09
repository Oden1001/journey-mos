#include "nametag.h"
#include "bridge.h"
#include "hudsnap.h"
#include "lang.h"
#include "menu.h"
#include "partner.h"
#include <cmath>
#include <cstdlib>

namespace mos {
namespace {

template <typename T>
void SafeRelease(T*& p) {
  if (p) {
    p->Release();
    p = nullptr;
  }
}

const wchar_t* kClass = L"JourneyMosNameTagClass";

struct Mat4 {
  float m[16]{};
};

bool Invert(const Mat4& in, Mat4& out) {
  float inv[16], det;
  const float* m = in.m;
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
           m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
           m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
           m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
            m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
           m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
           m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
           m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
            m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
           m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
           m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
            m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
            m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
           m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
           m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
            m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
            m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (fabsf(det) < 1e-8f) return false;
  det = 1.f / det;
  for (int i = 0; i < 16; ++i) out.m[i] = inv[i] * det;
  return true;
}

// Matches the pre-break nametag (journey_detour-style reconstructed proj).
Mat4 Perspective(float fovDeg, float aspect, float zn, float zf) {
  float coeff = 1.f - 0.5f * (fovDeg / 180.f) * (fovDeg / 180.f);
  float f = 1.f / tanf((fovDeg * coeff) * 3.14159265f / 180.f * 0.5f);
  Mat4 p{};
  p.m[0] = f / (aspect > 0.01f ? aspect : 1.f);
  p.m[5] = f;
  p.m[10] = (zf + zn) / (zn - zf);
  p.m[11] = -1.f;
  p.m[14] = (2.f * zf * zn) / (zn - zf);
  return p;
}

bool ProjectOne(float ox, float oy, float oz, const Mat4& view, const Mat4& proj, float width,
                float height, bool colMajor, float* sx, float* sy) {
  float ix, iy, iz, iw;
  if (colMajor) {
    ix = view.m[0] * ox + view.m[4] * oy + view.m[8] * oz + view.m[12];
    iy = view.m[1] * ox + view.m[5] * oy + view.m[9] * oz + view.m[13];
    iz = view.m[2] * ox + view.m[6] * oy + view.m[10] * oz + view.m[14];
    iw = view.m[3] * ox + view.m[7] * oy + view.m[11] * oz + view.m[15];
  } else {
    ix = view.m[0] * ox + view.m[1] * oy + view.m[2] * oz + view.m[3];
    iy = view.m[4] * ox + view.m[5] * oy + view.m[6] * oz + view.m[7];
    iz = view.m[8] * ox + view.m[9] * oy + view.m[10] * oz + view.m[11];
    iw = view.m[12] * ox + view.m[13] * oy + view.m[14] * oz + view.m[15];
  }
  if (fabsf(iw) > 1e-6f) {
    ix /= iw;
    iy /= iw;
    iz /= iw;
  }

  float cx, cy, cz, cw;
  if (colMajor) {
    cx = proj.m[0] * ix + proj.m[4] * iy + proj.m[8] * iz + proj.m[12];
    cy = proj.m[1] * ix + proj.m[5] * iy + proj.m[9] * iz + proj.m[13];
    cz = proj.m[2] * ix + proj.m[6] * iy + proj.m[10] * iz + proj.m[14];
    cw = proj.m[3] * ix + proj.m[7] * iy + proj.m[11] * iz + proj.m[15];
  } else {
    cx = proj.m[0] * ix + proj.m[1] * iy + proj.m[2] * iz + proj.m[3];
    cy = proj.m[4] * ix + proj.m[5] * iy + proj.m[6] * iz + proj.m[7];
    cz = proj.m[8] * ix + proj.m[9] * iy + proj.m[10] * iz + proj.m[11];
    cw = proj.m[12] * ix + proj.m[13] * iy + proj.m[14] * iz + proj.m[15];
  }
  // Clip-space W<=0 or NDC outside frustum → not on screen.
  if (cw <= 0.05f) return false;
  float ndcX = cx / cw;
  float ndcY = cy / cw;
  float ndcZ = cz / cw;
  if (ndcX < -1.05f || ndcX > 1.05f || ndcY < -1.05f || ndcY > 1.05f) return false;
  if (ndcZ < -0.05f || ndcZ > 1.05f) return false;

  *sx = (ndcX * width) * 0.5f + width * 0.5f;
  *sy = height - ((ndcY * height) * 0.5f + height * 0.5f);
  // Strict viewport — hide when off-screen (small margin for tag width).
  const float margin = 24.f;
  return *sx >= -margin && *sy >= -margin && *sx < width + margin && *sy < height + margin;
}

// mode: 0/1 = invert world as view (row/col), 2/3 = treat world as view (row/col).
bool ProjectMode(int mode, float ox, float oy, float oz, const Mat4& world, const Mat4& view,
                 const Mat4& proj, float width, float height, float* sx, float* sy) {
  switch (mode) {
    case 0:
      return ProjectOne(ox, oy, oz, view, proj, width, height, false, sx, sy);
    case 1:
      return ProjectOne(ox, oy, oz, view, proj, width, height, true, sx, sy);
    case 2:
      return ProjectOne(ox, oy, oz, world, proj, width, height, false, sx, sy);
    case 3:
      return ProjectOne(ox, oy, oz, world, proj, width, height, true, sx, sy);
    default:
      return false;
  }
}

bool WorldToScreen(float ox, float oy, float oz, const Mat4& world, const Mat4* gameProj,
                   float fov, float aspect, float nearP, float farP, float width, float height,
                   int* stickyMode, float* sx, float* sy) {
  Mat4 view{};
  if (!Invert(world, view)) return false;
  Mat4 proj = gameProj ? *gameProj : Perspective(fov, aspect, nearP, farP);

  const int prefer = (stickyMode && *stickyMode >= 0 && *stickyMode <= 3) ? *stickyMode : -1;
  if (prefer >= 0 && ProjectMode(prefer, ox, oy, oz, world, view, proj, width, height, sx, sy)) {
    return true;
  }
  for (int mode = 0; mode < 4; ++mode) {
    if (mode == prefer) continue;
    if (ProjectMode(mode, ox, oy, oz, world, view, proj, width, height, sx, sy)) {
      if (stickyMode) *stickyMode = mode;
      return true;
    }
  }
  return false;
}

bool ParseJsonFloatArray(const std::string& s, const char* key, float* out, int n) {
  auto p = s.find(std::string("\"") + key + "\":[");
  if (p == std::string::npos) return false;
  p = s.find('[', p);
  if (p == std::string::npos) return false;
  ++p;
  for (int i = 0; i < n; ++i) {
    while (p < s.size() && (s[p] == ' ' || s[p] == ',')) ++p;
    out[i] = (float)atof(s.c_str() + p);
    while (p < s.size() && s[p] != ',' && s[p] != ']') ++p;
    if (s[p] == ',') ++p;
  }
  return true;
}

bool ParseJsonFloat(const std::string& s, const char* key, float* out) {
  auto p = s.find(std::string("\"") + key + "\":");
  if (p == std::string::npos) return false;
  p = s.find(':', p);
  if (p == std::string::npos) return false;
  *out = (float)atof(s.c_str() + p + 1);
  return true;
}

bool HasTrue(const std::string& s, const char* key) {
  return s.find(std::string("\"") + key + "\":true") != std::string::npos;
}

}  // namespace

NameTag& NameTag::Instance() {
  static NameTag t;
  return t;
}

LRESULT CALLBACK NameTag::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_NCHITTEST:
      return HTTRANSPARENT;
    case WM_ERASEBKGND:
      return 1;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

bool NameTag::Init(HINSTANCE inst) {
  inst_ = inst;
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = Proc;
  wc.hInstance = inst;
  wc.lpszClassName = kClass;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

  hwnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED,
      kClass, L"JourneyMosNameTag", WS_POPUP, 0, 0, winW_, winH_, nullptr, nullptr, inst, nullptr);
  if (!hwnd_) return false;

  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_))) return false;
  if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(&dwrite_))))
    return false;

  D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0, 0,
      D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT);
  if (FAILED(d2d_->CreateDCRenderTarget(&props, &rt_))) return false;

  rt_->CreateSolidColorBrush(D2D1::ColorF(0.98f, 0.94f, 0.86f, 1.f), &brushText_);
  rt_->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.85f), &brushShadow_);
  dwrite_->CreateTextFormat(L"Microsoft YaHei UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.f, L"zh-cn",
                            &font_);
  if (font_) {
    font_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    font_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  }
  ShowWindow(hwnd_, SW_HIDE);
  return EnsureTarget(winW_, winH_);
}

bool NameTag::EnsureTarget(int w, int h) {
  if (w < 32) w = 32;
  if (h < 20) h = 20;
  if (dib_ && dibW_ == w && dibH_ == h && memDc_ && bits_) return true;

  if (dib_) {
    DeleteObject(dib_);
    dib_ = nullptr;
    bits_ = nullptr;
  }
  if (memDc_) {
    DeleteDC(memDc_);
    memDc_ = nullptr;
  }

  HDC screen = GetDC(nullptr);
  memDc_ = CreateCompatibleDC(screen);
  ReleaseDC(nullptr, screen);
  if (!memDc_) return false;

  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  dib_ = CreateDIBSection(memDc_, &bi, DIB_RGB_COLORS, &bits_, nullptr, 0);
  if (!dib_ || !bits_) return false;
  SelectObject(memDc_, dib_);
  dibW_ = w;
  dibH_ = h;
  winW_ = w;
  winH_ = h;
  RECT rc{0, 0, w, h};
  return SUCCEEDED(rt_->BindDC(memDc_, &rc));
}

void NameTag::Shutdown() {
  SafeRelease(font_);
  SafeRelease(brushShadow_);
  SafeRelease(brushText_);
  SafeRelease(rt_);
  SafeRelease(dwrite_);
  SafeRelease(d2d_);
  if (dib_) {
    DeleteObject(dib_);
    dib_ = nullptr;
  }
  if (memDc_) {
    DeleteDC(memDc_);
    memDc_ = nullptr;
  }
  bits_ = nullptr;
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

void NameTag::PaintToLayer(int screenX, int screenY) {
  if (!rt_ || !memDc_ || !bits_ || !hwnd_) return;

  const bool nameChanged = (name_ != paintedName_);
  const bool needRedraw = nameChanged || bitmapDirty_;
  // Always reposition when Tick asks — early-out on stale coords caused the
  // "appears once then freezes" bug when hud data stopped updating.
  if (needRedraw) {
    float needW = 120.f, needH = 28.f;
    if (dwrite_ && font_) {
      IDWriteTextLayout* layout = nullptr;
      if (SUCCEEDED(dwrite_->CreateTextLayout(name_.c_str(), (UINT32)name_.size(), font_, 400.f,
                                              40.f, &layout)) &&
          layout) {
        DWRITE_TEXT_METRICS tm{};
        if (SUCCEEDED(layout->GetMetrics(&tm))) {
          needW = tm.width + 16.f;
          needH = tm.height + 10.f;
        }
        layout->Release();
      }
    }
    if (!EnsureTarget((int)(needW + 0.5f), (int)(needH + 0.5f))) return;

    memset(bits_, 0, (size_t)dibW_ * (size_t)dibH_ * 4);
    rt_->BeginDraw();
    rt_->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
    if (font_ && brushText_ && brushShadow_) {
      auto box = D2D1::RectF(1, 1, (float)dibW_ - 1, (float)dibH_ - 1);
      const float o = 1.0f;
      const float offs[4][2] = {{-o, 0}, {o, 0}, {0, -o}, {0, o}};
      for (auto& d : offs) {
        rt_->DrawText(name_.c_str(), (UINT32)name_.size(), font_,
                      D2D1::RectF(box.left + d[0], box.top + d[1], box.right + d[0],
                                  box.bottom + d[1]),
                      brushShadow_);
      }
      rt_->DrawText(name_.c_str(), (UINT32)name_.size(), font_, box, brushText_);
    }
    rt_->EndDraw();
    paintedName_ = name_;
    bitmapDirty_ = false;
  }

  // Skip layered-window moves of <1px — UpdateLayeredWindow every tick looks like jitter.
  // Always blit after a redraw so a new glyph isn't stuck off-screen.
  if (!needRedraw && lastScreenX_ != INT_MIN) {
    const int dx = screenX - lastScreenX_;
    const int dy = screenY - lastScreenY_;
    if (dx * dx + dy * dy < 1) return;
  }

  SIZE size{dibW_, dibH_};
  POINT src{0, 0};
  POINT dst{screenX - dibW_ / 2, screenY - dibH_ - 6};
  BLENDFUNCTION blend{};
  blend.BlendOp = AC_SRC_OVER;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;
  UpdateLayeredWindow(hwnd_, nullptr, &dst, &size, memDc_, &src, 0, &blend, ULW_ALPHA);
  lastScreenX_ = screenX;
  lastScreenY_ = screenY;
}

void NameTag::Tick(HWND gameHwnd) {
  if (!hwnd_ || !gameHwnd || !IsWindow(gameHwnd)) {
    if (visible_) {
      visible_ = false;
      ShowWindow(hwnd_, SW_HIDE);
    }
    return;
  }

  bool show = false, mp = false;
  float cpos[3]{};
  float cam[16]{};
  float fov = 60.f, aspect = 1.777f, nearP = 0.1f, farP = 2000.f;
  Mat4 gameProj{};
  bool hasProj = false;

  HudSnap snap{};
  if (ReadHudSnap(&snap) && snap.showName) {
    show = true;
    mp = snap.mp != 0;
    for (int i = 0; i < 3; ++i) cpos[i] = snap.cpos[i];
    for (int i = 0; i < 16; ++i) cam[i] = snap.world[i];
    if (snap.hasProj) {
      for (int i = 0; i < 16; ++i) gameProj.m[i] = snap.proj[i];
      hasProj = true;
    }
    if (snap.fov > 1.f) fov = snap.fov;
    if (snap.aspect > 0.1f) aspect = snap.aspect;
    if (snap.nearP > 0.f) nearP = snap.nearP;
    if (snap.farP > nearP) farP = snap.farP;
  } else {
    // Fallback to JSON (same path that used to track the head).
    auto path = GameDir() + L"\\mod_status.json";
    std::ifstream in(path.c_str());
    if (!in) {
      if (visible_) {
        visible_ = false;
        ShowWindow(hwnd_, SW_HIDE);
      }
      return;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    auto s = ss.str();
    show = HasTrue(s, "showName");
    mp = HasTrue(s, "mp");
    ParseJsonFloatArray(s, "cpos", cpos, 3);
    ParseJsonFloatArray(s, "cam", cam, 16);
    ParseJsonFloat(s, "fov", &fov);
    ParseJsonFloat(s, "aspect", &aspect);
    ParseJsonFloat(s, "near", &nearP);
    ParseJsonFloat(s, "far", &farP);
  }

  // Need a companion position. mp flag alone is unreliable.
  bool hasPos = (cpos[0] != 0.f || cpos[1] != 0.f || cpos[2] != 0.f);
  if (!show || (!mp && !hasPos)) {
    stickyMode_ = -1;
    smoothInit_ = false;
    if (visible_) {
      visible_ = false;
      ShowWindow(hwnd_, SW_HIDE);
    }
    return;
  }

  const wchar_t* fallback = S(Menu::Instance().State().lang).companionFallback;
  if (!fallback || !*fallback) fallback = L"同伴";

  DWORD now = GetTickCount();
  if (now - lastNameRefresh_ > 200) {
    lastNameRefresh_ = now;
    std::string live = Partner_GetLiveName();
    hasLiveName_ = false;
    if (!live.empty()) {
      int n = MultiByteToWideChar(CP_UTF8, 0, live.c_str(), (int)live.size(), nullptr, 0);
      if (n > 0) {
        name_.assign(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, live.c_str(), (int)live.size(), name_.data(), n);
        hasLiveName_ = true;
      }
    }
  }
  if (!hasLiveName_ || name_.empty()) name_ = fallback;

  RECT crc{};
  GetClientRect(gameHwnd, &crc);
  float gw = (float)(crc.right - crc.left);
  float gh = (float)(crc.bottom - crc.top);
  if (gw < 64.f || gh < 64.f) return;

  Mat4 world{};
  for (int i = 0; i < 16; ++i) world.m[i] = cam[i];
  Mat4* projPtr = hasProj ? &gameProj : nullptr;

  // Head-ish offset: slightly above companion origin (was 1.55 — a bit high).
  constexpr float kHeadY = 1.35f;
  float sx = 0, sy = 0;
  bool ok = WorldToScreen(cpos[0], cpos[1] + kHeadY, cpos[2], world, projPtr, fov, aspect, nearP,
                          farP, gw, gh, &stickyMode_, &sx, &sy);
  if (!ok) {
    ok = WorldToScreen(cpos[0], cpos[1] + kHeadY, cpos[2], world, nullptr, fov, aspect, nearP, farP,
                       gw, gh, &stickyMode_, &sx, &sy);
  }
  if (!ok) {
    stickyMode_ = -1;
    smoothInit_ = false;
    if (visible_) {
      visible_ = false;
      ShowWindow(hwnd_, SW_HIDE);
    }
    return;
  }

  // Exponential smooth toward projected point — kills 1–3px thrash without lag.
  constexpr float kSmooth = 0.42f;
  if (!smoothInit_ || !visible_) {
    smoothSx_ = sx;
    smoothSy_ = sy;
    smoothInit_ = true;
  } else {
    smoothSx_ += (sx - smoothSx_) * kSmooth;
    smoothSy_ += (sy - smoothSy_) * kSmooth;
  }

  POINT origin{0, 0};
  ClientToScreen(gameHwnd, &origin);
  if (!visible_) {
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    visible_ = true;
  }
  PaintToLayer(origin.x + (int)(smoothSx_ + 0.5f), origin.y + (int)(smoothSy_ + 0.5f));
}

}  // namespace mos
