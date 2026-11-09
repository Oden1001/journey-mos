#pragma once
#include "pch.h"

namespace mos {

enum class UiLang : int { Zh = 0, En = 1, Ja = 2, Count = 3 };

inline UiLang NextLang(UiLang l) {
  int n = ((int)l + 1) % (int)UiLang::Count;
  return (UiLang)n;
}

inline const wchar_t* LangCode(UiLang l) {
  switch (l) {
    case UiLang::En: return L"EN";
    case UiLang::Ja: return L"日本語";
    default: return L"中文";
  }
}

struct UiStrings {
  const wchar_t* tabConfig;
  const wchar_t* tabActions;
  const wchar_t* tabAnims;
  const wchar_t* tabLevels;
  const wchar_t* openHint;
  const wchar_t* focused;
  const wchar_t* closeHint;
  const wchar_t* pendingSync;
  const wchar_t* syncFail;
  const wchar_t* synced;
  const wchar_t* clothApplied;
  const wchar_t* farmOn;
  const wchar_t* farmOff;
  const wchar_t* secMove;
  const wchar_t* secOnline;
  const wchar_t* secOther;
  const wchar_t* secValues;
  const wchar_t* syncNow;
  const wchar_t* infiniteEnergy;
  const wchar_t* speedBoost;
  const wchar_t* flyBoost;
  const wchar_t* noWind;
  const wchar_t* meetEverywhere;
  const wchar_t* disableLobby;
  const wchar_t* companionName;
  const wchar_t* skipCinematic;
  const wchar_t* autoFarm;
  const wchar_t* forceGlow;
  const wchar_t* attractGlyphs;
  const wchar_t* tipEnergy;
  const wchar_t* tipSpeed;
  const wchar_t* tipFly;
  const wchar_t* tipWind;
  const wchar_t* tipMeet;
  const wchar_t* tipLobby;
  const wchar_t* tipName;
  const wchar_t* tipSkip;
  const wchar_t* tipFarm;
  const wchar_t* tipGlow;
  const wchar_t* tipAttract;
  const wchar_t* speedMult;
  const wchar_t* flyMult;
  const wchar_t* timeMult;
  const wchar_t* farmInterval;
  const wchar_t* secCommon;
  const wchar_t* secNgPlus;
  const wchar_t* secCloth;
  const wchar_t* clothSteps;
  const wchar_t* secOutfit;
  const wchar_t* secPartnerSub;
  const wchar_t* secFx;
  const wchar_t* secNetgui;
  const wchar_t* secDebug;
  const wchar_t* subtitleHint;
  const wchar_t* subtitleEdit;
  const wchar_t* subtitleSent;
  const wchar_t* subtitleCancel;
  const wchar_t* subtitleLocalOnly;
  const wchar_t* subtitlePos;
  const wchar_t* posTop;
  const wchar_t* posCenter;
  const wchar_t* posBottom;
  const wchar_t* sendSubtitle;
  const wchar_t* secQuick;
  const wchar_t* secWarp;
  const wchar_t* chapterSelect;
  const wchar_t* respawnStart;
  const wchar_t* nextFast;
  const wchar_t* on;
  const wchar_t* off;
  const wchar_t* statusEnergy;
  const wchar_t* statusFarming;
  const wchar_t* langCycled;
  const wchar_t* companionFallback;  // nametag when Steam name unknown

  // Actions
  const wchar_t* actChirp;
  const wchar_t* actFill;
  const wchar_t* actMaxCloth;
  const wchar_t* actSave;
  const wchar_t* actTpCompanion;
  const wchar_t* actNextSave;
  const wchar_t* actNgStart;
  const wchar_t* actNgStop;
  const wchar_t* actNextFast;
  const wchar_t* actAchievements;
  const wchar_t* actWhite;
  const wchar_t* actRed;
  const wchar_t* actTier;
  const wchar_t* actNoCold;
  const wchar_t* actFlower;
  const wchar_t* actTrigFlower;
  const wchar_t* actDisconnect;
  const wchar_t* actChapters;
  const wchar_t* actGlow;
  const wchar_t* actLongGlow;
  const wchar_t* actSlow;
  const wchar_t* actFast;
  const wchar_t* actBoom;
  const wchar_t* actDm;
  const wchar_t* actNetToggle;
  const wchar_t* actNetRun;
  const wchar_t* actNetPrev;
  const wchar_t* actNetNext;
  const wchar_t* actMute;
  const wchar_t* actHud;

  // Levels
  const wchar_t* lvGrave;
  const wchar_t* lvBarrens;
  const wchar_t* lvDesert;
  const wchar_t* lvCanyon;
  const wchar_t* lvCave;
  const wchar_t* lvRuins;
  const wchar_t* lvMountain;
  const wchar_t* lvSummit;
  const wchar_t* lvCredits;

  // Anims section titles
  const wchar_t* animFav;
  const wchar_t* animIdle;
  const wchar_t* animShout;
  const wchar_t* animAir;
  const wchar_t* animPose;
};

const UiStrings& S(UiLang lang);

}  // namespace mos
