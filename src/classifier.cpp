#include "classifier.h"

namespace headsup
{
    Category Classify(const MobRecord* mob, int checkedLevel, const PlayerState& player)
    {
        if (mob == nullptr) return Category::Unknown;
        const bool notorious  = (mob->flags & kMobNotorious) != 0;
        const Category attack = notorious ? Category::NmWillAttack : Category::WillAttack;
        const Category ignore = notorious ? Category::NmWontAttack : Category::WontAttack;

        if (!IsAggressive(*mob)) return ignore;
        if (mob->flags & kMobAlwaysAggro) return attack;
        // Unknown player level, or a level only a spawn script knows: warn rather than paint an aggressive mob safe.
        if (player.level <= 0 || (checkedLevel <= 0 && mob->maxLevel == 0)) return attack;
        // This spawn's checked level, else the top of the range: if any spawn of this mob can aggro, warn.
        const int level = checkedLevel > 0 ? checkedLevel : mob->maxLevel + mob->expLevelMod;
        if (!IsTooWeak(player.level, level)) return attack;
        return player.sitting ? attack : ignore;
    }
}
