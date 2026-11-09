#pragma once
#include "lang.h"

namespace mos {

// Localized label for a PlayDudeAnim id. Falls back to id if unknown.
const wchar_t* AnimLabel(UiLang lang, const char* id);

}  // namespace mos
