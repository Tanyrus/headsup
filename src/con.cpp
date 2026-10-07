#include "con.h"

#include "generated/phoenix_rules.h"

#include <algorithm>
#include <iterator>

namespace headsup
{
    namespace
    {
        constexpr int kHighestCountedLevel = 99; // Phoenix's GetBaseExp counts no player level above this
        constexpr int kLevelsPerColumn     = 5;
        constexpr int kLastRow             = static_cast<int>(std::size(kBaseExp)) - 1;
    }

    uint32_t BaseExp(int playerLevel, int mobLevel)
    {
        playerLevel = std::min(playerLevel, kHighestCountedLevel);
        if (playerLevel <= 0) return 0;
        const int row = std::clamp(mobLevel - playerLevel - kFirstDifference, 0, kLastRow);
        return kBaseExp[row][(playerLevel - 1) / kLevelsPerColumn];
    }

    Con Difficulty(int playerLevel, int mobLevel)
    {
        const uint32_t exp = BaseExp(playerLevel, mobLevel);
        if (exp == 0) return Con::TooWeak;
        for (const DifficultyStep& step : kDifficulty)
            if (exp >= step.minExp) return step.con;
        if (exp >= kIncrediblyEasyPreyMinExp && mobLevel >= kIncrediblyEasyPreyMinLevel) return Con::IncrediblyEasyPrey;
        return Con::TooWeak;
    }

    bool IsTooWeak(int playerLevel, int mobLevel)
    {
        return Difficulty(playerLevel, mobLevel) == Con::TooWeak;
    }

    const char* Abbrev(Con con)
    {
        static const char* const kAbbrev[kConCount] = {"TW", "IEP", "EP", "DC", "EM", "T", "VT", "IT"};
        const auto i = static_cast<unsigned>(con);
        return i < kConCount ? kAbbrev[i] : "??";
    }
}
