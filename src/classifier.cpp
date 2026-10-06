#include "classifier.h"

namespace aggroglow
{
    bool IsSittingStatus(uint32_t status)
    {
        return status == 33 || status == 47 || (status >= 63 && status <= 73);
    }

    Category Classify(const MobRecord* mob, const PlayerState& player)
    {
        if (mob == nullptr) return Category::Unknown;
        if (!mob->aggro) return Category::WontAttack;
        // Unknown player level, or MobDB's 0/0 for an unknown mob level: warn rather than paint an aggressive mob as safe.
        if (player.level <= 0 || mob->maxLevel == 0) return Category::WillAttack;
        // The top of the level range: if any spawn of this mob can aggro, warn.
        if (!IsTooWeak(player.table, player.level, mob->maxLevel)) return Category::WillAttack;
        return player.sitting ? Category::WillAttack : Category::WontAttack;
    }
}
