#pragma once

#include "con.h"
#include "mobdata.h"

#include <cstdint>
#include <string>

namespace headsup
{
    enum class LabelShade : uint8_t
    {
        Unknown, // the level or the player's level is not known
        TooWeak,
        EasyPrey, // and Incredibly Easy Prey
        DecentChallenge,
        EvenMatch,
        Tough,
        VeryTough, // and Incredibly Tough
    };
    constexpr int kLabelShadeCount = static_cast<int>(LabelShade::VeryTough) + 1;

    struct Label
    {
        char text[48];
        LabelShade shade;
    };

    // A mob's server ID holds its zone above its target index, the ID's last three hex digits.
    constexpr int kTargetIndexWidth     = 12;
    constexpr uint32_t kTargetIndexBits = (1u << kTargetIndexWidth) - 1;

    enum class MobIdFormat : uint8_t
    {
        Off,
        LastThree, // the target index, as players name placeholders: "[006]"
        Full,
    };
    constexpr int kMobIdFormatCount = static_cast<int>(MobIdFormat::Full) + 1;

    std::string MobIdText(uint32_t serverId, bool placeholder, MobIdFormat format, bool markPlaceholders);

    Label LevelLine(const Label& level, bool showLevel, const std::string& id);

    LabelShade ShadeFor(Con con);

    Label MakeLabel(const MobRecord* mob, const CheckResult* checked, int playerLevel);
}
