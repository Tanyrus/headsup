#pragma once

#include "con.h"
#include "mobdata.h"

#include <cstdint>

namespace headsup
{
    enum class Category : uint8_t
    {
        WillAttack   = 0,
        WontAttack   = 1,
        Unknown      = 2,
        NmWillAttack = 3, // notorious monsters
        NmWontAttack = 4,
    };
    constexpr int kCategoryCount = 5;

    struct PlayerState
    {
        int level    = 0;     // main job level; 0 while unknown (zoning, logging in)
        bool sitting = false; // resting, /sit or a chair: Phoenix lets Too Weak aggressive mobs aggro then
    };

    // Phoenix's CBattleEntity::isSitting(): healing (33), sit (47) and sitchair 0-10 (63-73).
    bool IsSittingStatus(uint32_t status);

    // Phoenix's CZoneEntities::tapMobAggro with its mob data. examinedLevel is the level a /check reported
    // for this spawn, or 0 when it has not been examined.
    Category Classify(const MobRecord* mob, int examinedLevel, const PlayerState& player);
}
