#include "con.h"

#include <algorithm>

namespace aggroglow
{
    namespace
    {
#include "con_tables.inc"
    }

    uint32_t BaseExp(ConTable table, int playerLevel, int mobLevel)
    {
        playerLevel = std::min(playerLevel, 99);
        if (playerLevel <= 0) return 0;
        const int row     = std::clamp(mobLevel - playerLevel + 44, 0, 59);
        const auto& exp   = table == ConTable::Era ? kEraTable : kModernTable;
        return exp[row][(playerLevel - 1) / 5];
    }

    bool IsTooWeak(ConTable table, int playerLevel, int mobLevel)
    {
        const uint32_t exp = BaseExp(table, playerLevel, mobLevel);
        if (table == ConTable::Era)
            return exp < 15; // Easy Prey starts at 15; Incredibly Easy Prey is disabled in the era curve
        if (exp >= 60) return false;          // Easy Prey or better
        return !(exp >= 1 && mobLevel >= 56); // Incredibly Easy Prey: at least 1 exp from a level 56+ mob
    }
}
