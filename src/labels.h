#pragma once

#include "con.h"
#include "mobdata.h"

#include <cstdint>

namespace aggroglow
{
    struct Label
    {
        char text[24]; // "Lv 20-23 EP-DC" at most 16 characters
        uint32_t argb; // D3DCOLOR
    };

    // The color a con is drawn in (spec section 3); gray for anything outside Con.
    uint32_t ConArgb(Con con);

    // Before an examine: the level range and con range from the data ("Lv 20-23 EP-DC"), colored by the higher
    // con. After one: the exact level and the server's con ("Lv 22 DC"). "Lv ? ??" when the level is unknown.
    Label MakeLabel(const MobRecord* mob, const CheckResult* examined, int playerLevel);
}
