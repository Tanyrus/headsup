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
    constexpr int kCategoryCount = static_cast<int>(Category::NmWontAttack) + 1;
    constexpr int CategoryIndex(Category category) { return static_cast<int>(category); }

    struct PlayerState
    {
        int level    = 0;     // main job level; 0 while unknown (zoning, logging in)
        bool sitting = false; // resting, /sit or a chair: Phoenix lets Too Weak aggressive mobs aggro then
    };

    // Phoenix's CZoneEntities::tapMobAggro with its mob data. checkedLevel is the level a /check reported for this
    // spawn, which includes its level mod, or 0 without one.
    Category Classify(const MobRecord* mob, int checkedLevel, const PlayerState& player);
}
