#include "labels.h"

#include <cstdio>

namespace aggroglow
{
    namespace
    {
        constexpr uint32_t kGray = 0xFF999999; // 0.60, 0.60, 0.60
        // TW gray, IEP and EP green, DC blue, EM white, T yellow, VT and IT red.
        constexpr uint32_t kConArgb[kConCount] = {kGray, 0xFF66FF66, 0xFF66FF66, 0xFF73B3FF, 0xFFFFFFFF, 0xFFFFFF59,
            0xFFFF5959, 0xFFFF5959};
    }

    uint32_t ConArgb(Con con)
    {
        const auto i = static_cast<unsigned>(con);
        return i < kConCount ? kConArgb[i] : kGray;
    }

    Label MakeLabel(const MobRecord* mob, const CheckResult* examined, int playerLevel)
    {
        Label label{};
        label.argb = kGray;
        if (examined != nullptr && examined->level > 0)
        {
            std::snprintf(label.text, sizeof(label.text), "Lv %d %s", examined->level, Abbrev(examined->con));
            label.argb = ConArgb(examined->con);
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
        const Con easiest = Difficulty(playerLevel, low);
        const Con hardest = Difficulty(playerLevel, high);
        if (easiest == hardest)
            std::snprintf(label.text, sizeof(label.text), "Lv %s %s", levels, Abbrev(hardest));
        else
            std::snprintf(label.text, sizeof(label.text), "Lv %s %s-%s", levels, Abbrev(easiest), Abbrev(hardest));
        label.argb = ConArgb(hardest);
        return label;
    }
}
