#include "labels.h"

#include <cstdio>

namespace headsup
{
    namespace
    {
        constexpr LabelShade kConShades[kConCount] = {LabelShade::TooWeak, LabelShade::EasyPrey, LabelShade::EasyPrey,
            LabelShade::DecentChallenge, LabelShade::EvenMatch, LabelShade::Tough, LabelShade::VeryTough,
            LabelShade::VeryTough};
    }

    std::string IdText(uint32_t serverId)
    {
        char text[16];
        std::snprintf(text, sizeof(text), "[%03X]", static_cast<unsigned>(serverId & kTargetIndexBits));
        return text;
    }

    std::string ServerIdText(uint32_t serverId)
    {
        char text[16];
        std::snprintf(text, sizeof(text), "[%u]", static_cast<unsigned>(serverId));
        return text;
    }

    LabelShade ShadeFor(Con con)
    {
        const auto i = static_cast<unsigned>(con);
        return i < kConCount ? kConShades[i] : LabelShade::Unknown;
    }

    std::string LevelText(const MobRecord* mob, const CheckResult* checked)
    {
        if (checked != nullptr && checked->level > 0) return std::to_string(checked->level);
        if (mob == nullptr || mob->maxLevel == 0) return "?";
        if (mob->minLevel == mob->maxLevel) return std::to_string(mob->maxLevel);
        return std::to_string(mob->minLevel) + "-" + std::to_string(mob->maxLevel);
    }

    std::string ConText(const MobRecord* mob, const CheckResult* checked, int playerLevel)
    {
        if (checked != nullptr && checked->level > 0) return Abbrev(checked->con);
        if (mob == nullptr || mob->maxLevel == 0 || playerLevel <= 0) return "??";
        const Con easiest = Difficulty(playerLevel, mob->minLevel + mob->expLevelMod);
        const Con hardest = Difficulty(playerLevel, mob->maxLevel + mob->expLevelMod);
        return easiest == hardest ? Abbrev(hardest) : std::string(Abbrev(easiest)) + "-" + Abbrev(hardest);
    }

    Label MakeLabel(const MobRecord* mob, const CheckResult* checked, int playerLevel)
    {
        Label label{};
        label.shade = LabelShade::Unknown;
        const std::string text = "Lv " + LevelText(mob, checked) + " " + ConText(mob, checked, playerLevel);
        std::snprintf(label.text, sizeof(label.text), "%s", text.c_str());
        if (checked != nullptr && checked->level > 0)
            label.shade = ShadeFor(checked->con);
        else if (mob != nullptr && mob->maxLevel != 0 && playerLevel > 0)
            label.shade = ShadeFor(Difficulty(playerLevel, mob->maxLevel + mob->expLevelMod));
        return label;
    }
}
