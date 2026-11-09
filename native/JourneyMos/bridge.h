#pragma once
#include "pch.h"

namespace mos {

struct Config {
  bool infiniteEnergy = true;
  bool speedBoost = true;
  float speedMult = 3.f;
  bool flyBoost = true;
  float flyBoostMult = 2.f;
  float gameSpeed = 1.f;
  bool debugCam = false;
  bool forceGlow = false;
  bool disableAutoLobby = true;
  bool skipCinematic = false;
  bool autoFarm = false;
  float farmInterval = 12.f;
  bool meetEverywhere = false;
  bool noWind = false;
  bool showCompanionName = true;
  bool attractGlyphs = false;
  int uiLang = 0;  // 0=zh 1=en 2=ja
};

std::wstring GameDir();
void SetGameDir(const std::wstring& dir);

// Write config.lua only (no command). Use for coalesced sync.
bool WriteConfigFile(const Config& c);
// Write config.lua + one reload_config command.
bool WriteConfig(const Config& c);
bool LoadConfig(Config& c);

bool WriteCommand(const std::string& itemBody);
bool WriteCommands(const std::vector<std::string>& itemBodies);

// Helpers that emit full Lua item bodies.
bool CmdOp(const char* op);
bool CmdAnim(const char* anim);
bool CmdLevel(const char* op, const char* level);
bool CmdScaleTime(double mult, double duration);
bool CmdShowText(const char* text, const char* style = "center", double duration = 3.0);
bool CmdShowTextW(const wchar_t* text, const char* style = "center", double duration = 3.0);
bool CmdClothSet(int steps);
bool CmdSuperGlow(bool longPulse);
bool CmdRumble(double intensity = 0.45, double duration = 1.2);

struct Status {
  bool ok = false;
  std::string level;
  float pos[3]{};
  bool energy = false;
  bool speed = false;
  bool farm = false;
  float speedMult = 1.f;
  float gameSpeed = 1.f;
  int symbols = -1;  // collected secret glyphs 0..21; -1 unknown
};
Status ReadStatus();

}  // namespace mos
