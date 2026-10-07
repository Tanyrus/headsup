#pragma once

#include "con.h"
#include "mobdata.h"

#include <cstdint>
#include <string>

namespace headsup
{
    // Which of the settings' level/con colors a label is drawn in.
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
        char text[48]; // "Lv 20-23 EP-DC [17190918 (0x1065006)] [PH]" at most 43 characters
        LabelShade shade;
    };

    // How a mob's server ID shows on its level line.
    enum class MobIdFormat : uint8_t
    {
        Off,
        LastThree, // its last three hex digits, its target index, as players name placeholders: "[006]"
        Full,      // "[17190918 (0x1065006)]"
    };
    constexpr int kMobIdFormatCount = static_cast<int>(MobIdFormat::Full) + 1;

    // The ID part of a level line: empty when off. On a lottery placeholder, when marked, "[PH]" in place of the last
    // three digits, or after the full ID.
    std::string MobIdText(uint32_t serverId, bool placeholder, MobIdFormat format, bool markPlaceholders);

    // A level line: the level and con when shown, then the ID part, colored by the con.
    Label LevelLine(const Label& level, bool showLevel, const std::string& id);

    LabelShade ShadeFor(Con con);

    // Before a /check: the level range and con range from the data ("Lv 20-23 EP-DC"), colored by the higher con.
    // After one: the exact level and the server's con ("Lv 22 DC"). "Lv ? ??" when the level is unknown.
    Label MakeLabel(const MobRecord* mob, const CheckResult* checked, int playerLevel);
}
