#pragma once
#include "pch.h"

namespace mos {

#pragma pack(push, 1)
struct HudSnap {
  uint32_t magic;  // 'MOSH'
  uint8_t showName;
  uint8_t mp;
  uint8_t hasProj;
  uint8_t pad;
  float cpos[3];
  float world[16];
  float proj[16];
  float fov;
  float aspect;
  float nearP;
  float farP;
};
#pragma pack(pop)

constexpr uint32_t kHudMagic = 0x48534F4Du;

bool ReadHudSnap(HudSnap* out);

}  // namespace mos
