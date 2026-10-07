#include "classifier.h"

namespace headsup
{
    bool IsSittingStatus(uint32_t status)
    {
        return status == 33 || status == 47 || (status >= 63 && status <= 73);
    }

    Category Classify(const MobRecord* mob, int examinedLevel, const PlayerState& player)
    {
        if (mob == nullptr) return Category::Unknown;
        const bool notorious  = (mob->flags & kMobNotorious) != 0;
        const Category attack = notorious ? Category::NmWillAttack : Category::WillAttack;
        const Category ignore = notorious ? Category::NmWontAttack : Category::WontAttack;

        const bool aggressive = (mob->flags & (kMobAggressive | kMobAlwaysAggro)) != 0;
        if (!aggressive || (mob->flags & (kMobNoAggro | kMobNeutral)) != 0) return ignore;
        if (mob->flags & kMobAlwaysAggro) return attack;
        // This spawn's examined level, else the top of the range: if any spawn of this mob can aggro, warn.
        const int level = examinedLevel > 0 ? examinedLevel : mob->maxLevel;
        // Unknown player level, or a level only a spawn script knows: warn rather than paint an aggressive mob safe.
        if (player.level <= 0 || level == 0) return attack;
        if (!IsTooWeak(player.level, level)) return attack;
        return player.sitting ? attack : ignore;
    }
}
