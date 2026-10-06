#include "con.h"

#include <algorithm>

namespace aggroglow
{
    namespace
    {
#include "con_tables.inc"
    }

    uint32_t BaseExp(int playerLevel, int mobLevel)
    {
        playerLevel = std::min(playerLevel, 99);
        if (playerLevel <= 0) return 0;
        const int row = std::clamp(mobLevel - playerLevel + 44, 0, 59);
        return kEraTable[row][(playerLevel - 1) / 5];
    }

    Con Difficulty(int playerLevel, int mobLevel)
    {
        const uint32_t exp = BaseExp(playerLevel, mobLevel);
        if (exp >= 400) return Con::IncrediblyTough;
        if (exp >= 200) return Con::VeryTough;
        if (exp >= 120) return Con::Tough;
        if (exp >= 100) return Con::EvenMatch;
        if (exp >= 50) return Con::DecentChallenge;
        if (exp >= 15) return Con::EasyPrey;
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
