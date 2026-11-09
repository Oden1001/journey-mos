#include "anims_i18n.h"
#include <cstring>

namespace mos {
namespace {

struct Row {
  const char* id;
  const wchar_t* zh;
  const wchar_t* en;
  const wchar_t* ja;
};

// Keep in sync with menu DrawAnims lists.
const Row kRows[] = {
    {"NodHead", L"点头", L"Nod", L"うなずき"},
    {"ShakeHead", L"摇头", L"Shake head", L"首振り"},
    {"JumpRoll", L"跳滚", L"Jump roll", L"ジャンプロール"},
    {"Act01Surprise", L"惊讶", L"Surprise", L"驚き"},
    {"LookUp", L"抬头", L"Look up", L"見上げ"},
    {"ShakeOff", L"抖一抖", L"Shake off", L"払い落とし"},
    {"ShoutL", L"大叫", L"Shout L", L"叫び左"},
    {"Flap3", L"振翅", L"Flap", L"羽ばたき"},
    {"Med", L"打坐", L"Meditate", L"瞑想"},
    {"Crouch", L"蹲下", L"Crouch", L"しゃがむ"},
    {"LieF", L"趴下", L"Lie front", L"うつ伏せ"},
    {"SpinF", L"前转", L"Spin front", L"前回転"},

    {"IdleStretch", L"伸懒腰", L"Stretch", L"伸び"},
    {"IdleCheckFootL", L"看左脚", L"Check foot L", L"左足確認"},
    {"IdleCheckFootR", L"看右脚", L"Check foot R", L"右足確認"},
    {"LookBackL", L"左回头", L"Look back L", L"左振り返り"},
    {"LookBackR", L"右回头", L"Look back R", L"右振り返り"},
    {"TurnLookBackL", L"转身左看", L"Turn look L", L"左後ろ向き"},
    {"TurnLookBackR", L"转身右看", L"Turn look R", L"右後ろ向き"},
    {"Act01LookAround", L"环顾", L"Look around", L"見回す"},
    {"Act01LookDown", L"低头看", L"Look down", L"下を見る"},
    {"Act01Hold", L"静立", L"Stand still", L"静止"},
    {"Act06LookAround", L"环顾2", L"Look around 2", L"見回す2"},
    {"Act06LookLR", L"左右看", L"Look L/R", L"左右見る"},
    {"Cough", L"咳嗽", L"Cough", L"咳"},
    {"FixCloth", L"整理围巾", L"Fix scarf", L"スカーフ直し"},
    {"StandTurnL", L"原地左转", L"Turn L", L"その場左"},
    {"StandTurnR", L"原地右转", L"Turn R", L"その場右"},

    {"ShoutM", L"中叫", L"Shout M", L"叫び中"},
    {"ShoutR", L"大叫右", L"Shout R", L"叫び右"},
    {"ShoutBL", L"大叫后左", L"Shout back L", L"後ろ叫び左"},
    {"ShoutBR", L"大叫后右", L"Shout back R", L"後ろ叫び右"},
    {"Charge", L"蓄力", L"Charge", L"溜め"},
    {"ChargeAir", L"空中蓄力", L"Air charge", L"空中溜め"},
    {"ChargeWalk", L"走蓄力", L"Walk charge", L"歩き溜め"},
    {"ChargeRun", L"跑蓄力", L"Run charge", L"走り溜め"},

    {"Flap1", L"振翅1", L"Flap 1", L"羽ばたき1"},
    {"BoostFlap", L"振翅加速", L"Boost flap", L"加速羽ばたき"},
    {"BoostSpiralL", L"螺旋左", L"Spiral L", L"螺旋左"},
    {"BoostSpiralR", L"螺旋右", L"Spiral R", L"螺旋右"},
    {"AirFlipL", L"空翻左", L"Air flip L", L"宙返り左"},
    {"AirFlipR", L"空翻右", L"Air flip R", L"宙返り右"},
    {"RollF", L"前滚", L"Roll F", L"前転"},
    {"RollB", L"后滚", L"Roll B", L"後転"},
    {"RollL", L"左滚", L"Roll L", L"左転"},
    {"RollR", L"右滚", L"Roll R", L"右転"},
    {"HopL", L"小跳", L"Hop", L"ホップ"},
    {"MPAirSwing1L", L"空中摆左", L"Air swing L", L"空中振り左"},
    {"MPAirSwing1R", L"空中摆右", L"Air swing R", L"空中振り右"},
    {"MPRunFlip1L", L"跑翻左", L"Run flip L", L"走り宙返り左"},
    {"MPRunFlip1R", L"跑翻右", L"Run flip R", L"走り宙返り右"},
    {"MPFSpinL", L"自旋左", L"Spin L", L"スピン左"},
    {"MPFSpinR", L"自旋右", L"Spin R", L"スピン右"},
    {"PrepareToLaunch", L"准备起飞", L"Prepare launch", L"離陸準備"},

    {"LieB", L"仰躺", L"Lie back", L"仰向け"},
    {"GetUpF", L"爬起", L"Get up F", L"起き上がり前"},
    {"GetUpB", L"仰起", L"Get up B", L"起き上がり後"},
    {"Crouching", L"蹲着", L"Crouching", L"しゃがみ中"},
    {"Kneel", L"跪下", L"Kneel", L"ひざまずく"},
    {"Kneeling", L"跪着", L"Kneeling", L"ひざまずき中"},
    {"ReadyToMed", L"准备打坐", L"Ready meditate", L"瞑想準備"},
    {"StopMed", L"结束打坐", L"Stop meditate", L"瞑想終了"},
    {"Collapse", L"倒下", L"Collapse", L"倒れる"},
    {"Revive", L"起身", L"Revive", L"起き上がり"},
    {"BlownBack", L"吹飞", L"Blown back", L"吹き飛ばされ"},
    {"FallForward", L"前倒", L"Fall forward", L"前倒れ"},
    {"HitBodyL", L"挨打左", L"Hit L", L"被弾左"},
    {"HitBodyR", L"挨打右", L"Hit R", L"被弾右"},
    {"LiftA", L"抬起", L"Lift", L"持ち上げ"},
    {"LiftEL", L"抬起左", L"Lift L", L"持ち上げ左"},
};

}  // namespace

const wchar_t* AnimLabel(UiLang lang, const char* id) {
  if (!id) return L"?";
  for (const auto& r : kRows) {
    if (std::strcmp(r.id, id) == 0) {
      switch (lang) {
        case UiLang::En: return r.en;
        case UiLang::Ja: return r.ja;
        default: return r.zh;
      }
    }
  }
  // Fallback: show raw id as wide-ish ASCII (stack buffer not OK) — return short marker.
  return L"?";
}

}  // namespace mos
