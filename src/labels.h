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

    // Between the level line's parts: "Lv 22-23 IT [106]".
    constexpr const char* kLabelSeparator = " ";

    struct Label
    {
        static constexpr size_t kNoMark = static_cast<size_t>(-1);
        char text[72];
        LabelShade shade;
        size_t markAt = kNoMark; // where the placeholder mark starts in text: it is drawn in the placeholder's color
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
