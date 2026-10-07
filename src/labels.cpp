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

    LabelShade ShadeFor(Con con)
    {
        const auto i = static_cast<unsigned>(con);
        return i < kConCount ? kConShades[i] : LabelShade::Unknown;
    }

    Label MakeLabel(const MobRecord* mob, const CheckResult* checked, int playerLevel)
    {
        Label label{};
        label.shade = LabelShade::Unknown;
        if (checked != nullptr && checked->level > 0)
        {
            std::snprintf(label.text, sizeof(label.text), "Lv %d %s", checked->level, Abbrev(checked->con));
            label.shade = ShadeFor(checked->con);
            return label;
        }
        if (mob == nullptr || mob->maxLevel == 0)
        {
            std::snprintf(label.text, sizeof(label.text), "Lv ? ??");
            return label;
        }

        const int low = mob->minLevel, high = mob->maxLevel;
        char levels[12];
        if (low == high)
            std::snprintf(levels, sizeof(levels), "%d", high);
        else
            std::snprintf(levels, sizeof(levels), "%d-%d", low, high);
        if (playerLevel <= 0)
        {
            std::snprintf(label.text, sizeof(label.text), "Lv %s ??", levels);
            return label;
        }
        const Con easiest = Difficulty(playerLevel, low + mob->expLevelMod);
        const Con hardest = Difficulty(playerLevel, high + mob->expLevelMod);
        if (easiest == hardest)
            std::snprintf(label.text, sizeof(label.text), "Lv %s %s", levels, Abbrev(hardest));
        else
            std::snprintf(label.text, sizeof(label.text), "Lv %s %s-%s", levels, Abbrev(easiest), Abbrev(hardest));
        label.shade = ShadeFor(hardest);
        return label;
    }
}
