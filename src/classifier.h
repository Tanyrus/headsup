#pragma once

#include "con.h"
#include "mobdb.h"

#include <cstdint>

namespace aggroglow
{
    enum class Category : uint8_t
    {
        WillAttack = 0,
        WontAttack = 1,
        Unknown    = 2,
    };
    constexpr int kCategoryCount = 3;

    struct PlayerState
    {
        int level    = 0;     // main job level; 0 while unknown (zoning, logging in)
        bool sitting = false; // resting, /sit or a chair: Phoenix lets Too Weak aggressive mobs aggro then
        ConTable table = ConTable::Era;
    };

    // Phoenix's CBattleEntity::isSitting(): healing (33), sit (47) and sitchair 0-10 (63-73).
    bool IsSittingStatus(uint32_t status);

    // Phoenix's CZoneEntities::tapMobAggro, using MobDB's aggro flag and the mob's highest level.
    Category Classify(const MobRecord* mob, const PlayerState& player);
}
