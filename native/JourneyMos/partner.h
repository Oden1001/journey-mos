#pragma once
#include "pch.h"

namespace mos {

// Live companion Steam/persona name via Journey Matchmaker_GetPartnerName.
bool Partner_Init();
void Partner_Shutdown();
// Returns UTF-8 name; empty if offline / not resolved yet.
std::string Partner_GetLiveName();
bool Partner_IsReady();
// Menu toggles → native SecretSymbol vacuum (no Lua pos file).
void Partner_SetAttractGlyphs(bool on);

}  // namespace mos
