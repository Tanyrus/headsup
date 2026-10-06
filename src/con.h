#pragma once

#include <cstdint>

namespace aggroglow
{
    // Which Phoenix experience table and /check difficulty curve the server uses.
    enum class ConTable : uint8_t
    {
        Era,    // modules/era toau_experience_points.lua (level 75 era)
        Modern, // scripts/data/experience_table.lua with exp_difficulty_curve.lua
    };

    // Phoenix's charutils::GetBaseExp: base experience a player of playerLevel gets from a mob of mobLevel.
    uint32_t BaseExp(ConTable table, int playerLevel, int mobLevel);

    // Phoenix's charutils::CheckMob(...) == EMobDifficulty::TooWeak.
    bool IsTooWeak(ConTable table, int playerLevel, int mobLevel);
}
