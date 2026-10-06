#pragma once

#include "con.h"
#include "mobdata.h"

#include <cstdint>

namespace aggroglow
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
    constexpr int kLabelShadeCount = 7;

    struct Label
    {
        char text[24]; // "Lv 20-23 EP-DC" at most 16 characters
        LabelShade shade;
    };

    LabelShade ShadeFor(Con con);

    // Before an examine: the level range and con range from the data ("Lv 20-23 EP-DC"), colored by the higher
    // con. After one: the exact level and the server's con ("Lv 22 DC"). "Lv ? ??" when the level is unknown.
    Label MakeLabel(const MobRecord* mob, const CheckResult* examined, int playerLevel);
}
