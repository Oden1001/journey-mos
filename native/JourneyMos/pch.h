#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <imm.h>
// windows.h maps DrawText -> DrawTextW; D2D uses DrawText as a method name.
#ifdef DrawText
#undef DrawText
#endif
#ifdef DrawTextW
#undef DrawTextW
#endif
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <dxgi.h>
#include <d3d11.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
