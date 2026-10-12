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

    constexpr const char* kPlaceholderMark = "[PH]";

    struct Label
    {
        char text[24]; // "Lv 100-255 IEP-IT" at the longest
        LabelShade shade;
    };

    // A mob's server ID holds its zone above its target index, the ID's last three hex digits.
    constexpr int kTargetIndexWidth     = 12;
    constexpr uint32_t kTargetIndexBits = (1u << kTargetIndexWidth) - 1;

    // "[006]": the target index in three upper-case hex digits.
    std::string IdText(uint32_t serverId);
    // "[17190918]": the whole ID in decimal.
    std::string ServerIdText(uint32_t serverId);

    LabelShade ShadeFor(Con con);

    // "20-23", "22" once checked, "?" when the level is a script's or the mob unknown.
    std::string LevelText(const MobRecord* mob, const CheckResult* checked);
    // "EP-DC", "DC" once checked, "??" when the level or yours is unknown.
    std::string ConText(const MobRecord* mob, const CheckResult* checked, int playerLevel);

    Label MakeLabel(const MobRecord* mob, const CheckResult* checked, int playerLevel);
}
